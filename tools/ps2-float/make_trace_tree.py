"""Build a scratch source tree for a matcher trace core (docs/ps2-float.md "The matcher").

usage: make_trace_tree.py OUT_DIR
then:  SSX_CORE_CFLAGS=-DSSX_PS2_EXACT_FPU=1 CORE_OUT=... sh OUT_DIR/web/build-core.sh

OUT_DIR gets symlinks to the repository's top-level entries; web/ and engine/ (and their generated/) are directories of
symlinks, so quoted "../engine/x" includes stay in the tree. The arithmetic helpers are replaced by patched copies that
record every call's site (std::source_location), operands and result while enabled (tools/ps2-float/ps2_trace.hpp,
exports ps2_trace_*). Everything else is the live tree, the SSX_PS2_EXACT_FPU switch included. Every patch asserts the
exact text it replaces, and files are written as real copies, never through a symlink into the live tree.
A trace core is a measuring instrument, never a deploy candidate.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

TRACE_EXPORTS = r"""
// ---- ps2 trace exports (tools/ps2-float/make_trace_tree.py) ----
extern "C" EMSCRIPTEN_KEEPALIVE void ps2_trace_begin(){ssx::ps2trace::entries.clear();ssx::ps2trace::enabled=true;}
extern "C" EMSCRIPTEN_KEEPALIVE void ps2_trace_end(){ssx::ps2trace::enabled=false;}
extern "C" EMSCRIPTEN_KEEPALIVE int ps2_trace_count(){return int(ssx::ps2trace::entries.size());}
extern "C" EMSCRIPTEN_KEEPALIVE const void* ps2_trace_entries(){return ssx::ps2trace::entries.data();}
extern "C" EMSCRIPTEN_KEEPALIVE int ps2_trace_site_count(){return int(ssx::ps2trace::sites.size());}
extern "C" EMSCRIPTEN_KEEPALIVE const char* ps2_trace_site(int k){return ssx::ps2trace::sites.at(size_t(k)).c_str();}
"""


def mirror(source, target, recurse=()):
    """target as a real directory of symlinks to source's entries."""
    target.mkdir(parents=True, exist_ok=True)
    for entry in source.iterdir():
        link = target / entry.name
        if entry.name in recurse and entry.is_dir():
            mirror(entry, link)
            continue
        if link.is_symlink():
            link.unlink()
        if not link.exists():
            link.symlink_to(entry)


def write_copy(path, text):
    """Write a tree file as a real file: never through a symlink into the live tree."""
    if path.is_symlink():
        path.unlink()
    path.write_text(text)


def replace_once(text, old, new, name):
    assert text.count(old) == 1, f'{name}: {old[:60]!r} ({text.count(old)} matches)'
    return text.replace(old, new)


def trace_terrain(engine):
    text = (ROOT / 'engine/terrain_contact_math.hpp').read_text()
    for name in ('add', 'sub', 'mul', 'div'):
        text = replace_once(text, f'inline float {name}(float a,float b){{\n', f'inline float {name}Untraced(float a,float b){{\n', name)
    # Plain overloads with a defaulted call site (terrain_original's names share overload sets with local vector helpers,
    # which a function object would make ambiguous).
    wrappers = ''.join(f'inline float {name}(float a,float b,std::source_location loc=std::source_location::current())'
                       f'{{return ps2trace::record(ps2trace::{op},a,b,{name}Untraced(a,b),loc);}}\n'
                       for name, op in (('add', 'Add'), ('sub', 'Sub'), ('mul', 'Mul'), ('div', 'Div')))
    text = replace_once(text, 'inline float sum3(float a,float b,float c)', wrappers + 'inline float sum3(float a,float b,float c)', 'sum3')
    text = replace_once(text, 'inline float sqrt(float a){\n', 'inline float sqrtUntraced(float a){\n', 'sqrt')
    text = replace_once(text, 'using Rounding=OriginalRounding;',
                        'inline float sqrt(float a,std::source_location loc=std::source_location::current())'
                        '{return ps2trace::record(ps2trace::Sqrt,a,0.f,sqrtUntraced(a),loc);}\nusing Rounding=OriginalRounding;', 'Rounding')
    write_copy(engine / 'terrain_contact_math.hpp', text)
    # camera_transform.hpp takes terrain_original::mul's address: give it a function object.
    camera = (ROOT / 'engine/camera_transform.hpp').read_text()
    camera = replace_once(camera, 'M=terrain_original::mul;', 'M=ps2trace::TracedBinary{terrain_original::mulUntraced,ps2trace::Mul};', 'camera')
    write_copy(engine / 'camera_transform.hpp', camera)


def trace_original(engine):
    text = (ROOT / 'engine/original_float.hpp').read_text()
    text = replace_once(text, 'inline float originalScalarAdd(float a,float b){return originalScalarAddSub(a,b,false);}',
                        'inline float originalScalarAddUntraced(float a,float b){return originalScalarAddSub(a,b,false);}\n'
                        'inline constexpr ps2trace::TracedBinary originalScalarAdd{originalScalarAddUntraced,ps2trace::EeAdd};', 'add')
    text = replace_once(text, 'inline float originalScalarSubtract(float a,float b){return originalScalarAddSub(a,b,true);}',
                        'inline float originalScalarSubtractUntraced(float a,float b){return originalScalarAddSub(a,b,true);}\n'
                        'inline constexpr ps2trace::TracedBinary originalScalarSubtract{originalScalarSubtractUntraced,ps2trace::EeSub};', 'sub')
    # DIV.S / SQRT.S have one definition per platform branch: rename them all, and add the function objects at the end.
    for name, parameters in (('originalScalarDivide', '(float a,float b){'), ('originalScalarSqrt', '(float value){')):
        assert text.count(f'inline float {name}{parameters}') >= 1, name
        text = text.replace(f'inline float {name}{parameters}', f'inline float {name}Untraced{parameters}')
    closing = text.rindex('}')
    text = (text[:closing] + 'inline constexpr ps2trace::TracedBinary originalScalarDivide{originalScalarDivideUntraced,ps2trace::EeDiv};\n'
            'inline constexpr ps2trace::TracedUnary originalScalarSqrt{originalScalarSqrtUntraced,ps2trace::EeSqrt};\n' + text[closing:])
    write_copy(engine / 'original_float.hpp', text)


def trace_collision(engine):
    text = (ROOT / 'engine/collision_scalar.hpp').read_text()
    text = replace_once(text, 'inline float divide(float a,float b) {\n', 'inline float divideUntraced(float a,float b) {\n', 'divide')
    text = replace_once(text, 'inline float squareRoot(float x) {\n', 'inline float squareRootUntraced(float x) {\n', 'squareRoot')
    text = replace_once(text, 'inline float constant(uint32_t bits)',
                        'inline constexpr ps2trace::TracedBinary divide{divideUntraced,ps2trace::CollisionDiv};\n'
                        'inline constexpr ps2trace::TracedUnary squareRoot{squareRootUntraced,ps2trace::CollisionSqrt};\n'
                        'inline float constant(uint32_t bits)', 'constant')
    write_copy(engine / 'collision_scalar.hpp', text)


def main():
    out = Path(sys.argv[1]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    for entry in ROOT.iterdir():
        if entry.name in ('engine', 'web', '.git'):
            continue
        link = out / entry.name
        if not link.exists() and not link.is_symlink():
            link.symlink_to(entry)
    mirror(ROOT / 'web', out / 'web', recurse=('generated',))
    engine = out / 'engine'
    mirror(ROOT / 'engine', engine, recurse=('generated',))
    software = (ROOT / 'engine/software_float.hpp').read_text()
    software = replace_once(software, '#include <cstdint>\n', '#include <cstdint>\n#include "ps2_trace.hpp"\n', 'software_float')
    write_copy(engine / 'software_float.hpp', software)
    trace_header = engine / 'ps2_trace.hpp'
    if trace_header.is_symlink() or trace_header.exists():
        trace_header.unlink()
    trace_header.symlink_to(ROOT / 'tools/ps2-float/ps2_trace.hpp')
    trace_terrain(engine)
    trace_original(engine)
    trace_collision(engine)
    core = (ROOT / 'web/core.cpp').read_text()
    write_copy(out / 'web/core.cpp', core + TRACE_EXPORTS)
    # The trace buffers are plain globals: skip the rider-global and snapshot-registry checks in this build.
    build = (ROOT / 'web/build-core.sh').read_text()
    build = replace_once(build, 'node web/check-rider-globals.mjs "$tmp/core.map"', 'true # trace build', 'build globals')
    build = replace_once(build, 'node web/check-snapshot-registry.mjs "$tmp/core.map"', 'true # trace build', 'build registry')
    write_copy(out / 'web/build-core.sh', build)
    print(out)


if __name__ == '__main__':
    main()
