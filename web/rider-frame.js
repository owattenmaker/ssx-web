import {Vector3,Quaternion,Matrix4} from 'three';
const sourcePoint=(v,i=0)=>new Vector3(v[i]/100,v[i+2]/100,-v[i+1]/100);
const sourceRotation=(v,i=0)=>new Quaternion(v[i],v[i+2],-v[i+1],v[i+3]).normalize();
// Immutable pose snapshot in the renderer's existing coordinate convention.
export function captureRiderFrame(state,pose,info,parents,origin,physicalOrientation=null,physicalPosition=null){
 const position=(physicalPosition?sourcePoint(physicalPosition):new Vector3(state[0],state[1],state[2])).sub(origin);
 const normal=state[8]?new Vector3(state[4],state[5],state[6]):new Vector3(0,1,0);
 const forward=new Vector3(Math.sin(state[3]),0,Math.cos(state[3]));forward.addScaledVector(normal,-forward.dot(normal)).normalize();
 const right=new Vector3().crossVectors(normal,forward).normalize();
 const rotation=physicalOrientation?sourceRotation(physicalOrientation):new Quaternion().setFromRotationMatrix(new Matrix4().makeBasis(right.negate(),normal,forward.negate())).normalize();
 if(!state[8]&&info){position.add(sourcePoint(info,16).applyQuaternion(rotation));rotation.multiply(sourceRotation(info,6)).normalize();}
 const positions=[],rotations=[];
 for(let i=0;i<parents.length;i++){positions.push(sourcePoint(pose,i*7));rotations.push(sourceRotation(pose,i*7+3));}
 const bonePositions=[],boneRotations=[];
 for(let i=0;i<parents.length;i++){
  const parent=parents[i];
  if(parent<0){bonePositions.push(positions[i]);boneRotations.push(rotations[i]);}
  else {const inverse=rotations[parent].clone().invert();bonePositions.push(positions[i].clone().sub(positions[parent]).applyQuaternion(inverse));boneRotations.push(inverse.multiply(rotations[i]).normalize());}
 }
 return {position,rotation,bonePositions,boneRotations};
}
export function applyRiderFrame(rider,bones,previous,current,alpha){
 const prior=previous||current,t=Math.max(0,Math.min(1,alpha));
 rider.position.lerpVectors(prior.position,current.position,t);
 rider.quaternion.slerpQuaternions(prior.rotation,current.rotation,t);
 for(let i=0;i<bones.length;i++){
  bones[i].position.lerpVectors(prior.bonePositions[i],current.bonePositions[i],t);
  bones[i].quaternion.slerpQuaternions(prior.boneRotations[i],current.boneRotations[i],t);
 }
}
