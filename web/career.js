// Conquer the Mountain (career) and Single Event rules, ported from the original game-mode manager.
// Tables come from SLUS_207.72 via tools/export_career.py (web/public/assets/CAREER/career.json);
// the source address of every rule is given next to it and in docs/career-events.md.
//
//   GameModeMan (*0x4A2C6C, created by 0x237CF8): +0 round (1 qualifier/heat 1, 2 semi/heat 2, 3 final),
//   +0x70 next round, +0x74 round just played, +0x84 fresh event, +0x9C event complete.
//   Race handler 1: init 0x23A108, results 0x23A760. Freestyle handler 0: init 0x238E20, results 0x239230,
//   AI posted scores 0x239AA0 -> 0x1453D0. Medal/cash award 0x154EE8 (placement -> medal 0x155328,
//   platinum 0x1456A0). Peak goals / passes / event locks: goal lists 0x45AAD8, profile +0x278 lock bits.
import {loadSave,writeSave} from './career-save.js';
import {isPeakRun,peakSetup,peakResult} from './peak-run.js'; // peak challenges (docs/peak-mountain.md)
import {GearInventory,initialUber,uberEntries,SONG_FREE_CREDITS,SONG_PRICE} from './lodge.js';
import {recordRun,unlockedMonsters} from './monster-tricks.js'; // monster-trick medals (0x155420 / 0x155390)
import {buildRosterFrom,freestyleRoster,freestyleSlots,nextRosterSeed,nextWord,seededWords,presentationDraw} from './lineup.js'; // the roster generator 0x4C9548
import {bigChallengeRows} from './big-challenges.js';
import {pv} from './pv-flags.js'; // awardCascade (docs/ctm-parity.md "Awards, the PS2 way") // Big Challenge table 0x43EE10 (1544D0 counts a peak's completed challenges)

export const MODE={RACE:0,SLOPESTYLE:1,HALFPIPE:2,BIGAIR:3,RIVAL_TIME:4,RIVAL_POINTS:5};
export const PLAYER_NAME='PLAYER 1';   // 0x534FE0 player 0's name: sprintf("PLAYER %d", 1) (0x45A228), the records' name
export const MEDAL={NONE:-1,PLATINUM:0,GOLD:1,SILVER:2,BRONZE:3};
export const MEDAL_NAMES=['Platinum','Gold','Silver','Bronze'];
// Course kind (course table 0x43D950 code letters) -> standard game mode (0x43E978: 1 Slope Style, 2 Half Pipe, 3 Big Air).
const KIND_MODE={race:0,slopestyle:1,superpipe:2,bigair:3};
// Web rider ids -> CHARDB.DBL character index (0 Moby, 1 Kaori, 2 Allegra, 3 Mac, 4 Zoe, 5 Griff, 6 Elise,
// 7 Nate, 8 Psymon, 9 Viggo). Sam takes Mac's slot, as in the Sam PS2 build (tools/sam_ps2).
export const RIDER_CHARACTER={moby:0,kaori:1,allegra:2,mac:3,sam:3,zoe:4,griff:5,elise:6,nate:7,psymon:8,viggo:9};
export const ATTRIBUTES=['Acceleration','Edging','Speed','Spin','Stability','Toughness','Tricks'];
// Buy Attributes rows (0x4780B0 row -> attribute): Acceleration 1, Edging 3, Speed 0, Spin 4, Stability 6,
// Toughness 5, Tricks 2. Original byte order (profile +0xBDF.., runtime bank 0x535538): speed, accel, tricks,
// edging, spin, toughness, stability -- the progress bytes the stat getters 0x1494C0.. read.
export const ATTRIBUTE_ORIGINAL_INDEX=[1,3,0,4,6,5,2];
export function originalAttributeBytes(rows){const out=Array(7);rows.forEach((v,i)=>out[ATTRIBUTE_ORIGINAL_INDEX[i]]=v);return out;}
// Profile attribute raw values start at 5 (level 1.0) and end at 55 (11.0). The PS2 buys one raw point (+0.2) at a time (pv buyAttribs,
// buyAttributePoint); the old path (buyAttribute) bought 5 at once.
const ATTRIBUTE_START=5,ATTRIBUTE_STEP=5,ATTRIBUTE_MAX=55;

export const eventKey=(mode,course)=>`${mode}:${course??'-'}`;
const f32=Math.fround;

// Deterministic stand-in for the draws the port does not reproduce (reward picks, gear). xorshift32, state kept in the
// save. The event rosters and posted scores draw from the original roster generator instead (0x237CD8 = 0x317A08 on
// 0x4C9548, web/lineup.js; Career.rosterWords).
export function nextRandom(state){let x=state.seed>>>0||0x9E3779B9;x^=x<<13;x>>>=0;x^=x>>>17;x^=x<<5;x>>>=0;state.seed=x;return x;}

// 0x14AB68: record slot of an event (26 = none); modes 6..11 map to slots 0..5, otherwise table 0x45A2F8.
export function recordSlot(rules,mode,course){
 if(mode>=6&&mode<=11)return mode-6;
 if(course==null||course>=17)return 26;
 const row=rules.record_slots[course];
 if(row[0]===mode)return row[1];if(row[2]===mode)return row[3];return 26;
}
export const isTimed=mode=>mode===MODE.RACE||mode===MODE.RIVAL_TIME||(mode>=6&&mode<=8);

// 0x1456A0: platinum when a medal was earned and time <= T*60 ticks (race / rival time) or score >= T*100.
// 1589B0 / 158A50: the first medal of the peak's [platinum, gold, silver, bronze] row the count reaches.
export function exploreMedal(count,row){const i=row.findIndex((x)=>count>=x);return i<0?MEDAL.NONE:i;}
// 0x205098 @0x205EE0: the line under a Freeride goal count: the next medal and its threshold (none at platinum):
// kT_CMNPlatinumAt / kT_CMNGoldAt / kT_CMNSilverAt / kT_CMNBronzeAt.
export function exploreNext(count,row){
 if(count>=row[0])return null;
 return count>=row[1]?{key:'kT_CMNPlatinumAt',at:row[0]}:count>=row[2]?{key:'kT_CMNGoldAt',at:row[1]}:count>=row[3]?{key:'kT_CMNSilverAt',at:row[2]}:{key:'kT_CMNBronzeAt',at:row[3]};
}
// 0x204D40..0x204F9C, the All Mountain percentage: 20 x (golds/14 + rivals/12 + challenges/all + collectibles/all + highlights/24)
// in floats; 100 only when exactly complete, else truncated and at most 99.
export function mountainPercent({golds,rivals,challenges,challengeTotal,collected,collectTotal,highlights}){
 const f=Math.fround;
 const sum=f(f(f(f(f(f(golds)*f(1/14))+f(f(rivals)*f(1/12)))+f(f(challenges)/f(challengeTotal)))+f(f(collected)/f(collectTotal)))+f(f(highlights)*f(1/24)));
 const pct=f(sum*20);return pct===100?100:Math.min(99,Math.trunc(pct));
}
export function platinumThreshold(rules,mode,course){
 if(mode>=6)return null;
 if(mode===MODE.RIVAL_POINTS)return {score:rules.platinum[course+3][1]*100};
 const value=rules.platinum[course][1];
 return isTimed(mode)?{ticks:value*60}:{score:value*100};
}

// 0x155328: 1st gold, 2nd silver, 3rd bronze; rival challenges give gold only to the winner.
export function placementMedal(mode,rank){
 if(mode===MODE.RIVAL_TIME||mode===MODE.RIVAL_POINTS)return rank===0?MEDAL.GOLD:MEDAL.NONE;
 return rank<3?MEDAL.GOLD+rank:MEDAL.NONE;
}

// 0x122D78: finishing-time estimate for a rider still on course when the round is decided.
// average speed = (progress origin - remaining) / race ticks, at least 30 - place cm/tick.
export function estimateFinishTicks({raceTicks,origin,remaining,place},minimumSpeed=30){
 let speed=f32(f32(f32(origin)-f32(remaining))/f32(raceTicks));
 const floor=f32(minimumSpeed-place);
 if(speed<floor)speed=floor;
 return raceTicks+Math.trunc(f32(f32(remaining)/speed));
}

// 0x238BF8 / 0x238B70: ranks by time (ascending) or score (descending); ties keep slot order.
export function rankEntries(values,ascending){
 const order=values.map((v,i)=>i).sort((a,b)=>ascending?values[a]-values[b]||a-b:values[b]-values[a]||a-b);
 const rank=[];order.forEach((slot,r)=>rank[slot]=r);return rank;
}

// 0x1453D0: one computer rider's posted freestyle score. base = table score (x100) of that AI and round,
// scaled 0.9 at difficulty 0 / 1.2 at 2 (the leading AI is never scaled), +-5% jitter, rounded down to 20.
export function aiFreestyleScore(rules,course,round,aiIndex,level,random){
 const row=rules.freestyle_ai.find(r=>r.course===course&&r.round===round);if(!row)return -1;
 let scale=f32(1);if(aiIndex!==0){if(level===0)scale=f32(rules.ai_score.level0_scale);else if(level===2)scale=f32(rules.ai_score.level2_scale);}
 let s=Math.trunc(f32(f32(row.scores[aiIndex])*scale));
 const r=(random()%200)-100;
 s+=Math.trunc(f32(f32(s*r)*f32(rules.ai_score.jitter)));
 return s-(s%20);
}
export function freestyleTimeLimit(rules,course){const row=rules.freestyle_ai.find(r=>r.course===course);return row?row.time_limit*60:0;}

// 0x147D20 / 0x147E18: adaptive difficulty (profile char +0x280/+0x282 race, +0x284/+0x286 freestyle).
export function adaptLevel(state,up){
 let {level,counter}=state;
 if(up){counter++;if(counter>=2){if(level<2)level++;counter=0;}}
 else{counter--;if(counter<-1){if(level!==0)level--;counter=0;}}
 return {level,counter};
}

// Rider ranking 0x181BD0: sum of raw attributes rounded down to 5, /35, one decimal.
export function riderRanking(attributes){let sum=attributes.reduce((a,b)=>a+b,0);sum-=sum%5;return (sum/35).toFixed(1);}

function newRider(character){
 return {character,cash:0,earned:0,medals:{},best:{},events:{},attributes:ATTRIBUTES.map(()=>ATTRIBUTE_START),
  level:{race:{level:1,counter:0},freestyle:{level:1,counter:0}},peaks:[true,false,false],lodgePeak:1};
}

export class Career {
 // rosterSeed(): the roster generator's seed at a game construction 0x22EFE8 (one presentation draw, web/lineup.js
 // nextRosterSeed: draw 130 for the first load after boot); tests pass a fixed one.
 constructor(data,{storage=undefined/* web/save-store.js storage(): localStorage, or memory when it is blocked */,save=undefined,shop=null,rosterSeed=nextRosterSeed,presentation=presentationDraw}={}){
  this.data=data;this.shop=shop;this.rules=data.rules;this.courses=data.courses;this.storage=storage;this.rosterSeed=rosterSeed;this.presentation=presentation;
  this.save=save===undefined?loadSave(storage):save;
  if(!this.save)this.save={version:2,seed:0x13572468,records:null,riders:{}};
  if(!this.save.records)this.save.records=this.rules.records.map(slot=>slot.map(r=>({...r,value:r.value})));
 }
 persist(){return writeSave(this.save,this.storage);}
 // Conquer the Mountain collectibles (stage builtins 38/39): the character block's collect row of the course (C+4+12*row,
 // row = course index: count byte + 64-bit mask, bit = list index; 30C3E0 -> 153B00) and the award's cash (119EF8 ->
 // 150A90: C+0xAC4 cash, C+0xAC8 lifetime earnings). Marked at the pickup, kept through restarts and quits.
 collectMask(id,course){const m=this.rider(id).collected?.[course];return m?[m[0]>>>0,m[1]>>>0]:[0,0];}
 collectCount(id,course){const [lo,hi]=this.collectMask(id,course);let n=0;for(let v of [lo,hi])for(;v;v&=v-1)n++;return n;}
 markCollected(id,course,index,amount=0){
  if(!(index>=0&&index<64))return false;const r=this.rider(id);r.collected??={};const m=r.collected[course]??=[0,0];
  const peak=this.courses[course]?this.peakOf(course):0,before=peak?this.collectMedal(id,peak):MEDAL.NONE;
  m[index>>5]=(m[index>>5]|(1<<(index&31)))>>>0;if(amount>0)this.earnCash(id,amount);   // 30C3E0 -> 153B00, then 10F338 -> 119EF8 kind 3
  if(peak&&this.collectMedal(id,peak)!==before)this.exploreGoal(id,peak,'collect');   // 30B9A0 -> 10F338 -> 1599A0
  this.persist();return true;
 }
 // 119EF8 -> 1597B0 -> 159818: an award's cash in the world (tricks, point pickups and combos in Conquer the Mountain free ride,
 // collectibles, Big Challenges). 150A90 adds it to C+0xAC4 cash and C+0xAC8 lifetime earnings; when the earnings reach the
 // session's threshold (gp-0x1E90, 158BE0: the goal of the first peak whose earnings goal is still open) the three peaks'
 // earnings goals (157BF0 goal 3) are compared before and after and each one completed now grants its goal award 14 + peak
 // - 1 at once (159170 -> 159CD0: the cards and poster with the next peak's pass ...), without a reward list. The message
 // notices (1E3760: 247 / 248) run before, in web/career-messages.js.
 earnCash(id,amount){
  if(pv('awardCascade'))return this.earn159818(id,amount);   // the award cascade inside 159CD0 opens the passes
  const r=this.rider(id);amount|=0;if(!amount)return [];
  const open=[1,2,3].map(p=>!this.goalComplete(id,p,'earnings')),threshold=open.includes(true)?this.rules.earnings_goal[open.indexOf(true)]:0;
  const crossing=threshold&&r.earned+amount>=threshold;
  r.cash+=amount;r.earned+=amount;if(!crossing)return [];
  const got=[];[1,2,3].forEach((p,k)=>{if(open[k]&&this.goalComplete(id,p,'earnings'))got.push(...this.grantAward(id,5+3*3+p-1));});
  if(got.length)this.updatePeaks(id);
  return got;
 }
 // ---- the Freeride (EXPLORE) goal: collectible and Big Challenge medals of a peak --------------------------------------------
 // 153708: collectibles of a peak = the set bits of every course row (0..21) whose course table +0x54 is the peak;
 // 1544D0: the completed Big Challenges (status bit 3) whose course is on the peak (table 0x43EE10, web/big-challenges.js).
 // 1589B0 / 158A50: the medal is the first of [platinum, gold, silver, bronze] (0x45AFE8 / 0x45B018 row peak-1) the count
 // reaches (platinum = every one: 155 / 148 / 122 collectibles, 40 / 27 / 21 challenges).
 peakCollected(id,peak){let n=0;for(const c of this.courses)if(c.index<22&&c.field54===peak-1)n+=this.collectCount(id,c.index);return n;}
 peakCollectTotal(peak){return this.rules.collectible_medals[peak-1][0];}
 challengesDone(id,peak){
  const w=this.rider(id).bigChallenges,rows=bigChallengeRows();if(!Array.isArray(w)||!rows)return 0;
  let n=0;rows.forEach((x,i)=>{if(x.course>=0&&this.courses[x.course]?.field54===peak-1&&(w[i]&8))n++;});return n;
 }
 peakChallengeTotal(peak){return this.rules.challenge_medals[peak-1][0];}
 collectMedal(id,peak){return exploreMedal(this.peakCollected(id,peak),this.rules.collectible_medals[peak-1]);}
 challengeMedal(id,peak){return exploreMedal(this.challengesDone(id,peak),this.rules.challenge_medals[peak-1]);}
 // 1599A0 (a collectible) / 159B08 (a Big Challenge completed, 307308 -> 10F2D8): the peak's count reached its next medal
 // (the session's next threshold 0x440F48 / 0x440F58, 158C80 / 158D58). When that medal completes the Freeride goal (157BF0
 // goal 2: both medals), 159170 grants the goal award 5 + 2*3 + peak-1 at once (159CD0: cards and poster + the Peak 2 pass,
 // toy and art + the Peak 3 pass, a cheat character) and posts its message (web/career-messages.js award 11/12: the mail
 // icon). Nothing else shows in the world and the reward record stays empty (PS2 ctm-parity/runs/sd-goal: lock bit 12
 // cleared, +0xF28 bit 11, inbox item 249, the mail icon at the MISSION SUCCESS popup).
 exploreGoal(id,peak,kind=null){
  if(pv('awardCascade'))return this.exploreGoal1599A0(id,peak,kind);
  if(!this.goalComplete(id,peak,'freeride'))return [];
  const got=this.grantAward(id,11+peak-1);this.updatePeaks(id);return got;
 }
 rider(id){const s=this.save.riders;if(!s[id])s[id]=newRider(RIDER_CHARACTER[id]??3);return s[id];}
 // The reward and gear picks (0x157080 cards / posters / toys / art / cheats, 0x156C70 / 0x156EE0 gear): r = 0x3177F0() % (the
 // category's unowned count), then the r-th unowned item in index order. pv rewardRng: 0x3177F0 is the presentation generator
 // 0x4FF018 (lui 0x50, addiu -0xFE8; 0x317A08 on it), the one the career messages draw from (web/lineup.js presentationDraw,
 // the session's model of it). Off: the saved xorshift stand-in.
 random(){return pv('rewardRng')?this.presentation():nextRandom(this.save);}
 // The roster generator 0x4C9548 (0x237CD8 draws: race roster 0x23A4F0, freestyle roster 0x239938, posting 0x1453D0).
 // The game construction 0x22EFE8 seeds it (0x237CB0) with a presentation draw: a Single Event at each event load, Conquer
 // the Mountain once at the world load (PS2 menus/ctm: 0xB57109A9 = draw 130 from the free-ride start through a Snow Jam
 // race, 52 draws, and The Junction / R&B, 65 draws). seedRoster() is that world load (career-ui.js enter); the career's
 // words are kept in the save and every fresh career event draws on them.
 seedRoster(seed=this.rosterSeed()){this.save.roster=seededWords(seed);this.persist();return this.save.roster;}
 rosterWords(career){if(!career)return seededWords(this.rosterSeed());if(!Array.isArray(this.save.roster)||this.save.roster.length!==6)this.save.roster=seededWords(this.rosterSeed());return this.save.roster;}
 course(index){return this.courses[index];}
 peakOf(course){return this.courses[course].field54+1;}   // 0x144C78: course table +0x54 is the career peak.
 standardMode(course){return KIND_MODE[this.courses[course].kind];}
 eventName(mode,course){
  // CMNAMER kT_EventPk1Race / kT_EventPk2Race / kT_EventAllPeakRace (and ...Jam) name the peak events.
  if(mode>=6)return ['Peak 1 Race','Peak 2 Race','All Peak Race','Peak 1 Jam','Peak 2 Jam','All Peak Jam'][mode-6];
  const c=this.courses[course];return mode===MODE.RIVAL_TIME?`${c.name} Race`:mode===MODE.RIVAL_POINTS?`${c.name} Jam`:c.name;
 }
 // 0x145750: peak rival (Mac/Nate/Psymon; Griff/Zoe/Elise when the player rides that character).
 rival(peak,character){return [[3,5],[7,4],[8,6]][peak-1][character===[3,7,8][peak-1]?1:0];}
 medal(id,mode,course){return this.rider(id).medals[eventKey(mode,course)]??MEDAL.NONE;}

 // Peak goal lists 0x45AAD8 with the lock rules of 0x154EE8 (profile +0x278 bits 6+peak / 9+peak rival,
 // 14+peak / 17+peak peak challenge): rival opens when every standard event of the list has a medal, the peak
 // challenge when the rival also has one.
 goalEvents(id,peak,goal){
  const list=this.rules.goal_events.find(g=>g.peak===peak&&g.goal===goal)?.events||[];
  const has=e=>this.medal(id,e.mode,e.course)!==MEDAL.NONE;
  const standard=list.filter(e=>e.mode<4),rival=list.find(e=>e.mode===4||e.mode===5);
  return list.map(e=>{
   let locked=false,lock=null;
   if(e.mode===4||e.mode===5){locked=!standard.every(has);lock=goal==='race'?'kT_HELPLockCompRace':'kT_HELPLockCompFree';}
   else if(e.mode>=6){locked=!(standard.every(has)&&rival&&has(rival));lock={key:'kT_HELPLockCompEvent',arg:rival?this.courses[rival.course].name:''};}
   return {...e,name:this.eventName(e.mode,e.course),medal:this.medal(id,e.mode,e.course),locked:!this.rider(id).peaks[peak-1]||locked,lock};
  });
 }
 goalComplete(id,peak,goal){
  const r=this.rider(id);
  if(goal==='earnings')return r.earned>=this.rules.earnings_goal[peak-1];
  if(goal==='freeride')return this.collectMedal(id,peak)!==MEDAL.NONE&&this.challengeMedal(id,peak)!==MEDAL.NONE;   // 157BF0 goal 2
  return this.goalEvents(id,peak,goal).every(e=>e.medal!==MEDAL.NONE);
 }
 // Any goal of peak N opens peak N+1 (peak pass: clear +0x278 bit 12 / 13).
 updatePeaks(id){
  const r=this.rider(id),opened=[];
  for(const peak of [1,2])if(!r.peaks[peak]&&['race','freestyle','freeride','earnings'].some(g=>this.goalComplete(id,peak,g))){r.peaks[peak]=true;opened.push(peak+1);}
  return opened;
 }

 // ---- event sessions --------------------------------------------------------------------------------
 // career=true: Conquer the Mountain rounds. career=false: Single Event (round forced to the final, 0x23A174).
 // human: the gameplay character (a cheat skin's base rider, Sam = Mac's slot 3; default the rider's own).
 startEvent(id,mode,course,career=true,{human=null}={}){
  const r=this.rider(id),key=eventKey(mode,course);
  // pv freshEvent: every career entry is a fresh event (initGameMode 0x22D89C -> 0x238C80: GMM +0x84 = 1, +0x70 = +0x74 = 0); only
  // Next heat / Restart (no startEvent) carry the round
  let ev=career&&!pv('freshEvent')?r.events[key]:null;
  if(!ev||ev.fresh){
   ev={mode,course,round:career||mode===MODE.RIVAL_TIME||mode===MODE.RIVAL_POINTS?1:3,fresh:false,career};
   if(mode===MODE.RIVAL_TIME||mode===MODE.RIVAL_POINTS)ev.rival=this.rival(this.peakOf(course),r.character); // 0x145750
   if(mode>=1&&mode<=3)this.postFreestyleScores(id,ev,career,human??r.character);
   // Conquer the Mountain race: the round-1 roster 0x23A4F0 (52 draws) on the career's generator when the event is fresh
   // (+0x84); the semi and the final reuse it (web/ai-race.js prepare, web/lineup.js lineupFor entries).
   if(mode===MODE.RACE&&career){const h=human??r.character;ev.roster={human:h,entries:buildRosterFrom(this.rosterWords(true),h,this.peakOf(course)-1)};}
   if(career){r.events[key]=ev;this.persist();}
  }
  this.active={id,key,ev};
  return ev;
 }
 // 0x239AA0: five computer riders' posted heat 1 / heat 2 / final scores, generated when the event starts.
 // Slot 1 is the peak rival with the leading table column. Single Event reuses the heat-1 values for the final.
 // Roster 0x239938: GMM+0x18 player, +0x1C the peak rival (0x145750), the other characters shuffled (0x23C770) into +0x20..;
 // Single Event (0x535C11 != 0) puts the LAST shuffled character in +0x1C instead of the rival. Posting 0x239AA0 with
 // n = GMM+0x14 posted riders (0x238E20: 4 in slope style, else 5) and live = GMM+0x10 - n + 1 slots that ride:
 // slope style: slot 0 player, slot 1 the computer OPPONENT who rides the course (character +0x1C), slots 2..5 post table
 // columns 0..3 (+0x20..); pipe / big air: slots 1..5 post columns 0..4 (+0x1C, +0x20..). docs/slopestyle-bigair.md.
 // All draws come from the roster generator 0x4C9548 (Single Event: seeded at this event's load; career: the running
 // career generator), in the original order: the roster shuffle (50 draws), then one draw per posted score, rider by
 // rider, heat 1 / heat 2 / final (a Single Event draws heat 1 only and copies it, 0x535C11 != 0). Checked against the
 // R&B, Crow's Nest and Junction anchors, 29 derived R&B countdowns and the CTM R&B / Junction heat-1 savestates (all
 // three rounds of the handler table 0x57A604), web/test-slopestyle-bigair.mjs.
 // Career slope style: the round-1 path of 0x238E20 sets GMM+0x14 = 5, so NO computer opponent rides (0x535C04 = 0,
 // PS2 menus/rnbctm: 'Mac 254500' posted in heat 1, standings rows in the HUD, no OPPONENT line) and the rival owns posted
 // slot 1 with the leading table column, as in the pipe.
 postFreestyleScores(id,ev,career,me=this.rider(id).character){
  const level=this.rider(id).level.freestyle.level,peak=this.peakOf(ev.course);
  const rival=this.rival(peak,me),w=this.rosterWords(career),entries=freestyleRoster(w,me,rival);
  const {opponent,posted}=freestyleSlots(entries,{rival,single:!career,slope:ev.mode===MODE.SLOPESTYLE});
  ev.ai=posted.map((character,ai)=>({character,scores:[1,2,3].map(round=>round>1&&!career?null:aiFreestyleScore(this.rules,ev.course,round,ai,level,()=>nextWord(w)))}));
  if(!career)for(const a of ev.ai){a.scores[1]=a.scores[0];a.scores[2]=a.scores[0];}
  // slot 1 of Single Event slope style: the computer opponent who rides (web/ai-race.js prepare assembles this character);
  // scores[round] = its run score +0x198 stored when it crosses the finish (0x239230), the final's 0x122E50 estimate when
  // still riding at the decision.
  ev.opponent=opponent!=null?{character:opponent,scores:[0,0,0]}:null;
  ev.roster={human:me,entries};
  ev.player=[0,0,0];
 }
 roundName(ev){
  if(ev.mode===MODE.RIVAL_TIME||ev.mode===MODE.RIVAL_POINTS)return 'Rival Challenge';   // CMNAMER 0x0626EF15 (docs/backcountry.md)
  if(ev.mode===MODE.RACE)return ['Qualifier Round','Semi Final Round','Final Round'][ev.round-1];
  return ['Qualifier Heat 1','Qualifier Heat 2','Final Round'][ev.round-1];
 }
 // Pre-event text (race_pre 0x1FB9AC.., freestyle_pre 0x1FBF28..).
 objectives(ev){
  // Rival card (PS2 out-*-load/final.png): 'Face off against %s in a Rival Challenge!' then one bullet (web/rival-mode.js).
  if(ev.mode===MODE.RIVAL_TIME||ev.mode===MODE.RIVAL_POINTS)return [{key:0x0005ebd9,arg:this.data.characters?.[ev.rival]?.first??['Moby','Kaori','Allegra','Mac','Zoe','Griff','Elise','Nate','Psymon','Viggo'][ev.rival],headline:true},ev.mode===MODE.RIVAL_TIME?0x0177555d:0x083e70d3];
  if(ev.mode===MODE.RACE)return [['kT_CMNRaceTop3SemiFinal','kT_OVRRaceObjectives20','kT_OVRRaceObjectives15'][ev.round-1]];
  if(ev.round===1)return ['kT_FSOBJ2HeatsQF','kT_FSOBJHeatsGrandTtl'];
  if(ev.round===2){const need=this.pointsToQualify(ev);return need>0?[{key:'kT_FSOBJEarnXpts',arg:need}]:['kT_CMNHELPEarnedEnoughPoints'];}
  // Single Event freestyle card (PS2 frame single/09-rb-load-d): 'Place in the top three...' + point icons line.
  // big air / super pipe read 'Collect point icons to increase score.' (OVAMER 0x0FC8E913; PS2 nav/out-crows-load, peak3/nav/out-eba3-load,
  // out-ehp3-load); slope style 'Multipliers and point icons increase your score.'
  return ev.career?['kT_FSOBJFinalSolo','kT_OVRCMNPlaceTop3Medal']:['kT_OVRCMNPlaceTop3Medal',ev.mode===MODE.SLOPESTYLE?'kT_OVRCMNCollPointAndMulti':{key:0x0fc8e913}];
 }
 // Heat 2 target: 3rd best combined AI score minus the player's heat-1 score.
 pointsToQualify(ev){const totals=ev.ai.map(a=>a.scores[0]+a.scores[1]).sort((a,b)=>b-a);return totals[2]-ev.player[0];}
 // Posted computer-rider scores shown this round (heat 1: heat 1, heat 2: combined, final: final), best first -- the
 // freestyle_pre 'Current standings' card and the HUD standings rows. (The heat-1 RESULT ranks heat 1 + heat 2 totals.)
 postedStandings(ev){
  if(!ev.ai)return [];
  if(ev.opponent)return this.slopeStandings(ev);
  // 0x536640 at the card / in the HUD: heat 1 the heat-1 scores (PS2 CTM R&B and Junction heat-1 cards: 'Mac 254500',
  // 'Mac 162880'), heat 2 heat 1 + heat 2 (0x239118), the final the final's.
  const k=ev.round-1,value=a=>ev.round===3?a.scores[2]:a.scores[0]+(ev.round===2?a.scores[1]:0);
  return ev.ai.map(a=>({character:a.character,score:ev.career?value(a):a.scores[k]})).sort((a,b)=>b.score-a.score);
 }
 // 0x239AA0 at the player's finish: the player's run (+ heat 1 in heat 2) ranked with the round's posted values 0x536640 by 238B70
 // (ties keep slot order: the player, slot 0, first); slope style's slot 1 holds the opponent's run once it has crossed the finish
 // (opponentScore), 0 before. 0x239230 then sets rider+0x100 (the finish celebration) for a place < 3 (web/finish_gameplay.inc).
 finishPlace(ev,score,opponentScore=0){
  const mine=score+(ev.round===2?(ev.player?.[0]||0):0);
  return this.postedStandings(ev).filter(p=>(p.opponent?opponentScore:p.score)>mine).length;
 }
 // Slope style 'Current standings': the posted slots 2..5 and the opponent's slot 1 (0x536640 at the card, 0 until it rides).
 slopeStandings(ev){
  const k=ev.round-1,value=a=>ev.round===3?a.scores[2]:a.scores[0]+(ev.round===2?a.scores[1]:0);
  return [ev.opponent,...ev.ai].map(a=>({character:a.character,score:ev.career?value(a):a.scores[k]??0,opponent:a===ev.opponent})).sort((a,b)=>b.score-a.score);
 }
 // 0x122E50: final-round estimate of a computer rider still on course when the player finishes:
 // s1 = score +0x198 + 1; (s1 + cvt.w.s(max(rem - 1000, 0) * (float(s1) / max(orig - rem, 1)))), rem = +0x4D0, orig = +0x4D8 (113130).
 static opponentEstimate(score,remaining,origin){
  const f=Math.fround,s1=((score|0)+1)|0,rem=f(remaining),org=f(origin);
  let done=f(org-rem);if(done<1)done=1;let left=f(rem-1000);if(left<0)left=0;
  const v=f(left*f(f(s1)/done));return (s1+(v>=2147483648?0x7fffffff:v<=-2147483648?-0x80000000:Math.trunc(v)))|0;
 }
 timeLimit(ev){if(isPeakRun(ev.mode))return this.peakSetup(ev).limitTicks;/* 23B268 / 23C0D0: GMM+0x78, timed */return (ev.mode>=1&&ev.mode<=3)||ev.mode===MODE.RIVAL_POINTS?freestyleTimeLimit(this.rules,ev.course):0;}
 // Peak challenge setup (web/peak-run.js): the tier comes from the stored best medal of the event.
 peakSetup(ev){return peakSetup(this.rules,ev.mode,this.active?this.medal(this.active.id,ev.mode,ev.course):MEDAL.NONE);}
 // Peak challenge result (23B468 / 23C2D8): the event completes (+0x9C) either way; a failed run pays nothing and repeats.
 peakRunResult({ticks,score,dnf}){
  const {ev}=this.active,setup=this.peakSetup(ev),res=peakResult(this.rules,setup,{ticks,score,dnf});
  const out={round:1,place:res.place,fail:res.fail,value:res.value,setup,...this.completeEvent(res.place,{ticks:res.value,score:res.value})};
  ev.round=1;this.persist();return out;
 } // Rival Points: 23BB98 +0x78 = 1454F8 x 60 (Happiness 300 s)

 // Race round result (0x23A760). standings: [{name, character, human, finishTicks|null, remaining, place, dnf}];
 // unfinished riders get the 0x122D78 estimate at the human's finish; DNF riders 360000 ticks.
 raceResult(standings,{raceTicks,origin}){
  const {id,ev}=this.active,rules=this.rules;
  const times=standings.map(s=>s.dnf?rules.race.dnf_ticks:s.finishTicks!=null?s.finishTicks:estimateFinishTicks({raceTicks,origin:s.origin??origin,remaining:s.remaining,place:s.place??0},rules.race.estimate_min_speed));
  const rank=rankEntries(times,true),human=standings.findIndex(s=>s.human),place=rank[human];
  const round=ev.round,out={round,times,rank,place,rows:standings.map((s,i)=>({...s,time:times[i],rank:rank[i]}))};
  const r=this.rider(id);let lvl=r.level.race;
  if(place<3){
   out.advance=round<3;
   // 0x23AA24: a win moves the level up only by the margin between result slots 1 and 0 (human = slot 0).
   if(ev.career&&place===0&&times.length>1&&times[1]-times[0]>=rules.race.level_up_margin[round-1])lvl=adaptLevel(lvl,true);
   out.message=round===1?'kT_OVRCongratGoToSemi':round===2?'kT_OVRRaceObjectives7':null;
   if(round<3)ev.round=round+1;
  }else{
   out.advance=false;
   if(ev.career&&(round===1||(round===2&&place>=4)||(round===3&&place===5)))lvl=adaptLevel(lvl,false);
   out.message=round===1?'kT_OVRCMNSorryGetThirdSemi':round===2?'kT_OVRCMNSorryGetThird':null;
  }
  r.level.race=lvl;
  out.record=standings[human]?.dnf?-1:this.heatRecord(times[human]);
  if(round===3)Object.assign(out,this.completeEvent(place,{ticks:times[human]}));
  this.persist();return out;
 }
 // Freestyle round result (0x239230). score: the player's run (0 with dnf at the time limit, 0x125260).
 // opponent (slope style slot 1, from the live computer rider): {finished, score (+0x198 at its finish), estimate (0x122E50)}.
 freestyleResult(score,opponent=null){
  const {id,ev}=this.active,round=ev.round,r=this.rider(id);
  ev.player[round-1]=score;
  if(ev.opponent&&opponent){if(opponent.finished)ev.opponent.scores[round-1]=opponent.score|0;else if(round===3)ev.opponent.scores[2]=opponent.estimate|0;}
  const field=ev.opponent?[ev.opponent,...ev.ai]:ev.ai;
  let totals,shown;
  if(round===1){totals=[score+0,...field.map(a=>a.scores[0]+a.scores[1])];shown=[score,...field.map(a=>a.scores[0])];}
  else if(round===2){totals=[ev.player[0]+score,...field.map(a=>a.scores[0]+a.scores[1])];shown=[score,...field.map(a=>a.scores[1])];}
  else{totals=[score,...field.map(a=>a.scores[2])];shown=totals;}
  const rank=rankEntries(totals,false),place=rank[0];
  const out={round,totals,shown,rank,place,record:this.heatRecord(score),rows:[{human:true,character:r.character},...field.map(a=>({human:false,character:a.character,opponent:a===ev.opponent}))].map((row,i)=>({...row,score:shown[i],total:totals[i],rank:rank[i]}))};
  // OVAMER 0x0812F644 "Congratulations!  You've made it to the final round."
  if(round===1){out.advance=true;ev.round=place<3?3:2;out.message=place<3?0x0812f644:null;}
  else if(round===2){
   if(place<3){ev.round=3;out.advance=true;out.message=0x0812f644;if(ev.career&&place===0)r.level.freestyle=adaptLevel(r.level.freestyle,true);}
   else{ev.round=1;out.advance=false;out.message='kT_OVRCMNSorryGetThird';}
  }else{
   if(ev.career)r.level.freestyle=adaptLevel(r.level.freestyle,place<3);
   Object.assign(out,this.completeEvent(place,{score}));
  }
  this.persist();return out;
 }
 // Rival round result (Rival Time 0x23B8C8 / Rival Points 0x23BDB8, web/rival-mode.js rivalResult): the event is complete
 // (+0x9C) whatever the place; the winner gets gold (0x155328), a loss pays nothing and the challenge stays open.
 rivalResult(outcome,{ticks,score}){
  const {ev}=this.active,place=outcome.win?0:1;
  const out={round:1,place,win:outcome.win,values:outcome.values,rank:outcome.place,...this.completeEvent(place,{ticks,score})};
  if(ev.career){ev.fresh=outcome.win;ev.round=1;}   // GMM +0x84 = 1 only on a win (+0x70 = 0); a loss replays the round
  out.message=null;   // Single Event loss: the default 'Sorry, you didn't win.' (PS2 nav/bc/out-tuck-finish2/sample00600.png)
  this.persist();return out;
 }
 // Pause/results "Restart" (0x238348): the round just played (+0x74) is played again.
 restartRound(round){const ev=this.active?.ev;if(ev){ev.round=round;ev.fresh=false;}}
 // Event complete (+0x9C): medal/cash/records (0x154EE8), then goals and peak passes.
 completeEvent(place,{ticks,score}){
  const {id,ev,key}=this.active,r=this.rider(id),rules=this.rules;
  let medal=placementMedal(ev.mode,place);
  if(medal!==MEDAL.NONE&&ev.career){const t=platinumThreshold(rules,ev.mode,ev.course);
   if(t&&(t.ticks!=null?ticks<=t.ticks:score>=t.score))medal=MEDAL.PLATINUM;}
  const out={medal,cash:0,opened:[]};
  if(ev.mode>=MODE.RIVAL_TIME)out.record=this.heatRecord(isTimed(ev.mode)?ticks:score);   // rival / peak runs: their one round
  if(ev.career&&pv('awardCascade')){Object.assign(out,this.complete1591E8(medal,{ticks,score}));ev.fresh=place<3;ev.round=place<3?1:3;return out;}
  if(ev.career){
   const previous=r.medals[key]??MEDAL.NONE;
   if(medal!==MEDAL.NONE){
    const full=ev.mode<6?rules.cash[ev.course][medal]:rules.peak_challenges[(ev.mode-6)*3+(medal<=MEDAL.GOLD?2:medal===MEDAL.SILVER?1:0)][9]*100;
    out.cash=previous===MEDAL.NONE||medal<previous?full:full>>>1;
    r.cash+=out.cash;r.earned+=out.cash;
    if(previous===MEDAL.NONE||medal<previous)r.medals[key]=medal;
   }
   // 0x1591E8: the first gold/platinum of an event (slot gold count +2 still 0) grants award 17+peak (standard
   // events), 20+peak rival race, 23+peak rival jam, 26+i peak race, 29+i peak jam (peak events force the peak).
   r.golds??={};
   if(medal!==MEDAL.NONE&&medal<=MEDAL.GOLD){
    if(!r.golds[key]){const peak=ev.mode>=6?(ev.mode-6)%3:this.peakOf(ev.course)-1;out.awards=[...(out.awards||[]),...this.grantAward(id,(ev.mode<4?17:ev.mode===4?20:ev.mode===5?23:ev.mode<=8?26:29)+peak)];}
    r.golds[key]=(r.golds[key]||0)+1;
   }
   const best=r.best[key];const value=isTimed(ev.mode)?ticks:score;
   if(best==null||(isTimed(ev.mode)?value<best:value>best))r.best[key]=value;
   // A passed final starts the next attempt from the qualifier (+0x84); a failed one repeats the final.
   ev.fresh=place<3;ev.round=place<3?1:3;
   out.opened=this.updatePeaks(id);out.awards=[...(out.awards||[]),...this.checkAwards(id)];
  }
  return out;
 }
 // Top-5 records (defaults 0x43FB28, 26 slots x 5, copied to 0x535C18). Race records keep ticks here;
 // the original defaults are whole seconds.
 // 0x155420 per character: personal bests of the run statistics (web/monster-tricks.js) and the eight monster-trick medals.
 // Conquer the Mountain only (0x5305F9 == 0), not free ride; finish = {score, seconds, mode} for a finished run.
 monster(id){const r=this.rider(id);return r.monster??={bests:Array(10).fill(null),medals:Array(8).fill(0),bestCombo:0};}
 recordRunStats(id,stats,finish=null){
  const f=finish?{score:finish.score|0,seconds:Math.trunc((finish.ticks??0)/60),timeEvent:finish.mode===MODE.RACE,scoreEvent:[MODE.SLOPESTYLE,MODE.HALFPIPE,MODE.BIGAIR,MODE.RIVAL_POINTS].includes(finish.mode)}:null;
  const raised=recordRun(this.monster(id),stats,f);this.persist();return raised;}
 monsterUnlocked(id){return unlockedMonsters(this.monster(id).medals);}
 records(slot,timed){return this.save.records[slot].map(r=>({...r,ticks:timed&&r.ticks==null?r.value*60:r.ticks,...(r.player&&r.name==='YOU'?{name:PLAYER_NAME}:{})}));}   // saves before 2026-09-26 named the player 'YOU'
 // A finished heat's time / score into the event's top 5 (the result's record rank, which 20A8F8 reads at world state 7: a
 // new record opens "Top 5 Record Times" first). Every heat of a standard event, not only the final (PS2 ctm-left/runs:
 // the Snow Jam qualifier at 03:45 and the final at 03:55 with the records poked slower both show it before the results).
 heatRecord(value){
  const {ev,id}=this.active||{};if(!ev)return -1;const slot=recordSlot(this.rules,ev.mode,ev.course);if(slot>=26)return -1;
  return this.addRecord(slot,value,this.rider(id).character,isTimed(ev.mode));
 }
 addRecord(slot,value,character,timed){
  // the entry's name is the player's (0x534FE0, "PLAYER %d" 0x45A228 with player 1: "PLAYER 1" on the PS2 records screen): 0x154DDC
  // copies 0x147170, the Player Name once one was entered (pv playerName: set by web/fe-screens.js; PS2 lodge/runs/name-records: OWEN)
  // 0x154CD8..0x154D0C: a time is entered in whole seconds, cvt.w.s(f32(ticks) x f32 1/60 (gp-0x6DEC = 0x3C888889)); 0x154D58: the run
  // goes in at the first entry it is not strictly worse than (time: skip while entry < run; score: skip while run < entry), so a tie
  // ranks the new run above the old one (docs/online-records.md). Before 2026-09-30 the port compared ticks with a strict test.
  const secs=timed?Math.trunc(f32(f32(value)*f32(0.016666668))):value;
  const list=this.records(slot,timed),entry={value:secs,ticks:timed?value:undefined,character,name:this.playerName||PLAYER_NAME,player:true};
  const at=list.findIndex(r=>timed?!(r.value<secs):!(value<r.value));
  if(at<0)return -1;
  list.splice(at,0,entry);list.length=5;this.save.records[slot]=list;return at;
 }
 topRecord(mode,course){const slot=recordSlot(this.rules,mode,course);if(slot>=26)return null;const list=this.records(slot,isTimed(mode));return list[0];}

 // ---- goal / career awards 0x159CD0 (award bits profile +0xF28; ids 17..19 may repeat) -------------------
 // 0-4 cheat characters (conquer / all goals / all goals of peak 1..3), 5..16 peak goals 5+goal*3+peak:
 // peak 1 -> 4 random cards + 1 poster (+ Peak 2 pass), peak 2 -> 1 toy + 2 art (+ Peak 3 pass), peak 3 -> a cheat
 // character; 17..19 random gear of the peak pool (flag 0x100<<peak), 20..31 random special board (flag 0x800).
 grantAward(id,award){
  if(pv('awardCascade'))return this.grant159CD0(id,award);
  const r=this.rider(id);r.awards??=[];
  if(!(award>=17&&award<=19)&&r.awards.includes(award))return [];
  const got=[],cheat=cid=>{const i=this.rewardItems('cheat_character').findIndex(x=>x.character===cid);if(i>=0&&!this.owned(id,'cheat_character').includes(i)){this.grantReward(id,'cheat_character',i);got.push({category:'cheat_character',index:i});}};
  const random=(category,count)=>{for(let k=0;k<count;k++){const i=this.randomUnowned(id,category);if(i<0)break;this.grantReward(id,category,i);got.push({category,index:i});}};
  const CHEAT={0:0x1D,1:0x18,2:0x12,3:0x19,4:0x1A,7:0x1B,10:0x17,13:0x1C,16:0x15};
  if(CHEAT[award]!==undefined)cheat(CHEAT[award]);
  else if(award>=5&&award<=16){const peak=(award-5)%3;if(peak===0){random('trading_card',4);random('poster',1);}else if(peak===1){random('toy',1);random('art',2);}}
  else if(award>=17&&award<=31){const inv=this.gear(id);if(inv){const pool=inv.pool(award<=19?0x100<<(award-17):0x800);if(pool.length){const e=pool[this.random()%pool.length];inv.buy(e.item);this.saveGear(id,inv);got.push({category:'gear',item:e.item,name:e.name});}}}
  if(!r.awards.includes(award))r.awards.push(award);
  for(const g of got)if(g.category!=='gear'&&g.category!=='cheat_character')got.push(...this.collectionBonus(id,g.category));
  // the award id each item came with: the reward list (OV.LUI 62reward_list, 0x1FF7B8) prints it under the award's title (0x441C68)
  for(const g of got)g.award=award;
  if(!got.length&&award>=5&&award<=16)got.push({award,category:null});   // a goal award without items still has its title line
  return got;
 }
 // 0x157080: uniform pick among every unowned item of the category (peak / price are not filtered).
 randomUnowned(id,category){
  const all=this.rewardItems(category).length,owned=this.owned(id,category);if(owned.length>=all)return -1;
  let n=this.random()%(all-owned.length);for(let i=0;i<all;i++)if(!owned.includes(i)&&n--===0)return i;return -1;
 }
 // Collection bonuses 0x1581E0 / 0x158430 / 0x1580B8 / 0x158308: owning a whole category unlocks a cheat character.
 collectionBonus(id,category){
  const cid={trading_card:0x11,toy:0x13,poster:0x14,art:0x16}[category];if(cid===undefined)return [];
  if(this.owned(id,category).length<this.rewardItems(category).length)return [];
  const i=this.rewardItems('cheat_character').findIndex(x=>x.character===cid);
  if(i<0||this.owned(id,'cheat_character').includes(i))return [];
  this.grantReward(id,'cheat_character',i);return [{category:'cheat_character',index:i}];
 }
 // Cascade after results (0x15A0A0..): peak goals, then all goals of a peak / of the mountain.
 checkAwards(id){
  const got=[],G=['race','freestyle','freeride','earnings'];
  for(let peak=1;peak<=3;peak++)G.forEach((g,goal)=>{if(this.goalComplete(id,peak,g))got.push(...this.grantAward(id,5+goal*3+peak-1));});
  for(let peak=1;peak<=3;peak++)if(G.every(g=>this.goalComplete(id,peak,g)))got.push(...this.grantAward(id,1+peak));
  if([1,2,3].every(p=>G.every(g=>this.goalComplete(id,p,g))))got.push(...this.grantAward(id,1));
  return got;
 }

 // ---- awards the PS2 way (pv awardCascade; docs/ctm-parity.md "Awards, the PS2 way") -----------------------------------------------
 // The reward record 0x4C3EF0 (RAM, not in the save). 15A628(record, award, category, item): categories 0 trophy, 1 medal, 2 pass,
 // 3 poster, 4 trading card, 5 art, 6 video, 7 toy (a count per award, +6 + award*10 + category, and the item's bit in the category's
 // set, 15A5B0), 8 the cheat character id of the award (+0xE + award*10), 9 a gear flag per award with ONE gear item for the record
 // (+0xF + award*10 = 1, +4 = the item). 158E30 clears it (and the interface's cash +0x10 = 0, medal +0x14 = -1) at world state 10's
 // enter (0x2355C0: a location reached) and when the reward list closes (1FF700); 1591E8 sets the event's cash and medal.
 // PS2 ctm-parity/runs/sd-goal end.p2s: the Freeride goal award 11 stays in the record after the free-ride grant.
 clearRewardRecord(){this.rewardRecord={cash:0,medal:MEDAL.NONE,counts:new Map(),items:Array.from({length:10},()=>new Set()),cheats:new Map(),gear:new Set(),gearItem:null};return this.rewardRecord;}
 recordReward(award,category,item){
  const r=this.rewardRecord??this.clearRewardRecord();
  if(category===8){r.cheats.set(award,item);return;}
  if(category===9){r.gear.add(award);r.gearItem=item;return;}
  if(!(category>=0&&category<8))return;
  r.items[category].add(item);const c=r.counts.get(award)??Array(8).fill(0);c[category]++;r.counts.set(award,c);
 }
 // 158F30 -> 15A6F0: anything recorded (the results then open the reward list first, 20A8F8).
 rewardRecordCount(){const r=this.rewardRecord;if(!r)return 0;let n=r.cheats.size+r.gear.size;for(const c of r.counts.values())for(const k of c)n+=k;return n;}
 // 158F60(award, 1 | 2): arg 2 clears the Peak 3 pass bit (+0x278 bit 13) if set, recording pass item char*3+2 under the award;
 // then (arg 1 or 2) the Peak 2 pass bit 12, item char*3+1.
 openPass158F60(id,award,arg){
  const r=this.rider(id),c=r.character??RIDER_CHARACTER[id]??3;
  if(arg===2&&!r.peaks[2]){r.peaks[2]=true;this.recordReward(award,2,c*3+2);}
  if((arg===1||arg===2)&&!r.peaks[1]){r.peaks[1]=true;this.recordReward(award,2,c*3+1);}
 }
 // 157920 (per peak): gold or platinum on every standard event of the race and freestyle lists (0x45AAD8), any medal on the
 // rival and peak events (modes 4..11), PLATINUM on both Freeride medals (1589B0 / 158A50 return 0); earnings is not checked.
 peakConquered157920(id,peak){
  for(const goal of ['race','freestyle'])for(const e of this.goalEvents(id,peak,goal)){const m=e.medal;if(e.mode>=4?m===MEDAL.NONE:!(m!==MEDAL.NONE&&m<=MEDAL.GOLD))return false;}
  return this.collectMedal(id,peak)===MEDAL.PLATINUM&&this.challengeMedal(id,peak)===MEDAL.PLATINUM;
 }
 // 1577E0 (award 0, "Mountain conquered!"): 157920 on all three peaks and the eight highlight levels (+0xBB8,
 // getCurrentHighlightLevel) summing to 24 or more.
 mountainConquered(id){return [1,2,3].every(p=>this.peakConquered157920(id,p))&&this.monster(id).medals.reduce((a,b)=>a+(b|0),0)>=24;}
 // 157A78 (awards 2..4): every goal of the peak (157BF0 goals 0..3); 1578A0 (award 1): 157A78 on all three peaks.
 peakAllGoals(id,peak){return ['race','freestyle','freeride','earnings'].every(g=>this.goalComplete(id,peak,g));}
 granted(id,award){return !!this.rider(id).awards?.includes(award);}
 // 159CD0: an award's grant. 15A2E0 (+0xF28 bit) makes every award but 17..19 one-shot (a granted one returns at once, without the
 // cascade). By award (table 0x45AA40): 0..4 a cheat character (0x1D, 0x18, 0x12, 0x19, 0x1A) with its message; the Peak 1 goals
 // (5, 8, 11, 14) four trading cards (157080 -> 1580F8, category 4) and a poster (157FD0, category 3), then 158F60(award, 1);
 // the Peak 2 goals (6, 9, 12, 15) a toy (158348, category 7) and two art (158220, category 5), then 158F60(award, 2); the Peak 3
 // goals (7, 10, 13, 16) a cheat character (0x1B, 0x17, 0x1C, 0x15); 17..19 a gear item of the peak's pool (156C70), 20..31 a
 // special board (156EE0). Each item is recorded (15A628); a completed category's cheat character (1580F8 / 157FD0 / 158220 /
 // 158348 -> 158618) is granted without a record. Then the award's bit (15A358), the cheat (record 8 + 158618), the gear (record
 // 9 + 14B560), and the cascade (0x15A130): award 0 (1577E0), award 1 (1578A0), awards 2..4 (157A78 per peak), then every complete
 // goal of every peak (157BF0 -> 159170), each through this.grantAward (the message hooks see every grant).
 grant159CD0(id,award){
  const r=this.rider(id);r.awards??=[];
  if(!(award>=17&&award<=19)&&r.awards.includes(award))return [];
  const got=[],CAT={trading_card:4,poster:3,toy:7,art:5};
  const pick=(category,count)=>{for(let k=0;k<count;k++){const i=this.randomUnowned(id,category);if(i<0)break;this.grantReward(id,category,i);this.collectionBonus(id,category);this.recordReward(award,CAT[category],i);got.push({category,index:i,award});}};
  const CHEAT={0:0x1D,1:0x18,2:0x12,3:0x19,4:0x1A,7:0x1B,10:0x17,13:0x1C,16:0x15};
  let cheatId=null,gearItem=null;
  if(CHEAT[award]!==undefined)cheatId=CHEAT[award];
  else if(award>=5&&award<=16){const peak=(award-5)%3;
   if(peak===0){pick('trading_card',4);pick('poster',1);this.openPass158F60(id,award,1);}
   else if(peak===1){pick('toy',1);pick('art',2);this.openPass158F60(id,award,2);}}
  else if(award>=17&&award<=31){const inv=this.gear(id);if(inv){const pool=inv.pool(award<=19?0x100<<(award-17):0x800);if(pool.length)gearItem=pool[this.random()%pool.length];}}
  if(!r.awards.includes(award))r.awards.push(award);   // 15A358
  if(cheatId!=null){this.recordReward(award,8,cheatId);const i=this.rewardItems('cheat_character').findIndex(x=>x.character===cheatId);if(i>=0&&!this.owned(id,'cheat_character').includes(i)){this.grantReward(id,'cheat_character',i);got.push({category:'cheat_character',index:i,award});}}
  if(gearItem){this.recordReward(award,9,gearItem.item);const inv=this.gear(id);inv.buy(gearItem.item);this.saveGear(id,inv);got.push({category:'gear',item:gearItem.item,name:gearItem.name,award});}
  if(!got.length&&award>=5&&award<=16)got.push({award,category:null});
  if(this.mountainConquered(id)&&!this.granted(id,0))got.push(...this.grantAward(id,0));
  if([1,2,3].every(p=>this.peakAllGoals(id,p))&&!this.granted(id,1))got.push(...this.grantAward(id,1));
  for(let p=1;p<=3;p++)if(this.peakAllGoals(id,p)&&!this.granted(id,1+p))got.push(...this.grantAward(id,1+p));
  const G=['race','freestyle','freeride','earnings'];
  for(let p=1;p<=3;p++)G.forEach((g,goal)=>{const a=5+goal*3+p-1;if(this.goalComplete(id,p,g)&&!this.granted(id,a))got.push(...this.grantAward(id,a));});
  return got;
 }
 // 159818 (an award's cash: 1591E8's event cash, 1597B0's world cash): with the session threshold (158BE0: the earnings goal of the
 // first peak still open) reached, the three earnings goals before, 150A90 adds the cash, a new threshold, and each earnings goal
 // completed now -> 159170 (award 14 + peak - 1, through the cascade); else 150A90 alone.
 earn159818(id,amount){
  const r=this.rider(id);amount|=0;if(!amount)return [];
  const open=[1,2,3].map(p=>!this.goalComplete(id,p,'earnings')),threshold=open.includes(true)?this.rules.earnings_goal[open.indexOf(true)]:0;
  r.cash+=amount;r.earned+=amount;
  if(!threshold||r.earned<threshold)return [];
  const got=[];[1,2,3].forEach((p,k)=>{if(open[k]&&this.goalComplete(id,p,'earnings')&&!this.granted(id,14+p-1))got.push(...this.grantAward(id,14+p-1));});
  return got;
 }
 // 1599A0 (a collectible) / 159B08 (a Big Challenge completed): the peak's count reached its next medal (the caller). When the
 // Freeride goal (157BF0 goal 2) is complete and the count is the first medal's (0x45AFE8 / 0x45B018 row +0xC: bronze), 159170
 // grants award 11 + peak - 1; then award 0 (1577E0) if not granted.
 exploreGoal1599A0(id,peak,kind){
  let got=[];
  if(this.goalComplete(id,peak,'freeride')){
   const count=kind==='challenge'?this.challengesDone(id,peak):this.peakCollected(id,peak),row=kind==='challenge'?this.rules.challenge_medals[peak-1]:this.rules.collectible_medals[peak-1];
   if((kind==null||count===row[3])&&!this.granted(id,11+peak-1))got=this.grantAward(id,11+peak-1);
  }
  if(this.mountainConquered(id)&&!this.granted(id,0))got.push(...this.grantAward(id,0));
  return got;
 }
 // 1591E8 (from 154EE8 with a medal; career only): the event's cash 151040 (0x4405A0 / 0x440D18), the first gold of the event
 // (score stats +2 still 0) -> 159CD0(17 + peak | 20 | 23 | 26 | 29 ...) before anything is stored; a better medal (isBetterMedal):
 // the event goal's state, 159818(cash), 152528 stores the medal, the goal newly complete -> 159170; else 152528 and 159818(cash / 2).
 // Then award 0 (1577E0), and the interface's cash (+0x10) and medal (+0x14) for the reward list.
 complete1591E8(medal,{ticks,score}){
  const {id,ev,key}=this.active,r=this.rider(id),rules=this.rules,out={medal,cash:0,opened:[]};
  if(medal===MEDAL.NONE)return out;   // 154EE8 returns before 1591E8
  const before=[r.peaks[1],r.peaks[2]];
  const previous=r.medals[key]??MEDAL.NONE,peak=ev.mode>=6?(ev.mode-6)%3+1:this.peakOf(ev.course),goal=[0,4,6,7,8].includes(ev.mode)?'race':'freestyle';
  const full=ev.mode<6?rules.cash[ev.course][medal]:rules.peak_challenges[(ev.mode-6)*3+(medal<=MEDAL.GOLD?2:medal===MEDAL.SILVER?1:0)][9]*100;
  r.golds??={};const store=()=>{if(previous===MEDAL.NONE||medal<previous)r.medals[key]=medal;if(medal<=MEDAL.GOLD)r.golds[key]=(r.golds[key]||0)+1;
   const best=r.best[key],value=isTimed(ev.mode)?ticks:score;if(best==null||(isTimed(ev.mode)?value<best:value>best))r.best[key]=value;};
  const awards=[];
  if(medal<=MEDAL.GOLD&&!r.golds[key])awards.push(...this.grantAward(id,(ev.mode<4?17:ev.mode===4?20:ev.mode===5?23:ev.mode<=8?26:29)+peak-1));
  let cash=full;
  if(previous===MEDAL.NONE||medal<previous){
   const was=this.goalComplete(id,peak,goal);awards.push(...this.earn159818(id,cash));store();
   const a=5+(goal==='race'?0:1)*3+peak-1;if(!was&&this.goalComplete(id,peak,goal)&&!this.granted(id,a))awards.push(...this.grantAward(id,a));
  }else{store();cash=full>>>1;awards.push(...this.earn159818(id,cash));}
  if(this.mountainConquered(id)&&!this.granted(id,0))awards.push(...this.grantAward(id,0));
  const rec=this.rewardRecord??this.clearRewardRecord();rec.cash=cash;rec.medal=medal;
  out.cash=cash;out.awards=awards;out.opened=[1,2].filter(k=>!before[k-1]&&r.peaks[k]).map(k=>k+1);
  return out;
 }

 // ---- lodge: gear (BOLTPS2.DAT), uber tricks, songs ---------------------------------------------------------
 gear(id){
  if(!this.shop)return null;const r=this.rider(id);
  if(!this._gear||this._gearId!==id){this._gear=new GearInventory(this.shop.gear.runtime[r.character],r.gearFlags||null);this._gearId=id;if(!r.gearFlags)r.gearFlags=this._gear.serialize();}
  return this._gear;
 }
 saveGear(id,inv){this.rider(id).gearFlags=inv.serialize();}
 gearStatus(id,item,lodgePeak){
  const inv=this.gear(id),e=inv.by.get(item);if(!e)return null;const price=inv.price(item);
  if(inv.owned(item))return {entry:e,price,state:'owned'};
  if(e.tier!==lodgePeak)return {entry:e,price,state:'elsewhere',help:e.tier>=1&&e.tier<=3?`kT_129HELPBuyInPeak${e.tier}`:''};
  return {entry:e,price,state:this.rider(id).cash>=price?'buy':'short',help:this.rider(id).cash>=price?'kT_129HELPBuyItem':'kT_129HELPSaveCashItem'};
 }
 buyGear(id,item,lodgePeak){
  const st=this.gearStatus(id,item,lodgePeak);if(!st||st.state!=='buy')return false;
  const inv=this.gear(id);this.rider(id).cash-=st.price;inv.buy(item);this.saveGear(id,inv);this.persist();return true;
 }
 equipGear(id,item,on){const inv=this.gear(id);if(!inv||!inv.owned(item))return false;inv.equip(item,on);this.saveGear(id,inv);this.persist();return true;}
 uber(id){if(!this.shop)return null;const r=this.rider(id);return r.uber??=initialUber(this.shop,r.character);}
 uberStatus(id,category,entry){
  const u=this.uber(id)?.[category],e=uberEntries(this.shop,category)[entry];if(!u||!e||e.hidden||!(u.visible>>entry&1))return null;
  if(!(u.lock>>entry&1))return {entry:e,state:u.selected===entry?'selected':'owned'};
  return {entry:e,state:this.rider(id).cash>=e.price?'buy':'short'};
 }
 buyUber(id,category,entry){const st=this.uberStatus(id,category,entry);if(!st||st.state!=='buy')return false;this.rider(id).cash-=st.entry.price;this.uber(id)[category].lock&=~(1<<entry);this.persist();return true;}
 selectUber(id,category,entry){const st=this.uberStatus(id,category,entry);if(!st||st.state==='buy'||st.state==='short')return false;this.uber(id)[category].selected=entry;this.persist();return true;}
 // Selected uber trick ids per category (trick ids A/B of the chosen list entry) for the trick system.
 uberSelection(id){const u=this.uber(id);if(!u)return {};const out={};for(const [cat,st] of Object.entries(u)){const e=uberEntries(this.shop,+cat)[st.selected];if(e)out[cat]={entry:st.selected,name:e.name,trickIds:e.trick_ids,base:uberEntries(this.shop,+cat)[st.base]?.trick_ids};}return out;}
 songs(){return this.shop?.songs.songs||[];}
 songState(id){const r=this.rider(id);return r.songs??={owned:[],playlist:[]};}
 buySong(id,index){
  const st=this.songState(id);if(st.owned.includes(index)||!this.songs()[index])return false;
  const pay=st.owned.length>=SONG_FREE_CREDITS;if(pay&&this.rider(id).cash<SONG_PRICE)return false;
  if(pay)this.rider(id).cash-=SONG_PRICE;st.owned.push(index);st.playlist.push(index);this.persist();return true;
 }

 // ---- lodge: Rewards (DATA/BE/RWRDPS2.DAT catalog; owned bits profile +0xF30 posters, +0xF36 cards, +0xF45 art,
 // +0xF52 videos, +0xF53 toys, +0xF57 cheat characters). Items are sold for price only in their own peak lodge.
 owned(id,category){const r=this.rider(id);r.rewards??={};return r.rewards[category]??=[];}
 rewardItems(category){return this.data.rewards?.[category]?.items||[];}
 rewardStatus(id,category,index,lodgePeak){
  const item=this.rewardItems(category)[index];if(!item)return null;
  if(this.owned(id,category).includes(index))return {item,state:'owned',help:category==='cheat_character'?'kT_129HELPSelectCheatChar':'kT_129HELPViewItem'};
  if(!item.price||!item.peak)return {item,state:'locked',help:item.help||'kT_129HELPBuyInCTM'};
  if(item.peak!==lodgePeak)return {item,state:'elsewhere',help:`kT_129HELPBuyInPeak${item.peak}`};
  return {item,state:this.rider(id).cash>=item.price?'buy':'short',help:this.rider(id).cash>=item.price?'kT_129HELPBuyItem':'kT_129HELPSaveCashItem'};
 }
 buyReward(id,category,index,lodgePeak){
  const st=this.rewardStatus(id,category,index,lodgePeak);if(!st||st.state!=='buy')return false;
  this.rider(id).cash-=st.item.price;this.grantReward(id,category,index);this.lastBonus=this.collectionBonus(id,category);this.persist();return true;
 }
 grantReward(id,category,index){const list=this.owned(id,category);if(!list.includes(index)){list.push(index);list.sort((a,b)=>a-b);}}

 // ---- lodge: Buy Attributes (0x440550 costs by current level) ----------------------------------------
 attributeCost(value){return value>=ATTRIBUTE_MAX?null:this.rules.attribute_cost[Math.trunc(value/ATTRIBUTE_STEP)-1];}
 // pv buyAttribs (web/buy-attribs.js, docs/career-events.md "Buy Attributes"): 0x150C20 buys ONE raw point (+0.2 level) at
 // 0x150E50 = 0x440550[byte/5 - 1]; it fails when the cash is short or the byte is 55. The screen persists once per popup Yes.
 attributePointCost(value){const level=Math.trunc(value/5);return level>=1&&level<=this.rules.attribute_cost.length?this.rules.attribute_cost[level-1]:null;}
 buyAttributePoint(id,index){
  const r=this.rider(id),v=r.attributes[index],cost=this.attributePointCost(v);
  if(cost==null||r.cash<cost||v>=ATTRIBUTE_MAX)return false;
  r.cash-=cost;r.attributes[index]=v+1;return true;
 }
 buyAttribute(id,index){
  const r=this.rider(id),cost=this.attributeCost(r.attributes[index]);
  if(cost==null||r.cash<cost)return false;
  r.cash-=cost;r.attributes[index]+=ATTRIBUTE_STEP;this.persist();return true;
 }
}
