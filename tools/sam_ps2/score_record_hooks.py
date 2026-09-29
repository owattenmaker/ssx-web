"""Route result/medal record pointers while keeping original scoring rules."""
from profile_hooks import Code
# site, pointer cell, profile input, character input, char product, profile product, original words
SITES=[
 (0x158910,0x530b64,5,6,4,9,(0x24020f88,0x34039b50,0x00c22018,0x00a34818)),
 (0x158960,0x530b68,5,6,4,9,(0x24020f88,0x34039b50,0x00c22018,0x00a34818)),
 (0x155948,0x530b6c,16,2,2,4,(0x24030f88,0x34049b50,0x00431018,0x02042018)),
 (0x155a00,0x530b70,16,2,2,4,(0x24030f88,0x34049b50,0x00431018,0x02042018)),
 (0x159204,0x530b74,21,22,3,5,(0x34059b50,0x02c31818,0x02a52818,0x24426ca8)),
]

def hook(site,profile,character,char_product,profile_product,words):
 c=Code();c.emit((character<<21)|(24<<11)|0x21,(profile<<21)|(25<<11)|0x21)
 for w in words:
  if w&0xfc00003f==0x18:
   rd=(w>>11)&31;c.emit(w&~(31<<11),(rd<<11)|0x12)
  else:c.emit(w)
 c.emit(0x3b18001e);c.branch(5,24,0,'resume');c.emit(0)
 # Replace only the character-product term; remaining profile/base adds stay.
 d=char_product
 c.emit((25<<16)|(d<<11)|(12<<6),(25<<16)|(24<<11)|(7<<6),
        (d<<21)|(24<<16)|(d<<11)|0x23,
        (25<<16)|(24<<11)|(3<<6),(d<<21)|(24<<16)|(d<<11)|0x21,
        0x3c190053,0x8f3909fc,(d<<21)|(25<<16)|(d<<11)|0x21,
        (d<<21)|(profile_product<<16)|(d<<11)|0x23,
        0x3c19004a,0x27396ca8,(d<<21)|(25<<16)|(d<<11)|0x23)
 c.label('resume');c.emit((2<<26)|((site+16)>>2),0)
 return c.bytes()
