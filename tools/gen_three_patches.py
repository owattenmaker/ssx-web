#!/usr/bin/env python3
"""Writes web/three-patches.js from the exact text of web/node_modules/three/build/three.webgpu.js (three r186).
Each find must occur once; rerun after a three upgrade and re-verify (docs/web-render-performance.md "Per-frame garbage")."""
import json, hashlib, pathlib
WEB = pathlib.Path(__file__).resolve().parent.parent / 'web'
src = (WEB / 'node_modules/three/build/three.webgpu.js').read_text()
def block(start, end_marker):
    i = src.index(start); j = src.index(end_marker, i); return src[i:j + len(end_marker)]
P = []
def add(name, find, replace):
    n = src.count(find); assert n == 1, (name, n); P.append({'name': name, 'find': find, 'replace': replace})
a = 'const hash$1 = ( ...params ) => cyrb53( params );'
add('hash scratch', a, a + '\n\n// ssx three-patches (web/three-patches.js): reused argument arrays for the per-draw hashes (same cyrb53 input)\nconst _ssxHash2 = [ 0, 0 ], _ssxHash3 = [ 0, 0, 0 ];')
f = block('\t\tif ( this.camera.isArrayCamera ) {\n\n\t\t\tcacheKey = hash$1( cacheKey, this.camera.cameras.length );', '\t\treturn cacheKey;')
lean = '''\t\tif ( globalThis.__ssxThreeLean === true ) { // ssx three-patches: hash$1( ...params ) without the rest array per draw

\t\t\tif ( this.camera.isArrayCamera ) { _ssxHash2[ 0 ] = cacheKey; _ssxHash2[ 1 ] = this.camera.cameras.length; cacheKey = cyrb53( _ssxHash2 ); }
\t\t\tif ( this.object.receiveShadow ) { _ssxHash2[ 0 ] = cacheKey; _ssxHash2[ 1 ] = 1; cacheKey = cyrb53( _ssxHash2 ); }
\t\t\t_ssxHash3[ 0 ] = cacheKey; _ssxHash3[ 1 ] = this.renderer.contextNode.id; _ssxHash3[ 2 ] = this.renderer.contextNode.version;
\t\t\treturn cyrb53( _ssxHash3 );

\t\t}

'''
add('dynamic cache key', f, lean + f)
f = block("\t\tlet cacheBindings = true;\n\t\tlet cacheKey = '';", 'let version = 0;')
add('bindings key ids', f, f + "\n\t\t// ssx three-patches: the key's ids, kept on the bind group; the string is built only for updateBindings\n\t\tconst ssxLean = globalThis.__ssxThreeLean === true, ssxIds = ssxLean ? ( bindGroup._ssxKeyIds || ( bindGroup._ssxKeyIds = [] ) ) : null;\n\t\tlet ssxCount = 0;")
f = "\t\t\t\t\tneedsBindingsUpdate = true;\n\n\t\t\t\t}\n\n\t\t\t\tcacheKey += attribute.id + ',';"
add('bindings attribute id', f, f.replace("\t\t\t\tcacheKey += attribute.id + ',';", "\t\t\t\tif ( ssxLean ) ssxIds[ ssxCount ++ ] = attribute.id; else cacheKey += attribute.id + ',';"))
add('bindings texture id', "\t\t\t\t\tcacheKey += texture.id + ',';", "\t\t\t\t\tif ( ssxLean ) ssxIds[ ssxCount ++ ] = texture.id; else cacheKey += texture.id + ',';")
f = block('\t\tif ( needsBindingsUpdate === true ) {', "this.backend.updateBindings( bindGroup, bindings, cacheBindings ? cacheKey : '', version );")
add('bindings key string', f, f.replace('\t\t\tthis.backend.updateBindings(', "\t\t\tif ( ssxLean && cacheBindings ) for ( let i = 0; i < ssxCount; i ++ ) cacheKey += ssxIds[ i ] + ',';\n\t\t\tthis.backend.updateBindings("))
f = block('\tupdateTexture( texture, options = {} ) {', 'if ( textureData.initialized === true && textureData.version === texture.version ) return;')
add('updateTexture options', f, f.replace('\tupdateTexture( texture, options = {} ) {', '\tupdateTexture( texture, options ) {\n\n\t\t// ssx three-patches: the default options object only past the early return\n\t\tif ( options === undefined && globalThis.__ssxThreeLean !== true ) options = {};') + '\n\t\tif ( options === undefined ) options = {};')
f = block('\tupdateSampler( binding ) {\n\n\t\tconst backend = this.backend;', 'backend.hasCompatibility( Compatibility.TEXTURE_COMPARE );')
fast = '''
\t\t// ssx three-patches: the key of an unchanged sampler state, without building the string (the original finds the same cache
\t\t// entry, whose sampler is already the binding's, and returns that key)
\t\tconst ssxLean = globalThis.__ssxThreeLean === true;
\t\tif ( ssxLean ) {

\t\t\tconst d = backend.get( binding ), m = d._ssxSampler;
\t\t\tif ( m !== undefined && d.sampler !== undefined && d.samplerKey === m.key && m.minFilter === texture.minFilter && m.magFilter === texture.magFilter &&
\t\t\t\tm.wrapS === texture.wrapS && m.wrapT === texture.wrapT && m.wrapR === texture.wrapR && m.anisotropy === texture.anisotropy &&
\t\t\t\tm.depth === ( texture.isDepthTexture === true ) && m.comparison === isComparison && m.compareFunction === texture.compareFunction ) {

\t\t\t\tconst cached = this._samplerCache.get( m.key );
\t\t\t\tif ( cached !== undefined && cached.sampler === d.sampler ) return m.key;

\t\t\t}

\t\t}
'''
add('updateSampler fast path', f, f + fast)
i = src.index(f); j = src.index('\t\treturn samplerKey;', i)
tail = src[i:j + len('\t\treturn samplerKey;')]; tail = tail[tail.rindex('\t\t\tsamplerData.usedTimes ++;'):]
store = '''\t\tif ( ssxLean ) {

\t\t\tconst d = backend.get( binding ), m = d._ssxSampler || ( d._ssxSampler = {} );
\t\t\tm.key = d.samplerKey; m.minFilter = texture.minFilter; m.magFilter = texture.magFilter; m.wrapS = texture.wrapS; m.wrapT = texture.wrapT;
\t\t\tm.wrapR = texture.wrapR; m.anisotropy = texture.anisotropy; m.depth = texture.isDepthTexture === true; m.comparison = isComparison; m.compareFunction = texture.compareFunction;

\t\t}

'''
add('updateSampler memo', tail, tail.replace('\t\treturn samplerKey;', store + '\t\treturn samplerKey;'))
body = json.dumps(P, indent=1); digest = hashlib.sha256(body.encode()).hexdigest()[:12]
js = f'''// three r186 patches for per-draw JS garbage (docs/web-render-performance.md "Per-frame garbage"; Owen approved patching three,
// 2026-09-29). Generated by tools/gen_three_patches.py from the exact three source; applied to node_modules/three/build/three.webgpu.js
// as Vite transforms, in the build (plugins) and in the dev server's dependency pre-bundle (optimizeDeps.rolldownOptions.plugins).
// Each find must occur exactly once, or the build fails (a three upgrade must re-check them). The patched paths run only with
// globalThis.__ssxThreeLean === true (web/main.js from pv threeLean); otherwise three's own code runs.
// 1. RenderObject.getDynamicCacheKey (every draw): cyrb53 over reused argument arrays instead of hash$1( ...params ).
// 2. Bindings._update: the key's attribute / texture ids kept on the bind group; the key string built only for updateBindings.
// 3. Textures.updateTexture: the default options object made after the early return.
// 4. WebGPUTextureUtils.updateSampler: an unchanged sampler state returns its key without building the string.
const PATCHES = {body};
export const THREE_PATCH_VERSION = '{digest}';
const TARGET = /[\\\\/]node_modules[\\\\/]three[\\\\/]build[\\\\/]three\\.webgpu\\.js$/;
export function patchThree(code, id = 'three.webgpu.js') {{
  for (const p of PATCHES) {{
    const n = code.split(p.find).length - 1;
    if (n !== 1) throw new Error(`three-patches: "${{p.name}}" matches ${{n}} times in ${{id}} (three changed? rerun tools/gen_three_patches.py)`);
    code = code.split(p.find).join(p.replace);
  }}
  return code;
}}
export default function threePatches() {{
  return {{
    name: 'ssx-three-patches', version: THREE_PATCH_VERSION, enforce: 'pre',
    transform(code, id) {{ if (!TARGET.test(id.split('?')[0])) return null; return {{ code: patchThree(code, id), map: null }}; }},
  }};
}}
'''
(WEB / 'three-patches.js').write_text(js)
print(len(P), 'patches', digest)
