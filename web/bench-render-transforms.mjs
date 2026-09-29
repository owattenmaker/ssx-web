// Compare cached static-world matrices with automatic recomposition on the same
// loaded scene. Run while Vite serves web/: node bench-render-transforms.mjs [URL].
// This isolates scene-update CPU cost; it is not an end-to-end FPS benchmark.
import assert from 'node:assert/strict';
import {startBrowser} from './headless-chrome.mjs';
const browser=await startBrowser();
if(!browser)throw Error('Chrome is required for the render-transform benchmark');
try{
 await browser.goto(process.argv[2]??'http://127.0.0.1:5173/?course=ARA1&rider=zoe&autostart=1&mute=1&perf=1');
 await browser.waitFor("document.getElementById('stage')?.dataset.screen === 'game'",180000);
 const result=await browser.evaluate(`(()=>{
  const {scene,worldScene}=__perfScene();scene.updateMatrixWorld();
  const cached=[scene,worldScene];worldScene.traverse(o=>{if(o!==worldScene&&!o.matrixAutoUpdate&&!o.userData.liveComp&&o.userData.movingResource===undefined)cached.push(o);});
  const all=[];scene.traverse(o=>all.push([o,Array.from(o.matrixWorld.elements)]));
  const times={automatic:[],cached:[]};let mismatches=0;
  try{
   for(const o of cached)o.matrixAutoUpdate=true;scene.updateMatrixWorld();
   for(const [o,m] of all)for(let i=0;i<16;i++)if(o.matrixWorld.elements[i]!==m[i])mismatches++;
   for(let round=0;round<8;round++)for(const cache of (round%2?[true,false]:[false,true])){
    for(const o of cached)o.matrixAutoUpdate=!cache;scene.updateMatrixWorld();
    const start=performance.now();for(let i=0;i<100;i++)scene.updateMatrixWorld();times[cache?'cached':'automatic'].push((performance.now()-start)/100);
   }
  }finally{for(const o of cached)o.matrixAutoUpdate=false;}
  return {objects:all.length,cached:cached.length,mismatches,times};
 })()`);
 assert.equal(result.mismatches,0,'cached transforms must match full recomposition');
 const errors=browser.logs.filter(l=>/exception:|error:/.test(l));assert.deepEqual(errors,[],'browser runtime errors');
 const median=a=>[...a].sort((a,b)=>a-b)[a.length>>1],before=median(result.times.automatic),after=median(result.times.cached);
 console.log(JSON.stringify({...result,medianMs:{automatic:before,cached:after},ratio:after/before},null,2));
}finally{await browser.close();}
