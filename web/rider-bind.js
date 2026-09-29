import {Matrix4} from 'three';
// Authored static inverse bind matrices, mapped to the active browser skeleton.
// Geometry is already scaled in asset(); scale translations to the same units.
// Keep source rotation coefficients: do not invert a normalized host bind pose.
export function originalRiderBoneInverses(rig,scale){
 if(!rig.source_bind_matrix_words)return null;
 const rows=rig.source_bind_matrix_words,slots=rig.source_bone_slots;
 if(rig.source_bind_matrix_space!=='source-centimeters-Z-up'||!Number.isFinite(scale)||scale<=0||rows.length!==rig.bones.length||slots?.length!==rows.length||new Set(slots).size!==slots.length||slots.some(x=>!Number.isInteger(x)||x<0||x>=rig.source_bone_slot_count))throw Error('Invalid original rider bind mapping');
 const axes=[0,2,1,3],signs=[1,1,-1,1];
 return rows.map(words=>{
  if(words.length!==16||words.some(x=>!Number.isInteger(x)||x<0||x>0xffffffff))throw Error('Invalid original bind matrix words');
  const source=new Float32Array(new Uint32Array(words).buffer);if(!source.every(Number.isFinite))throw Error('Nonfinite original bind matrix');
  const matrix=new Matrix4(),out=matrix.elements;
  for(let col=0;col<4;col++)for(let row=0;row<4;row++)out[col*4+row]=signs[col]*signs[row]*source[axes[col]*4+axes[row]];
  for(let i=12;i<15;i++)out[i]*=scale/100;
  return matrix;
 });
}
