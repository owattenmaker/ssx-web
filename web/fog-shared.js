// Fog state shared with materials drawn after the fog composite (pv byteBlend, docs/visual-parity.md section 17). fog-renderer.js
// copies its 36AC00 CLUT state here on every update; the static-model additive class (ALPHA 0x48, model flag 8: searchlight beams,
// glows) drawn in the encoded pass (web/snow-composite.js) scales its contribution by the fog alpha at its own depth, which is what
// the GS gives when the composite fogs Cd + Cs*As after the model wrote its Z: ((Cd + Cs*As) - F) * a/128 + F.
import {DataTexture,RGBAFormat,UnsignedByteType,NearestFilter,NoColorSpace} from 'three/webgpu';
import {uniform,texture,vec2,floor,mod,round,select,mix,float} from 'three/tsl';
const palette=new DataTexture(new Uint8Array(1024),256,1,RGBAFormat,UnsignedByteType);
palette.minFilter=palette.magFilter=NearestFilter;palette.generateMipmaps=false;palette.colorSpace=NoColorSpace;palette.needsUpdate=true;
export const fogShared={palette,slope:uniform(0),offset:uniform(0),enabled:uniform(0),revision:-1};
// fog-renderer update: info = core._fog_palette_info() (enabled, revision, .., slope [4], offset [5]); rgba = the 1024-byte CLUT
export function copyFogState(info,rgba){
 fogShared.enabled.value=info[0];fogShared.slope.value=info[4];fogShared.offset.value=info[5];
 if(info[0]&&fogShared.revision!==info[1]&&rgba){palette.image.data.set(rgba);palette.needsUpdate=true;fogShared.revision=info[1];}
}
// The composite's factor on an additive contribution at depthCm (view depth in cm): a/128 of the palette entry of the original Z,
// 1 where the composite leaves the pixel as it is (fog off, Z past 65535).
export function fogScale(depthCm){
 const z=floor(depthCm.mul(fogShared.slope).add(fogShared.offset).mul(depthCm.reciprocal())).max(0);
 const entry=texture(palette,vec2(mod(floor(z.div(256)),256).add(.5).div(256),.5));
 return select(z.lessThan(65535),mix(float(1),round(entry.a.mul(255)).div(128),fogShared.enabled),float(1));
}
