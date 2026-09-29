// The frame's colour space (pv encodedBlend; docs/visual-parity.md section 38). Every material, composite and texture that decides how a
// colour reaches the frame goes through this module, so the choice is made once per page, before any material or Color is built.
//
// Linear (encodedBlend off): three's working space. Byte-domain materials do the GS maths on bytes and write EOTF(bytes / 255); the fog
// composite reads OETF(frame); the canvas gets three's sRGB output conversion. Blends inside the frame mix linear light.
// Encoded (on): the frame holds bytes / 255, as the GS frame buffer does, so every blend is the GS's (Cs - Cd) x As + Cd (0x44) or
// Cs x As + Cd (0x48) on encoded values (within 1 of the GS's truncation, WebGPU and WebGL2). Opaque pixels are the same bytes.
//
// Rules for a material (web/test-frame-space.mjs keeps the colour transfer functions and SRGBColorSpace in this file):
//  - GS bytes (0..255) or encoded 0..1 colour         -> toFrame(bytes.div(255)) as the output colour;
//  - colour three computes in linear light (legacy)   -> linearToFrame(linear) as the output colour, textures in linearTextureSpace;
//  - a stock material sampling a texture straight in  -> texture.colorSpace = frameTextureSpace;
//  - a pass or composite reading the frame            -> fromFrame(value) for encoded 0..1;
//  - deliberate linear-light maths inside byte maths  -> encodedToLinear / linearToEncoded (Sam's authored tint).
import {ColorManagement,NoColorSpace,SRGBColorSpace,LinearSRGBColorSpace,UnsignedByteType,HalfFloatType} from 'three/webgpu';
import {sRGBTransferEOTF,sRGBTransferOETF,vec4,output} from 'three/tsl';
import {pv} from './pv-flags.js';

export const encodedFrame=pv('encodedBlend');
// three converts a Color set from hex / CSS (sRGB) to linear when it is set: with an encoded frame the value stays the encoded byte it was
// written as (the sky clear, the lodge background, a fog colour), and three's output conversion does nothing.
if(encodedFrame)ColorManagement.enabled=false;

const same=x=>x;
// encoded 0..1 -> the value written to the frame
export const toFrame=encodedFrame?same:sRGBTransferEOTF;
// a value read from the frame (world / sky pass, a direct render) -> encoded 0..1
export const fromFrame=encodedFrame?same:sRGBTransferOETF;
// linear-light colour (three's own maths on sRGB-decoded textures) -> the value written to the frame
export const linearToFrame=encodedFrame?sRGBTransferOETF:same;
// a material's own output computed on linear light (a stock / legacy combine), written to the frame: material.outputNode = linearOutput()
export const linearOutput=()=>vec4(linearToFrame(output.rgb),output.a);
// colour transforms that are linear-light by definition, whatever the frame holds
export const encodedToLinear=sRGBTransferEOTF,linearToEncoded=sRGBTransferOETF;
// a texture a stock material samples straight into the frame (its texels are encoded bytes)
export const frameTextureSpace=encodedFrame?NoColorSpace:SRGBColorSpace;
// a texture sampled as linear light by the hardware, for materials whose output goes through linearToFrame
export const linearTextureSpace=SRGBColorSpace;

// pv frame8: the renderer's output buffer type (its pass targets: the world and sky passes), chosen with the renderer. An encoded frame holds
// bytes, so 8-bit unorm keeps every value and rounds each draw to bytes as the GS does; a linear frame keeps half-float.
export const frameBufferType=encodedFrame&&pv('frame8')?UnsignedByteType:HalfFloatType;
// Renderer init (main.js): the canvas gets the frame's bytes. With three's output space equal to its working space no frame-buffer
// target or output pass is added for a direct render (the front end, cutscenes); the race's fog composite writes the canvas itself.
export function configureFrameSpace(renderer){renderer.outputColorSpace=encodedFrame?LinearSRGBColorSpace:SRGBColorSpace;return renderer;}
