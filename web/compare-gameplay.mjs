import fs from 'node:fs';
import createCore from './runtime/core.js';
const work=new URL('../local/browser-validation/',import.meta.url);
const riderPackage=process.argv.includes('--zoe')?'RIDER_ZOE':'RIDER_SAM',suffix=process.argv.includes('--zoe')?'-zoe':'';
const scenarios=['neutral','jump-grab','steer-brake','manual-resets','spin-flip'].map(name=>({name,frames:Array.from({length:600},(_,i)=>[
 name==='spin-flip'?(i>=80&&i<140?.75:0):name==='steer-brake'?(i>=60&&i<120?.4:i>=120&&i<180?-.4:0):0,
 +((name==='jump-grab'&&i>90&&i<150)||(name==='spin-flip'&&i>=60&&i<110)),+(name==='steer-brake'&&i>=200&&i<260),name==='spin-flip'&&i>=115&&i<140?2:+(name==='jump-grab'&&i>160&&i<210),name==='manual-resets'&&(i===120||i===420)?0:-1,name==='spin-flip'&&i>=80&&i<180?1:0
])}));
const railCatalog=JSON.parse(fs.readFileSync(new URL('public/assets/ARA1/rails.json',import.meta.url))),railSegment=railCatalog.rails.find(r=>r.name==='spline_ARA1_RAIL_3007').segments[0].native;
const railDelta=railSegment.end.map((v,i)=>v-railSegment.start[i]),railLength=Math.hypot(railDelta[0],railDelta[2]),railDirection=railDelta.map(v=>v/railLength);
scenarios.push({name:'rail-grind',spawn:{position:railSegment.start.map((v,i)=>v-railDirection[i]*3.5),heading:Math.atan2(railDirection[0],railDirection[2]),velocity:[railDirection[0]*900,-railDirection[2]*900,0]},frames:Array.from({length:600},()=>[0,0,0,0,-1,0])});
scenarios.push({...scenarios.find(s=>s.name==='rail-grind'),name:'rail-jump',frames:Array.from({length:600},(_,i)=>[0,+(i>=50&&i<90),0,0,-1,0])});
scenarios.push({...scenarios.find(s=>s.name==='rail-grind'),name:'rail-rotations',frames:Array.from({length:600},(_,i)=>[0,0,0,0,-1,0,i>=20&&i<23?-1:i>=70&&i<73?1:0])});
scenarios.push({...scenarios.find(s=>s.name==='rail-grind'),name:'rail-crash',collisionEvent:{tick:21,animation:350,speed:900},frames:Array.from({length:360},()=>[0,0,0,0,-1,0])});
scenarios.push({...scenarios.find(s=>s.name==='rail-crash'),name:'rail-soft-collision',collisionEvent:{tick:21,animation:58,kind:'soft'}});
for(const mask of [1,2,4,8])scenarios.push({...scenarios.find(s=>s.name==='rail-grind'),name:'rail-uber-'+mask,meterAward:{tick:20,amount:1},frames:Array.from({length:360},(_,i)=>[0,0,0,i>=30&&i<90?mask:0,-1,0])});
scenarios.push({...scenarios.find(s=>s.name==='rail-uber-1'),name:'rail-uber-held-edge',frames:Array.from({length:600},(_,i)=>[0,0,0,i>=30&&i<300?1:0,-1,0])});
scenarios.push({name:'event-start',eventStart:true,frames:Array.from({length:600},()=>[0,0,0,0,-1,0])});
const powder=JSON.parse(fs.readFileSync(new URL('public/assets/ARA1/terrain.json',import.meta.url))).patches.find(p=>p.resource_id===119816);
const powderCenter=[0,0,0];for(let j=0;j<4;j++)for(let i=0;i<4;i++)for(let k=0;k<3;k++)powderCenter[k]+=powder.coefficients[j*4+i][k]*.5**(i+j);
powderCenter[1]+=.04;
const powderHeading=JSON.parse(fs.readFileSync(new URL('public/assets/ARA1/start.json',import.meta.url))).heading;
scenarios.push({name:'powder-snow',spawn:{position:powderCenter,heading:powderHeading},frames:Array.from({length:360},(_,i)=>[i<90?.5:0,+(i>=110&&i<170),+(i>=60&&i<90),0,-1,0])});
const rock=JSON.parse(fs.readFileSync(new URL('public/assets/ARA1/terrain.json',import.meta.url))).patches.find(p=>p.resource_id===100104);
const rockCenter=[0,0,0];for(let j=0;j<4;j++)for(let i=0;i<4;i++)for(let k=0;k<3;k++)rockCenter[k]+=rock.coefficients[j*4+i][k]*.5**(i+j);
rockCenter[1]+=.04;
scenarios.push({...scenarios.find(s=>s.name==='powder-snow'),name:'rock-spray',spawn:{position:rockCenter,heading:powderHeading}});
const pickupCatalog=JSON.parse(fs.readFileSync(new URL('../local/browser-pickups/ara1-catalog.json',import.meta.url)));
const pickupDefinitions=JSON.parse(fs.readFileSync(new URL('public/assets/ANIMATIONS/initial.json',import.meta.url))).original_pickups.items;
for(const item of pickupDefinitions){const record=pickupCatalog.items.find(p=>p.resource===item.resource);scenarios.push({name:'pickup-'+item.resource,spawn:{position:record.position_m,heading:powderHeading,velocity:[0,0,0]},frames:Array.from({length:150},()=>[0,0,0,0,-1,0])});}
const inputPath=new URL('gameplay-inputs.json',work);
if(process.argv.includes('--prepare')){fs.writeFileSync(inputPath,JSON.stringify(scenarios));process.exit(0);}
const inputs=JSON.parse(fs.readFileSync(inputPath)),core=await createCore();
process.on('uncaughtException',e=>{if(e instanceof Error)console.error(e);else console.error(core.getExceptionMessage(e));process.exit(1);});
const root=new URL('public/assets/',import.meta.url),read=p=>fs.readFileSync(new URL(p,root)),json=p=>JSON.parse(read(p));
const put=bytes=>{const p=core._malloc(bytes.length);core.HEAPU8.set(bytes,p);return p;};
const str=path=>put(Buffer.concat([read(path),Buffer.from([0])]));
const meta=str('ANIMATIONS/animation-packets.json'),rigPtr=str(riderPackage+'/rider.json'),cfg=str('ANIMATIONS/initial.json'),packets=read('ANIMATIONS/animation-packets.bin'),packetsPtr=put(packets);
core._init_animation(meta,rigPtr,cfg,packetsPtr,packets.length);core._init_race(cfg);core._animation_use_physics(1);
const mesh=read('ARA1/collision.bin'),mp=put(mesh);core._init_world(mp,mesh.length/4);
const terrain=str('ARA1/terrain.json'),world=str('ARA1/world_collision.json'),hash=put(Buffer.from(json('ARA1/terrain.json').source_sha256+'\0'));
core._init_terrain(terrain);core._init_world_collision(world,hash);core._init_body_terrain(terrain);const rails=str('ARA1/rails.json');core._init_rails(rails,hash);core._free(rails);
for(const p of [meta,rigPtr,cfg,packetsPtr,mp,terrain,world,hash])core._free(p);
const env=str('ARA1/environment.json'),envBytes=read('ARA1/environment.bin'),envPtr=put(envBytes);core._init_environment(env,envPtr,envBytes.length);core._free(env);core._free(envPtr);
const fogPackage=str('ARA1/fog-tree.json');core._init_fog(fogPackage);core._free(fogPackage);
const poseCount=json(riderPackage+'/rider.json').bones.length*7,start=json('ARA1/start.json');
const skinFloats=json(riderPackage+'/rider.json').source_bind_matrix_words?.length*16||0;
const paletteFloats=new Set((json(riderPackage+'/rider.json').source_skin||[]).map(x=>JSON.stringify(x))).size*16;
const fields=[['motion',16],['animation',19],['posedPhysical',12],['skinMatrixCount',1],['skinMatrices',skinFloats],['skinPaletteCount',1],['skinPalette',paletteFloats],['race',8],['camera',9],['cameraRenderView',16],['fog',11],['fogPalette',9],['environment',7],['irradianceInfo',10],['irradiance',40],['reset',9],['crash',12],['pose',poseCount],['localPose',poseCount],['poseControls',17],['sourcePhysics',10],['sourceCrash',10],['queryBounds',12],['rail',8],['railJump',3],['railRotation',4],['railScore',8],['railUber',6],['railExit',4],['boostHud',13],['pickups',26],['snowInfo',23],['snowFlipbook',20],['wakeInfo',13],['boostFx',12],['fxBufferHashes',30]];
const width=fields.reduce((n,[,count])=>n+count,0),nativeBytes=fs.readFileSync(new URL('native-gameplay'+suffix+'.bin',work)),native=new Float32Array(nativeBytes.buffer,nativeBytes.byteOffset,nativeBytes.length/4);
const fieldOffset=name=>fields.slice(0,fields.findIndex(([key])=>key===name)).reduce((n,[,count])=>n+count,0);
const total=inputs.reduce((n,s)=>n+s.frames.length,0);if(native.length!==total*width)throw Error('Native gameplay trace extent mismatch');
const f=(p,n)=>new Float32Array(core.HEAPF32.buffer,p,n).slice();const fxHashes=()=>{const parts=[],snow=f(core._snow_info(),23),wake=f(core._wake_info(),13);const hash=(pointer,count)=>{let h=2166136261;for(const b of new Uint8Array(core.HEAPF32.buffer,pointer,count*4))h=Math.imul(h^b,16777619)>>>0;parts.push(h&65535,h>>>16);};for(let i=0;i<10;i++)hash(core._snow_particles(i),snow[i]*8);hash(core._wake_vertices(),wake[11]*9);const boost=f(core._boost_fx_info(),12);for(let i=0;i<3;i++)hash(core._boost_fx_vertices(i),boost[6+i]*9);hash(core._fog_palette_rgba(),256);return parts;};const rows=[],reports=[];let globalTick=0;
for(const scenario of inputs){core._reset_fog();core._reset_animation();core._reset_race();const initial=scenario.spawn||start;core._reset_rider(...initial.position,initial.heading);if(initial.velocity)core._set_rider_velocity(...initial.velocity);if(scenario.eventStart)core._start_event();const report={scenario:scenario.name,frames:scenario.frames.length,firstMismatch:null,firstMismatchByGroup:{},maxPoseDifference:null,mismatchedFrames:0,maxPositionErrorMeters:0,maxPoseErrorCm:0,animationFieldMismatchFrames:0,maxAxisBlend:0};
 for(let tick=0;tick<scenario.frames.length;tick++,globalTick++){
  if(scenario.meterAward?.tick===tick)core._award_trick_meter(scenario.meterAward.amount);
  if(scenario.collisionEvent?.tick===tick){if(!f(core._rail_gameplay_info(),8)[0])throw Error('Rail crash fixture is not grinding');if(scenario.collisionEvent.kind==='soft')core._soft_collision_begin(scenario.collisionEvent.animation,0);else {core._hard_crash_begin(scenario.collisionEvent.animation,scenario.collisionEvent.speed);if(f(core._crash_info(),12)[2]!==1||f(core._rail_gameplay_info(),8)[0])throw Error('Rail crash handoff failed');}}
  const [turn,held,brake,grab,resetReason=-1,flip=0,rotation=0]=scenario.frames[tick];if(resetReason>=0)core._request_rider_reset(resetReason);core._rail_shoulder_input(grab);core._rail_preinput(flip);core._rail_rotation_input(rotation);core._race_begin();const r=f(core._step_rider(turn,held,brake,0),16);
  const pose=f(core._animation_tick(r[7],turn,brake,r[9],r[8],held,grab,0,0,0,r[15],flip),poseCount),physical=f(core._pose_physical(),12);
  const camera=f(core._step_camera_head(...physical.slice(9,12)),9),race=f(core._race_end(),8);
  const groups=[f(core._rider_state(),16),f(core._animation_info(),19),f(core._pose_physical(),12),[core._rider_skin_matrix_count()],f(core._rider_skin_matrices(),skinFloats),[core._rider_skin_palette_count()],f(core._rider_skin_palette(),paletteFloats),race,camera,f(core._camera_render_view(),16),f(core._fog_info(),11),f(core._fog_palette_info(),9),f(core._environment_info(),7),f(core._environment_irradiance_info(),10),f(core._environment_irradiance(),40),f(core._reset_info(),9),f(core._crash_info(),12),pose,f(core._sampled_local_pose(),poseCount),f(core._pose_controls_info(),17),f(core._source_motion_audit(),20).slice(0,10),f(core._source_motion_audit(),20).slice(10),f(core._rider_query_bounds(),12),f(core._rail_gameplay_info(),8),f(core._rail_jump_info(),3),f(core._rail_rotation_info(),4),f(core._rail_score_info(),8),f(core._rail_uber_info(),6),f(core._rail_exit_info(),4),f(core._boost_hud_info(),13),f(core._pickup_info(),26),f(core._snow_info(),23),f(core._snow_flipbook_info(),20),f(core._wake_info(),13),f(core._boost_fx_info(),12),fxHashes()];
  report.maxAxisBlend=Math.max(report.maxAxisBlend,f(core._animation_inputs(),17)[10]);
  const sourceAudit=f(core._source_motion_audit(),20),crashState=f(core._crash_info(),12);
  if(crashState[0]&&crashState[4]===0)for(let k=0;k<6;k++)if(sourceAudit[k]!==sourceAudit[10+k])throw Error(`Crash entry publisher changed source state ${scenario.name}:${tick}:${k}`);
  const row=groups.flatMap(x=>Array.from(x));if(!row.every(Number.isFinite))throw Error(`Nonfinite gameplay state ${scenario.name}:${tick}`);rows.push(...row);
  const expected=native.subarray(globalTick*width,(globalTick+1)*width);const nativeCrash=fieldOffset('crash'),nativePhysics=fieldOffset('sourcePhysics'),nativeActor=fieldOffset('sourceCrash');
  if(expected[nativeCrash]&&expected[nativeCrash+4]===0)for(let k=0;k<6;k++)if(expected[nativePhysics+k]!==expected[nativeActor+k])throw Error(`Native crash publisher changed source state ${scenario.name}:${tick}:${k}`);
  let mismatched=false,offset=0;
  fields.forEach(([name,count],group)=>{let different=false;for(let j=0;j<count;j++)if(row[offset+j]!==expected[offset+j]){different=mismatched=true;const difference={tick,group:name,field:j,native:expected[offset+j],wasm:row[offset+j]};if(!report.firstMismatch)report.firstMismatch=difference;if(!report.firstMismatchByGroup[name])report.firstMismatchByGroup[name]=difference;}if(name==='animation'&&different)report.animationFieldMismatchFrames++;
   if(name==='pose')for(let j=0;j<count;j+=7){const error=Math.hypot(...[0,1,2].map(k=>row[offset+j+k]-expected[offset+j+k]));if(error>report.maxPoseErrorCm){report.maxPoseErrorCm=error;report.maxPoseDifference={tick,bone:j/7,native:Array.from(expected.slice(offset+j,offset+j+3)),wasm:row.slice(offset+j,offset+j+3)};}}offset+=count;
  });
  report.mismatchedFrames+=mismatched;report.maxPositionErrorMeters=Math.max(report.maxPositionErrorMeters,Math.hypot(...[0,1,2].map(k=>row[k]-expected[k])));
 }
 if(scenario.name==='manual-resets'){const reset=f(core._reset_info(),9);report.completedResets=reset[3];if(reset[0]||reset[2]!==2||reset[3]!==2)throw Error('Repeated manual reset scenario did not complete both placements/recoveries');}
 if(scenario.name==='spin-flip'&&report.maxAxisBlend<=.001)throw Error('Spin/flip scenario never exercised blended airborne axes');
 if(scenario.name==='rail-grind'){const scoring=f(core._rail_score_info(),8);report.railPoints=scoring[3];report.railBoostAward=scoring[6];if(scoring[3]<1000||scoring[6]<=0)throw Error('Rail grind did not award source points/boost');const rail=f(core._rail_gameplay_info(),8);report.railAttachments=rail[2];report.railExits=rail[3];report.railFrames=rail[4];if(rail[2]<1||rail[3]<1||rail[4]<60)throw Error('Rail scenario did not exercise attach, motion and exit');}
 if(scenario.name==='rail-jump'){report.railJumps=f(core._rail_jump_info(),3)[2];if(report.railJumps!==1)throw Error('Rail jump scenario must launch exactly once');}
 if(scenario.name==='rail-rotations'){report.railRotations=f(core._rail_rotation_info(),4)[0];if(report.railRotations!==2)throw Error('Rail rotation pulses must produce exactly two rotations');}
 reports.push(report);
}
fs.writeFileSync(new URL('wasm-gameplay'+suffix+'.bin',work),Buffer.from(new Float32Array(rows).buffer));
const report={fields,scenarios:reports,note:'Full browser host pipeline against native engine/controller arithmetic with restored native graph scopes. Not a PCSX2 gameplay capture, rendering comparison or proof of source host scheduling.'};
fs.writeFileSync(new URL('gameplay-parity'+suffix+'.json',work),JSON.stringify(report,null,2));console.log(JSON.stringify(report,null,2));

if(reports.some(report=>report.mismatchedFrames))throw Error('Native/WASM gameplay trace differs; see gameplay-parity.json');
