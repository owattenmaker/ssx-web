import {boostOrb,boostOrbGlow,boostCoilGlows} from './boost-orb.js';
import { SPRITE_2D } from './sprite-canvas.js'; // offscreen sprite canvas kind (software in Firefox, docs/firefox-load.md)
// Source 21D1A0 uses bottom-up clipping of caps and eight middle sprites.
export function gaugeRects(profile,layer,fraction){
 const f=Math.max(0,Math.min(1,Number.isFinite(fraction)?fraction:0));
 if(f===0)return [];
 const top=profile.bottom-profile.fillHeight*f;
 return layer.rects.filter(r=>r.y+r.h>top).map(r=>{
  const cut=f===1?0:Math.max(0,top-r.y);
  return {...r,y:r.y+cut,h:r.h-cut,v:r.v+r.vh*cut/r.h,vh:r.vh*(1-cut/r.h)};
 });
}
// Ordinary (non-flashing) type6 palette from 1ECD60. GS channels use 128 as unity.
export function gaugeColour(profile,f){
 const [a,b,c]=profile.colours;let lo,hi,t;
 if(f<=.3500423729419708)return a;
 if(f<.4500329792499542){lo=a;hi=b;t=((f-.4000376760959625)+.0499952957034111)*10.000941276550293;}
 else if(f<=.7499593496322632)return b;
 else if(f<.8499499559402466){lo=b;hi=c;t=((f-.7999546527862549)+.0499952957034111)*10.000941276550293;}
 else return c;
 return lo.map((v,i)=>v+(hi[i]-v)*t);
}
export class BoostGauge {
  constructor(profile, atlas, glow) {
    this.profile = profile;
    this.atlas = atlas;
    this.tint = document.createElement('canvas');
    this.tint.width = atlas.width;
    this.tint.height = atlas.height;
    this.ctx = this.tint.getContext('2d', SPRITE_2D);
    this.ctx.drawImage(atlas, 0, 0);
    this.source = this.ctx.getImageData(0, 0, atlas.width, atlas.height);
    this.output = this.ctx.createImageData(atlas.width, atlas.height);
    this.red = document.createElement('canvas');
    this.red.width = atlas.width;
    this.red.height = atlas.height;
    this.red.getContext('2d', SPRITE_2D).drawImage(this.tinted([1, 1, 0, 0]), 0, 0);
    this.orbTints = profile.flashColours.map((colour) => {
      const c = document.createElement('canvas');
      c.width = atlas.width;
      c.height = atlas.height;
      c.getContext('2d', SPRITE_2D).drawImage(this.tinted(colour), 0, 0);
      return c;
    });
    this.glowTints = profile.flashColours.map((colour) => {
      const c = document.createElement('canvas');
      c.width = glow.width;
      c.height = glow.height;
      const context = c.getContext('2d', SPRITE_2D);
      context.drawImage(glow, 0, 0);
      const pixels = context.getImageData(0, 0, c.width, c.height);
      for (let i = 0; i < pixels.data.length; i += 4)
        for (let k = 0; k < 3; k++) pixels.data[i + k] *= Math.trunc(colour[k + 1] * 128) / 128;
      context.putImageData(pixels, 0, 0);
      return c;
    });
    this.glowColumns = this.glowTints.map((im) => {
      const c = document.createElement('canvas');
      c.width = 1;
      c.height = im.height;
      const dst = c.getContext('2d', SPRITE_2D),
        src = im.getContext('2d', SPRITE_2D).getImageData(im.width / 2 - 1, 0, 2, im.height).data,
        p = dst.createImageData(1, im.height);
      for (let y = 0; y < im.height; y++) {
        const i = y * 8,
          j = y * 4,
          a = src[i + 3] + src[i + 7];
        p.data[j + 3] = a / 2;
        for (let k = 0; k < 3; k++) p.data[j + k] = a ? (src[i + k] * src[i + 3] + src[i + 4 + k] * src[i + 7]) / a : 0;
      }
      dst.putImageData(p, 0, 0);
      return c;
    });
  }
  tinted(argb) {
    const rgba = [argb[1], argb[2], argb[3], argb[0]].map((x) => Math.max(0, Math.min(128, Math.trunc(x * 128))));
    const key = rgba.join(',');
    if (key !== this.key) {
      for (let i = 0; i < this.source.data.length; i++) this.output.data[i] = (this.source.data[i] * rgba[i % 4]) / 128;
      this.ctx.putImageData(this.output, 0, 0);
      this.key = key;
    }
    return this.tint;
  }
  drawOrb(ctx, phase, tier) {
    const o = boostOrb(this.profile, phase, tier),
      glow = boostOrbGlow(this.profile, phase, tier);
    const atlas = o.palette < 0 ? this.atlas : this.orbTints[o.palette],
      [u, v, u1, v1] = o.uv;
    ctx.save();
    ctx.scale(1, 448 / 480);
    if (o.size[0] > 0)
      ctx.drawImage(atlas, u * atlas.width, v * atlas.height, (u1 - u) * atlas.width, (v1 - v) * atlas.height, ...o.position, ...o.size);
    if (glow) {
      ctx.globalAlpha = Math.trunc(glow.argb[0] * 128) / 128;
      ctx.drawImage(this.glowTints[glow.palette], ...glow.position, ...glow.size);
    }
    ctx.restore();
  }
  draw(ctx, preview, stored, phase = -1, tier = 0) {
    const flashing = phase >= 0,
      palette = boostOrb(this.profile, phase, tier).palette;
    ctx.save();
    ctx.scale(1, 448 / 480);
    for (const g of boostCoilGlows(this.profile, phase, tier)) {
      const [u, v, u1, v1] = g.uv,
        im = this.glowTints[g.palette];
      ctx.globalAlpha = Math.trunc(g.argb[0] * 128) / 128;
      if (u === u1) ctx.drawImage(this.glowColumns[g.palette], ...g.position, ...g.size);
      else ctx.drawImage(im, u * im.width, v * im.height, (u1 - u) * im.width, (v1 - v) * im.height, ...g.position, ...g.size);
    }
    ctx.globalAlpha = 1;
    for (const layer of this.profile.layers) {
      const fraction = layer.widget === 6 ? stored : layer.widget === 7 ? preview : 1;
      const background = flashing && layer.widget <= 5;
      const atlas = background
        ? this.orbTints[palette]
        : flashing
          ? this.atlas
          : layer.widget === 6
            ? this.tinted(gaugeColour(this.profile, stored))
            : layer.widget === 7
              ? this.red
              : this.atlas;
      const rects = background ? this.profile.flashBackgrounds[layer.widget] : gaugeRects(this.profile, layer, fraction);
      for (const r of rects)
        ctx.drawImage(atlas, r.u * atlas.width, r.v * atlas.height, r.uw * atlas.width, r.vh * atlas.height, r.x, r.y, r.w, r.h);
    }
    ctx.restore();
  }
}
