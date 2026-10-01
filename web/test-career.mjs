// Career / event rules against the original tables (tools/export_career.py -> public/assets/CAREER/career.json).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {Career,MODE,MEDAL,eventKey,estimateFinishTicks,aiFreestyleScore,adaptLevel,rankEntries,recordSlot,platinumThreshold,placementMedal,riderRanking,nextRandom} from './career.js';
import {loadSave,writeSave} from './career-save.js';
import {GearInventory} from './lodge.js';
import {Locale,nameHash,format} from './locale.js';

const data=JSON.parse(fs.readFileSync('public/assets/CAREER/career.json'));
const rules=data.rules;
class Memory{constructor(){this.map=new Map();}getItem(k){return this.map.get(k)??null;}setItem(k,v){this.map.set(k,String(v));}removeItem(k){this.map.delete(k);}}

// --- original tables ---------------------------------------------------------------------------------
const byCode=Object.fromEntries(data.courses.map(c=>[c.code,c]));
assert.equal(data.courses.length,23);
assert.deepEqual(['ARA1','BRA2','CRA3','DRA4','ERA5'].map(c=>byCode[c].kind),Array(5).fill('race'));
assert.deepEqual(['ARA1','BRA2','CRA3','DRA4','ERA5'].map(c=>byCode[c].field54+1),[1,1,2,2,3],'career peak = course +0x54 (Intimidator is a Peak 2 race)');
assert.equal(byCode.BHP1.name,'The Junction');assert.equal(byCode.BHP1.kind,'superpipe');
assert.deepEqual(rules.cash[0],[25000,10000,5000,2500],'Snow Jam platinum/gold/silver/bronze cash (0x4405A0)');
assert.deepEqual(rules.cash[11],[25000,7500,5000,2500],'The Junction cash');
assert.deepEqual(rules.earnings_goal,[100000,250000,1000000]);
assert.deepEqual(rules.attribute_cost,[250,500,750,1000,1250,1500,1750,2000,2500,5000]);
assert.deepEqual(rules.race,{dnf_ticks:360000,level_up_margin:[601,301,181],estimate_min_speed:30});
assert.deepEqual(rules.freestyle_ai.find(r=>r.course===11&&r.round===3),{course:11,round:3,scores:[180000,120000,50000,40000,30000],time_limit:120});
assert.deepEqual(platinumThreshold(rules,MODE.RACE,0),{ticks:150*60});
assert.deepEqual(platinumThreshold(rules,MODE.HALFPIPE,11),{score:700000});
assert.deepEqual(platinumThreshold(rules,MODE.RIVAL_POINTS,14),{score:400000});
assert.equal(recordSlot(rules,MODE.RACE,0),12);assert.equal(recordSlot(rules,MODE.SLOPESTYLE,5),17);assert.equal(recordSlot(rules,MODE.RIVAL_POINTS,14),7);assert.equal(recordSlot(rules,6,null),0);
assert.deepEqual(rules.records[12].map(r=>r.value),[177,185,191,204,215],'Snow Jam default top times (PS2 transport shows Top time 02:57)');
assert.equal(rules.records[17][0].value,340000,'R&B default top score (PS2 transport shows 340000)');
const goal=rules.goal_events.find(g=>g.peak===1&&g.goal==='race').events;
assert.deepEqual(goal.map(e=>[e.mode,e.course]),[[0,0],[0,1],[4,14],[6,null]],'Peak 1 race goal: Snow Jam, Metro-City, Happiness Race, Peak 1 Race');
assert.deepEqual(rules.goal_events.find(g=>g.peak===1&&g.goal==='freestyle').events.map(e=>[e.mode,e.course]),[[1,5],[3,8],[2,11],[5,14],[9,null]]);
assert.deepEqual(data.characters.map(c=>c.first),['Moby','Kaori','Allegra','Mac','Zoe','Griff','Elise','Nate','Psymon','Viggo']);

// --- locale ------------------------------------------------------------------------------------------
const loc=new Locale(data.strings);
assert.equal(nameHash('kT_FULLBIO1Zoe')>>>0,nameHash('kT_FULLBIO1Zoe'));
assert.equal(loc.text('kT_OVRCMNQFHeat1Stand'),'Qualifier Heat 1 Standings');
assert.equal(loc.text('kT_TITLELodgePeak1'),'Lodge - Peak 1');
assert.equal(format(loc.text('kT_FSOBJEarnXpts'),1234),'Earn 1234 more points to qualify for the finals.');
assert.equal(loc.text(0x0812f644),"Congratulations!  You've made it to the final round.");

// --- helpers -----------------------------------------------------------------------------------------
assert.deepEqual(rankEntries([300,100,200],true),[2,0,1]);assert.deepEqual(rankEntries([5,5,9],false),[1,2,0]);
assert.equal(placementMedal(MODE.RACE,0),MEDAL.GOLD);assert.equal(placementMedal(MODE.RACE,2),MEDAL.BRONZE);assert.equal(placementMedal(MODE.RACE,3),MEDAL.NONE);
assert.equal(placementMedal(MODE.RIVAL_TIME,1),MEDAL.NONE);
// 0x122D78: 1000 ticks, 20000 cm covered -> 20 cm/tick, below the 30-place floor for the leader -> 30.
assert.equal(estimateFinishTicks({raceTicks:1000,origin:353496.15625,remaining:333496.15625,place:0}),1000+Math.trunc(Math.fround(333496.15625/30)));
assert.equal(estimateFinishTicks({raceTicks:1000,origin:100000,remaining:50000,place:5}),1000+1000);
assert.deepEqual(adaptLevel({level:1,counter:0},true),{level:1,counter:1});assert.deepEqual(adaptLevel({level:1,counter:1},true),{level:2,counter:0});
assert.deepEqual(adaptLevel({level:2,counter:1},true),{level:2,counter:0});assert.deepEqual(adaptLevel({level:1,counter:-1},false),{level:0,counter:0});
assert.equal(riderRanking([5,5,5,5,5,5,5]),'1.0');assert.equal(riderRanking([10,5,5,5,5,5,5]),'1.1');
{const s={seed:1};const a=nextRandom(s),b=nextRandom(s);assert.notEqual(a,b);}
// 0x1453D0: jitter r = rand%200-100 (199 -> +99 -> +4.95%), result rounded down to 20; leading AI never scaled.
{const s=180000+Math.trunc(Math.fround(Math.fround(180000*99)*Math.fround(rules.ai_score.jitter)));assert.equal(aiFreestyleScore(rules,11,3,0,0,()=>199),s-s%20);assert.equal(s-s%20,188900);}
assert.equal(aiFreestyleScore(rules,11,3,1,0,()=>100),Math.trunc(Math.fround(120000*Math.fround(rules.ai_score.level0_scale)))-Math.trunc(Math.fround(120000*Math.fround(rules.ai_score.level0_scale)))%20);
// Real PS2 frame (ARMSX2, local/ps2-capture/menus/single/09-rb-load-d.png): R&B Single Event posted scores
// Nate 254500, Kaori 124080, Elise 49800 = heat-1 columns 0..2 at difficulty 1 with jitter draws 36, 68, -8.
for(const [ai,value] of [[0,254500],[1,124080],[2,49800]]){
 const hits=[];for(let r=0;r<200;r++)if(aiFreestyleScore(rules,5,1,ai,1,()=>r)===value)hits.push(r-100);
 assert(hits.length>0,`PS2 posted score ${value} not reachable by 0x1453D0`);
}
for(let i=0;i<50;i++){const v=aiFreestyleScore(rules,5,1,2,2,()=>i*37);assert.equal(v%20,0);assert(v>=50000*1.2*.94&&v<=50000*1.2*1.06);}

// --- race event: qualifier -> semi -> final ------------------------------------------------------------
const storage=new Memory(),career=new Career(data,{storage});
const lineup=(humanTicks,others)=>[{human:true,character:3,finishTicks:humanTicks},...others.map((t,i)=>({human:false,character:i,finishTicks:t}))];
let ev=career.startEvent('sam',MODE.RACE,0);
assert.equal(ev.round,1);assert.deepEqual(career.objectives(ev),['kT_CMNRaceTop3SemiFinal']);
let r=career.raceResult(lineup(9000,[8000,8500,8800,9500,9900]),{raceTicks:9000,origin:353496});
assert.equal(r.place,3);assert.equal(r.advance,false);assert.equal(r.message,'kT_OVRCMNSorryGetThirdSemi');assert.equal(ev.round,1,'failed qualifier repeats');
assert.deepEqual(career.rider('sam').level.race,{level:1,counter:-1});
r=career.raceResult(lineup(9000,[8000,8500,9200,9500,9900]),{raceTicks:9000,origin:353496});
assert.equal(r.place,2);assert.equal(r.advance,true);assert.equal(r.message,'kT_OVRCongratGoToSemi');assert.equal(ev.round,2);
assert.deepEqual(career.objectives(ev),['kT_OVRRaceObjectives20']);
// unfinished riders are estimated at the human's finish (0x122D78), DNF riders get 360000 ticks
r=career.raceResult([{human:true,character:3,finishTicks:9000},{human:false,finishTicks:null,remaining:1000,place:1},{human:false,dnf:true},{human:false,finishTicks:8000},{human:false,finishTicks:null,remaining:90000,place:4},{human:false,finishTicks:9100}],{raceTicks:9000,origin:353496});
assert.equal(r.times[2],360000);assert.equal(r.times[1],estimateFinishTicks({raceTicks:9000,origin:353496,remaining:1000,place:1}));
assert.equal(r.place,1);assert.equal(ev.round,3);assert.equal(r.message,'kT_OVRRaceObjectives7');
assert.deepEqual(career.objectives(ev),['kT_OVRRaceObjectives15']);
// final: 1st by more than 181 ticks over slot 1, under the 150 s platinum time
r=career.raceResult(lineup(8900,[9200,9300,9400,9500,9600]),{raceTicks:8900,origin:353496});
assert.equal(r.medal,MEDAL.PLATINUM);assert.equal(r.cash,25000);assert.equal(career.rider('sam').cash,25000);
assert.equal(career.medal('sam',MODE.RACE,0),MEDAL.PLATINUM);assert.equal(r.record,0,'new Snow Jam record');
assert.deepEqual(career.rider('sam').level.race,{level:1,counter:0});
// passed final: next entry starts a fresh qualifier; repeating the same medal pays half
ev=career.startEvent('sam',MODE.RACE,0);assert.equal(ev.round,1);
career.raceResult(lineup(9000,[9100,9200,9300,9400,9500]),{raceTicks:9000,origin:353496});
career.raceResult(lineup(9000,[9100,9200,9300,9400,9500]),{raceTicks:9000,origin:353496});
r=career.raceResult(lineup(9100,[8000,9200,9300,9400,9500]),{raceTicks:9100,origin:353496});
assert.equal(r.medal,MEDAL.SILVER);assert.equal(r.cash,5000/2,'no improvement: half of the silver cash');
// platinum upgrades any earned medal (0x1456A0): 2nd place at 150 s is still platinum
assert.equal(platinumThreshold(rules,MODE.RACE,0).ticks,9000);
assert.equal(career.medal('sam',MODE.RACE,0),MEDAL.PLATINUM,'best medal kept');
// failed final: event complete without medal, the final is repeated next time
ev=career.startEvent('sam',MODE.RACE,1);ev.round=3;
r=career.raceResult(lineup(9000,[8000,8100,8200,8300,9500]),{raceTicks:9000,origin:353496});
assert.equal(r.medal,MEDAL.NONE);assert.equal(r.cash,0);assert.equal(career.startEvent('sam',MODE.RACE,1).round,1,'a new entry is a fresh qualifier (0x238C80)');

// --- peak goals, locks and passes ------------------------------------------------------------------------
let events=career.goalEvents('sam',1,'race');
assert.deepEqual(events.map(e=>e.locked),[false,false,true,true]);assert.equal(events[2].lock,'kT_HELPLockCompRace');
assert.deepEqual(events[3].lock,{key:'kT_HELPLockCompEvent',arg:'Happiness'});
career.rider('sam').medals[eventKey(0,1)]=MEDAL.BRONZE;
events=career.goalEvents('sam',1,'race');assert.deepEqual(events.map(e=>e.locked),[false,false,false,true],'Happiness Race opens after both Peak 1 races');
assert.equal(events[2].name,'Happiness Race');
assert.equal(career.goalComplete('sam',1,'race'),false);
assert.equal(career.rider('sam').peaks[1],false);
career.rider('sam').earned=rules.earnings_goal[0];
assert.deepEqual(career.updatePeaks('sam'),[2],'Earnings goal completes -> Peak 2 pass');assert.equal(career.goalEvents('sam',2,'race')[0].locked,false);

// --- freestyle event: heat 1 -> (heat 2) -> final -------------------------------------------------------
const fs2=new Career(data,{storage:new Memory()});
ev=fs2.startEvent('zoe',MODE.HALFPIPE,11);
assert.equal(ev.ai.length,5);assert.equal(ev.ai[0].character,3,'Peak 1 rival is Mac (slot 1, leading table column)');
assert.equal(new Career(data,{storage:new Memory()}).rival(1,3),5,'Mac riding: rival becomes Griff');
assert.equal(fs2.timeLimit(ev),120*60,'The Junction time limit 120 s (0x440B38)');
for(const a of ev.ai)for(const s of a.scores)assert.equal(s%20,0);
assert.deepEqual(fs2.objectives(ev),['kT_FSOBJ2HeatsQF','kT_FSOBJHeatsGrandTtl']);
// heat 1 is ranked against the computer riders' heat 1 + heat 2 totals (0x239458); a low score goes to heat 2
r=fs2.freestyleResult(1000);assert.equal(r.place,5);assert.equal(ev.round,2);assert.deepEqual(r.shown.slice(1),ev.ai.map(a=>a.scores[0]));
const need=fs2.pointsToQualify(ev);assert.deepEqual(fs2.objectives(ev),[{key:'kT_FSOBJEarnXpts',arg:need}]);
r=fs2.freestyleResult(need-20);assert.equal(r.place,3);assert.equal(ev.round,1,'failed heat 2 returns to heat 1');assert.equal(r.message,'kT_OVRCMNSorryGetThird');
r=fs2.freestyleResult(10000000);assert.equal(r.place,0);assert.equal(ev.round,3,'top 3 after heat 1 goes straight to the final');
assert.deepEqual(fs2.objectives(ev),['kT_FSOBJFinalSolo','kT_OVRCMNPlaceTop3Medal']);
r=fs2.freestyleResult(1000);assert.equal(r.medal,MEDAL.NONE);assert.equal(ev.round,3);
r=fs2.freestyleResult(ev.ai[1].scores[2]+20);assert.equal(r.place,1);assert.equal(r.medal,MEDAL.SILVER);assert.equal(r.cash,5000);
assert.equal(fs2.rider('zoe').cash,5000);assert.equal(ev.round,1);assert.equal(ev.fresh,true);
ev=fs2.startEvent('zoe',MODE.HALFPIPE,11);assert.equal(ev.round,1,'new event after a medal');
ev.round=3;r=fs2.freestyleResult(700000);assert.equal(r.medal,MEDAL.PLATINUM,'700000 on The Junction is platinum');assert.equal(r.cash,25000);

// --- Single Event: final only, no cash ---------------------------------------------------------------
const single=new Career(data,{storage:new Memory()});
ev=single.startEvent('sam',MODE.RACE,0,false);assert.equal(ev.round,3);
r=single.raceResult(lineup(9500,[9600,9700,9800,9900,9990]),{raceTicks:9500,origin:353496});
assert.equal(r.medal,MEDAL.GOLD);assert.equal(r.cash,0);assert.equal(single.rider('sam').cash,0);
ev=single.startEvent('sam',MODE.HALFPIPE,11,false);assert.equal(ev.round,3);assert.deepEqual(ev.ai.map(a=>a.scores[2]),ev.ai.map(a=>a.scores[0]),'Single Event final reuses heat-1 posted scores');

// --- attributes and save ------------------------------------------------------------------------------
const shop=new Career(data,{storage:new Memory()});
shop.rider('sam').cash=700;
assert.equal(shop.buyAttribute('sam',0),true);assert.equal(shop.rider('sam').attributes[0],10);assert.equal(shop.rider('sam').cash,450);
assert.equal(shop.buyAttribute('sam',0),false,'second level costs 500');
assert.equal(shop.attributeCost(55),null);
// persistence round trip and storage failures
const mem=new Memory();const a=new Career(data,{storage:mem});a.rider('sam').cash=1234;a.persist();
assert.equal(new Career(data,{storage:mem}).rider('sam').cash,1234);
const broken={getItem(){throw Error('blocked');},setItem(){throw Error('quota');},removeItem(){throw Error('blocked');}};
assert.equal(loadSave(broken),null);assert.equal(writeSave({version:1},broken),false);
const b=new Career(data,{storage:broken});b.rider('sam').cash=5;assert.equal(b.persist(),false);assert.equal(b.rider('sam').cash,5);
mem.setItem('ssx3.career.v2','{not json');assert.equal(loadSave(mem),null,'a damaged v2 save without a v1 copy loads nothing');

// --- lodge rewards (RWRDPS2.DAT): price, lodge peak, ownership ----------------------------------------------
{const rw=new Career(data,{storage:new Memory()});const posters=rw.rewardItems('poster');
 assert.equal(posters.length,43);assert.deepEqual([posters[0].name,posters[0].price,posters[0].peak],['Characters of SSX',500,1]);
 assert.equal(rw.rewardItems('trading_card').length,116);assert.equal(rw.rewardItems('art').length,100);assert.equal(rw.rewardItems('toy').length,28);
 const brodi=rw.rewardItems('cheat_character')[0];assert.deepEqual([brodi.name,brodi.price,brodi.peak,brodi.character],['Brodi',20000,1,10]);
 assert.equal(rw.rewardStatus('sam','poster',0,1).state,'short');assert.equal(rw.rewardStatus('sam','poster',0,1).help,'kT_129HELPSaveCashItem');
 rw.rider('sam').cash=600;assert.equal(rw.rewardStatus('sam','poster',0,2).help,'kT_129HELPBuyInPeak1','sold only in its own peak lodge');
 assert.equal(rw.buyReward('sam','poster',0,2),false);assert.equal(rw.buyReward('sam','poster',0,1),true);assert.equal(rw.rider('sam').cash,100);
 assert.equal(rw.rewardStatus('sam','poster',0,1).state,'owned');assert.equal(rw.buyReward('sam','poster',0,1),false);}

// --- gear (BOLTPS2.DAT), awards 0x159CD0, uber tricks 0x45AEB8, songs ---------------------------------------
{const shop=JSON.parse(fs.readFileSync('public/assets/CAREER/shop.json'));
 const g=new Career(data,{storage:new Memory(),shop});
 // JS port of 0x1513B8 init = the extractor's Python port, which matched the fresh-career lodge savestate bit for bit.
 for(let ch=0;ch<10;ch++){const inv=new GearInventory(shop.gear.runtime[ch]),want=shop.gear.runtime[ch].initial_flags;
  for(const [k,v] of Object.entries(want))assert.equal(inv.flags(+k),v,`rider ${ch} item ${k} start flags`);
  for(const [k,v] of inv.f)if(v)assert.equal(want[k],v);}
 // PS2 Buy Gear frames (Zoe, Peak 1 lodge): root Head/Upper Body/Lower Body/Boards; Hats start Peacekeeper $50,000.
 const zoe=g.gear('zoe');assert.deepEqual(zoe.buyList(-1,1).map(e=>e.name),['Head','Upper Body','Lower Body','Boards']);
 const head=zoe.buyList(zoe.buyList(-1,1)[0].item,1);assert.deepEqual(head.map(e=>e.name),['Hats','Eyewear','Accessories','Special']);
 const hats=zoe.buyList(head[0].item,1);assert.deepEqual(hats.slice(0,6).map(e=>e.name),['Peacekeeper','True Hero','Low Beanies','Beanies w Roller','Tall Beanie','Nordic Beanie']);
 assert.equal(zoe.price(hats[0].item),50000);
 const boards=zoe.buyList(zoe.buyList(-1,1)[3].item,1);assert.deepEqual(boards.map(e=>[e.name,zoe.price(e.item)]),[['Stuff',1000],['Element',1000],['Stuff II',1000],['dnL',1000]]);
 g.rider('zoe').cash=1500;assert.equal(g.gearStatus('zoe',boards[0].item,2).state,'elsewhere');
 assert.equal(g.buyGear('zoe',boards[0].item,1),true);assert.equal(g.rider('zoe').cash,500);assert.equal(zoe.owned(boards[0].item),true);
 assert.equal(zoe.buyList(zoe.buyList(-1,1)[3].item,1).length,3,'bought board leaves the lodge list');
 assert.equal(g.equipGear('zoe',boards[0].item,true),true);assert.equal(zoe.equipped(boards[0].item),true);
 const before=zoe.entries.filter(e=>e.cls===zoe.by.get(boards[0].item).cls&&zoe.equipped(e.item)&&e.item!==boards[0].item);assert.equal(before.length,0,'one board per slot class');
 // awards: peak 1 goal -> 4 cards + 1 poster (+ once only); first gold in a Peak 1 event -> one 0x100-pool gear item
 const aw=g.grantAward('zoe',5);assert.deepEqual(aw.map(x=>x.category),['trading_card','trading_card','trading_card','trading_card','poster']);
 assert.deepEqual(g.grantAward('zoe',5),[],'award bits are one-shot');
 const pool=zoe.pool(0x100).length;const gift=g.grantAward('zoe',17);assert.equal(gift[0].category,'gear');assert.equal(zoe.pool(0x100).length,pool-1);
 assert.equal(g.grantAward('zoe',17).length,1,'awards 17..19 repeat for every event');
 const cheat=g.grantAward('zoe',2);assert.equal(g.rewardItems('cheat_character')[cheat[0].index].character,0x12,'all Peak 1 goals -> Jurgen');
 // collection bonus: owning all 28 toys unlocks Svelte Luther (0x13)
 for(let i=0;i<28;i++)g.grantReward('zoe','toy',i);assert.equal(g.rewardItems('cheat_character')[g.collectionBonus('zoe','toy')[0].index].character,0x13);
 // uber tricks: Zoe Mute row = Bar Hop owned/selected, SSXorcist $10,000 (PS2 lodge/27-ubertrick-setup.png)
 const mute=[0,1,2,3,4,5].map(e=>g.uberStatus('zoe',1,e));assert.deepEqual(mute.map(x=>x&&[x.entry.name,x.state]),[null,null,['Bar Hop','selected'],['SSXorcist','short'],['dnL FlipIt','short'],['Katana','short']]);
 g.rider('zoe').cash=10000;assert.equal(g.buyUber('zoe',1,3),true);assert.equal(g.rider('zoe').cash,0);assert.equal(g.selectUber('zoe',1,3),true);assert.equal(g.uberSelection('zoe')[1].name,'SSXorcist');
 // songs: six free credits, then $5,000
 assert.equal(g.songs().length,35);for(let i=0;i<6;i++)assert.equal(g.buySong('zoe',i),true);assert.equal(g.buySong('zoe',6),false);g.rider('zoe').cash=5000;assert.equal(g.buySong('zoe',6),true);assert.equal(g.rider('zoe').cash,0);
 // a completed final with a first gold grants award 17 gear
 const run=new Career(data,{storage:new Memory(),shop});run.startEvent('zoe',MODE.RACE,0).round=3;
 const fin=run.raceResult(lineup(9500,[9600,9700,9800,9900,9990]),{raceTicks:9500,origin:353496});assert.equal(fin.medal,MEDAL.GOLD);assert.equal(fin.awards[0].category,'gear');}

console.log('Career rules/tables: races, freestyle heats, medals, cash, goals, peak passes, records, attributes, save OK');
