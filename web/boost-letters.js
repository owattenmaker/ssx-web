// Ordinary type8 draw loop at1ED7D4..1ED8CC. Transition/glow controller is separate.
export function boostLetters(profile,count,fraction,removed,pendingCount=0,pendingPhase=-1){
 const ninth=Math.fround(1/9),pop=1.8001313209533691;
 let lower=0,upper=ninth;
 const letters=Array.from({length:9},(_,letter)=>{
  const active=!removed&&letter<count&&fraction>=lower;
  const scale=active&&fraction<upper?1+(upper-fraction)*pop:1;
  lower=upper;upper=Math.fround(upper+ninth);
  return {letter,scale,active,outlineScaleX:1,draws:profile.letterSubmissions.find(c=>c.letter===letter&&c.mode===(active?2:0)).draws};
 });
 const firstPending=letters.filter(x=>x.active).length;
 if(pendingPhase>=0)for(let i=firstPending;i<Math.min(9,firstPending+pendingCount);i++){
  letters[i]={letter:i,active:false,scale:1,outlineScaleX:(pendingPhase-.5)*2,draws:profile.letterSubmissions.find(c=>c.letter===i&&c.mode===1).draws};
 }
 return letters;
}
