// Original1EFC58..1EFDB0, before the separate glow submission.
export function boostOrb(profile,phase,tier){
 const o=profile.orb,active=phase>=0;
 const width=o.size[0]*(active?(phase<1?phase:2-phase):1);
 const palette=tier<5?0:tier<10?1:tier===10?2:3;
 return {position:[o.position[0]+(o.size[0]-width)/2,o.position[1]],size:[width,o.size[1]],uv:o.uv,palette:active?palette:-1,argb:active?profile.flashColours[palette]:[1,1,1,1]};
}
// Original1EFDB4..1EFE90 +21E7E0, using the renderer-owned part texture.
export function boostOrbGlow(profile,phase,tier){
 if(phase<0)return null;
 const o=boostOrb(profile,phase,tier),g=profile.orbGlow;
 const pulse=phase>1?2-phase:phase;
 return {position:o.position.map(x=>x-g.padding),size:o.size.map(x=>x+g.extent),palette:o.palette,argb:[o.argb[0]*(g.alphaLow+(g.alphaHigh-g.alphaLow)*pulse),...o.argb.slice(1)],uv:[0,0,1,1]};
}
export function boostCoilGlows(profile,phase,tier){
 const glow=boostOrbGlow(profile,phase,tier);
 return glow?profile.coilGlows.map(r=>({...r,palette:glow.palette,argb:glow.argb})):[];
}
