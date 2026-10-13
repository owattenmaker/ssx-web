// Rider contexts in a native build (docs/core-migration.md "Native core"): the wasm core switches the six riders' state by setting
// __tls_base (web/rider_context.cpp, web/rider_tls.S); a native process cannot swap its TLS register (macOS arm64 does not let user
// code), so here each rider context is a thread of its own, its RIDER_LOCAL state that thread's thread_local storage. Only one runs
// at a time: running work "in a context" posts it to that context's thread and waits, serving its own queue meanwhile (a context's
// work may call back into the waiting one: the rider pairs reach every rider from inside one rider's 121750). The human's context is
// the main thread. Deterministic: the threads hand one task back and forth, never two at once.
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>

void rider_statics_core();
void rider_statics_rails();
void rider_statics_world();
void rider_statics_animation();
void rider_statics_environment();

namespace {
struct Context;
struct Task {
    std::function<void()> fn;
    Context* from = nullptr;
    std::exception_ptr error;
    bool done = false;
};
struct Context {
    std::mutex m;
    std::condition_variable cv;
    std::deque<Task*> queue;
    bool stop = false;
    std::thread thread;
};
Context humanContext;
thread_local Context* current = nullptr;
Context* self() {
    return current ? current : &humanContext;
}
// Serves this context's queue until done() holds (checked under its lock).
template <class Done>
void serve(Context* c, Done done) {
    for (;;) {
        Task* t = nullptr;
        {
            std::unique_lock<std::mutex> lock(c->m);
            c->cv.wait(lock, [&] { return done() || !c->queue.empty(); });
            if (done()) {
                return;
            }
            t = c->queue.front();
            c->queue.pop_front();
        }
        try {
            t->fn();
        } catch (...) {
            t->error = std::current_exception();
        }
        {
            std::lock_guard<std::mutex> lock(t->from->m);
            t->done = true;
        }
        t->from->cv.notify_all();
    }
}
}  // namespace

// Runs fn in a rider context (the calling thread waits and serves its own queue).
void native_run_in_context(void* block, const std::function<void()>& fn) {
    Context* target = static_cast<Context*>(block);
    Context* me = self();
    if (target == me) {
        fn();
        return;
    }
    Task t;
    t.fn = fn;
    t.from = me;
    {
        std::lock_guard<std::mutex> lock(target->m);
        target->queue.push_back(&t);
    }
    target->cv.notify_all();
    serve(me, [&] { return t.done; });
    if (t.error) {
        std::rethrow_exception(t.error);
    }
}

extern "C" {
void* native_tls_base(void) {
    return self();
}
// A new rider context: a thread whose RIDER_LOCAL state starts as a fresh core's (thread_local initialisation, then the units'
// static-initialisation work, as web/rider_context.cpp runs it in a new block).
void* rider_context_create() {
    Context* c = new Context;
    c->thread = std::thread([c] {
        current = c;
        serve(c, [c] { return c->stop; });
    });
    native_run_in_context(c, [] {
        rider_statics_core();
        rider_statics_rails();
        rider_statics_world();
        rider_statics_animation();
        rider_statics_environment();
    });
    return c;
}
void* rider_context_current() {
    return self();
}
// The wasm build's direct switches (the page's rider views, web/ai-racers.js) do not exist here: the native host runs work in a
// context through native_run_in_context.
void rider_context_enter(void*) {
    throw std::runtime_error("rider_context_enter: native contexts run work through native_run_in_context");
}
void rider_tls_set(void*) {
    throw std::runtime_error("rider_tls_set: native contexts run work through native_run_in_context");
}
unsigned rider_context_bytes() {
    return 0;
}
}
