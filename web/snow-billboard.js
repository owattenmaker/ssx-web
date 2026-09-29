// Half extent in metres, capped independently in the original framebuffer's
// pixel coordinates. Device pixel ratio and CSS scaling must not change it.
// out: optional [x, y] array to fill (per-particle callers reuse one instead of allocating per particle and frame).
export function snowBillboardScale(radius,depth,projectionX,projectionY,viewport,limit,out=[0,0]){
 if(depth<=0){out[0]=out[1]=0;return out;}
 const x=Math.abs(projectionX)*viewport[0]/2,y=Math.abs(projectionY)*viewport[1]/2;
 out[0]=Math.min(radius,limit*depth/x);out[1]=Math.min(radius,limit*depth/y);return out;
}
