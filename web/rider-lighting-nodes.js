import {Fn,clamp,floor,vec3,vec4,float,select,uint,int,If,floatBitsToUint,uintBitsToFloat,bitAnd,bitOr,bitXor,shiftLeft,shiftRight,countLeadingZeros} from 'three/tsl';

// GS HIGHLIGHT2/TCC1. Byte-domain inputs deliberately avoid implicit sRGB
// decoding: diffuse texture modulation and the rim term operate on encoded bytes.
// Sampling/interpolation and framebuffer blending belong to the caller.
export const riderHighlight2Node=Fn(([textureBytes,lightingBytes])=>{
 const rgb=clamp(floor(textureBytes.rgb.mul(lightingBytes.rgb).div(128)).add(lightingBytes.a),0,255);
 return vec4(rgb,textureBytes.a);
});

// VU-domain finite binary32 arithmetic. Integer mantissas prevent GPU fast-math
// from reassociating error-residual expressions. Denormal inputs/results flush
// to signed zero, matching the VU domain; overflowing finite results saturate.
const encodeChop=Fn(([sign,mantissa,exponent])=>{
 const result=uint(sign).toVar();
 If(exponent.greaterThanEqual(255),()=>{result.assign(bitOr(sign,uint(0x7f7fffff)));})
 .ElseIf(exponent.greaterThan(0),()=>{result.assign(bitOr(bitOr(sign,shiftLeft(uint(exponent),uint(23))),bitAnd(mantissa,uint(0x7fffff))));});
 return result;
}).setLayout({name:'ssxEncodeChop',type:'uint',inputs:[{name:'sign',type:'uint'},{name:'mantissa',type:'uint'},{name:'exponent',type:'int'}]});
export const chopAdd=Fn(([a,b])=>{
 const ba=floatBitsToUint(a),bb=floatBitsToUint(b),mask=uint(0x7fffffff);
 const swap=bitAnd(ba,mask).lessThan(bitAnd(bb,mask));
 const hi=select(swap,bb,ba),lo=select(swap,ba,bb);
 const ea=bitAnd(shiftRight(hi,uint(23)),uint(255)),eb=bitAnd(shiftRight(lo,uint(23)),uint(255));
 const sign=bitAnd(hi,uint(0x80000000)),different=bitAnd(bitXor(hi,lo),uint(0x80000000)).notEqual(uint(0));
 const result=hi.toVar();
 If(ea.equal(uint(0)),()=>{result.assign(bitAnd(bitAnd(ba,bb),uint(0x80000000)));}).ElseIf(eb.notEqual(uint(0)),()=>{
  const ma=bitOr(bitAnd(hi,uint(0x7fffff)),uint(0x800000));
  const mb=bitOr(bitAnd(lo,uint(0x7fffff)),uint(0x800000));
  const distance=ea.sub(eb),right=distance.sub(uint(2)).min(uint(31));
  const aligned=uint(0).toVar(),sticky=uint(0).toVar();
  If(distance.lessThanEqual(uint(2)),()=>{aligned.assign(shiftLeft(mb,uint(2).sub(distance)));}).Else(()=>{
   aligned.assign(shiftRight(mb,right));
   sticky.assign(select(distance.greaterThanEqual(uint(34)),uint(1),select(bitAnd(mb,shiftLeft(uint(1),right).sub(uint(1))).notEqual(uint(0)),uint(1),uint(0))));
  });
  const magnitude=select(different,shiftLeft(ma,uint(2)).sub(aligned).sub(sticky),shiftLeft(ma,uint(2)).add(aligned)).toVar();
  If(magnitude.equal(uint(0)),()=>{result.assign(uint(0));}).Else(()=>{
   const top=uint(31).sub(countLeadingZeros(magnitude));
   const shift=top.sub(uint(23));
   const mantissa=select(top.greaterThanEqual(uint(23)),shiftRight(magnitude,shift),shiftLeft(magnitude,uint(23).sub(top)));
   result.assign(encodeChop(sign,mantissa,int(ea).add(int(top)).sub(25)));
  });
 });
 return uintBitsToFloat(result);
}).setLayout({name:'ssxChopAdd',type:'float',inputs:[{name:'a',type:'float'},{name:'b',type:'float'}]});
export const chopMul=Fn(([a,b])=>{
 const ba=floatBitsToUint(a),bb=floatBitsToUint(b);
 const ea=bitAnd(shiftRight(ba,uint(23)),uint(255)),eb=bitAnd(shiftRight(bb,uint(23)),uint(255));
 const sign=bitAnd(bitXor(ba,bb),uint(0x80000000)),result=sign.toVar();
 If(ea.notEqual(uint(0)).and(eb.notEqual(uint(0))),()=>{
  const ma=bitOr(bitAnd(ba,uint(0x7fffff)),uint(0x800000)),mb=bitOr(bitAnd(bb,uint(0x7fffff)),uint(0x800000));
  const al=bitAnd(ma,uint(65535)),bl=bitAnd(mb,uint(65535)),ah=shiftRight(ma,uint(16)),bh=shiftRight(mb,uint(16));
  const p0=al.mul(bl),p1=ah.mul(bl),p2=al.mul(bh);
  const middle=shiftRight(p0,uint(16)).add(bitAnd(p1,uint(65535))).add(bitAnd(p2,uint(65535)));
  const low=bitOr(bitAnd(p0,uint(65535)),shiftLeft(bitAnd(middle,uint(65535)),uint(16)));
  const high=ah.mul(bh).add(shiftRight(p1,uint(16))).add(shiftRight(p2,uint(16))).add(shiftRight(middle,uint(16)));
  const extra=select(high.greaterThanEqual(uint(32768)),uint(1),uint(0)),shift=uint(23).add(extra);
  const mantissa=bitOr(shiftLeft(high,uint(32).sub(shift)),shiftRight(low,shift));
  const exponent=int(ea).add(int(eb)).sub(127).toVar();
  If(high.greaterThanEqual(uint(32768)),()=>{exponent.addAssign(int(1));});
  result.assign(encodeChop(sign,mantissa,exponent));
 });
 return uintBitsToFloat(result);
}).setLayout({name:'ssxChopMul',type:'float',inputs:[{name:'a',type:'float'},{name:'b',type:'float'}]});

// Coefficients have already received the original once-per-upload255 scaling.
// The caller supplies the transformed normal, without additional normalization.
export function riderIrradianceNode(coefficients,normal){
 if(coefficients.length!==10)throw Error('Expected ten irradiance rows');
 return Fn(()=>{
  const n=normal.toVar(),bank=coefficients.map(row=>row.toVar());
  const x=n.x,y=n.y,z=n.z;
  const weights=[chopMul(x,x),chopMul(y,y),chopMul(z,z),chopMul(x,y),chopMul(z,x),chopMul(y,z),x,y,z].map(value=>value.toVar());
  const lanes=[];
  for(let lane=0;lane<4;lane++){
   const value=bank[0].element(lane).toVar();
   for(let i=1;i<10;i++)value.assign(chopAdd(value,chopMul(bank[i].element(lane),weights[i-1])));
   lanes.push(floor(clamp(value,0,255)));
  }
  return vec4(...lanes);
 })();
}

// Program2 ITOF15 +04E0/04E8/04F0. Columns are selected/blended by the
// skinning owner; this stage must not normalize or build an inverse transpose.
export function riderTransformNormalNode(packedNormal,columns){
 if(columns.length!==3)throw Error('Expected three normal transform columns');
 return Fn(()=>{
  const packed=packedNormal.toVar(),matrix=columns.map(column=>column.toVar());
  const decoded=Array.from({length:3},(_,i)=>chopMul(packed.element(i),float(1/32768)).toVar());
  const lanes=[];
  for(let row=0;row<3;row++){
   const value=chopMul(matrix[0].element(row),decoded[0]).toVar();
   for(let col=1;col<3;col++)value.assign(chopAdd(value,chopMul(matrix[col].element(row),decoded[col])));
   lanes.push(value);
  }
  return vec3(...lanes);
 })();
}

//386BD0, one column of a selected skin matrix. Source integer percentage
//weights are multiplied by0.01f; all four lanes retain VU accumulation order.
export function riderSkinColumnNode(columns,integerWeights,influenceCount){
 if(columns.length!==4)throw Error('Expected four skin influence columns');
 return Fn(()=>{
  const weights=integerWeights.toVar(),count=influenceCount.toVar(),matrix=columns.map(c=>c.toVar());
  const first=chopMul(weights.x,float(.01)).toVar();
  const result=vec4(...Array.from({length:4},(_,lane)=>chopMul(matrix[0].element(lane),first))).toVar();
  for(let i=1;i<4;i++)If(count.greaterThan(i),()=>{
   const weight=chopMul(weights.element(i),float(.01)).toVar();
   for(let lane=0;lane<4;lane++)result.element(lane).assign(chopAdd(chopMul(result.element(lane),float(1)),chopMul(matrix[i].element(lane),weight)));
  });
  return result;
 })();
}

//Program2 upper04C0..04D8, before projection. Caller owns coordinate-space
//conversion and must not reapply the rider's scale/world transform afterward.
export function riderTransformPositionNode(point,columns){
 if(columns.length!==4)throw Error('Expected four position transform columns');
 return Fn(()=>{
  const p=point.toVar(),matrix=columns.map(c=>c.toVar()),lanes=[];
  for(let row=0;row<4;row++){
   const value=chopMul(matrix[0].element(row),p.x).toVar();
   for(let col=1;col<4;col++)value.assign(chopAdd(value,chopMul(matrix[col].element(row),p.element(col))));
   lanes.push(value);
  }
  return vec4(...lanes);
 })();
}
