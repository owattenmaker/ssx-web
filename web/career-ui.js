// Conquer the Mountain front end (MCOMM hub, Transport, event lists, lodge, round objectives, standings,
// medal award) drawn with the original atlases, fonts and locale text. Layouts follow real PS2 frames captured
// with tools/ps2_menu_capture.py (local/ps2-capture/menus, see docs/career-events.md); rules live in career.js.
import { rideIntoEvent } from './ctm-event.js';
import { heatSteps } from './cutscenes.js';   // pv stationFlow: the freestyle WS13 gate lists
import { rideAcrossSwitch, rideWanted } from './ctm-transport.js';
import {Career,exploreNext,mountainPercent,MODE,MEDAL,MEDAL_NAMES,ATTRIBUTES,riderRanking,isTimed,recordSlot,eventKey,RIDER_CHARACTER,originalAttributeBytes,rankEntries,placementMedal,platinumThreshold} from './career.js';
import {Locale,format,clean} from './locale.js';
import {raceTime,hudRaceTime} from './race-time.mjs';
import {pv} from './pv-flags.js';
import {SESSION_MAP,mapMarker,nearestSessionPoint,DOT,PLAYER} from './session-map.js';   // pv sessionMap
const SY_LUI=448/480;   // a LUI screen's 640 x 480 frame on the 448-line UI canvas
import {ResultsLui} from './results-lui.js';
import {FsStandings,standingsModel,cardModel} from './fs-standings.js';   // pv fsStandings: the CTM freestyle heats in OV.LUI 42freestyle_standings   // pv luiResults: the OV.LUI panel and results (docs/visual-parity.md section 9)
import {money} from './trick-hud.js';   // 0x198AF0 (English): cash as "$ n" with thousands separators (pv cashGap)
import {LodgeScreens,LODGE_SCREENS} from './lodge-ui.js';
import {BuyAttribs} from './buy-attribs.js';   // pv buyAttribs: the lodge's Buy Attributes as cFEStateBuyAttrib (FE.LUI 33buyattribs)
import {LuiFlash} from './lui-flash.js';   // pv lodgeFlash: every lodge screen change through the PS2's TransitionOut flash
import {glyphKeyRect} from './input-glyphs.js';
import {CareerMessages} from './career-messages.js';
import {CtmPda,ICON} from './ctm-pda.js';
import {CtmMap,transportRedPaths,INDICATOR_STATE,STATION_INDICATOR} from './ctm-map.js';   // pv transportMap: the Transport map from the Map LUI (routes, indicator, outline)   // the MCOMM (PDATemplate, 31paus_freeride, 87yndialog) from OV.LUI
import {isPeakRun,isTimeChallenge,PEAK_RUNS} from './peak-run.js';
import {COURSE_PEAK,freeRideWorldOf} from './free-ride.js'; // the career peak of a course (course table +0x54): the streamed world it belongs to   // peak challenges on the streamed mountain (docs/peak-mountain.md)
import {rivalResult} from './rival-mode.js';
import {bigChallengeInfo} from './big-challenges.js'; // Big Challenge counts (1542E0 / 1542A0) for the INFO panels
import {COLLECTIBLE_TOTALS} from './free-ride-hud.js'; // 0x43FA70: a location's collectibles (the freeride list INFO)
import {runStats} from './monster-tricks.js'; // career run statistics -> monster-trick medals (0x155420)   // backcountry rival challenges (docs/backcountry.md)   // Message Center, relationship messages, HUD mail icon

const CAREER='/assets/CAREER/';
const CURSOR_STATES=new Set(['ctm-lodge','ctm-details','ctm-trophies','ctm-attributes']);   // pv stateCursor: the lodge states whose activation restores the cursor (0x186518)
const SCREENS=['ctm-mcomm','ctm-peaks','ctm-goals','ctm-events','ctm-confirm','ctm-lodge','ctm-attributes','ctm-saved',
 'ctm-objectives','ctm-pause','ctm-giveup','ctm-restart','ctm-results','ctm-award','ctm-records','ctm-quit','ctm-saveprompt','ctm-enterlodge','ctm-session','ctm-sessconfirm','ctm-gopeak','ctm-quitsave'];
// Session points per course (table 0x440770 +0x18 via 1545F8), Peak 1: the streamed world's MCOMM Session menu (overlay 0x20).
export const SESSION_POINTS={14:7,17:1,0:7,5:7,8:2,18:1,1:8,11:2,/* Peak 2 (docs/peak2.md) */19:1,2:7,9:2,12:2,20:1,3:7,6:7,15:6,/* Peak 3 (docs/peak3.md: 0x440770 +0x18) */21:1,4:7,7:6,10:2,13:2,16:5};
// Freeride Transport list per peak, PS2 order (table 0x478D38: 8 rows of 0x6C per peak, +0 course, +0xC station).
const FREERIDE_ORDER=[[14,17,5,0,8,18,1,11],[19,2,9,12,15,20,6,3],[16,21,4,10,13,7]];
const STREAMED_PEAKS=[1,2,3]; // peaks whose streamed world is ported (web/free-ride.js peakWorldOf; Peak 2: docs/peak2.md; Peak 3: docs/peak3.md)
const GOALS=['race','freestyle','freeride','earnings'];
// pv transportLists: the Transport's Race / Freestyle rows by peak, {course, mode} from the tables the PS2 list builds from (0x2012D0):
// race 0x4781D0, freestyle 0x4786E0 (0x6C a row). The goal lists 0x45AAD8 order Peak 2 / 3 freestyle differently (big air, pipe,
// slope style); the Transport lists slope style, big air, pipe (PS2 allpeak/nav/out-apj-card fs-list). Peak 3's race list has a 4th
// row, course 23 (none), which the PS2 hides too. A row's name (cUITemplate_MAP_setupMenus 0x201570..): the course name (flag +0x10 = 1),
// else its inline name (+0x14: 'Happiness Jam', 'Ruthless Jam', 'Throne Jam'), else mode 4 the course name, else 1FD190 (the peak events).
const TRANSPORT_ROW_NAMES={14:'Happiness Jam',15:'Ruthless Jam',16:'Throne Jam'};
const TRANSPORT_ROWS={race:[[[0,0],[1,0],[14,4],[14,6]],[[2,0],[3,0],[15,4],[15,7]],[[4,0],[16,4],[16,8]]],
 freestyle:[[[5,1],[8,3],[11,2],[14,5],[14,9]],[[6,1],[9,3],[12,2],[15,5],[15,10]],[[7,1],[10,3],[13,2],[16,5],[16,11]]]};
// The Transport check box (PS2 ctm-parity/runs/explore-info): white with a dark outline; a complete goal / a medal gets a black
// check that rises above the box.
function checkBox(c,x,y,w,done){c.fillStyle='#f7fafb';c.fillRect(x,y,w,w);c.strokeStyle='#0c1a26';c.lineWidth=1;c.strokeRect(x+.5,y+.5,w-1,w-1);
 if(done){c.lineWidth=2.5;c.beginPath();c.moveTo(x+1,y+w*.5);c.lineTo(x+w*.38,y+w-1);c.lineTo(x+w+1,y-w*.3);c.stroke();}}
const MODE_LABEL={0:'Race',1:'Slopestyle',2:'Super Pipe',3:'Big Air',4:'Rival',5:'Rival'};
// Widg atlas FE_1-11 cells (PS2 medal hexes G/S/B/P, lock, check box).
const MEDAL_CELL={[MEDAL.GOLD]:[203,0,52,42],[MEDAL.SILVER]:[203,40,52,42],[MEDAL.BRONZE]:[203,80,52,42],[MEDAL.PLATINUM]:[203,120,52,42]};
const LOCK=[7,51,14,16];
// Reward list (OV.LUI 62reward_list, rows by 0x1FF7B8): nine rows of ~22.75 lines (PS2 ctm/caps race-f95 frame), the award
// titles of table 0x441C68 and the item formats of 0x441CE8 (trophy, medal, pass, poster, card, art, video, toy, cheat).
const REWARD_ROWS=9,REWARD_PITCH=22.75;
const REWARD_TITLE=['kT_REWMtnConquer','kT_REWAllEvents','kT_REWPeak1Comp','kT_REWPeak2Comp','kT_REWPeak3Comp',...Array(3).fill('kT_REWRaceGoalPK1'),...Array(3).fill('kT_REWFSGoalPk3'),
 ...Array(3).fill('kT_REWExploreGoalPk1'),...Array(3).fill('kT_REWEarningsGoalPk1'),...Array(3).fill('kT_REWGoldEarned'),'kT_REWHapinessRace','kT_REWRuthlessRace','kT_REWThroneRace',
 'kT_REWHapinessFS','kT_REWRuthlessFS','kT_REWThroneFS','kT_REWPeak1Race','kT_REWPeak2Race','kT_REWPeak3Race','kT_REWPeak1FS','kT_REWPeak2FS','kT_REWPeak3FS'];
const REWARD_ITEM={trophy:[0,'kT_REWTrophy'],medal:[1,'kT_REWMedal'],poster:[3,'kT_REWPoster'],trading_card:[4,'kT_REWTradeCard'],art:[5,'kT_REWArt'],video:[6,'kT_REWVideo'],toy:[7,'kT_REWToy'],cheat_character:[8,'kT_REWCheatChar'],gear:[9,null]};
// 480-line PS2 frames -> 448-line UI canvas.
const Y=y=>Math.round(y*448/480);

export class CareerScreens {
 constructor(ui){this.ui=ui;this.lodge=new LodgeScreens(this);this.messages=new CareerMessages(this);this.pda=new CtmPda(ui);this.ctmMap=new CtmMap(ui);this.resultsLui=new ResultsLui(ui);this.fsStandings=new FsStandings(ui);this.career=null;this.loc=new Locale();this.pictures={};this.info=false;this.peak=1;this.goal='race';this.active=null;this.pendingAttr=ATTRIBUTES.map(()=>0);this.buyAttribs=new BuyAttribs(this);this.lodgeFlash=new LuiFlash();this.cursorMemo=new Map();
  if(new URL(location.href).searchParams.has('qa'))globalThis.ssxCareer=this;}   // QA/test handle (?qa=1)
 // pv lodgeFlash (web/lui-flash.js, docs/visual-parity.md 42): a lodge screen change is an FE state change: TransitionOut's white
 // flash rises over the old screen, the switch at full white, the new screen's intro under the fall; no input meanwhile.
 // pv stateCursor: each FE state keeps its menu cursor. On exit vt+0x58 = 0x1865A8 stores the 'Menu' index in the table at gp+0x1D90 by
 // state id (1A0708); a state whose activation (vt+0x30) is 0x186518 sets it back (1A06F0 -> 39B960): the lodge (vtable 0x473AA8),
 // Rider Details (0x4739D8), Buy Attributes (0x473908), Trophies' mountain room (0x469858); the peak / trophy rooms (126 / 127) store
 // but do not restore. PS2 tout-kbd: Rider Details reopens on Trophies after a visit there. The table lives for the session.
 lodgeGo(to){const run=()=>{this.memoCursor();const r=to();if(r&&typeof r.then==='function')return r.then(()=>this.restoreCursor());this.restoreCursor();};if(pv('lodgeFlash'))this.lodgeFlash.go(run);else run();}
 memoCursor(){const ui=this.ui,s=ui.screen;if(pv('stateCursor')&&CURSOR_STATES.has(s)&&!ui.feScreens?.keyboard&&!ui.characterSelect?.overlay?.(s)&&!this.buyAttribs?.popup)this.cursorMemo.set(s,ui.index);}
 restoreCursor(){const ui=this.ui,s=ui.screen;if(pv('stateCursor')&&CURSOR_STATES.has(s)&&this.cursorMemo.has(s)){ui.index=this.cursorMemo.get(s);ui.sync();}}
 lodgeFlashing(s){return pv('lodgeFlash')&&this.lodgeFlash.active&&(s==='ctm-lodge'||this.lodge.owns(s));}
 async load(){
  try{const data=await (await fetch(CAREER+'career.json')).json();this.data=data;this.loc=new Locale(data.strings);
   // Lodge gear / uber trick / song tables (tools/export_lodge_shop.py); optional so the career still runs without them.
   const shop=await fetch(CAREER+'shop.json').then(r=>r.ok?r.json():null).catch(()=>null);
   this.career=new Career(data,{shop});if(pv('playerName'))this.career.playerName=this.ui.feScreens?.playerName;/* the records' name (0x147170) */await this.messages.load();await this.pda.load();if(pv('luiResults'))await this.resultsLui.load();if(pv('fsStandings'))await this.fsStandings.load();}
  catch(error){console.warn('Career tables missing (python3 tools/export_career.py)',error);}
 }
 get ready(){return !!this.career;}
 owns(screen){return SCREENS.includes(screen)||this.lodge.owns(screen)||this.messages.owns(screen)||!!this.ui.bigChallenges?.owns(screen);} // Big Challenge overlays: web/big-challenges.js
 t(key,fallback=''){return this.loc.text(key,fallback)||fallback;}
 picture(name){
  if(!this.pictures[name]){const im=new Image();im.src=CAREER+name+'.png';this.pictures[name]=im;im.onload=()=>this.ui.draw(this.ui.lastState);}
  return this.pictures[name].complete&&this.pictures[name].naturalWidth?this.pictures[name]:null;
 }
 get riderId(){return this.ui.rider?.id||'sam';}
 // Run start (main.js startRun): the run whose statistics 0x155420 records when it ends (career events only, not free ride).
 runBegin(){this.run=this.active?.career&&!this.freeRide&&this.career?{riderId:this.riderId,mode:this.active.mode}:null;}
 // Run end (finish 0x238358 -> 0x154AB8 with the event result; restart / quit 0x1297C8 / 0x12B090 without): score object words.
 runEnd(words,finish=null){const r=this.run;this.run=null;if(!r||!words)return;
  const dnf=!!finish?.dnf;this.career.recordRunStats(r.riderId,runStats(words),finish?{score:dnf?0:finish.score,ticks:dnf?360000:finish.ticks,mode:r.mode}:null);}
 get me(){return this.career.rider(this.riderId);}
 courseEntry(code){return (this.ui.courses||[]).find(c=>c.code===code);}
 // A location is playable when the maps pipeline has prepared it (courses.json 'ready').
 available(course){if(course==null)return false;const code=this.data.courses[course].code;const c=this.courseEntry(code);return c?c.ready!==false:code==='ARA1';}

 // ---- entry points from OriginalUI ------------------------------------------------------------------
 // Conquer the Mountain start = the original world load: the game construction seeds the roster generator (0x22EFE8,
 // Career.seedRoster; every fresh career event then draws on it).
 enter(){this.career.rider(this.riderId);this.career.seedRoster?.();this.career.persist();if(this.ui.cb.freeRide&&!this.freeRide){/* 1A0720: a new career starts at Happiness (plane drop), later ones at the last lodge (P+0x27C) */const r=this.me;this.goWorld(r.firstRun===false?(r.lastStation??17):14,{reload:true});return;}this.go('ctm-mcomm',1);}
 // The connected Peak 1 world (web/free-ride.js): the page reloads into ?course=PEAK1&peakCourse=N; the career resumes there.
 goWorld(course,opt=null){/* pv heli: another world -> the transport ride plays, then the switch runs under its held loop (web/ctm-transport.js) */if(!opt?.ridden&&rideWanted(this.ui,this,course)){rideAcrossSwitch({ui:this.ui,cu:this,dest:course,go:()=>this.goWorld(course,{...(opt||{}),ridden:true})});return;}const r=this.me;const cover=!opt?.ridden&&this.afterEvent&&this.ui.cutscene?.drawCover;this.afterEvent=false;this.returnFade=!!cover;if(cover)this.ui.loading.world=(c,b)=>this.ui.cutscene.drawCover(c,this.ui);if(course>=17&&!pv('ctmSmallFixes'))r.lastStation=course;/* pv ctmSmallFixes: 146E10 only at WS10 (the station reached: enterWorld / courseChanged) */if(this.freeRide)this.freeRide.booth=false;this.career.save.pending={rider:this.riderId,freeRide:course,...(pv('stationFlow')&&!opt?.reload?{transport:true}:{})};/* the streamer's transport flag (+0x1C8) for WS10 (0x235220) */this.career.persist();const went=this.ui.cb.freeRide(course,opt||undefined);if(went!==false)this.ui.loading.world=null;if(went==='transport'){/* already in the streamed world: transported inside it (main.js transportInWorld) */delete this.career.save.pending;this.career.persist();r.firstRun=false;this.active=null;/* pv ctmSmallFixes: the course stays the one left until the core's 0x535C08 changes (22DF50 -> WS11 -> WS10 at the destination: courseChanged marks it visited, makes a station the last lodge, and the MCOMM follows the peak); setting it here made courseChanged skip them, so an in-world transport never set the last lodge (146E10) nor the visited bit */this.transportTo=course;this.pendingArrival=null;/* pv crossingArrival: this course change is a transport's (WS10 at the arrival) */this.freeRide=pv('ctmSmallFixes')?{...(this.freeRide||{course:this.ui.cb.freeRideCourse?.()??course})}:{...(this.freeRide||{}),course};return;}if(went===false)return;delete this.career.save.pending;this.career.persist();this.enterWorld(course);}
 enterWorld(course,{transport=false}={}){this.careerMark(true);if(pv('awardCascade'))this.career?.clearRewardRecord();/* WS10 enter 0x2355C0 -> 158E30 */if(this.ui.cutscene?.held)this.ui.cutscene.release();/* the transport's held loop ends: the new world is in (web/ctm-transport.js) */const r=this.me,bc=course>=14&&course<=16,first=bc&&!(this.visitedMask(r)&(1<<course));this.active=null;this.freeRide={course};this.peak=COURSE_PEAK[course]??this.peak;/* the MCOMM Transport opens on the peak being ridden */
  /* world state 10 (0x234F40 / 0x235080): a backcountry the rider has not visited yet (+0xACC, 145D38) plays its arrival
   list before the ride: Happiness = the ABC1 movie (list 29) + abc1_heli_arr_midway (group 0) + heli_arrb_<char>_midwayabc1
   (group 1), Ruthless / The Throne = the DBC2 / EBC3 movie + <bc>_heli_arr (16) + heli_arrb_<char> (17); all flags 3, Cross
   skips the rest. A world load at a visited backcountry plays nothing (PS2 ctm/caps reenter: Happiness again, no cut). The
   ride (WS1 -> 4) starts when the list ends, and only then is the location marked visited (bit set at WS4). */
  const ride=()=>{if(this.freeRide?.course!==course)return;this.markVisited(course);if(course>=17)r.lastStation=course;this.career.persist();this.ui.set('game');this.ui.cb.start();
   /* back from an event through the map (WS15 / arrival): the free-ride view comes in from white (2E4CE8; PS2 ctm/caps sj-return) */if(this.returnFade){this.returnFade=false;this.ui.cutscene?.fadeFrom?.({ticks:58});}};
  if(first&&this.ui.cb.cutscene){Promise.resolve(this.ui.cb.cutscene({kind:'arrival',location:['ABC1','DBC2','EBC3'][course-14],firstVisit:true,hold:true,restore:false})).catch(()=>{}).then(ride);return;}
  /* pv stationFlow: a transport (not a world load) into a visited backcountry: 0x235220 with the streamer's +0x1C8 queues [16, 17] (the heli drop) */
  if(bc&&transport&&pv('stationFlow')&&this.ui.cb.cutscene){Promise.resolve(this.ui.cb.cutscene({kind:'arrival',location:['ABC1','DBC2','EBC3'][course-14],firstVisit:false,hold:true,restore:false})).catch(()=>{}).then(ride);return;}
  ride();}
 // The per-rider visited mask (profile +0xACC, 145D38): bit = course index, set when a location is entered (WS10 -> WS4).
 // Saves from before it: Happiness counts as visited once the old first-run flag was cleared, Ruthless / The Throne by `arrived`.
 // 0x1EBA10 (HUD prepass 1EA930): the hints (HUD flag 0x1000000: the RECOVER label, the Uber hint) are off for a rider whose visited mask
 // has any Peak 2 (0x45A7C4 = 0x18924C) or Peak 3 (0x45A7C8 = 0x21A490) course bit (145E68 -> 1523E8 on profile +0xACC).
 hintsOff(r=this.me){return !!r&&(this.visitedMask(r)&(0x18924C|0x21A490))!==0;}
 visitedMask(r=this.me){if(typeof r.visited!=='number')r.visited=((r.firstRun===false?1<<14:0)|(r.arrived?.[15]?1<<15:0)|(r.arrived?.[16]?1<<16:0))>>>0;return r.visited>>>0;}
 markVisited(course,r=this.me){if(course>=0&&course<32)r.visited=(this.visitedMask(r)|(1<<course))>>>0;}
 // 0x535C08 changed in free ride (22DF50: a riding crossing or a transport = world state 11, then 10 at the new location):
 // WS11 enter clears the new-career flag (+0, 145CB0 at 0x236928), WS10 marks the location visited and, at a station,
 // makes it the last lodge (146E10 at 0x2356FC). PS2 ctm/caps reenter: +0 cleared at the first crossing (Happiness -> A).
 // pv crossingArrival (PS2 local/ps2-capture/ctm-parity/mountain/runs/fr-dra4a: a free ride down Intimidator into Green Base Station):
 // riding across a connector, the Unload trigger (22CEA8 -> 22DF50) changes 0x535C08 and enters world state 11 (record 14732: WS 4 -> 11,
 // the new-career flag cleared at 0x236928); world state 10 comes only at the connector's Load trigger (22D088: the row turns active,
 // record 19215: WS 11 -> 10, the last lodge 20 -> 17 at 0x2356FC, the reward record cleared), and world state 4 the next tick
 // (record 19216: the visited bit 17). A transport's arrival is WS10 at once (the page answered 'transport': transportTo).
 courseChanged(c){const fr=this.freeRide;if(!fr||!this.career||c==null||c<0||c===fr.course)return;if(pv('mountainRide'))this.peak=COURSE_PEAK[c]??this.peak;/* pv mountainRide: riding into another peak (the whole mountain): the MCOMM Transport opens on the peak being ridden */
  if(pv('crossingArrival')&&this.transportTo!==c&&this.loadedCourse!==c){const r=this.me;fr.course=c;r.firstRun=false;this.pendingArrival=c;this.career.persist();return;}   /* WS11: the rest waits for the Load (crossingArrived); a Load met without its Unload (a reset past it) posts first: at once */
  this.transportTo=null;this.pendingArrival=null;this.loadedCourse=null;
  if(pv('awardCascade'))this.career.clearRewardRecord();/* WS11 -> WS10 enter 0x2355C0 -> 158E30 */const r=this.me;fr.course=c;r.firstRun=false;this.markVisited(c);if(c>=17)r.lastStation=c;this.career.persist();}
 // The connector's Load trigger (22D088, web/free-ride.js 'arrive') after a riding crossing: world state 10 (158E30, 146E10 at a station),
 // then 4 (the visited bit).
 crossingArrived(c){if(!pv('crossingArrival')||!this.freeRide||!this.career)return;if(this.pendingArrival!==c){if(this.freeRide.course!==c)this.loadedCourse=c;return;}this.pendingArrival=null;if(pv('awardCascade'))this.career.clearRewardRecord();const r=this.me;if(c>=17)r.lastStation=c;this.markVisited(c);this.career.persist();}
 // After a location reload (?course=..&autostart=1) the pending round resumes at its objectives screen.
 // pv careerReload: a page reload during a career is the PS2's re-entry (a reset -> the title -> Conquer the Mountain -> 1A0B00): the
 // world load at Happiness while the new-career flag is set (145C38 ? 14), else at the last lodge (146D98), with the career running
 // (before: a plain free ride / event with no career: no collectibles, cash, MCOMM). The career is marked in this tab's
 // sessionStorage (rider, the URL's course) while it runs; a reload of that URL with that rider re-enters.
 careerMark(on){if(!pv('careerReload'))return;try{if(on)sessionStorage.setItem('ssx3.careerSession',JSON.stringify({rider:this.riderId,course:new URL(location.href).searchParams.get('course')}));else sessionStorage.removeItem('ssx3.careerSession');}catch{}}
 careerMarked(){if(!pv('careerReload'))return null;try{const m=JSON.parse(sessionStorage.getItem('ssx3.careerSession')||'null'),q=new URL(location.href).searchParams;return m&&m.rider&&m.course===q.get('course')&&(!q.get('rider')||q.get('rider')===m.rider)?m:null;}catch{return null;}}
 reenter(m){
  const index=(this.ui.riders||[]).findIndex(r=>r.id===m.rider);if(index>=0)this.ui.riderIndex=index;if(m.rider!==this.riderId)return false;
  this.ui.careerMode=true;this.career.rider(this.riderId);const r=this.me,start=r.firstRun===false?(r.lastStation??17):14;
  if(this.ui.course?.freeRide&&this.ui.course.code===freeRideWorldOf(start)&&this.ui.cb.freeRideCourse?.()===start){this.enterWorld(start);return true;}   // the URL's world and location already loaded
  this.enter();return true;
 }
 resume(){
  if(pv('careerReload')){const p=this.career?.save.pending;if(p&&(p.freeRide!=null||p.career))this.ui.careerMode=true;if(!p){const m=this.careerMarked();return m?this.reenter(m):false;}}
  const p=this.career?.save.pending;if(!p)return false;
  const index=(this.ui.riders||[]).findIndex(r=>r.id===p.rider);if(index>=0)this.ui.riderIndex=index;   // main.js restored ?rider=
  if(p.rider!==this.riderId)return false;
  delete this.career.save.pending;this.career.persist();
  if(p.freeRide!=null){this.enterWorld(p.freeRide,{transport:!!p.transport});return true;}
  this.begin(p.mode,p.course,p.career,{rideIn:!!p.rideIn});return true;
 }
 // pv crossWorld: riding out of the peak world into the next peak's station (web/free-ride.js): the crossing (WS11 enter clears the
 // new-career flag, 0x236928), then the world switch without a transport ride; WS10 at the station (its entry, the last lodge).
 crossWorld(dest){if(!this.freeRide||!this.ui.cb.freeRide||this.crossing)return;this.crossing=true;this.me.firstRun=false;this.career.persist();this.goWorld(dest,{ridden:true,reload:true});setTimeout(()=>{this.crossing=false;},2000);}
 // pv stationFlow: the transport booth (builtin 68 action 3 -> WS14 arg 2, overlay 0x21 in map mode 3) opens Select Peak on the ridden peak.
 openBooth(station){if(!this.freeRide)return;this.freeRide.station=station;this.freeRide.booth=true;this.peak=COURSE_PEAK[station]??this.peak;this.ui.set('ctm-peaks');this.ui.index=3-this.peak;this.ui.sync();}
 // Free ride: riding into a course's RaceRideState gate (builtin 67) starts its event in the world, with no load screen
 // (web/ctm-event.js, docs/ctm-flow.md): fade under the bars and "Loading...", the venue fly-over, then the approach.
 rideIn(mode,course,opts={}){return rideIntoEvent({ui:this.ui,cu:this,mode,course,pause:opts.pause});}
 begin(mode,course,career=true,{rideIn=false}={}){
  const ev=this.career.startEvent(this.riderId,mode,course,career);this.active={mode,course,career,rideIn};if(career)this.careerMark(true);else this.careerMark(false);
  // Event load: the original load screen (loading-screen.js, GL.LUI 110ctrl_load) runs first, then the objectives.
  /* a rival challenge (rolling start, modes 4 / 5): the card over the ready state (pv rivalCard, main.js readyView) */const card=()=>{this.ui.set('ctm-objectives');if(mode===MODE.RIVAL_TIME||mode===MODE.RIVAL_POINTS)this.ui.cb.readyView?.();};
  if(this.ui.loadEvent)this.ui.loadEvent(card);else card();return ev;
 }
 // Single Event (Quick Play): one final round (0x23A174 forces round 3) through the same objectives/results.
 single(courseEntry){
  const course=this.data.courses.findIndex(c=>c.code===courseEntry?.code);if(course<0)return false;
  const mode=courseEntry.rivalMode??this.career.standardMode(course);   // backcountry: Single Event 'Happiness' / 'Happiness Jam'
  if(this.ui.cb.course&&this.ui.course?.code!==courseEntry.code){
   this.career.save.pending={rider:this.riderId,mode,course,career:false};this.career.persist();
   this.afterCourse(this.ui.cb.course(courseEntry),()=>this.begin(mode,course,false));return true;
  }
  this.begin(mode,course,false);return true;
 }
 // cb.course / cb.peakRun: false = the page is navigating (the pending round resumes after the reload), true = the
 // location is loaded, or a Promise of either (in-app course switching). then() runs once the course is there.
 afterCourse(r,then){
  const go=v=>{if(v===false)return;delete this.career.save.pending;this.career.persist();then();};
  if(r&&typeof r.then==='function')Promise.resolve(r).then(go,e=>console.error('Course change failed',e));else go(r);
 }
 // Single Event peak runs (Peak 1 Race / Jam once the career opened them): the whole-peak world, no career rounds.
 singlePeak(mode){if(!this.ui.cb.peakRun)return false;this.career.save.pending={rider:this.riderId,mode,course:null,career:false};this.career.persist();this.afterCourse(this.ui.cb.peakRun(mode),()=>this.begin(mode,null,false));return true;}
 transport(mode,course){
  if(isPeakRun(mode)&&this.ui.cb.peakRun){/* the whole-peak run starts on the streamed world at the top of the peak */
   this.career.save.pending={rider:this.riderId,mode,course,career:true};this.career.persist();
   this.afterCourse(this.ui.cb.peakRun(mode),()=>this.begin(mode,course,true));return;}
  const code=this.data.courses[course].code,entry=this.courseEntry(code);
  if(entry&&this.ui.cb.course&&this.ui.course?.code!==code){
   this.career.save.pending={rider:this.riderId,mode,course,career:true};this.career.persist();
   this.afterCourse(this.ui.cb.course(entry),()=>this.begin(mode,course,true));return;
  }
  this.begin(mode,course,true);
 }
 // Every round starts with the rider's bought attributes in the physics (0x1494C0.. getters read profile bytes).
 play(){this.gaveUp=false;this.ui.cb.attributes?.(originalAttributeBytes(this.me.attributes));this.ui.cb.timeLimit?.(this.career.timeLimit(this.career.active.ev));this.ui.set('game');this.ui.cb.start();}
 go(screen,index=0){this.ui.set(screen);if(index){this.ui.index=index;this.ui.sync();}}
 // pv singlePause: a Single Event pauses in the same PDA with Return / Restart / Audio / Options / Quit (PS2 menus/single/11-single-pause)
 singlePause(){return !!this.active&&!this.active.career&&pv('singlePause');}
 pauseItems(){return this.singlePause()?['Return','Restart','Audio','Options','Quit']:['Return','Restart','Messages','Audio','Options',this.t('kT_CMNGiveUp','Give Up')];}
 // 208840: item k = '%s %d' (kT_MAPSessionPt, k + 1); the first is kT_CMNTopOfRun, the last kT_CMNBottomOfRun unless there are two.
 sessionItems(){const n=SESSION_POINTS[this.ui.cb.freeRideCourse?.()??-1]??0;return Array.from({length:n},(_,k)=>k===0?this.t('kT_CMNTopOfRun','Top of run'):k===n-1&&n!==2?this.t('kT_CMNBottomOfRun','Bottom of run'):`${this.t('kT_MAPSessionPt','Session point')} ${k+1}`);}

 // ---- per-screen items ------------------------------------------------------------------------------
 items(screen){
  const c=this.career;if(this.lodge.owns(screen))return this.lodge.items(screen);if(this.messages.owns(screen))return this.messages.items();if(this.ui.bigChallenges?.owns(screen))return this.ui.bigChallenges.items(screen);
  switch(screen){
   case 'ctm-mcomm':return ['Return','Transport','Session','Messages','Audio','Options','Quit'];
   case 'ctm-peaks':return [this.t('kT_Peak3name','Peak 3'),this.t('kT_Peak2name','Peak 2'),this.t('kT_Peak1name','Peak 1'),this.t(0x03574b7e,'All Mountain')];
   case 'ctm-goals':return ['Race','Freestyle','Freeride','Earnings'];
   // PS2 transport lists: the race list names the rival run by its course, the freestyle list as '<course> Jam'.
   case 'ctm-events':return this.list().map(e=>pv('transportLists')&&e.mode===MODE.RIVAL_POINTS?TRANSPORT_ROW_NAMES[e.course]:e.mode===MODE.RIVAL_TIME?this.data.courses[e.course].name:e.mode>=6?this.t(['kT_EventPk1Race','kT_EventPk2Race','kT_EventAllPeakRace','kT_EventPk1Jam','kT_EventPk2Jam','kT_EventAllPeakJam'][e.mode-6],e.name):e.name);
   case 'ctm-confirm':case 'ctm-giveup':case 'ctm-restart':case 'ctm-quit':case 'ctm-saveprompt':case 'ctm-enterlodge':case 'ctm-sessconfirm':case 'ctm-gopeak':case 'ctm-quitsave':return [this.t('kT_CMNYes','Yes'),this.t('kT_CMNNo','No')];
   case 'ctm-session':return this.sessionItems();
   case 'ctm-lodge':return [this.t(0x05848cb5,'Return to Game'),this.t('kT_CMNEquipGear','Equip Gear'),this.t('kT_CMNBuyGear','Buy Gear'),this.t(0x09ef3a83,'Buy Attributes').replace('attributes','Attributes'),'Rider Details',this.t(0x055addb3,'Music'),this.t(0x0d799ba5,'Save Game'),'Quit'];
   case 'ctm-attributes':return pv('buyAttribs')&&this.buyAttribs.session?this.buyAttribs.items([...ATTRIBUTES]):[...ATTRIBUTES];
   case 'ctm-saved':return [this.t(0x0f3ab955,'Continue')];
   case 'ctm-award':{const l=this.rewardLines();return l.length?l.map(x=>x.text):[this.t(0x0f3ab955,'Continue')];}
   case 'ctm-objectives':return [this.t(0x0f3ab955,'Continue')];
   case 'ctm-pause':return this.pauseItems();
   case 'ctm-results':return this.resultItems();
   case 'ctm-records':return this.topTime?[this.t(0x0f3ab955,'Continue'),this.t(0x0b0bfc13,'Save Records')]:[this.t('kT_OVRCMNReturn','Return')];
  }
  return [];
 }
 disabled(screen,i){
  if(this.lodge.owns(screen))return this.lodge.disabled(screen,i);if(this.messages.owns(screen))return false;if(this.ui.bigChallenges?.owns(screen))return this.ui.bigChallenges.disabled(screen,i);
  if(screen==='ctm-mcomm'&&(i===1||i===2)&&this.freeRide&&this.ui.cb.freeRideCrossing?.())return true; // crossing: overlay 4, no Transport / Session
  if(screen==='ctm-mcomm'&&i===2)return !(this.freeRide&&this.sessionItems().length);
  if(screen==='ctm-mcomm')return !(i===0&&this.ui.cb.freeRide)&&![1,5,6].includes(i)&&!(i===4&&this.ui.audioMenus?.ready)&&!(i===3&&this.messages.ready);
  if(screen==='ctm-peaks')return i<3&&!this.me.peaks[2-i];
  if(screen==='ctm-goals')return i===3;
  if(screen==='ctm-events'){const e=this.list()[i];return !e||e.locked||!e.playable;}
  if(screen==='ctm-lodge')return [1,2,5].includes(i)&&!this.career.shop;
  if(screen==='ctm-pause')return this.singlePause()?i===2&&!this.ui.audioMenus?.ready:i===2&&!this.messages.ready||i===3&&!this.ui.audioMenus?.ready;
  if(screen==='ctm-results')return this.resultItems()[i]===this.t('kT_BTNReplay','Replay')&&!this.ui.cb.replayAvailable?.();   // the full replay (web/replay-ui.js, pv replay)
  if(screen==='ctm-records')return !!this.topTime&&i===1;   // Save Records: the memory card (the port saves the records at once)
  return false;
 }
 layout(screen,i){
  if(this.lodge.owns(screen))return this.lodge.layout(screen,i);if(this.messages.owns(screen))return this.messages.layout(i);if(this.ui.bigChallenges?.owns(screen))return this.ui.bigChallenges.layout(screen,i);
  if(screen==='ctm-mcomm'||screen==='ctm-pause')return this.pda?.ready?[195,Y(106)+i*Y(40),240,Y(34)]:[195,Y(94)+i*Y(40),240,Y(34)];   // 31paus_freeride rows: Menu (200, 110) + 40 i
  if(['ctm-peaks','ctm-goals','ctm-events'].includes(screen))return [0,Y(170)+this.rowY(screen,i),240,Y(24)];
  if(['ctm-confirm','ctm-giveup','ctm-restart','ctm-quit','ctm-saveprompt','ctm-enterlodge','ctm-sessconfirm','ctm-gopeak','ctm-quitsave'].includes(screen))return this.pda?.ready?[280,Y(234)+i*Y(25),120,Y(24)]:[260,Y(245)+i*Y(24),120,Y(24)];   // 87yndialog: Menu0000 (270, 240), rows 25 apart
  if(screen==='ctm-session')return [0,Y(170)+Y(25)*i,240,Y(24)];
  if(screen==='ctm-lodge')return [205,Y(128)+i*Y(20),315,Y(20)];
  if(screen==='ctm-attributes')return pv('buyAttribs')&&this.buyAttribs.session?this.buyAttribs.layout(i):[40,Y(146)+i*Y(20),420,Y(20)];
  if(screen==='ctm-results'&&this.peakResults())return [440,Y(339)+i*Y(16),150,Y(16)];if(screen==='ctm-records'&&this.topTime)return [430,Y(366)+i*Y(16),150,Y(16)];if(screen==='ctm-results')return [440,Y(318)+i*Y(15),150,Y(15)];
  if(screen==='ctm-award'){const k=i-(this.rewardTop||0);return k>=0&&k<REWARD_ROWS?[100,Y(180)+k*Y(REWARD_PITCH),440,Y(REWARD_PITCH)]:[0,0,0,0];}
  return [420,Y(366),170,Y(24)];
 }
 rowY(screen,i){return screen==='ctm-goals'&&i===3?Y(90):Y(25)*i;}
 // Transport lists: goal events (0x45AAD8) or, for Freeride, the peak's stations (the lodge) and runs.
 list(){
  const c=this.career,id=this.riderId;
  if(this.goal==='freeride'){
   const world=!!this.ui.cb.freeRide&&STREAMED_PEAKS.includes(this.peak); // the connected worlds ported: Peak 1 (docs/peak-mountain.md), Peak 3 (docs/peak3.md)
   return FREERIDE_ORDER[this.peak-1].map(i=>this.data.courses[i]).filter(Boolean)
    .map(x=>({name:x.kind==='station'?x.short:x.name,course:x.index,station:x.kind==='station',locked:false,playable:world||x.kind==='station',freeRide:world,medal:MEDAL.NONE}));
  }
  const goalRows=c.goalEvents(id,this.peak,this.goal),rows=pv('transportLists')&&TRANSPORT_ROWS[this.goal]?TRANSPORT_ROWS[this.goal][this.peak-1].map(([course,mode])=>goalRows.find(e=>e.mode===mode&&(mode>=6||e.course===course))).filter(Boolean):goalRows;
  return rows.map(e=>({...e,playable:e.mode<4||e.mode===4||e.mode===5?this.available(e.course):isPeakRun(e.mode)?!!this.ui.cb.peakRun:false}));/* every peak run is playable: the Peak 2 Jam stays in the PEAK2 world, the Peak 2 Race and the All Peak Race / Jam run in the whole-mountain world (web/free-ride.js peakRunWorld, docs/peak3.md section 6) */ /* peak runs: Peak 1 on the streamed world */
 }
 // A peak run's results on the PS2 (docs/peak3.md section 6 "Results"; local/ps2-capture/allpeak/flow/out-apr-results,
 // out-p2r-results): a new top-5 time opens "Top 5 Record Times" first ("Congratulations, you've got a top time!", Continue /
 // Save Records), then the rewards, then "Rival Challenge / <run> / Event Results" (Time to beat / Your time and the won /
 // lost text) with Transport / Restart / Quit.
 peakResults(){return !!this.active?.career&&!!this.result&&isPeakRun(this.result.mode);}
 // pv resultsMenu: the item the results menu opens on. 43final_standings' hook 0x1E7558 (vtable 0x474000 slot 0x40) moves
 // overlayMenu to item 1 (39B960(menu, 1)) when the game type byte 0x535C11 is 1 (Single Event), with no outcome test: a
 // Single Event opens on Restart (PS2 nav/bc/out-jam-finish (jam, lost) and menus/replay/bhp1-neutral (pipe, lost), no input);
 // the career results (races, freestyle, peak runs) open on item 0 (PS2 after-final / race-q / menus/race / p2r-timeup). They
 // open that way every time, back from the Top 5 Records too (PS2 menus/race 18-records -> 19-back-results on Transport, then
 // 3 x Up -> Replay: the menu wraps). `was` is the item the flag-off code uses.
 resultsFocus(was){if(!pv('resultsMenu'))return was;return this.peakResults()||this.active?.career?0:1;}
 peakRunName(mode){return this.t(['kT_EventPk1Race','kT_EventPk2Race','kT_EventAllPeakRace','kT_EventPk1Jam','kT_EventPk2Jam','kT_EventAllPeakJam'][mode-6],PEAK_RUNS[mode].name);}
 resultItems(){
  const r=this.result;if(!r)return [];
  if(this.peakResults())return [this.t('kT_OVRCMNGoToMap','Transport'),this.t('kT_BTNRestart','Restart'),this.t('kT_BTNQuit','Quit')];
  const single=!this.active?.career,next=r.outcome.advance&&!r.outcome.medalDone;
  let first=this.t('kT_OVRCMNGoToMap','Transport');
  if(single)first=this.t('kT_OVRCMNNextevnt','Next event');
  // 1E5AA0 / 0x1E6310: item 0 stays 1 = kT_OVRCMNextHeat 'Next heat' while the event goes on, relabelled
  // kT_OVRCMNFinalRound 'Final Round' when the next round (GMM+0x70) is the final (PS2 ctm/caps race-q: the won qualifier
  // shows 'Next heat'); 'Transport' once the event is over or the round was failed.
  else if(next)first=this.career.active.ev.round===3?this.t('kT_OVRCMNFinalRound','Final Round'):this.t('kT_OVRCMNextHeat','Next heat');
  return [first,this.t('kT_BTNRestart','Restart'),this.t('kT_BTNReplay','Replay'),this.t('kT_BTNRecords','Records'),this.t('kT_BTNQuit','Quit')];
 }

 // ---- navigation -------------------------------------------------------------------------------------
 key(e){
  const s=this.ui.screen;if(pv('buyAttribs')&&this.buyAttribs.flash&&(s==='ctm-lodge'||s==='ctm-attributes')){e.preventDefault();return true;}if(this.lodgeFlashing(s)){e.preventDefault();return true;}   // no input during the lodge <-> Buy Attributes flash
  if(this.lodge.owns(s))return this.lodge.key(e);if(this.messages.owns(s))return this.messages.key(e);if(this.ui.bigChallenges?.owns(s))return this.ui.bigChallenges.key(e);
  if(['ctm-peaks','ctm-goals','ctm-events'].includes(s)&&(e.code==='ShiftLeft'||e.code==='ShiftRight'||e.code==='KeyI')){this.info=!this.info;this.ui.draw(this.ui.lastState);return true;}
  if(s==='ctm-attributes'&&pv('buyAttribs')&&this.buyAttribs.session)return this.buyAttribs.key(e);   // Left / Right points, Up / Down wrap, the buy popup, Triangle
  if(s==='ctm-attributes'&&['ArrowLeft','ArrowRight'].includes(e.code)){e.preventDefault();this.adjust(this.ui.index,e.code==='ArrowRight'?1:-1);return true;}
  // pv resultsMenu: the OV.LUI overlay menus' Up / Down (0x39AB50 down, 0x39AC48 up): the next item whose element flags lack 0x20
  // (greyed), past either end back round to the other when the menu element has the wrap flag 0x80 (43final_standings /
  // 70peakchal_results / 42freestyle_standings overlayMenu 0x2cc / 0x2c9 / 0x2cf, 61toptimes' menu 0x2c0): PS2 menus/race
  // 19-back-results, three Ups from Transport land on Replay.
  if((s==='ctm-results'||s==='ctm-records')&&(e.code==='ArrowUp'||e.code==='ArrowDown')&&pv('resultsMenu')){
   e.preventDefault();if(e.repeat)return true;const ui=this.ui,n=ui.items().length,d=e.code==='ArrowUp'?-1:1;
   for(let k=1;k<n;k++){const j=((ui.index+d*k)%n+n)%n;if(!this.disabled(s,j)){ui.index=j;break;}}
   ui.sync();return true;}
  return false;
 }
 adjust(i,d){
  const r=this.me,level=r.attributes[i]+5*this.pendingAttr[i];
  if(d>0&&(this.career.attributeCost(level)==null||this.pendingCost()+this.career.attributeCost(level)>r.cash))return;
  if(d<0&&this.pendingAttr[i]===0)return;
  this.pendingAttr[i]+=d;this.ui.sync();
 }
 pendingCost(){let total=0;this.pendingAttr.forEach((n,i)=>{let v=this.me.attributes[i];for(let k=0;k<n;k++){total+=this.career.attributeCost(v);v+=5;}});return total;}
 choose(i){
  const ui=this.ui,s=ui.screen,c=this.career;
  if(pv('buyAttribs')&&this.buyAttribs.flash&&(s==='ctm-lodge'||s==='ctm-attributes'))return;
  if(this.lodgeFlashing(s))return;
  if(this.disabled(s,i))return;
  if(this.lodge.owns(s))return this.lodge.choose(i);if(this.messages.owns(s))return this.messages.choose(i);if(this.ui.bigChallenges?.owns(s))return this.ui.bigChallenges.choose(i);
  switch(s){
   case 'ctm-mcomm':if(i===0&&this.freeRide){ui.set('game');ui.cb.resume();return;}if(i===2&&this.freeRide){ui.set('ctm-session');ui.index=pv('sessionMap')?this.sessionFocus():0;ui.sync();return;}if(i===0&&ui.cb.freeRide){this.goWorld(this.me.lastStation??17);return;}if(i===1){ui.set('ctm-peaks');ui.index=3-this.peak;ui.sync();}else if(i===5){ui.optionsReturn='ctm-mcomm';ui.set('options');}else if(i===6){this.quitFrom='ctm-mcomm';ui.set('ctm-quit');ui.index=1;ui.sync();}/* 'Quit Game' opens on No (PS2 ctm/caps quit-ctm) */else if(i===3&&this.messages.ready)this.messages.open(()=>{ui.set('ctm-mcomm');ui.index=3;ui.sync();});else if(i===4&&ui.audioMenus?.ready)ui.audioMenus.open('audio',{back:()=>{ui.set('ctm-mcomm');ui.index=4;ui.sync();}});/* Audio: web/audio-menu.js (142audio_pda) */return;
   case 'ctm-peaks':if(i<3){const peak=3-i;/* Select Peak on another peak than the one ridden (PS2 peak3/nav out-ctm-to-e: "Go to this peak now?", OVAMER 0x079ACB07; Yes = world state 10 at the peak's backcountry, the first arrival with its cinematic) */if(this.freeRide&&ui.cb.freeRide&&STREAMED_PEAKS.includes(peak)&&COURSE_PEAK[this.freeRide.course??17]!==peak){this.goPeak=peak;ui.set('ctm-gopeak');ui.index=0;ui.sync();return;}this.peak=peak;this.info=false;ui.set('ctm-goals');}return;
   case 'ctm-gopeak':if(i===0){this.peak=this.goPeak;this.goWorld([14,15,16][this.goPeak-1]);return;}ui.set('ctm-peaks');ui.index=3-this.goPeak;ui.sync();return;
   case 'ctm-goals':this.goal=GOALS[i];this.info=false;ui.set('ctm-events');return;
   case 'ctm-events':{const e=this.list()[i];this.selected=e;ui.set('ctm-confirm');return;}
   case 'ctm-confirm':if(i===0){const e=this.selected;if(e.freeRide&&ui.cb.freeRide){this.goWorld(e.course);return;}/* in free ride a course event transports to that course (PS2 ps2b sj-transport: the heli, the arrival, then the rider rides into the start gate, which starts it) */if((this.freeRide||this.afterEvent)&&ui.cb.freeRide&&!e.station&&e.course<14&&!isPeakRun(e.mode)&&this.available(e.course)){this.goWorld(e.course);return;}if(e.station){this.lodgePeak=this.peak;ui.set('ctm-lodge');}else this.transport(e.mode,e.course);}else{ui.set('ctm-events');}return;
   case 'ctm-lodge':if(i===0)ui.set('ctm-saveprompt');else if(i===3){this.pendingAttr=ATTRIBUTES.map(()=>0);if(pv('buyAttribs'))this.buyAttribs.enter();else ui.set('ctm-attributes');}else if(i===1||i===2)this.lodgeGo(()=>this.lodge.openGear(i===1?'equip':'buy'));else if(i===4)this.lodgeGo(()=>ui.set('ctm-details'));else if(i===5){if(ui.audioMenus?.ready)this.lodgeGo(()=>ui.audioMenus.open('fe-music',{ctm:true,back:()=>{ui.set('ctm-lodge');ui.index=5;ui.sync();if(pv('lodgeFlash'))this.lodgeFlash.fall();}}));else ui.set('ctm-music');}/* Music: 140audio + 16radio with buying (web/audio-menu.js) */else if(i===6){if(pv('lodgeSave')&&this.lodge.saveGame?.ready())this.lodgeGo(()=>this.lodge.saveGame.open());else{this.saved=c.persist();ui.set('ctm-saved');}}/* pv lodgeSave: the Save game screen (web/save-game.js) */else if(i===7){this.quitFrom='ctm-lodge';ui.set('ctm-quit');}return;
   case 'ctm-attributes':if(pv('buyAttribs')&&this.buyAttribs.session){this.buyAttribs.choose(i);return;}{let bought=0;this.pendingAttr.forEach((n,k)=>{for(let j=0;j<n;j++)bought+=c.buyAttribute(this.riderId,k)?1:0;});this.pendingAttr=ATTRIBUTES.map(()=>0);c.persist();ui.set('ctm-lodge');ui.index=3;ui.sync();return;}
   case 'ctm-saved':ui.set('ctm-lodge');ui.index=6;ui.sync();return;
   // Lodge 'Return to Game' asks kT_CMNSaveProgress 'Save progress?' before going back to the mountain.
   case 'ctm-saveprompt':if(i===0)c.persist();if(this.freeRide&&this.ui.cb.freeRide){/* lodge Return to Game: GL.LUI 118loadoutlodge (cGameLoadStateOutLodge), then ride in above the station */const station=this.freeRide.course>=17?this.freeRide.course:(this.me.lastStation??17);/* the lodge exit is a world load at the station (cGameLoadStateOutLodge -> WS10 -> 11D390: 11DE60(rider, 0, 2), riding from the station's first session point; PS2 ctm/caps start-lodge): no lodge prompt until the rider rides into the door again */const back=()=>{this.me.lastStation=station;this.career.persist();this.freeRide.course=station;ui.set('game');/* pv lodgeWorldLoad: the world start's ride (a new rider at the 11D390 entry, the ride start, the world load's audio; main.js) */if(pv('lodgeWorldLoad')&&ui.cb.freeRideWorldLoad?.(station))return;ui.cb.resume();ui.cb.freeRideRespawn?.(station);};/* the rider from the lodge (pv careerRider, main.js refreshRider) loads under the load-out screen */const rider=ui.cb.refreshRider?.();if(ui.transitions){ui.set('transition');Promise.all([ui.transitions.run('118loadoutlodge'),rider]).then(back);}else if(rider)Promise.resolve(rider).then(back);else back();return;}this.go('ctm-mcomm',1);return;
   // 98enterlodge (overlay 0x1F, 1F7198): Yes -> Lodge - Peak N (230488 -> world state 6); No -> ride in again above the station.
   case 'ctm-enterlodge':if(i===0){this.lodgePeak=[1,1,2,2,3][(this.freeRide.station??17)-17]??1;/* Lodge - Peak N of the station's peak (Yellow / Red: Peak 2, docs/peak2.md) */this.freeRide.course=this.freeRide.station??this.freeRide.course;/* FL.LUI 117loadinlodge (cFELoadStateInLodge), web/transition-screens.js */if(ui.transitions){ui.set('transition');ui.transitions.run('117loadinlodge').then(()=>{ui.set('ctm-lodge');this.restoreCursor();});}else{ui.set('ctm-lodge');this.restoreCursor();}}else{ui.set('game');ui.cb.resume();/* pv doorNoPlace (PS2 ctm-parity/door: No at tick 636 -> the rider at the station's session point 0, (-65782, 33404) at Green, moving at forward x 833 cm/s: 11DE60(rider, 0, 2) + 11DF18; under the cut the NIS held it at mdl_A_NIS_Lodge_0 with no speed). Before (pv stationFlow alone): it stayed at the door */if(!pv('stationFlow')||pv('doorNoPlace'))ui.cb.freeRideRespawn?.(this.freeRide.station);}return;
   // MCOMM Quit (overlay 3) -> 'Quit Game' (No focused) -> Yes -> 'Save progress before quitting?' (Yes focused) -> the save
   // (121Profilesave_pda) -> the Hints load -> the TITLE screen (PS2 ctm/caps quit-ctm, quit-save, quit-save-tri). The lodge's
   // Quit ('Quit to Title screen?', Yes focused) asks the same save question over the lodge (PS2 ctm/caps lodge-quit).
   case 'ctm-quit':if(i===0&&this.quitFrom==='ctm-pause'){this.quitToTitle();return;}if(i===0){ui.set('ctm-quitsave');ui.index=0;ui.sync();}else{ui.set(this.quitFrom||ui.previousScreen||'ctm-mcomm');if(ui.screen==='ctm-mcomm'){ui.index=6;ui.sync();}else if(ui.screen==='ctm-pause'){ui.index=4;ui.sync();}}return;
   // the browser card saves as it goes (web/career-save.js), so Yes writes it once more and No leaves it as it stands
   case 'ctm-quitsave':if(i===0)this.saved=c.persist();this.quitToTitle();return;
   // Session (overlay 0x20): 'Session this area?' Yes -> P[0xA]+0x10 = point, world state 15 (236058): placement 11DE60(rider, point, 2).
   case 'ctm-session':this.sessionPoint=i;ui.set('ctm-sessconfirm');ui.index=0;ui.sync();return;
   case 'ctm-sessconfirm':if(i===0){ui.set('game');ui.cb.resume();ui.cb.freeRideSession?.(this.sessionPoint+1);/* the item's value +0x18 = k + 1 (208840) -> overlay +0xD8 -> P[0xA]+0x10 */}else{ui.set('ctm-session');ui.index=this.sessionPoint??0;ui.sync();}return;
   case 'ctm-objectives':if(this.cardOpenAt&&(performance.now()-this.cardOpenAt)*60/1000<30)return;this.play();return;
   case 'ctm-pause':
    // Single Event (r3-pause-*): Restart 'Are you sure?' -> the round's card; Quit 'Quit Game' (No focused) -> Yes -> the title, no save prompt
    if(this.singlePause()){if(i===0){ui.set('game');ui.cb.resume();}else if(i===1)this.go('ctm-restart',1);
     else if(i===2&&ui.audioMenus?.ready)ui.audioMenus.open('audio',{back:()=>{ui.set('ctm-pause');ui.index=2;ui.sync();}});
     else if(i===3){ui.optionsReturn='ctm-pause';ui.set('options');}else if(i===4){this.quitFrom='ctm-pause';this.go('ctm-quit',1);}return;}
    if(i===0){ui.set('game');ui.cb.resume();}
    else if(i===1)this.go('ctm-restart',1);
    else if(i===2&&this.messages.ready)this.messages.open(()=>{ui.set('ctm-pause');ui.index=2;ui.sync();});   // Messages: web/career-messages.js
    else if(i===4){ui.optionsReturn='ctm-pause';ui.set('options');}
    else if(i===3&&ui.audioMenus?.ready)ui.audioMenus.open('audio',{back:()=>{ui.set('ctm-pause');ui.index=3;ui.sync();}});   // Audio: web/audio-menu.js (142audio_pda)
    else if(i===5)this.go('ctm-giveup',1);   // PS2 'Are you sure?' defaults to No
    return;
   case 'ctm-giveup':if(i===0)this.giveUp();else ui.set('ctm-pause');return;
   case 'ctm-restart':{const from=this.restartFrom;this.restartFrom=null;if(i===0)this.restartToCard(from==='results');else if(from==='results'){ui.set('ctm-results');ui.index=1;ui.sync();}else{ui.set('ctm-pause');ui.index=1;ui.sync();}return;}
   case 'ctm-results':return this.resultAction(i);
   case 'ctm-award':if(pv('awardCascade'))this.career.clearRewardRecord();/* 1FF700 -> 158E30: the list's Continue clears the record */ui.set('ctm-results');ui.index=this.resultsFocus(0);ui.sync();return;
   case 'ctm-records':if(this.topTime){if(i!==0)return;this.topTime=false;const to=this.active?.career&&this.rewardLines().length?'ctm-award':'ctm-results';ui.set(to);ui.index=to==='ctm-results'?this.resultsFocus(0):0;ui.sync();return;}ui.set('ctm-results');ui.index=this.resultsFocus(3);ui.sync();return;
  }
 }
 back(){
  const ui=this.ui,s=ui.screen;if(this.lodgeFlashing(s))return;if(this.lodge.owns(s))return this.lodge.back();if(this.messages.owns(s))return this.messages.goBack();if(ui.bigChallenges?.owns(s))return ui.bigChallenges.back();
  if(pv('stationFlow')){
   if(s==='ctm-enterlodge'){this.choose(1);return;}   // 1F72E0: event 6 (Triangle) sets the same done flag as No
   if(s==='ctm-peaks'&&this.afterEvent)return;        // map mode 4 (the results' Transport): Back is ignored (0x2022A4)
   if(s==='ctm-peaks'&&this.freeRide?.booth){this.freeRide.booth=false;ui.set('game');ui.cb.resume();ui.cb.freeRideSession?.(1);return;}   // booth mode 3: WS15 at the station (0x20220C..54, 0x2365D8)
  }
  if(s==='ctm-attributes'&&pv('buyAttribs')&&this.buyAttribs.session){if(!this.buyAttribs.popup)this.buyAttribs.leave();return;}   // Triangle: the lodge, Buy Attributes focused, pending dropped
  const to={'ctm-restart':'ctm-pause','ctm-saveprompt':'ctm-lodge','ctm-peaks':'ctm-mcomm-1','ctm-goals':'ctm-peaks','ctm-events':'ctm-goals','ctm-confirm':'ctm-events','ctm-attributes':'ctm-lodge','ctm-saved':'ctm-lodge','ctm-giveup':'ctm-pause','ctm-records':'ctm-results','ctm-objectives':this.active?.career?'ctm-events':'event'}[s];
  if(s==='ctm-records'&&!this.topTime&&pv('resultsMenu')){ui.set('ctm-results');ui.index=this.resultsFocus(0);ui.sync();return;}   // the results open again (19-back-results)
  if(s==='ctm-pause'){ui.set('game');ui.cb.resume();return;}
  if(s==='ctm-mcomm'&&this.freeRide&&pv('startRules')){ui.set('game');ui.cb.resume();return;}   // Triangle 'Previous' closes the free-ride MCOMM (PS2 startprobe/mcomm-tri); main.js's Escape toggle did it before
  if(s==='ctm-quit'||s==='ctm-quitsave'){const to=this.quitFrom||'ctm-mcomm';ui.set(to);ui.index=to==='ctm-mcomm'?6:to==='ctm-lodge'?7:to==='ctm-results'?(this.peakResults()?2:4):to==='ctm-pause'?4:0;ui.sync();return;}
  if(s==='ctm-session'){this.go('ctm-mcomm',2);return;}
  if(s==='ctm-sessconfirm'){ui.set('ctm-session');ui.index=this.sessionPoint??0;ui.sync();return;}
  if(s==='ctm-gopeak'){ui.set('ctm-peaks');ui.index=3-(this.goPeak??this.peak);ui.sync();return;}
  if(to==='ctm-mcomm-1'){this.go('ctm-mcomm',1);return;}
  if(to){ui.set(to);if(s==='ctm-goals'){ui.index=3-this.peak;ui.sync();}if(s==='ctm-events'){ui.index=GOALS.indexOf(this.goal);ui.sync();}}
 }
 // Restart (results or pause, 0x20D7DC -> 2302A8): world reset and the round's handler again, then the same ride back up as
 // a next heat (PS2 ctm/caps sj-restart-yes: gond_inair #150 -> gond_inair_<char> #146, no fly-over) and the round's card
 // over the start-gate idle; Continue starts the countdown (docs/ctm-flow.md).
 restartToCard(fromResults){
  const ui=this.ui;if(fromResults)this.career.restartRound(this.result.outcome.round);
  if(!ui.cb.cutscene){this.play();return;}
  ui.gameAudio?.restartRun?.({fromResults});/* pv ctmRestartAudio: a restart keeps the world's audio (web/game-audio.js restartRun) */ui.cb.quit?.();let open=false;const card=()=>{if(open)return;open=true;ui.set('ctm-objectives');};
  // pv pauseRestart: the pause's Restart goes straight to the start-gate idle under the card (PS2 menus/race/r3-restart: the gate at
  // +30 samples, the card at +170; R&B r3-pause-restartyes the same); the results' Restart rides the gondola first (r3-results-restart)
  const kind=fromResults||!pv('pauseRestart')?'heat':'restart',mode=this.career.active?.ev?.mode,final=this.career.active?.ev?.round===3;
  // pv stationFlow: the results' Restart is world state 13 (0x235AA0): the gondola (27A860) only for races (event type 0); a freestyle
  // event queues its gate lists alone (27AAF8: [4 hut, 5] in a final, else [5]); a rival event (types 5 / 6) queues nothing
  if(kind==='heat'&&pv('stationFlow')&&mode!==MODE.RACE){if(mode===MODE.RIVAL_TIME||mode===MODE.RIVAL_POINTS||(pv('ws13Rival')&&isPeakRun(mode))){card();return;}/* pv ws13Rival: WS13's rival branch is event type 5 / 6 (0x535C10: the rival challenges AND the peak runs, 0x235B38..0x235B60): no gondola, no gate lists, then WS1 arg 3 -> WS2 -> the 68rival_pre card */ui.cb.cutscene({kind:'restart',steps:heatSteps(final).slice(2),onIdle:card,restore:false}).then(r=>{if(!r?.played)card();},card);return;}
  ui.cb.cutscene({kind,final,onIdle:card,restore:false}).then(r=>{if(!r?.played)card();},card);
 }
 // Results 'Transport' (0x20CF80 -> WS14 arg 2, 0x236250): list 1 group 11 (endevent_trans_arr #152; Snow Jam's list is
 // empty) over the event world, then the map (overlay 0x21). A free-ride destination restores free ride (2018A8: event
 // type 4, mode 12, handler 2): the same course at session point 1 (WS15), else a transport (web/free-ride.js).
 async transportAfterEvent(){
  const ui=this.ui;
  if(ui.cb.cutscene&&this.active?.career){try{await ui.cb.cutscene({kind:'transport-arrive',restore:false});}catch{}}
  // a peak run ends on its finish course's peak (Metro-City: Peak 1, "You are here" on the PS2's Select Peak)
  if(isPeakRun(this.active?.mode)&&COURSE_PEAK[PEAK_RUNS[this.active.mode].finish])this.peak=COURSE_PEAK[PEAK_RUNS[this.active.mode].finish];
  ui.cb.quit?.();this.afterEvent=true;ui.set('ctm-peaks');ui.index=3-this.peak;ui.sync();
 }
 resultAction(i){
  if(this.peakResults()&&i===2)i=4;   // Transport / Restart / Quit
  const ui=this.ui,c=this.career,items=this.resultItems();
  if(i===0){
   if(!this.active.career){ui.cb.quit?.();this.active=null;ui.set('event');return;}
   if(this.result.outcome.advance){/* next heat (world state 13): the gondola ride back up, then the card over the gate idle loop (web/cutscenes.js heatSteps) */if(this.ui.cb.cutscene&&this.result.mode===MODE.RACE){let open=false;const card=()=>{if(open)return;open=true;ui.set('ctm-objectives');};this.ui.cb.cutscene({kind:'heat',final:this.career.active?.ev?.round===3,onIdle:card,restore:false}).then(r=>{if(!r?.played)card();},card);return;}/* pv stationFlow: a freestyle next heat (WS13) queues [5] (or [4, 5] before the final) behind the card, no gondola */if(this.ui.cb.cutscene&&pv('stationFlow')&&this.result.mode>=MODE.SLOPESTYLE&&this.result.mode<=MODE.BIGAIR){let open=false;const card=()=>{if(open)return;open=true;ui.set('ctm-objectives');};this.ui.cb.cutscene({kind:'restart',steps:heatSteps(this.career.active?.ev?.round===3).slice(2),onIdle:card,restore:false}).then(r=>{if(!r?.played)card();},card);return;}ui.set('ctm-objectives');return;}
   this.transportAfterEvent();return;   // results 'Transport' (WS14 arg 2): endevent_trans_arr where the course has one, then the map
  }
  if(i===1){this.restartFrom='results';this.go('ctm-restart',1);return;}   // 0x20CF9C: overlay 0x17 'Are you sure?' first
  if(i===2){this.ui.cb.replay?.();return;}   // 0x20CE0C: the full replay (web/replay-ui.js); greyed without it
  if(i===3){ui.set('ctm-records');return;}
  // Quit (0x20CE60 -> 230488(S, 0), world state 6): 'Save progress before quitting?', then the front end
  if(i===4){if(this.active.career){this.quitFrom='ctm-results';ui.set('ctm-quitsave');ui.index=0;ui.sync();}else{ui.cb.quit?.();this.active=null;ui.set('main');}}
 }
 // Pause "Give Up" (0x20DA58 -> 1253D0): the menu closes and the run goes on for one tick; +0x480 = 1 makes 125228 finish it
 // (125368), so the TIME'S UP banner shows, then the results (288 ticks after the finish, main.js) with the player DNF
 // (race 360000 ticks, freestyle 0 points shown as DNF: 0x5366D0[slot]). PS2: local/ps2-capture/menus/pipegu, race/15-*.
 quitToTitle(){this.careerMark(false);const ui=this.ui;ui.cb.quit?.();this.active=null;this.freeRide=null;this.afterEvent=false;this.quitFrom=null;ui.set('title');}
 giveUp(){this.gaveUp=true;if(this.ui.cb.giveUp){this.ui.cb.giveUp();return;}this.ui.cb.quit?.();this.finish({dnf:true});}

 // pv fsCelebrate (web/game-tick.js at the finish): the freestyle place of career.js finishPlace, null for other modes.
 freestyleFinishPlace(score){
  const c=this.career,ev=this.active&&c?.active?.ev;if(!ev||!(ev.mode===MODE.SLOPESTYLE||ev.mode===MODE.HALFPIPE||ev.mode===MODE.BIGAIR))return null;
  const live=ev.opponent?this.ui.cb.opponent?.(true):null;
  return c.finishPlace(ev,score|0,live?.finished?((live.finishScore??live.score)|0):0);
 }
 // ---- results ----------------------------------------------------------------------------------------
 // result: {ticks, raceTicks, score, standings, dnf}. Race standings come from the AI racers (cb.standings)
 // when they run; without computer riders only the player is ranked (documented gap).
 finish(result){
  const c=this.career,ev=c.active.ev,human=this.ui.rider?.name||'Sam';
  if(ev.career){this.me.firstRun=false;c.persist();}   // WS7 enter 0x236E10: 145CB0(P, human, 0) clears the new-career flag
  let outcome,rows;
  if(ev.mode===MODE.RIVAL_TIME||ev.mode===MODE.RIVAL_POINTS){
   // 23B8C8 / 23BDB8 (web/rival-mode.js): the rival as it stood when the player finished; its time / latched score, else
   // the 0x122D78 / 0x122E50 estimate. DNF = +0x480 (give up, or the Rival Points 5:00 limit).
   const st=(result.standings||this.ui.cb.standings?.()||[]).map(x=>({...x})),opp=this.ui.cb.opponent?.(true);
   const riderRow=st.find(x=>!x.human)||{},humanRow={human:true,dnf:!!result.dnf,finishTicks:result.dnf?null:result.ticks??null,score:result.dnf?0:Math.round(result.score||0)};
   const rival={human:false,dnf:!!riderRow.dnf,finishTicks:riderRow.finishTicks??null,remaining:riderRow.remaining??opp?.remaining??0,origin:riderRow.origin??opp?.origin??0,
    score:opp?(opp.finished?opp.finishScore:opp.score):0,character:riderRow.character??opp?.character};
   const rr=rivalResult(ev.mode,[humanRow,rival],result.raceTicks??result.ticks??0);
   outcome=c.rivalResult(rr,{ticks:rr.values[0],score:rr.values[0]});
   rows=[humanRow,rival].map((x,i)=>({name:x.human?human:this.characterName(x),human:x.human,rank:rr.place[i],dnf:x.dnf,
    value:x.dnf?'DNF':ev.mode===MODE.RIVAL_TIME?raceTime(rr.values[i],false):String(rr.values[i])}));
   this.unopposed=!st.length&&!opp;
  }else if(isPeakRun(ev.mode)){
   // 23B468 / 23C2D8 (web/peak-run.js): solo; the stored value is the finish time (360000 ticks on time up / give up)
   // or the score (0); medal by the tier rows, a failed run pays nothing and repeats.
   outcome=c.peakRunResult({ticks:result.ticks??result.raceTicks??0,score:Math.round(result.score||0),dnf:!!result.dnf});
   rows=[{name:human,human:true,rank:0,dnf:!!result.dnf,value:result.dnf?'DNF':isTimeChallenge(ev.mode)?raceTime(outcome.value,false):String(outcome.value)}];
   this.unopposed=true;
  }else if(ev.mode===MODE.RACE){
   let standings=result.standings||this.ui.cb.standings?.()||null;
   standings=standings&&standings.length?standings.map(s=>({...s})):[{human:true}];
   const h=standings.find(s=>s.human)||standings[0];h.human=true;h.dnf=!!result.dnf;if(!result.dnf&&result.ticks!=null)h.finishTicks=result.ticks;
   outcome=c.raceResult(standings,{raceTicks:result.raceTicks??result.ticks??0,origin:result.origin??0});
   rows=outcome.rows.map(r=>({name:r.human?human:this.characterName(r),human:r.human,rank:r.rank,value:r.dnf?'DNF':raceTime(r.time,false),dnf:r.dnf}));
   this.unopposed=standings.length<2;
  }else{
   // slope style: the live opponent (slot 1) as it stood when the player finished (web/ai-race.js opponentAtFinish): its
   // finish score, or in the final the 0x122E50 estimate; its character is the rider that actually rode.
   let opp=null;const live=ev.opponent?this.ui.cb.opponent?.(true):null;
   if(live){const k=this.data.characters.findIndex(ch=>ch.first.toLowerCase()===String(live.character).toLowerCase());if(k>=0)ev.opponent.character=k;
    opp={finished:live.finished,score:live.finishScore??live.score,estimate:Career.opponentEstimate(live.score,live.remaining,live.origin)};}
   outcome=c.freestyleResult(result.dnf?0:Math.round(result.score||0),opp);
   rows=outcome.rows.map(r=>({name:r.human?human:this.characterName(r),human:r.human,rank:r.rank,value:r.human&&this.gaveUp?'DNF':String(r.score)}));
   this.unopposed=false;
  }
  outcome.medalDone=outcome.medal!==undefined;
  this.result={outcome,rows,mode:ev.mode,course:ev.course};
  // WS7 enter 236DA0 -> 20A8F8(7): when the result granted anything (158F30 > 0: cash, awards) the reward list overlay 0x10
  // (OV.LUI 62reward_list) comes first and its Continue opens the results (0x20CF1C); else the results at once.
  // PS2 ctm/caps race-f: Rewards (Cash / Gold medal earned / Accessory ...), then "Final Results".
  this.rewardTop=0;this.topTime=outcome.record>=0;/* 20A8F8(7): the result's record rank (+0x18 time / +0x1C score) >= 0 opens overlay 0xF (Top 5 Record Times) first in every event, standard ones too (PS2 ctm-left/runs final-top, qual-top) */const go=()=>{const to=this.topTime?'ctm-records':this.active.career&&this.rewardLines().length?'ctm-award':'ctm-results';this.ui.set(to);if(to==='ctm-results'&&this.resultsFocus(0)){this.ui.index=this.resultsFocus(0);this.ui.sync();}};
  this.finishCutscenes(ev,outcome).then(go,go);
 }
 // World state 5 -> 12 (0x233CD8 -> 0x27AC60, docs/cutscenes.md): in Conquer the Mountain, when the event is complete
 // (final round / one-round event, not a rival challenge or peak run) and the human placed 1st-3rd, the podium scene
 // (winner: win_ps_<char>{a,b,c}, 2nd/3rd: placeshow) with the winner's chartune; then, once per peak and discipline, the
 // newly opened rival challenge (bc_chal_<loc> + the rival's scene; not for cheat skins 10-20).
 async finishCutscenes(ev,outcome){
  const cb=this.ui.cb;if(!cb.cutscene||!this.active?.career||!outcome.medalDone)return;
  if(ev.mode===MODE.RIVAL_TIME||ev.mode===MODE.RIVAL_POINTS||isPeakRun(ev.mode))return;
  const code=this.data.courses[ev.course]?.code;if(!code)return;
  const idOf=r=>r.human?null:typeof r.character==='number'?(this.data.characters[r.character]?.first||'').toLowerCase():String(r.character||r.name||'').toLowerCase();
  const sorted=(outcome.rows||[]).slice().sort((a,b)=>(a.rank??9)-(b.rank??9)),place=sorted.findIndex(r=>r.human);
  // WS12: the podium list starts with its header fade (out 30: the finish view fades to black, PS2 podium-* s406..435)
  if(place>=0&&place<3){await this.ui.cutscene?.preFade?.({ticks:30});try{await cb.cutscene({kind:'podium',place,roles:sorted.slice(0,3).map(idOf)});}finally{this.ui.cutscene?.clearOverlay?.();}}
  const c=this.career,me=c.rider(this.riderId),peak=c.peakOf(ev.course),goal=ev.mode===MODE.RACE?'race':'freestyle';
  const rival=c.goalEvents(this.riderId,peak,goal).find(e=>e.mode===4||e.mode===5),key=`${peak}:${goal}`,skin=this.ui.rider?.kind==='cheat'&&(this.ui.rider.character>=10&&this.ui.rider.character<=20);
  me.rivalShown??={};
  if(rival&&!rival.locked&&!me.rivalShown[key]&&!skin){
   me.rivalShown[key]=true;c.persist();   // 0x146320 marks it shown
   // D = the first computer rider of the race (list +0x48), not the peak's named rival (PS2 capture: Zoe's Snow Jam
   // queued ss_bc_psymon for her first computer rider Psymon); playCutscene's default cast supplies it.
   await cb.cutscene({kind:'rival'});
  }
 }
 characterName(r){
  if(r.name)return r.name;
  if(typeof r.character==='number')return this.data.characters[r.character]?.first||`Rider ${r.character}`;
  if(typeof r.character==='string')return r.character[0].toUpperCase()+r.character.slice(1);
  return 'Rider';
 }
 // Freestyle time limit: the core ends the run (125228 in OriginalRaceSession::endTick: +0x480 DNF and the finish routine
 // 125108 once race ticks pass int(limit/60)*60); the results arrive through ui.showResults({dnf}). No browser-side timer.
 tick(s){}
 // Freestyle in-game HUD. 1EA930 sets owner+0x3CC from table 0x478078 by the mode byte 0x535C12: race 0x1530C047, slope
 // style 0x1530C056, pipe/big air 0x1530C016. Freestyle drops the race place (bit 0x1) and the progress meter (0x40) and
 // adds the standings (0x10). Clock (0x4, 1EC3F8, descriptor 0x1A at (320,20), HH:MM:SS): when the event is timed
 // (GMM+0x88) it counts down int(GMM+0x78/60)*60 - race ticks, shown as ceil(ticks/60) seconds; under 10 s (< 601 ticks)
 // owner+0x3D0 advances 1/60 per tick and the clock is red (0x4C8688) while it is >= 0.5, and stays red at 0.
 // Standings (type-7 case 1ECFFC, descriptor 0x17 at (20,20), rows from 1EB160): three rows over the posted scores sorted
 // best first, the player's row (name, score +0x198 plus the heat carry) inserted where it is >= the posted score.
 // Finished (state+0x88 = 1 finish, 2 time up): state+0x80 = 0xFFEFFFFF hides everything but the 0x100000 banner (21F660,
 // descriptor 0x1B at (320,180) 240x41): the 'fini' sprite (owner+0x4B4) with the run score under it, or 'timeup' (+0x49C).
 freestyleHud(){const ev=this.career?.active?.ev;return !!(this.active&&ev&&((ev.mode>=1&&ev.mode<=3)||ev.mode===MODE.RIVAL_POINTS));}
 hud(ctx,s){
  if(!this.freestyleHud())return;
  const ui=this.ui,ev=this.career.active.ev,score=Math.round(s.score||0);
  if(s.message){
   const t=s.finishElapsed??0;
   // TIME'S UP (+0x480: timeout or Give Up) has no finishov panel: after 3 s the HUD stays empty until the results
   // (PS2 pipe-brake-dnf ticks 7566..7625, pipe giveup snapshots).
   if(t>=3){if(!s.timedOut)this.finishPanel(ctx,ev,score,t-3);return;}
   if(pv('finishBanner')&&ui.trickHud&&ui.trickHudRenderer){const list=[];ui.trickHud.finishBanner(list,!!s.timedOut,String(score));ui.trickHudRenderer.render(ctx,list);return;}   // 0x21F660: 'fini' + "%d" of the score, or 'timeup'
   if(s.timedOut)ui.sprite('OV_1-3',1.5,127.5,169,23,200,159.5,240,41);
   else{ui.sprite('OV_1-3',1.5,1.5,131,24,200,159.5,240,41);ui.text(ctx,String(score),320,201,26,'#eef5ee','HUDFONT','center');}
   return;
  }
  // the live limit GMM+0x78 (R&B checkpoints add 60 s each, 0x2398E8; web/freestyle-event.js), else the table value
  const limit=s.timeLimitTicks>0?s.timeLimitTicks:this.career.timeLimit(ev),ticks=s.raceTicks??0;
  let clockTicks=ticks,red=false;
  if(limit){const left=Math.max(0,Math.trunc(limit/60)*60-ticks);clockTicks=Math.ceil(left/60)*60;if(left<601)red=left===0||((601-left)/60)%1>=.5;}
  if(pv('raceHud')&&ui.trickHud&&ui.trickHudRenderer){const t=Math.trunc(Math.fround(Math.fround(clockTicks)*Math.fround(0.01666666753590107))),list=[];ui.trickHud.raceClock(list,Math.trunc(t/3600),Math.trunc(t%3600/60),t%60,red?[1,1,0,0]:null);ui.trickHudRenderer.render(ctx,list);}/* pv raceHud: 0x1F16C0 at descriptor 0x1A, the +0x3D0 red override 0x4C8688 (docs/visual-parity.md) */
  else ui.text(ctx,hudRaceTime(clockTicks),320,20,21,red?'#ff0000':'#eef5ee','HUDFONT','center');
  // OPPONENT line only with a computer rider (1EA930: 2+ riders -> place, no standings): Single Event slope style and Rival
  // Points (event kind 6). Career slope style rides alone (0x238E20 GMM+0x14 = 5): standings rows as in the pipe (PS2
  // menus/rnbctm/zoe-b: '1ST 254500 / 2ND 121260 / 3RD 48620').
  if((ev.mode===MODE.SLOPESTYLE&&ev.opponent)||ev.mode===MODE.RIVAL_POINTS){this.opponentLine(ctx,score);return;}
  const carry=ev.round===2?(ev.player?.[0]||0):0,mine=score+carry,rows=[];let placed=false;
  // pv hudStandings: the rows of 0x1EB160 drawn as 0x1ED104..0x1ED4AC does (web/trick-hud.js standings: descriptor 0x17, value
  // column at x + 66.997 right-aligned on the widest row, '1'..'3' with 'ST'/'ND'/'RD' at half scale +1.6 px down, rows 21 apart;
  // the human's row: the name cut to 3 characters over the red 0x4C8628 box). PS2 pipe-brake tick 2018: '1ST 162880' ...
  if(pv('hudStandings')&&ui.trickHud&&ui.trickHudRenderer){const hud=ui.trickHud,list=[];
   hud.standings(list,hud.standingsRows(this.career.postedStandings(ev).map(p=>p.score),mine,hud.riderName(ui.rider)));ui.trickHudRenderer.render(ctx,list);return;}
  for(const p of this.career.postedStandings(ev)){if(!placed&&mine>=p.score){rows.push({human:true,score:mine});placed=true;}rows.push(p);}
  rows.slice(0,3).forEach((r,i)=>{const y=20+Math.round(i*18.7);
   if(r.human)ui.text(ctx,(ui.rider?.name||'').toUpperCase(),20,y,14,'#eef5ee','HUDFONT');
   else{ui.text(ctx,String(i+1),20,y,14,'#eef5ee','HUDFONT');ui.text(ctx,['ST','ND','RD'][i],30,y-1,8,'#eef5ee','HUDFONT');}
   ui.text(ctx,String(r.score),193,y,14,'#eef5ee','HUDFONT','right');});
 }

 // Slope style HUD (1EA930 with one computer rider: owner+0x3CC = 0x1D31C047 while riding): race place 21E1B0 by score
 // (rank mode 2, drawn by ui.js from the AI race), the progress meter (0x40, ui.js) and the OPPONENT line (bit 0x08000000,
 // type-7 case 1ED5C8..1ED78C, drawn when (flags & 0x08000001) == 0x08000001): the ASCII literal "OPPONENT" (0x46EB60,
 // descriptor 0x4A at (20,65)) and N = opponent +0x198 - player +0x198 as "+%d" (0x4A2298) when N >= 0 else "%d"
 // (descriptor 0x4B at (20,78)), both scale 0.7 in the HUD font; colour N > 0 red 0x4C88C8, -5000 <= N <= 0 pale yellow
 // 0x4C88A8, N < -5000 green 0x4C8888 (A,R,G,B floats). No standings rows (0x10 cleared).
 slopeHud(){const ev=this.career?.active?.ev;return !!(this.freestyleHud()&&(ev.mode===MODE.SLOPESTYLE||ev.mode===MODE.RIVAL_POINTS));}
 opponentLine(ctx,score){
  const o=this.ui.cb.opponent?.();if(!o)return;
  const n=((o.score|0)-(score|0))|0,col=n>0?'rgb(200,16,11)':n>=-5000?'rgb(255,255,204)':'rgb(83,255,90)';
  const ui=this.ui;if(pv('raceHud')&&ui.trickHud&&ui.trickHudRenderer){   // pv raceHud: the HUD text path (web/trick-hud.js) at descriptors 0x4A / 0x4B, A,R,G,B colours 0x4C88C8 / 0x4C88A8 / 0x4C8888
   const argb=n>0?[1,0.7828530073165894,0.06140600144863129,0.04146299883723259]:n>=-5000?[1,1,0.7977359890937805,0]:[1,0.32487601041793823,1,0.35280099511146545],list=[];
   ui.trickHud.text(list,'HUDFONT',0x4A,'OPPONENT',{colour:argb});ui.trickHud.text(list,'HUDFONT',0x4B,(n>=0?'+':'')+n,{colour:argb});ui.trickHudRenderer.render(ctx,list);return;}
  this.ui.text(ctx,'OPPONENT',20,62,12,col,'HUDFONT'); // glyph box matched to the PS2 frame (rnb start sample 600)
  this.ui.text(ctx,(n>=0?'+':'')+n,20,74,12,col,'HUDFONT');
 }
 // "finishov" freestyle overlay (ctor 21CF60, vtable 0x473D28, setup 1E8200): after the banner (rider +0x470 finish timer
 // >= 3 s on the PS2 pipe-finishov captures) a panel scales in (~0.25 s) and its text fades in: the run label by round
 // (GMM+0 1/2/3: '1st run' / '2nd run' / 'Final run'), 'Nth place' = 0x536730[player]+1 clamped to 6 (238B70 ranks the
 // heat scores 0x536640 descending: the player's run and the five posted scores of this round) and 'pts' (kT_OVRCMNPointstotal).
 finishPanel(c,ev,score,t){
  let posted;const rival=ev.mode===MODE.RIVAL_POINTS;const ui=this.ui;
  if(rival){/* Rival Points (PS2 nav/bc/out-jam-finish/sample04620.png '2nd place  1210 pts', no run label): ranked against the rival's
   latched finish score, or its 0x122E50 estimate when it is still riding (23BDB8) */const o=ui.cb.opponent?.(true);posted=[o?(o.finished?o.finishScore:Career.opponentEstimate(o.score,o.remaining,o.origin)):0];}
  else posted=[...(ev.opponent?[ev.opponent]:[]),...(ev.ai||[])].map(a=>ev.round===3?a.scores[2]:a.scores[ev.round-1]??0); // slope style: slot 1 = the opponent
  const place=Math.min(6,rankEntries([score,...posted],false)[0]+1);
  if(pv('finishLui')&&this.finishLui(c,ev,score,t,place,rival))return;
  const k=Math.min(1,Math.max(0,t/.25)),alpha=Math.min(1,Math.max(0,(t-.25)/.2));if(k<=0)return;
  const cx=318,cy=Y(367),w=500*k,h=Y(150)*k,x=cx-w/2,y=cy-h/2,n=10*k;
  c.save();c.beginPath();c.moveTo(x+n,y);c.lineTo(x+w-n,y);c.lineTo(x+w,y+n);c.lineTo(x+w,y+h-n);c.lineTo(x+w-n,y+h);c.lineTo(x+n,y+h);c.lineTo(x,y+h-n);c.lineTo(x,y+n);c.closePath();
  c.fillStyle='rgba(59,120,160,.95)';c.fill();c.strokeStyle='#72aacb';c.lineWidth=2;c.stroke();c.restore();
  if(alpha<=0)return;c.save();c.globalAlpha=alpha;
  if(!rival)ui.text(c,this.t([0x091d8b7e,0x09038b7e,0x05b544fe][ev.round-1],['1st run','2nd run','Final run'][ev.round-1]),320,Y(318),20,'#e8eef0','FEFONT','center');
  ui.text(c,this.t([0x0d5f28e4,0x0d5f2904,0x0d5f2ac4,0x0d5f2be8,0x0d5f2ce8,0x0d5f2de8][place-1],`${place}th place`),170,Y(356),21,'#e8eef0');
  ui.text(c,this.t(0x06d7089c,'%d pts').replace('%d',String(score)),362,Y(361),14,'#e8eef0');
  c.restore();
 }

 // pv finishLui: the panel is OV.LUI finishov, set up as 0x1E8200 does (tools/export_audio_menus.py -> UI/audio-menus.json):
 // - the run label by round: GMM+0 1 '1st run', 2 '2nd run', 3 'Final run' ('3rd run' is never shown); none for event kind 6
 //   (points challenges: rival points, peak jams);
 // - 'place%d' for the player's place 1..5 (0x536730[player]), 'place6' for 6th and below; 'pointstotal' kT_OVRCMNPointstotal of the run;
 // - the medal sprite of the event's medal record (obj 0xD +0x14: 0 platinum, 1 gold, 2 silver, 3 bronze) in the final round or a
 //   Rival Points run; the record is written by the career award 0x154EE8 only (a Single Event shows none);
 // - 'new_record' when the run's time / score rank (obj 8 +0x18 / +0x1C) >= 0; 'Menu0000' hidden (+0x90 |= 8).
 // The timeline plays from frame 1 (the 3D Ov shapes grow, frames 1..20), the texts come in from 20, frame 45 holds (label
 // 004d81c3). The whole panel is hidden for multiplayer, TIME'S UP (not drawn here), points challenges 9..11 and round 1 with
 // GMM+0x70 (next round) 3.
 finishLui(c,ev,score,t,place,rival){
  const ui=this.ui,am=ui.audioMenus,lui=am?.screenLui?.('finishov');if(!lui||!am.images['OV_1-1'])return false;
  if(ev.mode>=9&&ev.mode<=11)return true;
  // round 1 with GMM+0x70 (the next round, set by the round decision 0x239230 at the finish) 3: the player went straight to the
  // final (Career.freestyleResult: top 3 of the run against the field's two-heat totals)
  if(ev.career&&(ev.round??3)===1&&!rival){const field=ev.opponent?[ev.opponent,...(ev.ai||[])]:(ev.ai||[]);if(rankEntries([score,...field.map(a=>(a.scores?.[0]||0)+(a.scores?.[1]||0))],false)[0]<3)return true;}
  lui.shapeScale=true;   // the 3D Ov shapes grow by their scale props (9 / 10), as the Session popup's (web/ctm-pda.js)
  // frame 1 two ticks after the 3 s mark: the overlay is created then entered (PS2 pipe-finishov2 4589..4614 fit best at -2, +-1)
  const frame=Math.min(45,t*60-2),events=[];if(frame<1)return true;for(const e of lui.screen.events)if(e.frame<=frame&&e.frame<=45)events.push({ev:e,start:e.frame});
  const round=ev.round??3,run=rival||ev.mode>=6?null:round===2?'secondrun':round===3?'finalrun':'firstrun';
  let medal=MEDAL.NONE;if(ev.career&&(round===3||ev.mode===MODE.RIVAL_POINTS)){medal=placementMedal(ev.mode,place-1);
   if(medal!==MEDAL.NONE){const pt=platinumThreshold(this.career.rules,ev.mode,ev.course);if(pt&&(pt.ticks==null&&score>=pt.score))medal=MEDAL.PLATINUM;}}
  const medalName=['platinum','gold','silver','bronze'][medal]??null;
  const slot=recordSlot(this.career.rules,ev.mode,ev.course),record=slot<26&&this.career.records(slot,isTimed(ev.mode)).findIndex(r=>score>r.value)>=0;
  const pts=(am.data?.strings?.kT_OVRCMNPointstotal??'%d pts').replace('%d',String(score));
  const override=e=>{const n=e.label;
   if(n==='Menu0000')return {hidden:true};
   if(n==='firstrun'||n==='secondrun'||n==='thirdrun'||n==='finalrun')return n===run?null:{hidden:true};
   if(/^place[1-6]$/.test(n))return n==='place'+Math.min(6,place)?null:{hidden:true};
   if(n==='gold'||n==='silver'||n==='bronze'||n==='platinum')return n===medalName?null:{hidden:true};
   if(n==='new_record')return record?null:{hidden:true};
   if(n==='pointstotal')return {text:pts};
   return null;};
  c.save();c.scale(1,SY_LUI);lui.draw(c,events,frame,override);c.restore();
  return true;
 }

 // ---- drawing ----------------------------------------------------------------------------------------
 draw(c,b){
  const s=this.ui.screen;
  if(!this.career){this.ui.text(c,'Career tables missing: python3 tools/export_career.py',320,220,15,'#ffffff','FEFONT','center');return;}
  if(this.messages.owns(s))return this.messages.draw(c,b);
  if(this.ui.bigChallenges?.owns(s))return this.ui.bigChallenges.draw(c,b);
  if(s==='ctm-session'||s==='ctm-sessconfirm')return this.drawSession(c,b,s);
  if(['ctm-mcomm','ctm-peaks','ctm-goals','ctm-events','ctm-confirm','ctm-pause','ctm-giveup','ctm-restart','ctm-quit','ctm-saveprompt','ctm-enterlodge','ctm-gopeak','ctm-quitsave'].includes(s))return this.drawMcomm(c,b,s);
  if(['ctm-lodge','ctm-attributes','ctm-saved'].includes(s)){this.memoCursor();this.drawLodge(c,b,s);if(pv('buyAttribs'))this.buyAttribs.drawFlash(c);this.lodgeFlash.draw(c);return;}   // pv buyAttribs: the TransitionOut flash between the lodge and Buy Attributes; pv lodgeFlash: the other lodge screens'
  if(this.lodge.owns(s)){this.memoCursor();const r=this.lodge.draw(c,b);this.lodgeFlash.draw(c);return r;}
  if(s==='ctm-objectives')return this.drawObjectivesOpen(c);
  if(s==='ctm-results')return this.drawResults(c);
  if(s==='ctm-award')return this.drawAward(c);
  if(s==='ctm-records')return this.drawRecords(c);
 }
 help(c,text,buttons=[['cross','Select'],['triangle','Previous']]){
  c.fillStyle='#2c6590';c.fillRect(0,Y(378),640,Y(62));
  const cell={cross:[55,122],triangle:[10,122],square:[33,122],circle:[78,122]};
  const capX=Math.min(Infinity,...buttons.map(([button],i)=>glyphKeyRect(this.ui,'OV_1-2',...cell[button],24,24,457,Y(388)+i*Y(17),16,16)?.x??Infinity));/* keyboard: the help text stops short of a wide key cap (web/input-glyphs.js) */
  this.ui.wrap(text,Math.min(400,capX-52),14).slice(0,3).forEach((line,i)=>this.ui.text(c,line,44,Y(396)+i*16,14,'#0c1a26'));
  buttons.forEach(([button,label],i)=>{const [u,v]=cell[button];this.ui.sprite('OV_1-2',u,v,24,24,457,Y(388)+i*Y(17),16,16);this.ui.text(c,label,476,Y(388)+i*Y(17),15,'#d9e6ee');});
 }
 // Session (overlay 0x20, page with 'SessionPoint%d' items; the SessionViewPoints map is not drawn) and its confirm.
 // pv sessionMap: the Session menu opens on the point nearest the rider (0x2087F0: 0x26B680 - 1), else the first
 sessionFocus(){const m=this.ui.cb.freeRideSessionMap?.(),n=this.sessionItems().length;if(!m||!n)return 0;return Math.max(0,nearestSessionPoint(m.points.slice(0,n),m.rider)-1);}
 // pv sessionMap: OV.LUI 38session (web/session-map.js): the rows, the location's map picture, its points and the rider
 drawSessionMap(c,b,s){
  const ui=this.ui,am=ui.audioMenus,lui=am?.screenLui?.('38session'),m=ui.cb.freeRideSessionMap?.(),entry=m&&SESSION_MAP[m.course];
  if(!lui||!entry||!am.images['OV_1-1'])return false;
  lui.shapeScale=true;   // the popup's 3D Ov shapes are authored at 56 % (as the MCOMM icons: web/ctm-pda.js)
  const items=this.sessionItems(),n=items.length,confirm=s==='ctm-sessconfirm',focus=confirm?(this.sessionPoint??0):ui.index;
  const pic='MAPGFX_'+entry[0];if(!am.images[pic]){const im=this.picture(pic);if(im)am.images[pic]=im;}
  const now=performance.now()*60/1000;if(!this.sessionDrawnAt||now-this.sessionDrawnAt>15)this.sessionOpenAt=now;this.sessionDrawnAt=now;
  const frame=now-this.sessionOpenAt;if(this.sessionFocusAt?.[0]!==focus)this.sessionFocusAt=[focus,frame];
  const events=[];for(const ev of lui.screen.events){if(ev.frame<=48&&ev.frame<=frame)events.push({ev,start:ev.frame});else if(!confirm&&ev.frame===50+10*focus)events.push({ev,start:this.sessionFocusAt[1]});}   // under the question the rows lose the focus (PS2 r3-session-confirm)
  const rows=Array.from({length:8},(_,k)=>(0x00e0c750+k).toString(16).padStart(8,'0'));
  const override=e=>{const k=rows.indexOf(e.name);if(k>=0)return k<n?{text:items[k]}:{hidden:true};
   switch(e.name){
    case '053856f3':return am.images[pic]?{sprite:{page:pic,sx:0,sy:0,sw:256,sh:256}}:{hidden:true};   // MapPic
    case '0c35a534':return {text:this.t('kT_OVRHELPMAP','Session areas or restart current run.')};
    case '006bfdc0':return confirm?{alpha:255}:{hidden:true};   // ConfirmPopup (0x208F88)
    case '0ed232d4':return {text:this.t('kT_OVRCMNSessArea','Session this area?')};
    case '05fb4223':case '0f5fb48f':{const yes=e.name==='05fb4223',on=(yes?0:1)===ui.index,v=on?255:0;return {text:yes?this.t('kT_CMNYes','Yes'):this.t('kT_CMNNo','No'),props:{14:v,15:v,16:v}};}   // ConfirmMenu: focused white
    case '0b422288':return ui.index===0?null:{hidden:true};case '0fb48bc8':return ui.index===1?null:{hidden:true};
   }return null;};
  this.mcommFrame(c,b);
  c.save();c.scale(1,SY_LUI);
  lui.draw(c,events,frame,override,l=>l<11);
  // 0x209970 / 0x2096A8 / 0x209E78: the points (focused: its highlight) and the rider, on the map (layers 11..13)
  const atlas=am.images['OV_1-1'],spr=am.data?.sprites?.OV||{},put=(name,[x,y],size)=>{const q=spr[name];if(q)c.drawImage(atlas,q.sx,q.sy,q.sw,q.sh,x,y,size,size);};
  for(let k=0;k<n;k++){const p=m.points[k];if(p&&k!==focus)put('dot_visited',mapMarker(entry[2],p[0],p[1]),DOT);}
  if(m.points[focus]&&focus<n)put('indicator',mapMarker(entry[2],m.points[focus][0],m.points[focus][1]),DOT);
  put('location',mapMarker(entry[2],m.rider[0],m.rider[1]),PLAYER);
  lui.draw(c,events,frame,override,l=>l>=11);
  c.restore();return true;
 }
 drawSession(c,b,s){
  if(pv('sessionMap')&&this.drawSessionMap(c,b,s))return;
  const ui=this.ui,items=this.sessionItems();
  this.mcommFrame(c,b,this.t('kT_MAPSession','Session'));
  ui.text(c,this.t(0x0e974903,'Choose a session point.'),106,Y(92),17,'#e8eef2');
  const sel=s==='ctm-session'?ui.index:(this.sessionPoint??0);
  items.forEach((t,i)=>{const y=Y(170)+Y(25)*i;if(sel===i){c.fillStyle='#c4540a';c.fillRect(0,y,240,Y(24));}ui.text(c,t,236,y+2,18,sel===i?'#f4f6f2':'#0f2533','FEFONT','right');});
  if(s==='ctm-session'){this.help(c,this.t('kT_OVRHELPMAP','Session areas or restart current run.'));return;}
  c.fillStyle='rgba(58,108,148,.96)';c.fillRect(110,Y(128),420,Y(200));c.strokeStyle='#8fb6cf';c.lineWidth=3;c.strokeRect(118,Y(136),404,Y(184));
  ui.text(c,this.t('kT_OVRCMNSessArea','Session this area?'),320,Y(190)-8,18,'#0c1a26','FEFONT','center');
  ui.items().forEach((t,i)=>{if(ui.index===i)this.ui.sprite('OV_1-2',55,122,24,24,272,Y(245)+i*Y(24),16,16);ui.text(c,t,296,Y(245)+i*Y(24),18,ui.index===i?'#eef4f7':'#0c1a26');});
 }
 mcommFrame(c,b,title){
  if(this.pda?.ready){/* the PDA frame opens (its intro, 0x20A778) when a PDA screen is shown after a non-PDA one */const now=performance.now();if(!this.pdaDrawnAt||now-this.pdaDrawnAt>250)this.pda.opening();this.pdaDrawnAt=now;this.pda.frame(c,b);}
  else{const g=b.createLinearGradient(0,0,640,448);g.addColorStop(0,'#5d8aa9');g.addColorStop(1,'#8fb3ca');b.fillStyle=g;b.fillRect(0,0,640,448);
  b.fillStyle='#c9d3da';b.fillRect(0,0,640,Y(18));b.fillRect(0,Y(452),640,Y(28));
  this.ui.sprite('OV_1-7',0,0,256,80,390,0,250,Y(82));}
  if(title){this.ui.text(c,title,106,Y(52),24,'#eef4f7');c.strokeStyle='#dbe7ee';c.lineWidth=2;c.beginPath();c.moveTo(0,Y(78));c.lineTo(402,Y(78));c.stroke();}
 }
 drawMcomm(c,b,s){
  const ui=this.ui;
  if(s==='ctm-mcomm'||s==='ctm-pause'){
   this.mcommFrame(c,b);if(this.hideRows)return;   // under a Yes/No popup the PDA shows no menu (PS2 ctm/48-quit, caps quit-ctm)
   const help=s==='ctm-mcomm'?[this.t('kT_OVRHELPGetBoarding'),this.t('kT_MAPHELPTransport'),this.t('kT_OVRHELPMAP'),this.t('kT_OVRHELPMessages'),this.t('kT_OVRHELPChangeMusic'),this.t('kT_OVRHELPOptions'),this.t('kT_MAPHELPQuitGame')][ui.index]
    :this.singlePause()?[this.t('kT_OVRHELPGetBoarding'),this.t('kT_OVRHELPRestartComp'),this.t('kT_OVRHELPChangeMusic'),this.t('kT_OVRHELPOptions'),this.t('kT_MAPHELPQuitGame')][ui.index]
    :[this.t('kT_OVRHELPGetBoarding'),this.t('kT_OVRHELPRestartComp'),this.t('kT_OVRHELPMessages'),this.t('kT_OVRHELPChangeMusic'),this.t('kT_OVRHELPOptions'),this.t('kT_OVRHELPQuitComp')][ui.index];
   const note=s==='ctm-mcomm'&&(this.ui.cb.freeRide?(this.freeRide?[]:[2]):[0,2]).concat(this.messages.ready?[]:[3]).concat(ui.audioMenus?.ready?[]:[4]).includes(ui.index)?' (Free ride world not in this port.)':'';
   // 31paus_freeride: the rows' icons by item id (table 0x441C30, 0x1F8448), the focused row white with its icon's focus state
   if(this.pda?.ready){const icons=s==='ctm-mcomm'?[ICON.return,ICON.map,ICON.session,ICON.messages,ICON.audio,ICON.options,ICON.quit]:this.singlePause()?[ICON.return,ICON.restart,ICON.audio,ICON.options,ICON.quit]:[ICON.return,ICON.restart,ICON.messages,ICON.audio,ICON.options,ICON.quit];
    this.pda.menu(c,ui.items().map((label,i)=>({label,icon:icons[i],disabled:this.disabled(s,i)})),ui.index,(help||'')+note);return;}
   b.fillStyle='#f2f5f6';b.fillRect(112,Y(92),62,Y(282));
   ui.items().forEach((t,i)=>ui.text(c,t,200,Y(98)+i*Y(40),21,ui.index===i?'#f4f6f2':this.disabled(s,i)?'#51708a':'#0f2533'));
   this.help(c,(help||'')+note);return;
  }
  if(['ctm-confirm','ctm-giveup','ctm-restart','ctm-quit','ctm-saveprompt','ctm-enterlodge','ctm-gopeak','ctm-quitsave'].includes(s)){
   // the screen under the popup keeps its cursor (the list row or menu item that opened it) and hides its rows (PS2 32-transport-confirm-no:
   // title, map and help stay; the lodge menu stays under its Quit prompt, 64-lodge-quit-confirm)
   const under=(fn,index)=>{const at=ui.index,items=ui.items;ui.index=index;this.hideRows=true;try{fn();}finally{ui.index=at;this.hideRows=false;}};
   const listAt=this.selected?Math.max(0,this.list().findIndex(e=>e.course===this.selected.course&&e.mode===this.selected.mode&&e.name===this.selected.name)):0;
   if(s==='ctm-gopeak')under(()=>this.drawMcomm(c,b,'ctm-peaks'),3-(this.goPeak??this.peak));else if(s==='ctm-enterlodge'){/* over the paused world, MCOMM frame */}else if(s==='ctm-saveprompt'||(s==='ctm-quit'&&this.quitFrom==='ctm-lodge'))this.drawLodge(c,b,'ctm-lodge',s==='ctm-saveprompt'?0:7);else if(s==='ctm-quitsave'){if(this.quitFrom==='ctm-results')this.drawResults(c,true);else if(this.quitFrom==='ctm-lodge')this.drawLodge(c,b,'ctm-lodge',7);else this.mcommFrame(c,b);}else if(s==='ctm-confirm')under(()=>this.drawMcomm(c,b,'ctm-events'),listAt);else under(()=>this.drawMcomm(c,b,['ctm-giveup','ctm-restart'].includes(s)||this.quitFrom==='ctm-pause'?'ctm-pause':'ctm-mcomm'),0);
   // over the PDA (MCOMM, Transport, pause): the Yes / No popup 87yndialog; over the lodge / results the port's box
   const overPda=!(s==='ctm-saveprompt'||(this.quitFrom==='ctm-lodge'&&(s==='ctm-quit'||s==='ctm-quitsave'))||(s==='ctm-quitsave'&&this.quitFrom==='ctm-results'));
   if(overPda&&s==='ctm-enterlodge')this.mcommFrame(c,b);   // 98enterlodge sits in the MCOMM frame (PS2 start-lodge s1400)
   if(overPda&&this.pda?.ready&&this.pda.dialog(c,this.promptLines(s).flatMap(t=>t.split('\n')),ui.index))return;
   // pv lodgeLui: over the lodge, the FE popup with its veil, sized to the question (PS2 menus/ctm/64-lodge-quit-confirm; web/fe-screens.js)
   const overLodge=s==='ctm-saveprompt'||(this.quitFrom==='ctm-lodge'&&(s==='ctm-quit'||s==='ctm-quitsave'));
   if(overLodge&&pv('lodgeLui')&&ui.feScreens?.drawLodgePrompt?.(c,{message:this.promptLines(s).join(' '),index:ui.index}))return;
   c.fillStyle='rgba(58,108,148,.96)';c.fillRect(110,Y(128),420,Y(200));c.strokeStyle='#8fb6cf';c.lineWidth=3;c.strokeRect(118,Y(136),404,Y(184));
   const q=this.promptLines(s);
   const lines=q.flatMap(t=>t.split('\n'));lines.forEach((t,i)=>ui.text(c,t,320,Y(190)+i*21-lines.length*8,18,'#0c1a26','FEFONT','center'));
   ui.items().forEach((t,i)=>{if(ui.index===i)this.ui.sprite('OV_1-2',55,122,24,24,272,Y(245)+i*Y(24),16,16);ui.text(c,t,296,Y(245)+i*Y(24),18,ui.index===i?'#eef4f7':'#0c1a26');});
   return;
  }
  // Transport: Select Peak / Peak Goal / Event.
  const title=this.t('kT_TITLETransport','Transport');
  const lm=this.ctmMap?.on?(this.ctmMap.ensure(),this.ctmMap.lui?this.ctmMap:null):null;   // pv transportMap (web/ctm-map.js)
  this.mcommFrame(c,b,lm?null:title);
  const sub={'ctm-peaks':this.t('kT_240SelectPeak','Select Peak'),'ctm-goals':this.t('kT_FESelPeakGoal','Select Peak Goal'),'ctm-events':{race:this.t('kT_FESelRaceEvent'),freestyle:this.t('kT_FESelFSEvent'),freeride:this.t('kT_FESelFREvent')}[this.goal]}[s];
  if(lm)lm.title(c,sub);else (ui.wrap?ui.wrap(sub||'',224,17):[sub]).forEach((l,i)=>ui.text(c,l,236,Y(92)+i*Y(22),17,'#e8eef2','FEFONT','right'));
  const items=ui.items();
  if(!this.hideRows)items.forEach((t,i)=>{
   const y=Y(170)+this.rowY(s,i);
   if(ui.index===i){c.fillStyle='#c4540a';c.fillRect(0,y,240,Y(24));}
   const off=this.disabled(s,i)&&!(s==='ctm-events'&&!this.list()[i]?.locked)&&!(s==='ctm-goals'&&i===3);   // Earnings is not selectable but not greyed (PS2 explore-info s480)
    ui.text(c,t,236,y+2,18,ui.index===i?'#f4f6f2':off&&!(lm&&(s==='ctm-peaks'||(s==='ctm-events'&&this.list()[i]?.locked)))?'#35526a':'#0f2533','FEFONT','right');   // lm: a locked peak or event keeps black text (PS2 menus/ctm/17, 19b, 25, 28)
   // status box: lock (locked peak / event), check (goal complete), medal hex (event medal)
   const bx=244;
    if(s==='ctm-peaks'&&i<3&&!this.me.peaks[2-i]){if(!lm)ui.sprite('FE_1-11',...LOCK,bx,y+3,12,14);}   // lm: the Map LUI's Locks
   else if(s!=='ctm-peaks'||i<3){
    let medal=MEDAL.NONE,done=false;
    if(s==='ctm-goals')done=this.career.goalComplete(this.riderId,this.peak,GOALS[i]);
    if(s==='ctm-events'){const e=this.list()[i];medal=e?.medal??MEDAL.NONE;}
    if(medal!==MEDAL.NONE)ui.sprite('FE_1-11',...MEDAL_CELL[medal],bx-2,y+1,18,17);
    else if(!(s==='ctm-events'&&this.goal==='freeride'))checkBox(c,bx,y+5,12,done);   // the freeride list has no boxes (PS2 menus/ctm/53-freeride-list)
   }
  });
  // right panel: picture (map) or INFO
  const px=262,py=Y(95),pw=348,ph=Y(270);
  const showInfo=this.info||(s==='ctm-goals'&&ui.index===3)||(s==='ctm-peaks'&&ui.index===3);
  if(!lm||showInfo){b.fillStyle='#2e6a95';b.fillRect(px-12,py,pw+24,ph);}
  let label='';
  if(lm&&!showInfo)lm.map(c,s,this.transportMapState(s,ui.index,false));
  else if(!showInfo){
   let pic=null;
   if(s==='ctm-peaks')pic='MAPGFX_map_mtn';
   else if(s==='ctm-goals'){pic='MAPGFX_map_peak'+'ABC'[this.peak-1];label=this.t(`kT_Peak${this.peak}name`,`Peak ${this.peak}`);}
   else{const e=this.list()[ui.index];if(e){const code=e.course!=null?this.data.courses[e.course].code:null;pic='MAPGFX_map_peak'+'ABC'[this.peak-1];label=this.goal==='freeride'?(e.station||this.data.courses[e.course]?.kind==='backcountry'?'Freeride':MODE_LABEL[this.career.standardMode(e.course)]||''):e.station?'Freeride':e.mode>=4?'Rival':MODE_LABEL[e.mode]||'';}}
   const im=pic&&this.picture(pic);if(im)b.drawImage(im,0,0,256,256,px,py+Y(18),pw,ph-Y(24));
   if(label){c.fillStyle='#2e6a95';c.fillRect(px,py,140,Y(20));ui.text(c,label,px+60,py+1,17,'#eef4f7','FEFONT','center');}
  }else{this.drawInfo(c,s,px,py,pw);if(lm)lm.map(c,s,this.transportMapState(s,ui.index,true));}
  const infoBtn=showInfo?this.t('kT_BTNShowMap','Show MAP'):this.t('kT_BTNShowInfo','Show INFO');
  if(!lm&&(s!=='ctm-peaks'||ui.index!==3)){this.ui.sprite('OV_1-2',33,122,24,24,414,Y(345),15,15);ui.text(c,infoBtn,434,Y(345),15,'#eef4f7');}
  this.help(c,this.helpText(s,ui.index),this.disabled(s,ui.index)&&!(s==='ctm-events'&&this.list()[ui.index]?.locked)&&!(lm&&s==='ctm-peaks')?[['triangle','Previous']]:[['cross','Select'],['triangle','Previous']]);   // lm: a locked peak keeps Select (PS2 19b)
 }
 // pv transportMap (web/ctm-map.js): what the Map LUI shows for the focused row. The tab: the peak (goals) or the event type
 // (events); the red routes of 0x206690; the start indicator (the Map state of the focused course / peak run); "You are
 // here" on the peak being ridden; "Show INFO" / "Show MAP" except on All Mountain and Earnings (PS2 menus/ctm 19, 24).
 transportMapState(s,index,info){
  const courses=this.data.courses,code=k=>k!=null?courses[k]?.code??null:null,peak=this.peak;
  const entry=e=>e?(isPeakRun(e.mode)?{key:`peak${peak}-${e.mode>=9?'jam':'race'}`}:{code:code(e.course),station:!!e.station,course:e.course}):null;
  const goal=s==='ctm-goals'?GOALS[index]:this.goal,focused=s==='ctm-events'?entry(this.list()[index]):null;
  const goalLists={};if(s==='ctm-goals')for(const g of ['race','freestyle'])goalLists[g]=this.career.goalEvents(this.riderId,peak,g).map(e=>({code:isPeakRun(e.mode)?null:code(e.course)}));
  const hereCourse=this.freeRide?.course??this.active?.course??courses.findIndex(x=>x.code===this.ui.course?.code);
  let tab='';
  if(s==='ctm-goals')tab=this.t(`kT_Peak${peak}name`,`Peak ${peak}`);
  else if(s==='ctm-events'){const e=this.list()[index];if(e)tab=this.goal==='freeride'?(e.station||courses[e.course]?.kind==='backcountry'?'Freeride':MODE_LABEL[this.career.standardMode(e.course)]||''):e.station?'Freeride':e.mode>=4?'Rival':MODE_LABEL[e.mode]||'';}
  const indicator=focused?(INDICATOR_STATE[focused.key??focused.code]??STATION_INDICATOR[focused.course]??null):null;
  return {index,peak,info,tab,focusPeak:s==='ctm-peaks'&&index<3?3-index:null,herePeak:COURSE_PEAK[hereCourse]??peak,
   red:transportRedPaths(s,{peak,goal,focused,goalLists}),indicator,locked:[2,1,0].map(k=>!this.me.peaks[k]),
   showTab:!(s==='ctm-peaks'&&index===3)&&!(s==='ctm-goals'&&index===3),tabText:info?this.t('kT_BTNShowMap','Show MAP'):this.t('kT_BTNShowInfo','Show INFO')};
 }
 // The question of a Yes/No popup (87yndialog / the MCOMM popup) by screen.
 promptLines(s){return s==='ctm-quitsave'?[this.t(0x0ff92d84,'Save progress before quitting?')]:s==='ctm-giveup'?[this.t('kT_CMNGiveUp','Give Up'),this.t(0x0ebe93e5,'Are you sure?')]:s==='ctm-restart'?[this.t('kT_BTNRestart','Restart'),this.t(0x0ebe93e5,'Are you sure?')]:s==='ctm-gopeak'?[this.t(0x079acb07,'Go to this peak now?')]:s==='ctm-enterlodge'?[this.t(0x0ea7ffb5,'Would you like to enter\\the lodge?').replace(/ *\\+/g,'\n')]:s==='ctm-saveprompt'?[this.t('kT_CMNSaveProgress','Save progress?')]:s==='ctm-quit'?[this.quitFrom==='ctm-lodge'?this.t('kT_OVRCMNQuitTitlePrompt'):this.t('kT_OVRCMNQuitGame','Quit Game')]:this.selected?.station&&!pv('stationFlow')?[this.t(0x0ea7ffb5,'Would you like to enter the lodge?')]:[this.t('kT_MAPTransArea','Transport to this area now?')];}/* pv stationFlow: a station row asks the Transport question too (PS2 peak3/nav out-fr-to-black-station prompt) */
 helpText(s,i){
  if(s==='ctm-peaks')return i===3?'':i<3&&!this.me.peaks[2-i]?this.t(i===0?'kT_MAPHELPLockedPeak3':'kT_MAPHELPLockedPeak2'):this.t('kT_MAPHELPOnPeak');
  if(s==='ctm-goals')return [this.t('kT_CMNHELPGetMdlRaceRival'),this.t('kT_CMNHELPGetMdlFSRival'),this.t('kT_CMNHELPGetMdlChallColl'),this.t('kT_CMNHELPEarnCashEvChalColl')][i];
  const e=this.list()[i];if(!e)return '';
  if(e.station)return this.t('kT_CMNHELPStations');
  // pv transportLists (0x207430): a locked peak Jam formats "%S Jam" (0x4A26A0) into kT_HELPLockCompEvent ("Complete Happiness Jam to
  // unlock."); an open rival or peak event row reads kT_HELPChalAvail "A battle against your rival." (0x471668)
  if(e.locked){const l=e.lock;return typeof l==='string'?this.t(l):format(this.t(l.key),pv('transportLists')&&e.mode>=9?format('%s Jam',l.arg):l.arg);}
  if(pv('transportLists')&&e.mode>=4)return this.t('kT_HELPChalAvail','A battle against your rival.');
  const code=e.course!=null?this.data.courses[e.course].code:null;
  let text=code?(this.t('kT_HELP'+code+'Blah','')||this.t('kT_HELP'+code,'')):'';/* kT_HELPBRA2 collides with kT_HELPCBA2 (Launch Time): Metro-City's own text is ...Blah */
  if(!e.playable)text+=(text?'  ':'')+(e.mode>=6?'(Peak challenges need the whole mountain: not in this port yet.)':'(This location is not ported yet.)');
  return text;
 }
 drawInfo(c,s,px,py,pw){
  const ui=this.ui;
  const row=(label,value,y,box=true,done=false)=>{c.fillStyle='#e2ebf1';c.fillRect(px,y,pw,Y(22));if(box)checkBox(c,px+4,y+4,11,done);ui.text(c,label,px+(box?20:6),y+1,16,'#0c1a26');if(value!=null)ui.text(c,String(value),px+pw-8,y+1,16,'#0c1a26','FEFONT','right');};
  const cu=this.career,id=this.riderId;
  if(s==='ctm-peaks'&&ui.index<3){
   const p=3-ui.index,counts=GOALS.map(g=>g==='earnings'?[cu.goalComplete(id,p,g)?1:0,1]:g==='freeride'?[(cu.collectMedal(id,p)!==MEDAL.NONE)+(cu.challengeMedal(id,p)!==MEDAL.NONE),2]:(l=>[l.filter(e=>e.medal!==MEDAL.NONE).length,l.length])(cu.goalEvents(id,p,g)));
   c.fillStyle='#e2ebf1';c.fillRect(px,py+Y(38),pw,Y(24));ui.text(c,format(this.t(0x095083d3,'Peak 1 Goals').replace('1','%d'),p),px+6,py+Y(40),16,'#0c1a26');
   ['Race','Freestyle','Freeride','Earnings'].forEach((g,i)=>row(g,`${counts[i][0]} / ${counts[i][1]}`,py+Y(78)+i*Y(25),true,counts[i][0]>=counts[i][1]));return;
  }
  if(s==='ctm-peaks'){
   const all=[1,2,3].flatMap(p=>['race','freestyle'].flatMap(g=>this.career.goalEvents(this.riderId,p,g)));
   const golds=all.filter(e=>e.mode<4&&(e.medal===MEDAL.GOLD||e.medal===MEDAL.PLATINUM)).length,rivals=all.filter(e=>e.mode>=4&&e.medal!==MEDAL.NONE).length;
   const bc=[1,2,3].reduce((n,p)=>n+cu.challengesDone(id,p),0),bcAll=[1,2,3].reduce((n,p)=>n+cu.peakChallengeTotal(p),0);
   const got=[1,2,3].reduce((n,p)=>n+cu.peakCollected(id,p),0),gotAll=[1,2,3].reduce((n,p)=>n+cu.peakCollectTotal(p),0);
   const hl=cu.monster(id).medals.reduce((a,b)=>a+b,0);
   row(this.t(0x03574b7e,'All Mountain'),`${mountainPercent({golds,rivals,challenges:bc,challengeTotal:bcAll,collected:got,collectTotal:gotAll,highlights:hl})}%`,py+Y(30),false);
   row(this.t('kT_MAPGoldColon','Gold Medals:'),`${golds} / 14`,py+Y(78),true,golds>=14);row(this.t('kT_MAPRivalChalColon','Rival Challenges:'),`${rivals} / 12`,py+Y(103),true,rivals>=12);
   row(this.t('kT_MAPBigChallenges','Big Challenges:'),`${bc} / ${bcAll}`,py+Y(128),true,bc>=bcAll);row(this.t('kT_MAPCollectibles','Collectibles:'),`${got} / ${gotAll}`,py+Y(153),true,got>=gotAll);
   row(this.t('kT_MAPCareerHighColon','Career Highlights:'),`${hl} / 24`,py+Y(178),true,hl>=24);
   return;
  }
  if(s==='ctm-goals'){
   const g=GOALS[ui.index],p=this.peak;
   if(!this.ctmMap?.lui||!this.ctmMap.on)ui.text(c,this.t(`kT_Peak${p}name`,`Peak ${p}`),px+pw/2,py+1,18,'#eef4f7','FEFONT','center');
   if(g==='earnings'){
    const goal=this.career.rules.earnings_goal[p-1],done=this.me.earned>=goal;
    c.fillStyle='#e2ebf1';c.fillRect(px,py+Y(40),pw,Y(40));ui.text(c,`Peak ${p} Earnings Goal`,px+pw/2,py+Y(42),16,'#0c1a26','FEFONT','center');ui.text(c,`${done?1:0}/1 Complete`,px+pw/2,py+Y(60),12,'#0c1a26','FEFONT','center');
    row(`Earn $${goal.toLocaleString('en-US')}`,null,py+Y(90));ui.text(c,format(this.t('kT_CMNYouEarnedNum',"You've earned: %s"),'$ '+this.me.earned),px+20,py+Y(115),16,'#0c1a26');return;
   }
   if(g==='freeride'){
    const parts=[[cu.challengesDone(id,p),cu.peakChallengeTotal(p),cu.rules.challenge_medals[p-1],'kT_CMNChalCompNum'],[cu.peakCollected(id,p),cu.peakCollectTotal(p),cu.rules.collectible_medals[p-1],'kT_CMNColllectNum']];
    const medals=parts.filter(([n,,t])=>n>=t[3]).length;
    c.fillStyle='#e2ebf1';c.fillRect(px,py+Y(40),pw,Y(32));
    ui.text(c,format(this.t('kT_CMNPeakFRGoals','Peak %d Freeride Goals'),p),px+pw/2,py+Y(41),16,'#0c1a26','FEFONT','center');ui.text(c,format(this.t('kT_CMNNumComplete','%d/%d Complete'),medals,2),px+pw/2,py+Y(58),12,'#0c1a26','FEFONT','center');
    parts.forEach(([n,total,t,key],i)=>{
     const y=py+Y(77)+i*Y(51);c.fillStyle='#e2ebf1';c.fillRect(px,y,pw,Y(45));
     checkBox(c,px+4,y+4,11,n>=t[3]);
     ui.text(c,format(this.t(key),n,total),px+20,y+1,14,'#0c1a26');
     // the next medal: none at platinum, else Platinum / Gold / Silver / Bronze at its threshold (0x45B018 / 0x45AFE8)
     const next=exploreNext(n,t);
     if(next)ui.text(c,format(this.t(next.key),next.at),px+20,y+Y(23),14,'#0c1a26');
    });
    return;
   }
   const list=this.career.goalEvents(this.riderId,p,g),done=list.filter(e=>e.medal!==MEDAL.NONE).length;
   c.fillStyle='#e2ebf1';c.fillRect(px,py+Y(40),pw,Y(40));
   ui.text(c,format(this.t(g==='race'?'kT_CMNPeakNumRaceGoal':'kT_CMNPeakNumFreestyleGoal'),p),px+pw/2,py+Y(42),16,'#0c1a26','FEFONT','center');ui.text(c,`${done}/${list.length} Complete`,px+pw/2,py+Y(60),12,'#0c1a26','FEFONT','center');
   list.forEach((e,i)=>row(e.name,e.medal!==MEDAL.NONE?MEDAL_NAMES[e.medal]:null,py+Y(90)+i*Y(25)));
   return;
  }
  const e=this.list()[ui.index];if(!e)return;
  const course=e.course!=null?this.data.courses[e.course]:null;
  if(this.goal==='freeride'){
   const kind=!course||course.kind==='station'||course.kind==='backcountry'?'Freeride':MODE_LABEL[cu.standardMode(course.index)];
   if(!this.ctmMap?.lui||!this.ctmMap.on)ui.text(c,kind,px+pw/2,py+1,18,'#eef4f7','FEFONT','center');
   row(this.t(0x005a387e,'Run:'),course?course.name:e.name,py+Y(78),false);
   row(this.t('kT_OVRCMNEventColon','Event:'),kind,py+Y(103),false);
   row(this.t('kT_MAPCollectibles','Collectibles:'),course?`${cu.collectCount(id,course.index)} / ${COLLECTIBLE_TOTALS[course.index]}`:'',py+Y(128),false);
   row(this.t('kT_MAPBigChallenges','Big Challenges:'),course?bigChallengeInfo(cu,id,course.index)||'N/A':'N/A',py+Y(153),false); // 207430: 1542E0 / 1542A0, N/A without challenges (web/big-challenges.js)
   return;
  }
  if(!this.ctmMap?.lui||!this.ctmMap.on)ui.text(c,e.station?'Freeride':e.mode>=4?'Rival':MODE_LABEL[e.mode],px+pw/2,py+1,18,'#eef4f7','FEFONT','center');
  row(this.t(0x005a387e,'Run:'),course?course.name:e.name,py+Y(78),false);
  row(this.t('kT_OVRCMNEventColon','Event:'),e.station?'Freeride':e.mode>=4?(e.mode===4||e.mode>=6&&e.mode<=8?'Race':'Jam'):MODE_LABEL[e.mode],py+Y(103),false);
  if(!e.station){
   row(this.t('kT_MAPMedal','Medal:'),e.medal===MEDAL.NONE?this.t('kT_CMNNoMedal','No medal'):MEDAL_NAMES[e.medal],py+Y(128),false);
   const top=this.career.topRecord(e.mode,e.course);
   if(top)row(isTimed(e.mode)?this.t('kT_CMNTopTime','Top time:'):this.t('kT_CMNTopScore','Top score:'),isTimed(e.mode)?raceTime(top.ticks,false):top.value,py+Y(153),false);
  }
 }
 // Yes/No popup over the current screen (PS2 'Transport to this area now?' / 'Buy item?' box).
 dialog(c,lines){
  const ui=this.ui;c.fillStyle='rgba(58,108,148,.96)';c.fillRect(110,Y(128),420,Y(200));c.strokeStyle='#8fb6cf';c.lineWidth=3;c.strokeRect(118,Y(136),404,Y(184));
  const all=lines.flatMap(t=>t.split('\n'));all.forEach((t,i)=>ui.text(c,t,320,Y(190)+i*21-all.length*8,18,'#0c1a26','FEFONT','center'));
  ui.items().forEach((t,i)=>{if(ui.index===i)ui.sprite('OV_1-2',55,122,24,24,272,Y(245)+i*Y(24),16,16);ui.text(c,t,296,Y(245)+i*Y(24),18,ui.index===i?'#eef4f7':'#0c1a26');});
 }
 check(c,x,y){c.fillStyle='#f4f7f8';c.fillRect(x,y,12,12);c.strokeStyle='#1a2a36';c.lineWidth=2;c.beginPath();c.moveTo(x+2,y+6);c.lineTo(x+5,y+10);c.lineTo(x+11,y+1);c.stroke();}
 feFrame(c,b,title){
  const g=b.createLinearGradient(0,0,640,0);g.addColorStop(0,'#e6eef2');g.addColorStop(.55,'#9ec0d6');g.addColorStop(1,'#6f9dbd');b.fillStyle=g;b.fillRect(0,0,640,448);
  b.fillStyle='#dc6a10';b.beginPath();b.moveTo(0,0);b.lineTo(150,0);b.quadraticCurveTo(205,40,150,88);b.lineTo(20,88);b.lineTo(20,440);b.lineTo(0,440);b.fill();
  b.fillStyle='#e8eef1';b.beginPath();b.moveTo(22,20);b.lineTo(140,20);b.quadraticCurveTo(175,45,140,70);b.lineTo(22,70);b.fill();
  this.ui.sprite('FE_1-7',0,0,102,52,531,17,93,47);
  this.ui.text(c,'· '+title,48,Y(40),23,'#1a2a36');
  c.setLineDash([6,4]);c.strokeStyle='#50788f';c.lineWidth=2;c.beginPath();c.moveTo(200,Y(125));c.lineTo(560,Y(125));c.moveTo(520,Y(95));c.lineTo(520,Y(370));c.stroke();c.setLineDash([]);
  b.fillStyle='#2c6590';b.fillRect(0,Y(395),640,Y(52));
 }
 drawLodge(c,b,s,cursor=null){
  const ui=this.ui,r=this.me,peak=this.lodgePeak||1;
  if(s==='ctm-attributes'&&pv('buyAttribs')&&this.buyAttribs.draw(c,b))return;   // FE.LUI 33buyattribs (web/buy-attribs.js)
  if(s==='ctm-attributes'){
   this.feFrame(c,b,this.t(0x09ef3a83,'Buy Attributes').replace('attributes','Attributes'));
   ui.text(c,`${this.t(0x0918c5a5,'You have:')} $ ${r.cash-this.pendingCost()}`,86,Y(95),21,'#1a2a36');ui.text(c,this.t(0x0b80bcd2,'Cost'),548,Y(98),17,'#1a2a36');
   ATTRIBUTES.forEach((name,i)=>{
    const y=Y(146)+i*Y(20),value=r.attributes[i],pending=this.pendingAttr[i],level=(value+5*pending)/5;
    if(ui.index===i){c.fillStyle='#c4540a';c.fillRect(24,y,440,Y(20));}
    ui.text(c,name,205,y,17,ui.index===i?'#f4f6f2':'#1a2a36','FEFONT','right');
    for(let j=0;j<11;j++){c.fillStyle=j<value/5?'#dc6a10':j<level?'#f0a24f':'#3e6f92';c.fillRect(247+j*17,y+2,14,Y(16));c.strokeStyle='#16303f';c.lineWidth=1;c.strokeRect(247+j*17,y+2,14,Y(16));}
    ui.text(c,level.toFixed(1),470,y,16,'#1a2a36');
    const cost=this.career.attributeCost(value+5*pending);ui.text(c,cost==null?'-':`$ ${cost}`,592,y,16,'#1a2a36','FEFONT','right');
   });
   ui.text(c,`${this.t(0x06b21b5c,'Total:')} $ ${this.pendingCost()}`,400,Y(322),19,'#1a2a36');
   ui.text(c,`Rider ranking ${riderRanking(r.attributes.map((v,i)=>v+5*this.pendingAttr[i]))}`,40,Y(360),14,'#1a2a36');
   // FEAMER attribute help: 'ACCELERATION - Increases acceleration from slow speeds.' etc. (TRICKS text not in the locale).
   this.help(c,this.t([0x0f53e0fc,0x0caf5fc7,0x0f4e12f4,0x0af4e17e,0x0ed2ad19,0x0281d283,0][ui.index],'TRICKS'),[['cross',this.t(0x09ef3a83,'Buy attributes')],['triangle','Previous']]);return;
  }
  // pv lodgeLui: the lodge menu is the original 28lodge (web/fe-screens.js drawLodgeMenu): its frame, rows, focus bar, help lines
  if(s==='ctm-lodge'&&pv('lodgeLui')&&ui.feScreens?.drawLodgeMenu?.(c,b,{index:cursor??ui.index,still:cursor!=null,title:this.t(`kT_TITLELodgePeak${peak}`,`Lodge - Peak ${peak}`),disabled:i=>this.disabled('ctm-lodge',i)}))return;
  this.feFrame(c,b,this.t(`kT_TITLELodgePeak${peak}`,`Lodge - Peak ${peak}`));
  if(s==='ctm-saved'){ui.text(c,this.saved?this.t('kT_MEMOVSaveDone','Save complete.'):'Save failed (browser storage unavailable).',320,Y(220),19,'#1a2a36','FEFONT','center');this.help(c,'',[['cross','Continue']]);return;}
  const at=cursor??ui.index,items=cursor==null?ui.items():this.items('ctm-lodge');   // under its Save / Quit popups the lodge menu keeps its cursor
  items.forEach((t,i)=>{const y=Y(128)+i*Y(20);if(at===i){c.fillStyle='#b45410';c.fillRect(205,y,315,Y(20));}ui.text(c,t,508,y+1,17,at===i?'#f4f6f2':this.disabled(s,i)?'#6d8ea4':'#1a2a36','FEFONT','right');});
  if(cursor!=null)return;
  const help=[this.t(0x01ab2e35,'Return to game.'),this.t(0x03d729a2,'Equip gear onto your character.'),this.t(0x0ce8b1e2,'Buy gear for your character.'),this.t(0x05d79240),'','',this.t('kT_HELPCreateSaveNewFile','Save the game.'),this.t(0x048dcc9e)][ui.index]||'';
  this.help(c,help+(this.disabled(s,ui.index)?' (Not in this port yet.)':''),[['cross','Select']]);
 }
 // pv luiResults: the original panel OV_darkblue with the 43final_standings header lines (web/results-lui.js)
 luiPanels(){return pv('luiResults')&&!!this.resultsLui?.ready;}
 panel(c,title,sub){
  if(this.luiPanels()){this.resultsLui.panel(c,title,sub);return;}
  c.fillStyle='rgba(13,58,79,.92)';c.fillRect(50,Y(62),540,Y(350));c.strokeStyle='#6698a9';c.lineWidth=3;c.strokeRect(53,Y(88),534,Y(320));
  this.ui.sprite('OV_1-6',0,226,256,30,330,Y(86),256,Y(58));
  this.ui.text(c,title,96,Y(92),21,'#d1e1e2');this.ui.text(c,sub,96,Y(117),19,'#66a9bb');
 }
 eventTitle(ev){if(isPeakRun(ev.mode))return this.peakRunName(ev.mode);/* PS2 'All Peak Race / Rewards', 'Peak 2 Race / Top 5 Record Times' */if(ev.mode===MODE.RIVAL_TIME||ev.mode===MODE.RIVAL_POINTS)return `${this.data.courses[ev.course].name} - ${ev.mode===MODE.RIVAL_TIME?'Race':'Jam'}`;/* PS2 results 'Happiness - Race' (nav/bc/out-tuck-finish2/sample00600.png) */return `${this.career.eventName(ev.mode,ev.course)} - ${ev.mode>=4?'Rival':MODE_LABEL[ev.mode]}`;}
 // The round card opens over the start-gate idle (overlay 8 "40race_pre", PS2 metro-intro s722..758): nothing for 3 ticks,
 // the title bar grows from the left over 12, holds, its frame at 24, the empty box at 27, the text at 33; Cross is
 // ignored until the card is open (~30 ticks).
 cardTicks(){const now=performance.now();if(!this.cardDrawnAt||now-this.cardDrawnAt>250)this.cardOpenAt=now;this.cardDrawnAt=now;return (now-this.cardOpenAt)*60/1000;}
 drawObjectivesOpen(c){
  this.ui.cutscene?.drawFade?.(c);
  const t=this.cardTicks();if(t>=33)return this.drawObjectives(c);if(t<3)return;
  if(this.luiPanels()){this.resultsLui.panel(c,'','',{openOnly:true});return;}   // OV_darkblue's own open animation
  c.save();
  if(t<27){const w=540*Math.min(1,(t-3)/12);c.beginPath();c.rect(50,Y(62),w,Y(60));c.clip();c.fillStyle='rgba(13,58,79,.92)';c.fillRect(50,Y(62),540,Y(60));this.ui.sprite('OV_1-6',0,226,256,30,330,Y(86),256,Y(58));if(t>=24){c.strokeStyle='#6698a9';c.lineWidth=3;c.strokeRect(53,Y(88),534,Y(32));}}
  else{c.fillStyle='rgba(13,58,79,.92)';c.fillRect(50,Y(62),540,Y(350));c.strokeStyle='#6698a9';c.lineWidth=3;c.strokeRect(53,Y(88),534,Y(320));this.ui.sprite('OV_1-6',0,226,256,30,330,Y(86),256,Y(58));}
  c.restore();
 }
 drawObjectives(c){
  const ui=this.ui,ev=this.career.active.ev;
  if(ev.mode===MODE.RIVAL_TIME||ev.mode===MODE.RIVAL_POINTS){this.drawRivalObjectives(c,ev);return;}
  if(isPeakRun(ev.mode)){this.drawPeakObjectives(c,ev);return;}
  // freestyle heat 1 (PS2 menus/rnbctm/zoe-a.f03400, pipegu/nav2.final): subtitle and score column 'Heat 1' (kT_OVRCMNHeat1)
  const heat1=ev.career&&ev.mode>=1&&ev.mode<=3&&ev.round===1;
  if(!ev.ai&&this.luiPanels()){   // 40race_pre (web/results-lui.js): the same objective text, riders, record and Continue
   const lineup=ui.cb.lineup?.()||[],top=this.career.topRecord(ev.mode,ev.course);
   const riders=[{name:ui.rider?.name||'Sam',human:true},...lineup.map(name=>({name})),...(lineup.length?[]:[{name:'(no computer riders in this build)',note:true}])];
   if(this.resultsLui.raceCard(c,{title:this.eventTitle(ev),sub:ev.career?this.career.roundName(ev).replace(' Round',''):this.t('kT_CMNStateQuickPlay','Single Event'),
    objective:this.career.objectives(ev).map(o=>typeof o==='string'?this.t(o):format(this.t(o.key),o.arg)).join('  '),riders,
    recordLabel:isTimed(ev.mode)?this.t('kT_OVRCMNRecordTime','Record time:'):this.t('kT_OVRCMNRecordScore','Record score:'),record:top?(isTimed(ev.mode)?raceTime(top.ticks,false):String(top.value)):null,
    continueLabel:this.t(0x0f3ab955,'Continue')}))return;
  }
  // pv fsStandings: the CTM freestyle heat card in 41freestyle_pre (web/fs-standings.js, 0x1FBD20): the round's tab, the objective
  // lines, 'Current standings' with Heat 1 / Heat 2 / Total before heat 2, 'Up next' with the heat 1 score, the record score.
  if(ev.ai&&!ev.opponent&&ev.career&&this.fsStandings?.cardReady){
   const posted=ev.ai.map(a=>({name:this.data.characters[a.character]?.first||'',heat1:a.scores[0],heat2:a.scores[1],final:a.scores[2]}));
   const model=cardModel({round:ev.round,posted,human:{name:ui.rider?.name||'Sam',heat1:ev.player?.[0]}}),top=this.career.topRecord(ev.mode,ev.course);
   this.fsStandings.card(c,{title:this.eventTitle(ev),model,objectives:this.career.objectives(ev).map(o=>typeof o==='string'?this.t(o):format(this.t(o.key),o.arg)),
    record:top?{label:this.t('kT_OVRCMNRecordScore','Record score:'),value:String(top.value)}:null,continueLabel:this.t(0x0f3ab955,'Continue'),t:(k,f)=>this.t(k,f)});
   return;
  }
  this.panel(c,this.eventTitle(ev),heat1?this.t('kT_OVRCMNHeat1','Heat 1'):ev.career?this.career.roundName(ev).replace(' Round',''):this.t('kT_CMNStateQuickPlay','Single Event'));
  let ly=Y(160);for(const o of this.career.objectives(ev)){const lines=ui.wrap(typeof o==='string'?this.t(o):format(this.t(o.key),o.arg),470,15);c.fillStyle='#8cc1d0';c.fillRect(84,ly+8,4,4);lines.forEach((l,i)=>ui.text(c,l,96,ly+i*18,15,'#b9d6de'));ly+=lines.length*18+4;}
  if(ev.ai){
   // freestyle_pre: 'Current standings' (top three posted scores) and 'Up next' (the player).
   ui.text(c,this.t(0x05695f83,'Current standings'),84,Y(222),17,'#d1e1e2');
   [[this.t('kT_OVRCMNRanking','Rank'),104],[this.t(0x0b18be53,'Riders'),202],[heat1?this.t('kT_OVRCMNHeat1','Heat 1'):this.t('kT_OVRCMNScore','Score'),262+60]].forEach(([h,x])=>ui.text(c,h,x,Y(244),16,'#d1e1e2'));
   this.career.postedStandings(ev).slice(0,3).forEach((r,i)=>{const y=Y(266)+i*Y(23);ui.text(c,String(i+1),124,y,16,'#71b8c6','FEFONT','center');ui.text(c,this.data.characters[r.character]?.first||'',202,y,16,'#71b8c6');ui.text(c,String(r.score),322,y,16,'#71b8c6');});
   ui.text(c,this.t(0x09d3c594,'Up next'),84,Y(340),16,'#d1e1e2');ui.text(c,ui.rider?.name||'Sam',202,Y(340),16,'#e8883a');ui.text(c,'- - -',322,Y(340),16,'#e8883a');
  }else{
  ui.text(c,this.t(0x0b18be53,'Riders'),150,Y(228),19,'#d1e1e2');
  // Race lineups come from the AI racers (cb.lineup: names in slot order); freestyle from the posted-score riders.
  const lineup=ui.cb.lineup?.()||[];
  [ui.rider?.name||'Sam',...lineup].forEach((n,i)=>ui.text(c,n,262,Y(228)+i*Y(23),17,i===0?'#e8a24a':'#8ec3cf'));
  if(!lineup.length)ui.text(c,'(no computer riders in this build)',262,Y(254),12,'#6c93a0');
  }
  const top=this.career.topRecord(ev.mode,ev.course);
  if(top)ui.text(c,`${isTimed(ev.mode)?this.t('kT_OVRCMNRecordTime','Record time:'):this.t('kT_OVRCMNRecordScore','Record score:')} ${isTimed(ev.mode)?raceTime(top.ticks,false):top.value}`,94,Y(370),17,'#d1e1e2');
  ui.sprite('OV_1-2',55,122,24,24,437,Y(370),16,16);ui.text(c,this.t(0x0f3ab955,'Continue'),460,Y(370),16,'#d1e1e2');
 }
 // Rival card (PS2 local/ps2-capture/nav/bc/out-*-load/final.png, out-mac/ready.png): 'Rival Challenge' / '<course> - Race|Jam',
 // the challenge line and one bullet, no riders list or record (web/rival-mode.js objectives).
 // 1FD190 "68rival_pre" for a peak run: Rival Challenge / Peak N Race|Jam, the challenge by the peak rival (0x145750), the
 // route bullet, the pass/event bullet (kT_HELPBeatPostedTime when the next pass is owned), and the tier's target.
 drawPeakObjectives(c,ev){
  const ui=this.ui,time=isTimeChallenge(ev.mode),setup=this.career.peakSetup(ev),peak=(ev.mode-6)%3+1;
  const rival=this.career.rival(peak,this.career.rider(this.riderId).character),name=this.data.characters?.[rival]?.first??['Moby','Kaori','Allegra','Mac','Zoe','Griff','Elise','Nate','Psymon','Viggo'][rival];
  if(this.luiPanels()){   // 68rival_pre (web/results-lui.js): the same headline, bullets and target
   const passOwned=!!this.career.rider(this.riderId).peaks?.[peak],s=Math.floor(setup.limitTicks/60),target=time?`${String(Math.floor(s/60)).padStart(2,'0')}:${String(s%60).padStart(2,'0')}`:String(setup.target);
   if(this.resultsLui.rivalCard(c,{title:this.t(0x0626ef15,'Rival Challenge'),sub:PEAK_RUNS[ev.mode].name,headline:format(time?this.t(0x00f4ae95,"You've been challenged by %s to a Peak Race!"):this.t(0x080cc2fd,"You've been challenged by %s to a Peak Jam!"),name),
    bullets:[time?this.t(0x0a16a5fd,'Race from the top of Backcountry to the bottom of Metro-City.'):this.t(0x0c285605,'Make your way down the Backcountry and Slopestyle runs.'),
     time?(passOwned||ev.mode===8?this.t(0x02809655,"Beat the rival's time to win the event."):this.t(0x065f6983,"Beat the rival's time to win a peak pass.")):(passOwned||ev.mode===11?this.t(0x080a40c5,"Beat the rival's score to win the event"):this.t(0x00da7c23,"Beat the rival's score to win a peak pass."))],
    target:format(time?this.t(0x0865fdc4,'Time to beat: %s'):this.t(0x07d1a4c4,'Score to beat: %d'),target),cont:this.t(0x0f3ab955,'Continue')}))return;
  }
  this.panel(c,this.t(0x0626ef15,'Rival Challenge'),PEAK_RUNS[ev.mode].name);
  // PS2 layout (local/ps2-capture/peak1/peak1-race-objectives.png): the headline wrapped at the left, two bullets, the
  // target bottom left, Continue bottom right.
  let ly=Y(166);for(const l of ui.wrap(format(time?this.t(0x00f4ae95,"You've been challenged by %s to a Peak Race!"):this.t(0x080cc2fd,"You've been challenged by %s to a Peak Jam!"),name),440,18)){ui.text(c,l,92,ly,18,'#e7f0f1');ly+=Y(22);}
  const passOwned=!!this.career.rider(this.riderId).peaks?.[peak];
  const bullets=[time?this.t(0x0a16a5fd,'Race from the top of Backcountry to the bottom of Metro-City.'):this.t(0x0c285605,'Make your way down the Backcountry and Slopestyle runs.'),
   time?(passOwned||ev.mode===8?this.t(0x02809655,"Beat the rival's time to win the event."):this.t(0x065f6983,"Beat the rival's time to win a peak pass.")):(passOwned||ev.mode===11?this.t(0x080a40c5,"Beat the rival's score to win the event"):this.t(0x00da7c23,"Beat the rival's score to win a peak pass."))];
  for(const b of bullets){const lines=ui.wrap(b,450,15);c.fillStyle='#8cc1d0';c.fillRect(102,ly+6,4,4);lines.forEach((l,i)=>ui.text(c,l,112,ly+i*Y(20),15,'#8cc1d0'));ly+=lines.length*Y(20)+Y(26);}
  const s=Math.floor(setup.limitTicks/60),target=time?`${String(Math.floor(s/60)).padStart(2,'0')}:${String(s%60).padStart(2,'0')}`:String(setup.target);
  ui.text(c,format(time?this.t(0x0865fdc4,'Time to beat: %s'):this.t(0x07d1a4c4,'Score to beat: %d'),target),112,Y(350),16,'#e7f0f1');
  ui.sprite('OV_1-2',55,122,24,24,437,Y(350),16,16);ui.text(c,this.t(0x0f3ab955,'Continue'),460,Y(350),16,'#d1e1e2');
 }
 drawRivalObjectives(c,ev){
  const ui=this.ui,[head,bullet]=this.career.objectives(ev);
  if(this.luiPanels()&&this.resultsLui.rivalCard(c,{title:this.t(0x0626ef15,'Rival Challenge'),sub:`${this.data.courses[ev.course].name} - ${ev.mode===MODE.RIVAL_TIME?'Race':'Jam'}`,   // 68rival_pre (web/results-lui.js)
   headline:format(this.t(head.key),head.arg),bullets:[this.t(bullet)],target:null,cont:this.t(0x0f3ab955,'Continue')}))return;
  this.panel(c,this.t(0x0626ef15,'Rival Challenge'),`${this.data.courses[ev.course].name} - ${ev.mode===MODE.RIVAL_TIME?'Race':'Jam'}`);
  // a headline wider than the box wraps left-aligned and pushes the bullet down (PS2 The Throne: "Face off against Psymon in a
  // Rival" / "Challenge!", local/ps2-capture/peak3/nav/out-era5-load/final.png; "... Elise ..." fits on one line)
  const headText=format(this.t(head.key),head.arg),headLines=ui.wrap(headText,480,16),drop=(headLines.length-1)*24;
  if(headLines.length<2)ui.text(c,headText,340,Y(172),16,'#e7f0f1','FEFONT','center');
  else headLines.forEach((l,i)=>ui.text(c,l,93,Y(172)+i*24,16,'#e7f0f1'));
  const lines=ui.wrap(this.t(bullet),470,13);c.fillStyle='#8cc1d0';c.fillRect(104,Y(220)+drop,4,4);lines.forEach((l,i)=>ui.text(c,l,114,Y(213)+drop+i*18,13,'#8cc1d0'));
  ui.sprite('OV_1-2',55,122,24,24,437,Y(350),16,16);ui.text(c,this.t(0x0f3ab955,'Continue'),460,Y(350),16,'#d1e1e2');
 }
 drawPeakResults(c,noItems){
  const ui=this.ui,r=this.result,o=r.outcome,time=isTimeChallenge(r.mode),target=o.setup?.target??0;
  if(this.luiPanels()&&this.resultsLui.peakResults(c,{title:this.t(0x0626ef15,'Rival Challenge'),sub:this.peakRunName(r.mode),   // 70peakchal_results (web/results-lui.js)
   targetLabel:time?this.t(0x0d871cfe,'Time to beat:'):this.t(0x0c7497de,'Score to beat:'),target:time?raceTime(target,false):String(target),
   yourLabel:time?this.t(0x01a50c3e,'Your time:'):this.t(0x0b7c64c5,'Your score:'),yours:r.rows[0]?.value??'',
   message:!o.fail?this.t(0x0a1458dc,'Congratulations!  You have won this challenge.'):time?this.t(0x07a82a15,"Sorry, you didn't win.  You must beat the posted time to complete the challenge."):this.t(0x0a820465,"Sorry, you didn't win.  You must beat the posted score to complete the challenge."),
   items:ui.items(),index:ui.index,disabled:i=>this.disabled('ctm-results',i),noItems}))return;
  this.panel(c,this.t(0x0626ef15,'Rival Challenge'),this.peakRunName(r.mode));
  ui.text(c,this.t(0x0c7d2d33,'Event Results'),97,Y(166),19,'#d1e1e2');
  const rowText=(label,value,y)=>{ui.text(c,label,329,y,19,'#66a9bb','FEFONT','right');ui.text(c,value,343,y,19,'#66a9bb');};
  rowText(time?this.t(0x0d871cfe,'Time to beat:'):this.t(0x0c7497de,'Score to beat:'),time?raceTime(target,false):String(target),Y(216));
  rowText(time?this.t(0x01a50c3e,'Your time:'):this.t(0x0b7c64c5,'Your score:'),r.rows[0]?.value??'',Y(250));
  // 23B468 / 23C2D8: won, or the posted time / score not beaten; a time up too ("Your time: DNF", PS2 out-p2r-timeup)
  const msg=!o.fail?this.t(0x0a1458dc,'Congratulations!  You have won this challenge.'):time?this.t(0x07a82a15,"Sorry, you didn't win.  You must beat the posted time to complete the challenge."):this.t(0x0a820465,"Sorry, you didn't win.  You must beat the posted score to complete the challenge.");
  ui.wrap(msg,305,15).slice(0,4).forEach((l,i)=>ui.text(c,l,95,Y(321)+i*Y(18),15,'#e7f0f1'));
  if(!noItems)ui.items().forEach((t,i)=>{if(ui.index===i)ui.sprite('OV_1-2',55,122,24,24,424,Y(338)+i*Y(16),14,14);ui.text(c,t,452,Y(339)+i*Y(16),12,ui.index===i?'#dce5e4':this.disabled('ctm-results',i)?'#35606d':'#559aad');});
 }
 drawResults(c,noItems=false){
  if(this.peakResults())return this.drawPeakResults(c,noItems);
  const ui=this.ui,r=this.result,ev=this.career.active.ev,o=r.outcome;
  const sub=!this.active.career?this.t('kT_OVRCMNQuickPlayResults','Single Event Results'):isPeakRun(r.mode)?this.t('kT_OVRFinalResults','Final Results'):r.mode===MODE.RACE?[this.t('kT_OVRQualifierResults'),this.t('kT_OVRSemiFinalResults'),this.t('kT_OVRFinalResults')][o.round-1]:[this.t('kT_OVRCMNQFHeat1Stand'),this.t('kT_OVRCMNQFHeat2Stand'),this.t('kT_OVRFinalResults')][o.round-1];
  const msg=o.message!=null?this.t(o.message):o.medal!==undefined?(o.medal===MEDAL.NONE?this.t('kT_OVRHELPSorryLose',"Sorry, you didn't win."):this.t(['kT_OVRCongratulationsPlatinum','kT_OVRCongratulationsGold','kT_OVRCongratulationsSilver','kT_OVRCongratulationsBronze'][o.medal],'')):'';
  const extra=this.unopposed?' (No computer riders in this run.)':'',timed=r.mode===MODE.RACE||r.mode===MODE.RIVAL_TIME||isTimeChallenge(r.mode);
  // pv fsStandings: a CTM freestyle qualifying heat (rounds 1 and 2) in 42freestyle_standings (web/fs-standings.js, 0x1E5B80):
  // Heat 1 / Heat 2 / Total per rider from the posted scores (ev.ai / ev.opponent scores[0..1]) and the player's (ev.player).
  if(this.fsStandings?.ready&&this.active.career&&o.round<3&&(r.mode===MODE.SLOPESTYLE||r.mode===MODE.HALFPIPE||r.mode===MODE.BIGAIR)&&o.rows?.length===r.rows.length){
   const field=ev.opponent?[ev.opponent,...ev.ai]:ev.ai||[];
   const rows=o.rows.map((x,i)=>({name:r.rows[i].name,human:!!x.human,rank:x.rank,dnf:!!x.human&&this.gaveUp,heat1:x.human?ev.player?.[0]:field[i-1]?.scores?.[0],heat2:x.human?ev.player?.[1]:field[i-1]?.scores?.[1]}));
   const model=standingsModel({round:o.round,rows,qualified:o.round===1&&!!o.advance&&ev.round===3});
   this.fsStandings.draw(c,{title:this.eventTitle(ev),sub,model,items:ui.items(),index:ui.index,disabled:i=>this.disabled('ctm-results',i),noItems,t:(k,f)=>this.t(k,f)});
   return;
  }
  if(this.luiPanels()){   // 43final_standings (web/results-lui.js): the same rows, message, menu and focus
   const rows=[...r.rows].sort((a,b)=>a.rank-b.rank).map(row=>({rank:row.rank+1,name:row.name,value:row.value,human:!!row.human})),hr=rows.findIndex(x=>x.human);
   const medal=hr>=0&&o.medal!=null&&o.medal!==MEDAL.NONE&&MEDAL_CELL[o.medal]?{row:hr,kind:o.medal}:null;
   this.resultsLui.results(c,{title:this.eventTitle(ev),sub,heads:[this.t('kT_OVRCMNRanking','Rank'),this.t(0x0b18be53,'Riders'),timed?this.t('kT_OVRCMNTime','Time'):this.t('kT_OVRCMNScore','Score')],rows,medal,message:msg+extra,items:ui.items(),index:ui.index,disabled:i=>this.disabled('ctm-results',i),noItems});
   return;
  }
  this.panel(c,this.eventTitle(ev),sub);
  ui.text(c,this.t('kT_OVRCMNRanking','Rank'),128,Y(160),17,'#d1e1e2');ui.text(c,this.t(0x0b18be53,'Riders'),204,Y(160),17,'#d1e1e2');
  ui.text(c,r.mode===MODE.RACE||r.mode===MODE.RIVAL_TIME||isTimeChallenge(r.mode)?this.t('kT_OVRCMNTime','Time'):this.t('kT_OVRCMNScore','Score'),420,Y(160),17,'#d1e1e2');
  [...r.rows].sort((a,b)=>a.rank-b.rank).forEach((row,i)=>{const y=Y(184)+i*Y(20),col=row.human?'#e8883a':'#71b8c6';if(row.human&&o.medal!=null&&o.medal!==MEDAL.NONE&&MEDAL_CELL[o.medal])ui.sprite('FE_1-11',...MEDAL_CELL[o.medal],104,y-2,22,19);/* the medal hex left of the winner's rank (PS2 after-final) */ui.text(c,String(row.rank+1),152,y,16,col,'FEFONT','center');ui.text(c,row.name,204,y,16,col);ui.text(c,row.value,420,y,16,col);});
  // a completed event: kT_OVRCongratulations<medal> 'Congratulations!  You've received a Gold medal.' (PS2 ctm/caps after-final) or the loss text
  ui.wrap(msg+extra,360,15).slice(0,4).forEach((l,i)=>ui.text(c,l,62,Y(328)+i*18,15,'#e7f0f1'));
  if(!noItems)ui.items().forEach((t,i)=>{if(ui.index===i)ui.sprite('OV_1-2',55,122,24,24,440,Y(318)+i*Y(15),14,14);ui.text(c,t,458,Y(318)+i*Y(15),12,ui.index===i?'#dce5e4':this.disabled('ctm-results',i)?'#35606d':'#559aad');});
 }
 // The reward list of the result (0x1FF7B8 line by line): "Cash: $n" (kT_CMNCashEarnedNum), then each granted award in id order:
 // its title (0x441C68; 17..19 'Gold medal earned', platinum 'Platinum medal earned') and, indented ("      %s"), the peak pass
 // it opened and its items by category (0x441CE8 formats '<category> [name]', gear 'Accessory' / 'Board graphic').
 rewardLines(){
  if(pv('awardCascade'))return this.rewardLinesRecord();
  const o=this.result?.outcome;if(!o)return [];const lines=[];
  // pv cashGap: 0x1FF7B8 prints kT_CMNCashEarnedNum 'Cash: %S' with 0x198AF0(cash), which puts '$' and a space before the
  // digits ("$ 10,000"; the other formats put " $" after them): PS2 race-f / race-f95 final.png draw '$' at 171..180 and '1' at
  // 185..188, a space (7 font units, 3.8 px at the row's 55 %) the browser's "$10,000" left out.
  if(o.cash)lines.push({text:format(this.t('kT_CMNCashEarnedNum','Cash: %S'),pv('cashGap')?money(o.cash):'$'+o.cash.toLocaleString('en-US')),indent:0});
  const groups=new Map();for(const g of o.awards||[]){if(!groups.has(g.award))groups.set(g.award,[]);if(g.category)groups.get(g.award).push(g);}
  const passes=new Map(),loose=[];
  for(const p of o.opened||[]){const goal=[...groups.keys()].filter(a=>a>=5&&a<=16&&(a-5)%3===p-2).sort((a,b)=>a-b)[0];if(goal==null)loose.push(p);else passes.set(goal,[...(passes.get(goal)||[]),p]);}
  const item=g=>{const [,key]=REWARD_ITEM[g.category]||[];const name=g.category==='gear'?g.name:this.career.rewardItems(g.category)[g.index]?.name;
   const fmt=g.category==='gear'?this.t(g.award>=20?'kT_REWBoardGraphic':'kT_REWAccessory',g.award>=20?'Board graphic [%S]':'Accessory [%S]'):this.t(key,'%S');
   return name?format(fmt,name).replace(/\[/g,'(').replace(/\]/g,')'):null;};   // FEFONT draws the brackets as parentheses (PS2 frame)
  for(const award of [...groups.keys()].sort((a,b)=>a-b)){
   const title=award>=17&&award<=19&&o.medal===MEDAL.PLATINUM?this.t('kT_REWPlatEarned','Platinum medal earned'):this.t(REWARD_TITLE[award]||'','');
   if(title)lines.push({text:title,indent:0});
   for(const p of passes.get(award)||[])lines.push({text:this.t(`kT_REWPeak${p}Pass`,`Peak ${p} pass`),indent:1});
   for(const g of groups.get(award).slice().sort((a,b)=>(REWARD_ITEM[a.category]?.[0]??9)-(REWARD_ITEM[b.category]?.[0]??9))){const t=item(g);if(t)lines.push({text:t,indent:1});}
  }
  for(const p of loose)lines.push({text:this.t(`kT_REWPeak${p}Pass`,`Peak ${p} pass`),indent:1});
  return lines;
 }
 // pv awardCascade: the list is the reward record 0x4C3EF0 (web/career.js recordReward), as 0x1FF7B8 walks it: the cash line (the
 // interface's +0x10, set by 1591E8) first; then awards 0..31, each with anything recorded: its title (0x441C68; 17..19
 // 'Platinum medal earned' when the medal +0x14 is 0), then categories 0..9 in order: for 0..7 the award's count of the category's
 // items, taken in item order from the category's set with one running cursor per category across the awards (2 = the pass, item
 // char*3 + peak - 1), 8 the award's cheat character, 9 the record's gear item (Accessory / Board graphic by its 0x800 flag).
 rewardLinesRecord(){
  const c=this.career,rec=c?.rewardRecord,lines=[];if(!rec)return lines;
  if(rec.cash)lines.push({text:format(this.t('kT_CMNCashEarnedNum','Cash: %S'),pv('cashGap')?money(rec.cash):'$'+rec.cash.toLocaleString('en-US')),indent:0});
  const CAT={0:'trophy',1:'medal',3:'poster',4:'trading_card',5:'art',6:'video',7:'toy'};
  const named=(key,fallback,name)=>format(this.t(key,fallback),name).replace(/\[/g,'(').replace(/\]/g,')');   // FEFONT draws the brackets as parentheses
  const sorted=rec.items.map(set=>[...set].sort((a,b)=>a-b)),cursor=Array(8).fill(0);
  for(let award=0;award<32;award++){
   const counts=rec.counts.get(award),cheat=rec.cheats.get(award),gear=rec.gear.has(award);
   if(!counts&&cheat==null&&!gear)continue;
   const title=award>=17&&award<=19&&rec.medal===MEDAL.PLATINUM?this.t('kT_REWPlatEarned','Platinum medal earned'):this.t(REWARD_TITLE[award]||'','');
   if(title)lines.push({text:title,indent:0});
   for(let cat=0;cat<8;cat++)for(let k=0;k<(counts?.[cat]||0);k++){
    const item=sorted[cat][cursor[cat]++];if(item==null)continue;
    if(cat===2){const p=item%3+1;lines.push({text:this.t(`kT_REWPeak${p}Pass`,`Peak ${p} pass`),indent:1});continue;}
    const category=CAT[cat],name=c.rewardItems(category)[item]?.name,[,key]=REWARD_ITEM[category]||[];
    if(name&&key)lines.push({text:named(key,'%S',name),indent:1});
   }
   if(cheat!=null){const name=c.rewardItems('cheat_character').find(x=>x.character===cheat)?.name;if(name)lines.push({text:named(REWARD_ITEM.cheat_character[1],'%S',name),indent:1});}
   if(gear){const e=c.gear(this.riderId)?.by.get(rec.gearItem);if(e)lines.push({text:named(e.flags&0x800?'kT_REWBoardGraphic':'kT_REWAccessory',e.flags&0x800?'Board graphic [%S]':'Accessory [%S]',e.name),indent:1});}
  }
  return lines;
 }
 // Overlay 0x10 (OV.LUI 62reward_list): '<Event> - <Kind>' / 'Rewards', kT_OVRCMNCongratUser, the list (focused row white, the
 // others teal; Up/Down scroll it, the down arrow while rows are below), Continue (PS2 ctm/caps race-f, race-f95).
 drawAward(c){
  const ui=this.ui,ev=this.career.active.ev,lines=this.rewardLines(),at=Math.max(0,Math.min(ui.index,lines.length-1));
  let top=this.rewardTop||0;if(at<top)top=at;if(at>=top+REWARD_ROWS)top=at-REWARD_ROWS+1;top=Math.max(0,Math.min(top,Math.max(0,lines.length-REWARD_ROWS)));
  if(top!==this.rewardTop){this.rewardTop=top;queueMicrotask(()=>ui.sync());}
  if(this.luiPanels()&&this.resultsLui.awards(c,{title:this.eventTitle(ev),sub:this.t('kT_TITLERewards','Rewards'),intro:this.t('kT_OVRCMNCongratUser',"Congratulations!  You've been awarded:"),lines,top,at,cont:this.t(0x0f3ab955,'Continue')}))return;   // 62reward_list (web/results-lui.js)
  this.panel(c,this.eventTitle(ev),this.t('kT_TITLERewards','Rewards'));
  ui.text(c,this.t('kT_OVRCMNCongratUser',"Congratulations!  You've been awarded:"),100,Y(158),16,'#e7f0f1');
  lines.slice(top,top+REWARD_ROWS).forEach((l,k)=>ui.text(c,l.text,l.indent?143:120,Y(180)+k*Y(REWARD_PITCH),15,top+k===at?'#e7f0f1':'#71b8c6'));
  if(lines.length>top+REWARD_ROWS){c.fillStyle='#e7f0f1';c.beginPath();c.moveTo(74,Y(387));c.lineTo(94,Y(387));c.lineTo(84,Y(397));c.closePath();c.fill();}
  ui.sprite('OV_1-2',55,122,24,24,452,Y(392),16,16);ui.text(c,this.t(0x0f3ab955,'Continue'),472,Y(392),15,'#d1e1e2');
 }
 drawRecords(c){
  const ui=this.ui,ev=this.career.active.ev,timed=isTimed(ev.mode),slot=recordSlot(this.career.rules,ev.mode,ev.course);
  if(this.luiPanels()&&this.resultsLui.records(c,{title:isPeakRun(ev.mode)?this.peakRunName(ev.mode):this.eventTitle(ev),timed,   // 61toptimes (web/results-lui.js)
   rows:slot<26?this.career.records(slot,timed).map(r=>({name:r.name,rider:this.data.characters[r.character]?.first||'',value:timed?raceTime(r.ticks,false):String(r.value),player:!!r.player})):[],
   message:this.topTime?(timed?this.t(0x0eea5fd5,"Congratulations, you've got a top time!"):this.t(0x0ea6d945,"Congratulations, you've got a top score!")):null,
   items:ui.items(),index:ui.index,disabled:i=>this.disabled('ctm-records',i)}))return;
  this.panel(c,isPeakRun(ev.mode)?this.peakRunName(ev.mode):this.eventTitle(ev),timed?this.t(0x03ee61f5,'Top 5 Record Times'):this.t(0x0ee6bae5,'Top 5 Record Scores'));
  [[this.t('kT_OVRCMNRanking','Rank'),60],['Name',150],['With',318],[timed?this.t('kT_OVRCMNTime','Time'):this.t('kT_OVRCMNScore','Score'),460]].forEach(([h,x])=>ui.text(c,h,x,Y(165),16,'#d1e1e2'));
  if(slot<26)this.career.records(slot,timed).forEach((r,i)=>{const y=Y(195)+i*Y(26),col=r.player?'#e8883a':'#71b8c6';ui.text(c,String(i+1),80,y,16,col);ui.text(c,r.name,150,y,16,col);/* the player's entry: 'PLAYER 1' (web/career.js PLAYER_NAME), as the PS2 records */ui.text(c,this.data.characters[r.character]?.first||'',318,y,16,col);ui.text(c,timed?raceTime(r.ticks,false):String(r.value),460,y,16,col);});
  if(this.topTime){   // a new top time / score (PS2 out-apr-results/results.png, ctm-left/runs/final-top): the message and Continue / Save Records
   ui.wrap(timed?this.t(0x0eea5fd5,"Congratulations, you've got a top time!"):this.t(0x0ea6d945,"Congratulations, you've got a top score!"),310,17).slice(0,2).forEach((l,k)=>ui.text(c,l,80,Y(366)+k*Y(21),17,'#e7f0f1'));
   ui.items().forEach((t,i)=>{if(ui.index===i)ui.sprite('OV_1-2',55,122,24,24,429,Y(367)+i*Y(16),14,14);ui.text(c,t,450,Y(366)+i*Y(16),13,ui.index===i?'#dce5e4':this.disabled('ctm-records',i)?'#35606d':'#559aad');});}
  else{ui.sprite('OV_1-2',55,122,24,24,437,Y(370),16,16);ui.text(c,this.t('kT_OVRCMNReturn','Return'),460,Y(370),16,'#d1e1e2');}
 }
}
