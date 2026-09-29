import assert from 'node:assert/strict';import {planTerrainDetail} from './terrain-detail.js';
// Ranking-only fixture. Reciprocal neighbor metadata isolates selection from
// mesh generation, which is covered by the authored package/stitching tests.
const patches=[{resource:10,bounds:[[0,0,0],[1,1,1]],neighbors:[]},{resource:20,bounds:[[10,0,0],[11,1,1]],neighbors:[]}];for(let i=0;i<2;i++)patches[i].neighbors=Array.from({length:4},(_,edge)=>({patch:1-i,edge}));
const refined=plan=>plan.plan.filter(p=>p.resolution>8).map(p=>p.index);
const first=planTerrainDetail(patches,[5.49,.5,.5],{maxRefined:1});assert.deepEqual(refined(first),[0]);
assert.deepEqual(refined(planTerrainDetail(patches,[5.51,.5,.5],{maxRefined:1,hysteresis:0})),[1]);
const retained=planTerrainDetail(patches,[5.51,.5,.5],{maxRefined:1,previousRefined:[0]});assert.deepEqual(refined(retained),[0]);
assert.deepEqual(refined(planTerrainDetail(patches,[8,.5,.5],{maxRefined:1,previousRefined:[0]})),[1],'A clearly nearer patch must replace retained detail');
assert.deepEqual(refined(planTerrainDetail(patches,[5.51,.5,.5],{maxRefined:1,previousRefined:[0],focusResource:20})),[1],'Contact priority must override retention');
assert.deepEqual(refined(planTerrainDetail(patches,[-30,.5,.5],{maxRefined:1,previousRefined:[0]})),[0]);
assert.deepEqual(refined(planTerrainDetail(patches,[-33,.5,.5],{maxRefined:1,previousRefined:[0]})),[],'Old detail must leave the expanded exit radius');
assert.throws(()=>planTerrainDetail(patches,[0,0,0],{hysteresis:NaN}),/hysteresis/);
console.log('Terrain detail hysteresis preserves contact priority, budget, replacement and exit behavior.');
