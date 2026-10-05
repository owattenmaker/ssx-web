"""Build a scratch source tree whose core arithmetic runs on engine/ps2_fpu.hpp (docs/ps2-float.md "Core swap").

usage: make_swap_tree.py OUT_DIR [--trace]

OUT_DIR gets symlinks to the repository's top-level entries; web/ and engine/ (and their generated/) are
directories of symlinks, so quoted "../engine/x" includes stay in the tree, and engine/'s arithmetic layer is
replaced by patched copies:
  - software_float.hpp: add / sub / mul / div / sqrt (the toward-zero helpers every rider context uses)
    become the PS2's (guard-masked adder, multiplier with its one-ULP deficit, SRT divide / root);
  - original_float.hpp: the EE scalar add / sub, DIV.S and SQRT.S become the PS2's in every rounding policy
    (the EE FPU has no rounding-mode field);
  - terrain_contact_math.hpp: the native build's toward-zero branch matches the WebAssembly one;
  - collision_scalar.hpp: DIV.S / SQRT.S become the PS2's;
  - stage_script_vm.hpp: the LUN VM's MUL.S / DIV.S / CVT.W.S become the PS2's.
--trace also wraps the helpers with call-site recording (tools/ps2-float/ps2_trace.hpp) for the matcher.
Every patch asserts the exact text it replaces, so a change to the live file stops the script instead of
being silently skipped. The live tree is never written. Build with:
  CORE_OUT=... sh OUT_DIR/web/build-core.sh
"""
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Each patch inserts an early return on the runtime switch software_float::exactArithmetic (default off: mode-1 results,
# bit for bit), so one core runs both profiles. The comparer turns it on at the capture's first tick
# (tools/ps2-float/exact_hook.mjs): an exact-mode capture from a mode-1 baseline carries mode-1 history up to there.
EXACT = 'software_float::exactArithmetic'
PATCHES = {
    'software_float.hpp': [
        ('#include <cstdint>\n', '#include <cstdint>\n#include "ps2_fpu.hpp"\n'),
        ('namespace ssx::software_float {\n', 'namespace ssx::software_float {\n'
         '// The console model (engine/ps2_fpu.hpp) instead of mode-1 arithmetic (tools/ps2-float/make_swap_tree.py).\n'
         'inline bool exactArithmetic = false;\n'),
        ('inline float add(float a,float b){\n', 'inline float add(float a,float b){\n    if(exactArithmetic)return ssx::ps2fpu::add(a,b);\n'),
        ('inline float mul(float a,float b){\n', 'inline float mul(float a,float b){\n    if(exactArithmetic)return ssx::ps2fpu::mul(a,b);\n'),
        ('inline float div(float a,float b){return eeFlush(divExact(eeFlush(a),eeFlush(b)));}\n',
         'inline float div(float a,float b){if(exactArithmetic)return ssx::ps2fpu::div(a,b);return eeFlush(divExact(eeFlush(a),eeFlush(b)));}\n'),
        ('inline float sqrt(float a){return eeFlush(sqrtExact(eeFlush(a)));}\n',
         'inline float sqrt(float a){if(exactArithmetic)return ssx::ps2fpu::sqrt(a);return eeFlush(sqrtExact(eeFlush(a)));}\n'),
    ],
    'original_float.hpp': [
        ('inline float originalScalarAddSub(float a,float b,bool subtract){\n',
         'inline float originalScalarAddSub(float a,float b,bool subtract){\n'
         f'    if({EXACT})return subtract?ps2fpu::sub(a,b):ps2fpu::add(a,b);\n'),
    ],
    'collision_scalar.hpp': [
        ('inline float divide(float a,float b) {\n', f'inline float divide(float a,float b) {{\n    if({EXACT})return ps2fpu::div(a,b);\n'),
        ('inline float squareRoot(float x) {\n', f'inline float squareRoot(float x) {{\n    if({EXACT})return ps2fpu::sqrt(x);\n'),
    ],
    'stage_script_vm.hpp': [
        ('inline float mulS(float a,float b){\n', f'inline float mulS(float a,float b){{\n    if({EXACT})return ps2fpu::mul(a,b);\n'),
        ('inline float divS(float a,float b){\n', f'inline float divS(float a,float b){{\n    if({EXACT})return ps2fpu::div(a,b);\n'),
        ('inline int32_t cvtWS(float x){\n',
         f'inline int32_t cvtWS(float x){{\n    if({EXACT})return ps2fpu::floatToIntBits(std::bit_cast<uint32_t>(x));\n'),
    ],
}

# Patches that occur more than once (the WebAssembly and native definitions of DIV.S / SQRT.S).
MULTI_PATCHES = {
    'original_float.hpp': [
        ('inline float originalScalarDivide(float a,float b){', f'inline float originalScalarDivide(float a,float b){{if({EXACT})return ps2fpu::div(a,b);'),
        ('inline float originalScalarSqrt(float value){', f'inline float originalScalarSqrt(float value){{if({EXACT})return ps2fpu::sqrt(value);'),
    ],
}

# Form fixes in web/ (files there are replaced by patched copies in the tree): places where the port computes a value
# another way than the PS2 does, which agree under mode-1 arithmetic and not on the console model.
WEB_PATCHES = {
    # 13D8F0: dt = mul.s rider+0x300 (fs), gp-0x7064 1/60 (ft). With +0x300 = 1.0 as fs the multiplier comes back one ULP
    # low (0x3C888888), where timeScale / 60 is 0x3C888889.
    'core.cpp': [
        ('originalGroundVisualTargets(physicsProfile,physicsState,physicsState.timeScale/60.f,relative);',
         'originalGroundVisualTargets(physicsProfile,physicsState,software_float::exactArithmetic?ps2fpu::mul(physicsState.timeScale,'
         'std::bit_cast<float>(0x3c888889u)):physicsState.timeScale/60.f,relative);'),
    ],
}

ARITH_EXPORT = """
// ---- the arithmetic profile switch (tools/ps2-float/make_swap_tree.py) ----
extern "C" EMSCRIPTEN_KEEPALIVE void ps2_arith_exact(int on){ssx::software_float::exactArithmetic=on!=0;}
"""

def patch_terrain(text):
    for name in ('add', 'sub', 'mul', 'div'):
        operator = {'add': '+', 'sub': '-', 'mul': '*', 'div': '/'}[name]
        wasm = f' if(originalRoundingMode==FE_TOWARDZERO)return software_float::{name}(a,b);\n'
        assert text.count(wasm) == 1, name
        text = text.replace(wasm, f' if({EXACT}||originalRoundingMode==FE_TOWARDZERO)return software_float::{name}(a,b);\n')
        native = f' volatile float r=software_float::eeFlush(software_float::eeFlush(a){operator}software_float::eeFlush(b));return software_float::eeFlush(r);\n'
        assert text.count(native) == 1, name
        text = text.replace(native, f' if({EXACT}||std::fegetround()==FE_TOWARDZERO)return software_float::{name}(a,b);\n{native}')
    wasm_sqrt = ' if(originalRoundingMode==FE_TOWARDZERO)return software_float::sqrt(a);\n'
    assert text.count(wasm_sqrt) == 1
    text = text.replace(wasm_sqrt, f' if({EXACT}||originalRoundingMode==FE_TOWARDZERO)return software_float::sqrt(a);\n')
    return text


def mirror(source, target, recurse=()):
    """target as a real directory of symlinks to source's entries, so a quoted "../x" include resolves inside the tree."""
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


TRACE_EXPORTS = r"""
// ---- ps2 trace exports (tools/ps2-float/make_swap_tree.py --trace) ----
extern "C" EMSCRIPTEN_KEEPALIVE void ps2_trace_begin(){ssx::ps2trace::entries.clear();ssx::ps2trace::enabled=true;}
extern "C" EMSCRIPTEN_KEEPALIVE void ps2_trace_end(){ssx::ps2trace::enabled=false;}
extern "C" EMSCRIPTEN_KEEPALIVE int ps2_trace_count(){return int(ssx::ps2trace::entries.size());}
extern "C" EMSCRIPTEN_KEEPALIVE const void* ps2_trace_entries(){return ssx::ps2trace::entries.data();}
extern "C" EMSCRIPTEN_KEEPALIVE int ps2_trace_site_count(){return int(ssx::ps2trace::sites.size());}
extern "C" EMSCRIPTEN_KEEPALIVE const char* ps2_trace_site(int k){return ssx::ps2trace::sites.at(size_t(k)).c_str();}
"""


def apply_trace(engine, web):
    """Turn the helpers into call-site-recording function objects (a trace build: never a deploy candidate)."""
    software = (engine / 'software_float.hpp').read_text()
    software = software.replace('#include "ps2_fpu.hpp"\n', '#include "ps2_fpu.hpp"\n#include "ps2_trace.hpp"\n', 1)
    (engine / 'software_float.hpp').write_text(software)
    trace_header = engine / 'ps2_trace.hpp'
    if trace_header.is_symlink() or trace_header.exists():
        trace_header.unlink()
    trace_header.symlink_to(ROOT / 'tools/ps2-float/ps2_trace.hpp')
    terrain = (engine / 'terrain_contact_math.hpp').read_text()
    for name in ('add', 'sub', 'mul', 'div'):
        old = f'inline float {name}(float a,float b){{\n'
        assert terrain.count(old) == 1, name
        terrain = terrain.replace(old, f'inline float {name}Untraced(float a,float b){{\n')
    # Plain overloads with a defaulted call site (terrain_original's names share overload sets with local vector helpers,
    # which a function object would make ambiguous); camera_transform.hpp's one address-taking use gets a function object.
    wrappers = ''.join(f'inline float {name}(float a,float b,std::source_location loc=std::source_location::current())'
                       f'{{return ps2trace::record(ps2trace::{op},a,b,{name}Untraced(a,b),loc);}}\n'
                       for name, op in (('add', 'Add'), ('sub', 'Sub'), ('mul', 'Mul'), ('div', 'Div')))
    terrain = terrain.replace('inline float sum3(float a,float b,float c)', wrappers + 'inline float sum3(float a,float b,float c)', 1)
    old_sqrt = 'inline float sqrt(float a){\n'
    assert terrain.count(old_sqrt) == 1
    terrain = terrain.replace(old_sqrt, 'inline float sqrtUntraced(float a){\n')
    terrain = terrain.replace('using Rounding=OriginalRounding;',
                              'inline float sqrt(float a,std::source_location loc=std::source_location::current())'
                              '{return ps2trace::record(ps2trace::Sqrt,a,0.f,sqrtUntraced(a),loc);}\nusing Rounding=OriginalRounding;', 1)
    (engine / 'terrain_contact_math.hpp').write_text(terrain)
    camera = (ROOT / 'engine/camera_transform.hpp').read_text()
    old_camera = 'M=terrain_original::mul;'
    assert camera.count(old_camera) == 1
    camera = camera.replace(old_camera, 'M=ps2trace::TracedBinary{terrain_original::mulUntraced,ps2trace::Mul};')
    (engine / 'camera_transform.hpp').unlink()
    (engine / 'camera_transform.hpp').write_text(camera)
    original = (engine / 'original_float.hpp').read_text()
    replacements = [
        ('inline float originalScalarAdd(float a,float b){return originalScalarAddSub(a,b,false);}',
         'inline float originalScalarAddUntraced(float a,float b){return originalScalarAddSub(a,b,false);}\n'
         'inline constexpr ps2trace::TracedBinary originalScalarAdd{originalScalarAddUntraced,ps2trace::EeAdd};'),
        ('inline float originalScalarSubtract(float a,float b){return originalScalarAddSub(a,b,true);}',
         'inline float originalScalarSubtractUntraced(float a,float b){return originalScalarAddSub(a,b,true);}\n'
         'inline constexpr ps2trace::TracedBinary originalScalarSubtract{originalScalarSubtractUntraced,ps2trace::EeSub};'),
        ('inline float originalScalarDivide(float a,float b){return ps2fpu::div(a,b);}',
         'inline float originalScalarDivideUntraced(float a,float b){return ps2fpu::div(a,b);}\n'
         'inline constexpr ps2trace::TracedBinary originalScalarDivide{originalScalarDivideUntraced,ps2trace::EeDiv};'),
        ('inline float originalScalarSqrt(float value){return ps2fpu::sqrt(value);}',
         'inline float originalScalarSqrtUntraced(float value){return ps2fpu::sqrt(value);}\n'
         'inline constexpr ps2trace::TracedUnary originalScalarSqrt{originalScalarSqrtUntraced,ps2trace::EeSqrt};'),
    ]
    for old, new in replacements:
        assert original.count(old) == 1, old[:50]
        original = original.replace(old, new)
    (engine / 'original_float.hpp').write_text(original)
    collision = (engine / 'collision_scalar.hpp').read_text()
    for old, new in [
        ('inline float divide(float a,float b) {\n', 'inline float divideUntraced(float a,float b) {\n'),
        ('inline float squareRoot(float x) {\n', 'inline float squareRootUntraced(float x) {\n'),
        ('inline float constant(uint32_t bits)', 'inline constexpr ps2trace::TracedBinary divide{divideUntraced,ps2trace::CollisionDiv};\n'
         'inline constexpr ps2trace::TracedUnary squareRoot{squareRootUntraced,ps2trace::CollisionSqrt};\ninline float constant(uint32_t bits)'),
    ]:
        assert collision.count(old) == 1, old[:40]
        collision = collision.replace(old, new)
    (engine / 'collision_scalar.hpp').write_text(collision)
    core = (web / 'core.cpp').read_text()
    (web / 'core.cpp').write_text(core + TRACE_EXPORTS)
    # A trace build adds plain globals (the trace buffers): skip the rider-global and snapshot-registry checks.
    build = (ROOT / 'web/build-core.sh').read_text()
    build = build.replace('node web/check-rider-globals.mjs "$tmp/core.map"', 'true # trace build')
    build = build.replace('node web/check-snapshot-registry.mjs "$tmp/core.map"', 'true # trace build')
    target = web / 'build-core.sh'
    if target.is_symlink():
        target.unlink()
    target.write_text(build)


def main():
    trace = '--trace' in sys.argv
    out = Path([argument for argument in sys.argv[1:] if not argument.startswith('--')][0]).resolve()
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
    for name in list(PATCHES) + ['terrain_contact_math.hpp']:
        target = engine / name
        text = (ROOT / 'engine' / name).read_text()
        for old, new in PATCHES.get(name, []):
            assert text.count(old) == 1, f'{name}: {old[:60]!r}'
            text = text.replace(old, new)
        for old, new in MULTI_PATCHES.get(name, []):
            assert text.count(old) >= 1, f'{name}: {old[:60]!r}'
            text = text.replace(old, new)
        if name == 'terrain_contact_math.hpp':
            text = patch_terrain(text)
        if target.is_symlink():
            target.unlink()
        target.write_text(text)
    for name, patches in WEB_PATCHES.items():
        target = out / 'web' / name
        text = (ROOT / 'web' / name).read_text()
        for old, new in patches:
            assert text.count(old) == 1, f'web/{name}: {old[:60]!r}'
            text = text.replace(old, new)
        if name == 'core.cpp':
            text += ARITH_EXPORT
        if target.is_symlink():
            target.unlink()
        target.write_text(text)
    # The switch is a plain global: skip the rider-global and snapshot-registry checks in this scratch build.
    build = (ROOT / 'web/build-core.sh').read_text()
    build = build.replace('node web/check-rider-globals.mjs "$tmp/core.map"', 'true # scratch arithmetic build')
    build = build.replace('node web/check-snapshot-registry.mjs "$tmp/core.map"', 'true # scratch arithmetic build')
    build_target = out / 'web/build-core.sh'
    if build_target.is_symlink():
        build_target.unlink()
    build_target.write_text(build)
    if trace:
        apply_trace(engine, out / 'web')
    print(out)

if __name__ == '__main__':
    main()
