// Original Widescreen option (front-end Options, FEAMER "Widescreen": CMNAMER "Off" / "16:9" / "Anamorphic").
// Profile word 0x535610 bits 20..21 hold the mode; 0x228C08 (boot 0x152DBC, menu 0x189D34) passes it to the render
// context 0x61BA60 vtable+0x140 = 0x377950, which stores ctx+0x6B94 mode and four factors:
//   +0x6B98 viewport top clamp, +0x6B9C viewport height clamp (fractions of the 448-line buffer, applied by 0x376A70),
//   +0x6BA0 / +0x6BA4 horizontal / vertical GS projection scale factors (applied by 0x376C58 after scale = 0.5*w/tan(fov)).
export const WIDESCREEN_MODES=Object.freeze([
 Object.freeze({mode:0,label:'Off',top:0,height:1,scaleX:1,scaleY:1}),
 Object.freeze({mode:1,label:'16:9',top:.125,height:.75,scaleX:.75,scaleY:.75}),
 Object.freeze({mode:2,label:'Anamorphic',top:0,height:1,scaleX:.75,scaleY:1}),
]);
const KEY='ssx3.widescreen';

// PS2 projection for a mode: GS pixel scales and the clamped 3D viewport of a width x height buffer.
// fov is the DEFAULT_3 half horizontal angle; 1.3333 (gp-0x27B0) * h/w is the 4:3 pixel-aspect correction.
export function originalProjection(mode,fov,width=512,height=448){
 const m=WIDESCREEN_MODES[mode]??WIDESCREEN_MODES[0],x=.5*width/Math.tan(fov);
 return {scaleX:x*m.scaleX,scaleY:x*(4/3)*height/width*m.scaleY,viewport:[0,m.top*height,width,m.height*height]};
}

// Browser presentation. Off and 16:9 reproduce the PS2's 4:3 output frame (16:9 = 3D and in-race HUD inside the
// 12.5%/75% letterbox band); Anamorphic is the 4:3 frame as a 16:9 TV stretches it (stage 16:9, HUD widened 4/3).
// Every mode keeps the 4:3 vertical view angle; 16:9 and Anamorphic widen the horizontal tangent by 1/0.75.
export function widescreenView(mode,fov){
 const m=WIDESCREEN_MODES[mode]??WIDESCREEN_MODES[0],stageAspect=m.mode===2?16/9:4/3;
 return {mode:m.mode,stageAspect,band:[m.top,m.height],cameraAspect:stageAspect/m.height,fovY:2*Math.atan(Math.tan(fov)*.75)*180/Math.PI};
}

// Default when the player has never chosen: Anamorphic on widescreen displays (aspect nearer 16:9 than 4:3), else Off.
// An explicit choice from Options is saved and always wins.
export function defaultWidescreen(width=globalThis.screen?.width??globalThis.innerWidth,height=globalThis.screen?.height??globalThis.innerHeight){
 if(!(width>0&&height>0))return 0;
 return Math.max(width,height)/Math.min(width,height)>=(4/3+16/9)/2&&width>height?2:0;
}
export function loadWidescreen(){
 let saved=null;try{saved=localStorage.getItem(KEY);}catch{}
 if(saved!==null){const v=Number(saved);if(v===0||v===1||v===2)return v;}
 return defaultWidescreen();
}
export function saveWidescreen(mode){try{localStorage.setItem(KEY,String(mode));}catch{}}
