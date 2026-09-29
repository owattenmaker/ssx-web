// The race and saved result clocks are integer60Hz ticks. Format directly from
// ticks so binary float seconds cannot turn an exact .70 boundary into .69.
export function raceTime(ticks,centiseconds=true){
 if(!Number.isInteger(ticks)||ticks<0)return '--:--';
 const seconds=Math.floor(ticks/60),minutes=Math.floor(seconds/60);
 const base=`${String(minutes).padStart(2,'0')}:${String(seconds%60).padStart(2,'0')}`;
 return centiseconds?`${base}:${String(Math.floor((ticks%60)*100/60)).padStart(2,'0')}`:base;
}
// In-race HUD clock (0x21F81C..0x21F858): whole seconds = trunc(float(ticks) * 1/60), printed
// "%02d:%02d:%02d" as hours, minutes, seconds (the PS2 shows 00:00:07 seven seconds after GO).
export function hudRaceTime(ticks){
 if(!Number.isInteger(ticks)||ticks<0)return '--:--:--';
 const seconds=Math.trunc(Math.fround(Math.fround(ticks)*Math.fround(0.01666666753590107)));
 const pad=(x)=>String(x).padStart(2,'0');
 return `${pad(Math.trunc(seconds/3600))}:${pad(Math.trunc(seconds%3600/60))}:${pad(seconds%60)}`;
}
