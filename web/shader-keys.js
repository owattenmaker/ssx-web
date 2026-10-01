// Identical shaders -> one shader module and one render pipeline (docs/firefox-load.md).
//
// Firefox's WebGPU (wgpu -> naga -> Metal) builds every render pipeline serially in the GPU process, ~100-150 ms each
// when the Metal shader cache is cold, and a pipeline in a presented frame holds the compositor. Two three.js (r186)
// details made the port create many more pipelines than it has shaders:
//
// 1. An unnamed uniform/storage buffer is named after its node id (`NodeBuffer_<id>`, WGSLNodeBuilder
//    getUniformFromNode). Every skinned mesh (its own `skeleton.boneMatrices` buffer) and every material with its own
//    uniformArray therefore had its own WGSL text, shader module and pipeline although the shader was the same (Snow Jam:
//    241 vertex modules for 56 distinct shaders). The ids also change from visit to visit, so the Metal shader cache
//    never hit. Here the name is the buffer's index within the shader being built (`NodeBuffer<k>`): identical graphs
//    give identical text (Pipelines.programs is keyed by the code), and the text is the same on every visit. Only the
//    identifier changes; bindings, layouts and data are untouched. Buffers named by the caller keep their names.
// 2. The render pipeline key carries the ids of the geometry's morph attributes (RenderObject.getGeometryCacheKey), so
//    each morphing mesh had its own pipeline. Morph targets live in a texture bound per object; they are not part of the
//    pipeline (vertex layout, shaders, state), which stay in the key. The morph target count stays in it too.
//
// ?shaderKeys=0 turns both off (A/B checks).
import { WGSLNodeBuilder, WebGPUBackend, GLSLNodeBuilder, MaterialReferenceNode } from 'three/webgpu';

const PATCHED = Symbol.for('ssx3.shaderKeys');

export function stableBufferNames(Builder = WGSLNodeBuilder) {
  const proto = Builder?.prototype;
  if (!proto || proto[PATCHED]) return;
  const base = proto.getUniformFromNode;
  proto.getUniformFromNode = function (node, type, shaderStage, name = null) {
    const uniform = base.call(this, node, type, shaderStage, name);
    if (!name && uniform && typeof uniform.name === 'string' && uniform.name.startsWith('NodeBuffer_') && uniform.stableName !== true) {
      const names = this.stableBufferNames ??= new Map();
      let k = names.get(node);
      if (k === undefined) { k = names.size; names.set(node, k); }
      uniform.name = 'NodeBuffer' + k;
      uniform.stableName = true;
    }
    return uniform;
  };
  proto[PATCHED] = true;
}

// 'morph-position,5685,5686,...,' -> 'morph-position,27,' (count only)
export const morphFreeKey = (key) => key.replace(/morph-([^,]*),((?:\d+,)*)/g, (_, name, ids) => `morph-${name},${ids ? ids.split(',').length - 1 : 0}#,`);

export function morphFreePipelineKeys(Backend = WebGPUBackend) {
  const proto = Backend?.prototype;
  if (!proto || proto[PATCHED]) return;
  const base = proto.getRenderCacheKey;
  proto.getRenderCacheKey = function (renderObject) {
    const key = base.call(this, renderObject);
    return key.includes('morph-') ? morphFreeKey(key) : key;
  };
  proto[PATCHED] = true;
}

// the same for the WebGL2 backend's GLSL (the fallback: Firefox before WebGPU, older Safari, Chrome without WebGPU). There
// a buffer is a uniform block named after its node (`uniform NodeBuffer_<id> { mat4 buffer<id>[n]; }`), and the backend binds it
// by that name (WebGLBackend._setupBindings: getUniformBlockIndex(program, binding.name)), once per program with the first render
// object's bindings; the render objects sharing a program bind their own buffers at the same binding points (bindings order). So
// a builder gives each buffer of a non-shared group the name of its index in this shader (block `NodeBuffer<k>`, array `buffer<k>`)
// and its own copy of the buffer binding carrying that block name (the node's shared binding is cloned per render object anyway,
// NodeBuilderState.createBindings); the node's own name is swapped in only while the shader text is written. Buffers of shared groups
// (bound once for every object) keep their names.
const GLSL_PATCHED = Symbol.for('ssx3.glslKeys');
export function stableGlslBufferNames(Builder = GLSLNodeBuilder) {
  const proto = Builder?.prototype;
  if (!proto || proto[GLSL_PATCHED] || typeof proto.getUniformFromNode !== 'function' || typeof proto.getUniforms !== 'function') return false;
  const base = proto.getUniformFromNode, uniforms = proto.getUniforms;
  proto.getUniformFromNode = function (node, type, shaderStage, name = null) {
    const uniform = base.call(this, node, type, shaderStage, name);
    // a caller-named buffer keeps its name; three passes the node's own auto name (`NodeBuffer_<id>`) back after its first build
    if (type !== 'buffer' || !uniform || node?.groupNode?.shared === true || typeof node?.name !== 'string' || !node.name.startsWith('NodeBuffer_') || (name && name !== node.name)) return uniform;
    const blocks = this.ssxBlocks ??= new Map(); // node -> {k, block, binding: this builder's binding for it}
    let b = blocks.get(node);
    // the binding base created for this stage (a NodeUniformBuffer of this node; r186 does not keep one per node:
    // getSharedDataFromNode never stores its data) -> this builder's one binding for the node, named as its block
    const stage = this.bindings?.[shaderStage];
    if (stage) for (const g in stage) {
      const list = stage[g];
      for (let i = 0; i < list.length; i++) {
        const x = list[i]; if (!x?.isNodeUniformBuffer || x.nodeUniform !== node) continue;
        if (!b) { const k = blocks.size; b = { k, block: 'NodeBuffer' + k, binding: x }; x.name = b.block; blocks.set(node, b); }
        else if (x !== b.binding) list[i] = b.binding;
      }
    }
    if (b) uniform.name = 'buffer' + b.k;
    return uniform;
  };
  proto.getUniforms = function (shaderStage) {
    const blocks = this.ssxBlocks; if (!blocks?.size) return uniforms.call(this, shaderStage);
    const saved = [];
    for (const [node, b] of blocks) { saved.push([node, node.name]); node.name = b.block; }
    try { return uniforms.call(this, shaderStage); } finally { for (const [node, n] of saved) node.name = n; }
  };
  proto[GLSL_PATCHED] = true;
  return true;
}

// a material property node (MaterialReferenceNode, e.g. MaterialNode's cached materialReference('map') behind materialColor)
// is shared by every material and re-pointed per drawn object (updateReference in its OBJECT update), but ReferenceNode.setup reads the
// value it had when the build reached its generate stage: often another material's texture, so the uniform dedup against the material's
// own texture(map) node depended on draw order (two bindings of the map, or one). Generate re-points it at the material being built.
const REF_PATCHED = Symbol.for('ssx3.refKeys');
export function currentMaterialReferences(Node = MaterialReferenceNode) {
  const proto = Node?.prototype;
  if (!proto || proto[REF_PATCHED] || typeof proto.generate !== 'function' || typeof proto.updateReference !== 'function' || typeof proto.updateValue !== 'function') return false;
  // Node.build re-points every node in the setup stage, but three builds asynchronously (compileAsync yields between stages), and
  // other objects' updates re-point the shared node before this build's generate stage writes the uniform: generate re-points too.
  const generate = proto.generate;
  proto.generate = function (builder, output) { if (this.material === null && builder?.material) { this.updateReference(builder); this.updateValue(); } return generate.call(this, builder, output); };
  proto[REF_PATCHED] = true;
  return true;
}

if (!/[?&]shaderKeys=0\b/.test(globalThis.location?.search ?? '')) { stableBufferNames(); morphFreePipelineKeys(); stableGlslBufferNames(); currentMaterialReferences(); }
