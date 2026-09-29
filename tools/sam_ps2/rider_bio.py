"""Sam's fictional SSX profile and guarded routing around ten-row UI tables."""
from profile_hooks import Code

DNA=['Sam','Unc',"5'11\"",'160lbs','American','Chopped Unc','Regular','Unknown']
FAVES=['Going uphill','Going upside down','The flat hills of Wisconsin','Anyone who waits at the bottom',
       'My own knees','Hiking','A small, right-side-up air','Freeride',
       'The least steep way down','Cheese curds','Skins, snacks and warm mittens','Getting home with knees intact']
QNA=['Long johns. It is Wisconsin.','My confidence, mostly.','Uphill route inspector.','Gravity can wait.']
BIO=("Raised on the famously flat mountains of Wisconsin, Sam learned to make a little hill last a long afternoon. "
     "At 5'11\" and 160lbs, this regular-footed rider brings Midwest manners, a trusty vest and extremely reasonable expectations to the mountain. "
     "He is not the best rider in the lineup, and he would like to keep his head above his board. Inversions are somebody else's problem. "
     "These days the chopped unc prefers the uphill part: a quiet climb, a good view, and plenty of time to discuss how his knees are feeling. "
     "His plan for the descent is simple: link a few turns, keep it upright, and make it back for snacks.")

def locale_entries():
 entries={f'kT_{kind}{i+1}Sam':text for kind,values in [('DNA',DNA),('FAVES',FAVES),('QNA',QNA)] for i,text in enumerate(values)}
 entries['kT_FULLBIO1Sam']=BIO
 return entries

def table_hook(kind):
 cell={'DNA':0xa14,'FAVES':0xa18,'QNA':0xa1c}[kind]
 c=Code();c.emit(0x8e820048,0x2408001e);c.branch(5,2,8,'original');c.emit(0)
 c.emit(0x3c080053,0x8d080000|cell,0x00681821,0x03e00008,0)
 c.label('original')
 if kind=='FAVES':c.emit(0x00550018,0x00001012) # Standard MULT/MFLO replaces EE MULT rd.
 else:c.emit(0x00021140 if kind=='DNA' else 0x00021100)
 c.emit(0x00621021,0x03a21821,0x03e00008,0)
 return c.bytes()
