"""Temporary guest-pad stimulus for the isolated storage test, via PINE.
Writes only the two verified SDK pad DMA input payloads. This races the IOP's
updates and is a menu-navigation aid, NOT a deterministic replay mechanism.
Never use against the user's normal emulator configuration or other CRCs.
"""
import argparse,os,struct,time,zipfile
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from pcsx2_pine import Pine
from reference_replay import pad_frame
parser=argparse.ArgumentParser();parser.add_argument('buttons',nargs='*');parser.add_argument('--snapshot',action='store_true');parser.add_argument('--single-frame',action='store_true');parser.add_argument('--duration',type=float,default=.20);parser.add_argument('--left',nargs=2,type=int,default=[127,127]);parser.add_argument('--until-character',type=int);parser.add_argument('--menu-address',type=lambda s:int(s,0));parser.add_argument('--until-menu-index',type=int);parser.add_argument('--widget-name',default='Menu');parser.add_argument('--index-offset',type=lambda s:int(s,0),default=0x95);parser.add_argument('--active-menu',action='store_true');parser.add_argument('--until-focus-lost',action='store_true');parser.add_argument("--enabled-widget",action="store_true");parser.add_argument("--state-screen-offset",type=lambda s:int(s,0),default=0x40);args=parser.parse_args()
from loc_file import name_hash
wanted_hash=name_hash(args.widget_name)
assert 0<=args.index_offset<0x1000 and all(0<=v<=255 for v in args.left)
with Pine(Path(os.environ.get('TMPDIR','/tmp'))/'pcsx2.sock.28020') as p:
 info=p.info();assert info['game_id']=='SLUS-20772' and info['game_crc'] in ('18670b91','18790b91','b0790b91','b0790b84','b0790bc0','b2390a14','b2390a0d','2a5d5c74','2a5d5c1b','09cc560e','09cc5630','09cc565f','08fff00d','b2390a33','b2390a26','9a195c76','a70eb610','9f0957f6','930d7464','9ea95b32','fc9c90b5','5259b799','d25cfc95','4ab86251','d71fa4e9','d71f9c11','de3fc75b','6edad4a1','629f1300') and info['status']=='running',info
 base=struct.unpack('<I',p.read(0x526250,4))[0];assert 0x100000<=base<0x2000000-256 and base%128==0
 data=p.read(base,256)
 assert data[0:2]==b'\x00\x79' and data[128:130]==b'\x00\x79'
 assert struct.unpack_from('<I',data,0x60)[0]==32 and struct.unpack_from('<I',data,0xe0)[0]==32
 def write(buttons,active=False):
  payload=pad_frame(buttons,left=args.left if active else (127,127))
  request=b''.join(b'\x04'+struct.pack('<I',base+offset+2+i)+bytes([value]) for offset in (0,128) for i,value in enumerate(payload))
  p.request(request)
 focus_state=None
 if args.active_menu or args.until_focus_lost:
  ram=p.read(0,32*1024*1024);matches=[];at=0
  def word(a):return struct.unpack_from('<I',ram,a)[0] if 0<=a<len(ram)-4 else 0
  while True:
   at=ram.find(struct.pack('<I',wanted_hash),at)
   if at<0:break
   widget=at-0x38;at+=4
   if widget%4 or not 0x430000<word(widget+8)<0x4a0000:continue
   if args.enabled_widget and word(widget+0x14)&0x50!=0x50:continue
   screen=word(widget+0x5c);state=word(screen+0xd0)
   if not 0x100000<screen<0x2000000-0x100 or not 0x100000<state<0x2000000-0x100:continue
   owner=word(state+0x10)
   count=word(word(screen+0x38));array=word(screen+0x3c)
   if not 0<count<1000 or not 0x100000<array<0x2000000-count*4:continue
   live=[word(array+i*4) for i in range(count)]
   if widget not in live:continue # Ignore stale freed widgets whose pointers were reused.
   if next((w for w in live if word(w+0x38)==wanted_hash),None)!=widget:continue
   if word(state+args.state_screen_offset)==screen and word(state+0x1c)&0x20 and word(owner+0x1c)==state:matches.append((widget,state))
  assert len(matches)==1,matches
  args.menu_address,focus_state=matches[0]
  focus_owner=word(focus_state+0x10)
  print('Focused menu',hex(args.menu_address),'state',hex(focus_state),'state_type',hex(word(focus_state+0xc)),'index',p.read(args.menu_address+args.index_offset,1)[0])
 if args.until_menu_index is not None:
  if args.menu_address is None:
   ram=p.read(0,32*1024*1024);matches=[];at=0
   def word(a):
    if not 0<=a<=len(ram)-4:raise ValueError('UI pointer outside RAM')
    return struct.unpack_from('<I',ram,a)[0]
   while True:
    at=ram.find(struct.pack('<I',0x46d000),at)
    if at<0:break
    candidate=at-8;at+=1
    if candidate%4 or candidate<0:continue
    screen=word(candidate+0x40)
    if not 0x100000<screen<len(ram)-0x100:continue
    countptr=word(screen+0x38);array=word(screen+0x3c)
    if not 0x100000<countptr<len(ram)-4:continue
    count=word(countptr)
    if not 0<count<1000:continue
    for i in range(count):
     widget=word(array+i*4)
     if 0x100000<widget<len(ram)-0x100 and word(widget+0x38)==wanted_hash:matches.append(widget)
   assert len(matches)==1,matches
   args.menu_address=matches[0]
  assert p.read(args.menu_address+0x38,4)==struct.pack('<I',wanted_hash)
  print('Selection menu',hex(args.menu_address),'index',p.read(args.menu_address+args.index_offset,1)[0])
 if (args.buttons or args.left!=[127,127]) and args.single_frame:
  def counter():return max(struct.unpack('<II',p.request(b'\x02'+struct.pack('<I',base+0x58)+b'\x02'+struct.pack('<I',base+0xd8))))
  def next_counter(last):
   deadline=time.monotonic()+2
   while time.monotonic()<deadline:
    now=counter()
    if now!=last:return now
    time.sleep(.001)
   raise TimeoutError('Pad DMA frame did not advance')
  frame=next_counter(counter())
  try:write(args.buttons,True);next_counter(frame)
  finally:write([])
 elif args.buttons or args.left!=[127,127]:
  try:
   assert 0<args.duration<=2
   until=time.monotonic()+args.duration
   while time.monotonic()<until:
    if args.until_focus_lost and (struct.unpack('<I',p.read(focus_owner+0x1c,4))[0]!=focus_state or args.enabled_widget and struct.unpack('<I',p.read(args.menu_address+0x14,4))[0]&0x50!=0x50):
     print('UI stack changed');break
    if args.until_character is not None and p.read(0x535b31,1)[0]==args.until_character:
     print('Reached character',args.until_character);break
    if args.until_menu_index is not None and p.read(args.menu_address+args.index_offset,1)[0]==args.until_menu_index:
     print('Reached menu index',args.until_menu_index);break
    write(args.buttons,True)
    time.sleep(.001)
  finally:write([])
 if args.until_menu_index is not None:
  actual=p.read(args.menu_address+args.index_offset,1)[0]
  assert actual==args.until_menu_index,f'Target not reached: wanted {args.until_menu_index}, actual {actual}; input released, retry navigation before selecting'
 if args.snapshot:
  time.sleep(.15) # Let a completed GS frame reflect the last guest input.
  folder=Path(__file__).resolve().parents[2]/'local/sam-ps2'
  state=folder/'pcsx2-storage-test/savestates'/f"SLUS-20772 ({info['game_crc'].upper()}).09.p2s"
  old=state.stat().st_mtime_ns if state.exists() else 0
  p.request(bytes([9,9]));until=time.monotonic()+8
  while time.monotonic()<until:
   if state.exists() and state.stat().st_mtime_ns!=old:
    try:
     with zipfile.ZipFile(state) as z:image=z.read('Screenshot.png')
     (folder/'roster/latest-state.png').write_bytes(image)
     print('New snapshot complete:',state);break
    except (OSError,zipfile.BadZipFile,EOFError):pass
   time.sleep(.02)
  else:raise TimeoutError('Snapshot did not complete; previous image must not be used')
 print('Isolated pad stimulus:',args.buttons,'snapshot requested:',args.snapshot)
