import {DepthTexture,DepthStencilFormat,UnsignedInt248Type} from 'three/webgpu';

// The board track's GS destination-alpha passes (0x386E78) are emulated with a
// stencil attachment, so the post-processed world pass needs depth+stencil.
// three r186 binds every sampled texture with aspect 'all', which WebGPU rejects
// for combined depth-stencil formats. Sampled views (the binding path always
// sets a view dimension; attachment views leave it undefined) of such textures
// are narrowed to their depth aspect, which is what the fog/snow passes read.
let patched=false;
function patchDepthStencilSampling(){
 if(patched||typeof GPUTexture==='undefined')return;patched=true;
 const createView=GPUTexture.prototype.createView;
 GPUTexture.prototype.createView=function(descriptor){
  if(descriptor&&descriptor.dimension&&(descriptor.aspect===undefined||descriptor.aspect==='all')&&/-stencil8$/.test(this.format)&&(this.usage&GPUTextureUsage.TEXTURE_BINDING))
   descriptor={label:descriptor.label,format:descriptor.format,dimension:descriptor.dimension,aspect:'depth-only',baseMipLevel:descriptor.baseMipLevel,mipLevelCount:descriptor.mipLevelCount,baseArrayLayer:descriptor.baseArrayLayer,arrayLayerCount:descriptor.arrayLayerCount};
  return createView.call(this,descriptor);
 };
}
export function depthStencilPassOptions(){
 patchDepthStencilSampling();
 const depthTexture=new DepthTexture();depthTexture.format=DepthStencilFormat;depthTexture.type=UnsignedInt248Type;
 return {stencilBuffer:true,depthTexture};
}
