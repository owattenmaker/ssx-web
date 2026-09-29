import fs from 'node:fs';
import createCore from './runtime/core.js';
const work=new URL('../local/browser-validation/',import.meta.url);
const scenarios=['neutral','jump-grab','steer-brake','manual-resets','spin-flip'].map(name=>({name,frames:Array.from({length:600},(_,i)=>[
 name==='spin-flip'?(i>=80&&i<140?.75:0):name==='steer-brake'?(i>=60&&i<120?.4:i>=120&&i<180?-.4:0):0,
 +((name==='jump-grab'&&i>90&&i<150)||(name==='spin-flip'&&i>=60&&i<110)),+(name==='steer-brake'&&i>=200&&i<260),name==='spin-flip'&&i>=115&&i<140?2:+(name==='jump-grab'&&i>160&&i<210),name==='manual-resets'&&(i===120||i===420)?0:-1,name==='spin-flip'&&i>=80&&i<180?1:0
])}));
const railCatalog=JSON.parse(fs.readFileSync(new URL('public/assets/ARA1/rails.json',import.meta.url))),railSegment=railCatalog.rails.find(r=>r.name===(process.env.SSX_RAIL_PROBE||'spline_ARA1_RAIL_fence_3012')).segments[0].native;
const railDelta=railSegment.end.map((v,i)=>v-railSegment.start[i]),railLength=Math.hypot(railDelta[0],railDelta[2]),railDirection=railDelta.map(v=>v/railLength);
scenarios.push({name:'rail-grind',spawn:{position:railSegment.start.map((v,i)=>v-railDirection[i]*3.5),heading:Math.atan2(railDirection[0],railDirection[2]),velocity:[railDirection[0]*900,-railDirection[2]*900,0]},frames:Array.from({length:600},()=>[0,0,0,0,-1,0])});
scenarios.push({...scenarios.find(s=>s.name==='rail-grind'),name:'rail-jump',frames:Array.from({length:600},(_,i)=>[0,+(i>=50&&i<90),0,0,-1,0])});
scenarios.push({...scenarios.find(s=>s.name==='rail-grind'),name:'rail-rotations',frames:Array.from({length:600},(_,i)=>[0,0,0,0,-1,0,i>=20&&i<23?-1:i>=70&&i<73?1:0])});
const inputPath=new URL('gameplay-inputs.json',work);
if(process.argv.includes('--prepare')){fs.writeFileSync(inputPath,JSON.stringify(scenarios));process.exit(0);}
const core=await createCore();
process.on('uncaughtException',e=>{if(e instanceof Error)console.error(e);else console.error(core.getExceptionMessage(e));process.exit(1);});
const root=new URL('public/assets/',import.meta.url),read=p=>fs.readFileSync(new URL(p,root)),json=p=>JSON.parse(read(p));
const put=bytes=>{const p=core._malloc(bytes.length);core.HEAPU8.set(bytes,p);return p;};
const str=path=>put(Buffer.concat([read(path),Buffer.from([0])]));
const meta=str('ANIMATIONS/animation-packets.json'),rigPtr=str('RIDER_SAM/rider.json'),cfg=str('ANIMATIONS/initial.json'),packets=read('ANIMATIONS/animation-packets.bin'),packetsPtr=put(packets);
core._init_animation(meta,rigPtr,cfg,packetsPtr,packets.length);core._init_race(cfg);core._animation_use_physics(1);
const mesh=read('ARA1/collision.bin'),mp=put(mesh);core._init_world(mp,mesh.length/4);
const terrain=str('ARA1/terrain.json'),world=str('ARA1/world_collision.json'),hash=put(Buffer.from(json('ARA1/terrain.json').source_sha256+'\0'));
core._init_terrain(terrain);core._init_world_collision(world,hash);core._init_body_terrain(terrain);const rails=str('ARA1/rails.json');core._init_rails(rails,hash);core._free(rails);
for(const p of [meta,rigPtr,cfg,packetsPtr,mp,terrain,world,hash])core._free(p);
const poseCount=json('RIDER_SAM/rider.json').bones.length*7,start=json('ARA1/start.json');


core._reset_animation();core._reset_race();const spawn=scenarios.find(s=>s.name==='rail-grind').spawn;core._reset_rider(...spawn.position,spawn.heading);core._set_rider_velocity(...spawn.velocity);
let earnedAt=-1,jumpAt=-1,advanced=false,landed=false,peakTier=0;
const definition=json('ANIMATIONS/initial.json').original_grab_control.profile.uber[0][0];
for(let tick=0;tick<3600;tick++){
 const before=new Float32Array(core.HEAPF32.buffer,core._rider_state(),16).slice(),boost=new Float32Array(core.HEAPF32.buffer,core._boost_info(),8).slice(),rail=new Float32Array(core.HEAPF32.buffer,core._rail_gameplay_info(),8);
 if(boost[3]>0&&boost[5]>0&&earnedAt<0)earnedAt=tick;
 const railScore=new Float32Array(core.HEAPF32.buffer,core._rail_score_info(),8);if(jumpAt<0&&rail[0]&&railScore[1]>=1)jumpAt=tick;
 const held=+(jumpAt>=0&&tick<jumpAt+60),airGrab=jumpAt>=0&&tick>=jumpAt+63&&tick<jumpAt+115;
 core._rail_preinput(0);core._rail_rotation_input(process.env.SSX_RAIL_ROTATE==='0'?0:tick%60<3?1:0);core._race_begin();const r=new Float32Array(core.HEAPF32.buffer,core._step_rider(0,held,0,0),16).slice();core._animation_tick(r[7],0,0,r[9],r[8],held,airGrab?1:0,airGrab?1:0,0,0,r[15],0);core._race_end();
 const ai=new Float32Array(core.HEAPF32.buffer,core._animation_info(),19),b=new Float32Array(core.HEAPF32.buffer,core._boost_info(),8);advanced ||= ai[0]===definition.semantic;peakTier=Math.max(peakTier,b[3]);
 if(advanced&&r[11]){landed=true;break;}
}
console.log({earnedAt,jumpAt,advanced,landed,peakTier,semantic:definition.semantic,finalBoost:Array.from(new Float32Array(core.HEAPF32.buffer,core._boost_info(),8))});

if(!(earnedAt>=0&&advanced&&landed&&peakTier>=2))throw Error("Earned Uber progression remains unverified in this fixture");
