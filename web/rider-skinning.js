import {DataTexture,RGBAFormat,FloatType,NearestFilter,NoColorSpace,BufferAttribute,Vector3,Vector4} from 'three';
import {Fn,attribute,textureLoad,ivec2,vec4,uniform,select,mix,modelViewProjection,cameraProjectionMatrix,cameraViewMatrix} from 'three/tsl';
import {riderTransformPositionNode,riderTransformNormalNode} from './rider-lighting-nodes.js';
import {pv} from './pv-flags.js';
import {boardFlexWeights,MORPH_COLUMN_COUNT} from './board-flex.js';
// Rider morphs (pv boardFlex, web/board-flex.js): the board's and the hands' morph targets, added to the source position before skinning.
// Source world-space matrices already include pose, body scale, inverse bind and weights.
// The source path bypasses modelViewProjection's model transform, while menus
// retain Three's existing posed skeleton. Interpolation is presentation-only.
// flex: web/board-flex.js loadBoardFlex's {parts} (with configureBoardFlex's layout), or null (the rest shape).
export function createOriginalRiderSkinning(rig,vertices,origin,flex=null){
 if(!rig.source_bind_matrix_words||!rig.source_skin)return null;
 const boneCount=rig.bones.length,count=vertices.length/10;
 if(rig.source_skin_weight_units!=='integer-percent'||rig.source_skin.length!==count)throw Error('Invalid source skin attributes');
 const positions=new Float32Array(count*3),normals=new Float32Array(count*3),slots=new Float32Array(count),groupMap=new Map();
 for(let i=0;i<count;i++){
  positions.set([vertices[i*10]*100,-vertices[i*10+2]*100,vertices[i*10+1]*100],i*3);
  const sourceNormal = [vertices[i * 10 + 3], -vertices[i * 10 + 5], vertices[i * 10 + 4]];
  if (rig.source_normal_quantization === 32768)
    for (let k = 0; k < 3; k++) sourceNormal[k] = Math.max(-32768, Math.min(32767, Math.round(sourceNormal[k] * 32768))) / 32768;
  normals.set(sourceNormal,i*3);
  const group=rig.source_skin[i];if(group.length<1||group.length>4)throw Error('Invalid source influence count');
  const key=JSON.stringify(group);if(!groupMap.has(key))groupMap.set(key,groupMap.size);slots[i]=groupMap.get(key);
  group.forEach(([bone,weight])=>{if(!Number.isInteger(bone)||bone<0||bone>=boneCount||!Number.isInteger(weight)||weight<0||weight>32767)throw Error('Invalid source skin influence');});
 }
 const groupCount=groupMap.size,stride=groupCount*16,data=new Float32Array(stride*2),texture=new DataTexture(data,4,groupCount*2,RGBAFormat,FloatType);
 texture.minFilter=texture.magFilter=NearestFilter;texture.generateMipmaps=false;texture.colorSpace=NoColorSpace;texture.needsUpdate=true;
 const enabled=uniform(false),alpha=uniform(1),worldOrigin=uniform(new Vector3().copy(origin));let available=false,resetPending=false;
 // web/rider-shadow.js fits its box to the pose this skin draws: the world_pose_bones / pose_physical words captured with
 // each palette (previous, current), blended with the same alpha. A box fitted to the current tick alone lets the
 // interpolated silhouette run out of its +/-70 cm margin at speed (big boosted airs: ~1 m per tick) and be clipped.
 let poseBones=[null,null];
 const posePhysical=new Float32Array(12),shadow={bones:null,physical:posePhysical};
 // Row of the previous palette (= the rider's group count): a uniform, not a literal, so every rider (the human, the computer
 // riders, their shadows) shares one shader and one pipeline (docs/firefox-load.md). Same integer value, same texel.
 const previousRows=uniform(groupCount);
 // Board flex: every rider gets the same nodes while the switch is on (one shader); a rider without board morphs reads row 0 (zeros).
 const boardFlex=pv('boardFlex')?createBoardFlex(flex,count):null;
 // Source world position (PS2 cm, Z up, homogeneous) of the interpolated palette; also used by web/rider-shadow.js.
 const worldNode=Fn(()=>{
  const slot=attribute('sourceSkinGroup','float').toVar(),point=vec4(attribute('sourcePosition','vec3'),1);
  const worldAt=(offset,at)=>{
   const columns=Array.from({length:4},(_,col)=>textureLoad(texture,ivec2(col,slot.add(offset))));
   return riderTransformPositionNode(at,columns);
  };
  if(!boardFlex)return mix(worldAt(0,point),worldAt(previousRows,point),alpha);
  // each palette with its own tick's weights: the previous palette (row 0) with the previous weights
  return mix(worldAt(0,boardFlex.morphed(point,0)),worldAt(previousRows,boardFlex.morphed(point,1)),alpha);
 })();
 const vertexNode=Fn(()=>{
  const world=worldNode.toVar();
  const relative=vec4(world.x.mul(.01).sub(worldOrigin.x.mul(world.w)),world.z.mul(.01).sub(worldOrigin.y.mul(world.w)),world.y.mul(-.01).sub(worldOrigin.z.mul(world.w)),world.w);
  return select(enabled,cameraProjectionMatrix.mul(cameraViewMatrix).mul(relative),modelViewProjection);
 })();
 // GC-derived mesh normals are already decoded. Expressing them in ITOF15
 // units is exact for these binary fixed-point values; this does not claim the
 // GC source integer encoding equals the PS2 signed16 encoding.
 const lightingNormal=Fn(()=>{
  const slot=attribute('sourceSkinGroup','float'),packed=attribute('sourceNormal','vec3').mul(32768);
  const normalAt=offset=>riderTransformNormalNode(packed,Array.from({length:3},(_,col)=>textureLoad(texture,ivec2(col,slot.add(offset))).rgb));
  return mix(normalAt(0),normalAt(previousRows),alpha);
 })();
 return {
   vertexNode,
   worldNode,
   lightingNormal,
   enabledNode: enabled,
   attach(geometry) {
     geometry.setAttribute('sourcePosition', new BufferAttribute(positions, 3));
     geometry.setAttribute('sourceNormal', new BufferAttribute(normals, 3));
     geometry.setAttribute('sourceSkinGroup', new BufferAttribute(slots, 1));
     if (boardFlex) geometry.setAttribute('sourceFlexRow', new BufferAttribute(boardFlex.rows, 1));
   },
   // A tick with no pose (count 0, no palette: the core cleared its cached world pose, rider+0x2C, and has not posed since: the
   // mission builtin 64 teleport 300948 -> 1234D0 poses at the next tick) draws the last palette again, as the PS2 draws +0x2C as it
   // stands; a reset asked for then applies at the next capture. Returns false for such a tick. A palette of another size throws.
   capture(core, reset = false) {
     const size = core._rider_skin_palette_count(),
       pointer = size ? core._rider_skin_palette() : 0;
     if (!size && !pointer) {
       resetPending ||= reset;
       if (available) {
         data.copyWithin(0, stride);
         texture.needsUpdate = true;
         boardFlex?.hold();
         if (poseBones[1] && poseBones[0]?.length === poseBones[1].length) poseBones[0].set(poseBones[1]);
       }
       return false;
     }
     reset ||= resetPending;
     resetPending = false;
     const hadPose = available && !reset;
     if (size !== groupCount || !pointer) throw Error('Missing live source skin palette');
     if (!available) {
       const p = core._rider_skin_palette_indices();
       if (!p) throw Error('Missing skin palette indices');
       const ids = new Uint32Array(core.HEAPU8.buffer, p, count);
       for (let i = 0; i < count; i++) if (ids[i] !== slots[i]) throw Error('Skin palette vertex mapping differs');
     }
     const current = new Float32Array(core.HEAPF32.buffer, pointer, stride);
     if (available && !reset) data.copyWithin(0, stride);
     else data.set(current, 0);
     data.set(current, stride);
     boardFlex?.capture(core, available && !reset);
     available = true;
     texture.needsUpdate = true;
     if (core._world_pose_bones && core._pose_physical) {
       const bp = core._world_pose_bones() >> 2,
         pp = core._pose_physical() >> 2,
         F = core.HEAPF32,
         length = 1 + 7 * F[bp];
       const keep = hadPose && poseBones[1]?.length === length;
       if (poseBones[1]?.length !== length) {
         poseBones = [new Float32Array(length), new Float32Array(length)];
         shadow.bones = new Float32Array(length);
       }
       const previous = poseBones[0];
       poseBones[0] = poseBones[1];
       poseBones[1] = previous;
       poseBones[1].set(F.subarray(bp, bp + length));
       if (!keep) poseBones[0].set(poseBones[1]);
       posePhysical.set(F.subarray(pp, pp + 12));
     }
     return true;
   },
   // The fit inputs of the drawn pose: {bones (world_pose_bones layout, blended), physical (current pose_physical)}.
   // Scratch result consumed immediately by rider-shadow.js; callers retaining it must copy.
   shadowPose() {
     if (!available || !poseBones[1]) return null;
     const [a, b] = poseBones,
       t = Math.max(0, Math.min(1, alpha.value)),
       out = shadow.bones;
     out[0] = b[0];
     for (let i = 1; i < b.length; i++) out[i] = a[i] + (b[i] - a[i]) * t;
     return shadow;
   },
   display(active, fraction) {
     enabled.value = !!active && available;
     alpha.value = Math.max(0, Math.min(1, fraction));
     return enabled.value;
   },
   reset() {
     available = false;
     resetPending = false;
     enabled.value = false;
   },
   dispose() {
     texture.dispose();
     boardFlex?.dispose();
   }
 };
}

// The morph nodes of one rider: a delta texture (column = weight column: the board's 0..7, the hands' 8..25; row = 1 + morph vertex,
// PS2 cm Z up; row 0 zeros), a per-vertex row attribute and the weights of the two palettes (previous, current) as vec4 uniforms.
function createBoardFlex(flex, vertexCount) {
  const parts = (flex?.parts || []).filter((p) => p.meta.first_vertex + p.meta.vertex_count <= vertexCount);
  const morphVertices = parts.reduce((n, p) => n + p.meta.vertex_count, 0);
  const data = new Float32Array(MORPH_COLUMN_COUNT * (morphVertices + 1) * 4);
  const rows = new Float32Array(vertexCount);
  let row = 1;
  for (const part of parts) {
    const { meta, deltas, column } = part;
    for (let m = 0; m < meta.morph_count; m++) {
      const at = meta.offsets[m] / 4;
      for (let k = 0; k < meta.vertex_count; k++) {
        const d = deltas.subarray(at + k * 3, at + k * 3 + 3);
        // vertices.bin frame (Y up, metres) -> sourcePosition (PS2 cm, Z up), as the positions above
        data.set([d[0] * 100, -d[2] * 100, d[1] * 100, 0], ((row + k) * MORPH_COLUMN_COUNT + column + m) * 4);
      }
    }
    for (let k = 0; k < meta.vertex_count; k++) rows[meta.first_vertex + k] = row + k;
    row += meta.vertex_count;
  }
  const texture = new DataTexture(data, MORPH_COLUMN_COUNT, morphVertices + 1, RGBAFormat, FloatType);
  texture.minFilter = texture.magFilter = NearestFilter;
  texture.generateMipmaps = false;
  texture.colorSpace = NoColorSpace;
  texture.needsUpdate = true;
  const lanes = Math.ceil(MORPH_COLUMN_COUNT / 4);
  const values = [new Float32Array(lanes * 4), new Float32Array(lanes * 4)];
  const weights = [0, 1].map(() => Array.from({ length: lanes }, () => uniform(new Vector4())));
  const upload = () => {
    for (let p = 0; p < 2; p++) for (let l = 0; l < lanes; l++) weights[p][l].value.fromArray(values[p], l * 4);
  };
  return {
    rows,
    // point + sum of weight x delta, palette p (0 previous, 1 current)
    morphed(point, p) {
      const at = attribute('sourceFlexRow', 'float');
      let sum = vec4(0, 0, 0, 0);
      for (let m = 0; m < MORPH_COLUMN_COUNT; m++) {
        const weight = weights[p][m >> 2][['x', 'y', 'z', 'w'][m & 3]];
        sum = sum.add(textureLoad(texture, ivec2(m, at)).mul(weight));
      }
      return point.add(sum);
    },
    // a new palette: the current weights become the previous ones (keep), or both are the new ones. The core's weights come in
    // flex.layout's order (web/board-flex.js configureBoardFlex), each part into its columns.
    capture(core, keep) {
      const layout = flex?.layout || [];
      const total = layout.reduce((n, l) => n + l.count, 0);
      const live = total ? boardFlexWeights(core, total) : null;
      if (keep) values[0].set(values[1]);
      values[1].fill(0);
      if (live) {
        let at = 0;
        for (const l of layout) {
          values[1].set(live.subarray(at, at + l.count), l.column);
          at += l.count;
        }
      }
      if (!keep) values[0].set(values[1]);
      upload();
    },
    // a tick without a pose draws the last palette again (rider-skinning capture)
    hold() {
      values[0].set(values[1]);
      upload();
    },
    dispose() {
      texture.dispose();
    }
  };
}
