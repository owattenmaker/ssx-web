"""Read-only extraction of the original reset path bank and retained human route."""
import hashlib,math,struct

def extract_reset(memory,rider):
 def read(fmt,at):
  if at<0 or at+struct.calcsize('<'+fmt)>len(memory):raise ValueError('Reset data outside EE memory')
  return struct.unpack_from('<'+fmt,memory,at)
 def u(at):return read('I',at)[0]
 def v(at,n=3):
  result=list(read('f'*n,at))
  if not all(map(math.isfinite,result)):raise ValueError('Nonfinite reset path')
  return result
 interface=u(rider+0x6c0)
 if u(interface+0x44)!=0x140bc0 or read('h',interface+0x40)[0]!=-0x6c0:raise ValueError('Unsupported reset path-end predicate')
 count,base=u(0x4d33a8),u(0x4d33ac)
 if not 0<count<=200:raise ValueError('Reset path bank extent')
 paths=[]
 for i in range(count):
  p=base+i*64;ne,events=u(p),u(p+4);ns,segments=u(p+8),u(p+24)
  if u(p+52)!=0x481478 or not 0<ns<=100000 or ne>10000:raise ValueError('Reset path layout')
  paths.append(dict(origin=v(p+12),low=v(p+28),high=v(p+40),segments=[v(segments+k*16,4) for k in range(ns)],events=[dict(zip(('type','value','start','end'),read('IIff',events+k*16))) for k in range(ne)],flags38=u(p+56),field3c=u(p+60)))
 retained=u(rider+0xab8);index=(retained-base)//64
 if not 0<=index<count or retained!=base+index*64:raise ValueError('Retained reset route outside bank')
 cache=u(rider+0xabc);variant=read('b',0x535c12)[0]
 # 12F454 calls 14DD58(mode,0): the mode object's vtable+8 slot 1530E0 case 0 returns the 1448D8 singleton (never
 # null), so the predicate is always true. Variant 0 (races) places at 200 cm either way (kept False, historical);
 # variant 2 (The Junction super pipe, config byte 0x535C12) places at 1000 cm.
 # 12F468: only mode 2 selects 1000 cm; slope style (1) and big air (3) keep the 200 cm default like races (R&B/Crow's Nest
 # savestates, docs/slopestyle-bigair.md). Other modes are left unverified.
 # Rival Time/Points (4/5, Happiness ABC1; docs/backcountry.md) keep the 200 cm default (12F468 compares with 2 only).
 # Peak challenges (6..11) and free ride (12, docs/peak-mountain.md) too: 12F468 compares with 2 only.
 if variant not in (0,1,2,3,4,5,6,7,8,9,10,11,12):raise ValueError('Reset event variant not verified')
 return dict(paths=paths,route=dict(path_index=index,cache=dict(origin=v(cache),distance=v(cache+16,1)[0],segment=read('i',cache+20)[0]),closest_point=v(rider+0x490),lookahead_point=v(rider+0x4a0),previous_distance=v(rider+0x4c0,1)[0],current_distance=v(rider+0x4c4,1)[0],lateral_distance=v(rider+0x4c8,1)[0],heading=v(rider+0x4cc,1)[0]),allow_path_end=bool(u(rider+0x874)),stance=u(rider+0x324),event_variant=variant,event_mode_active=variant!=0,device_index=read('i',rider+0x870)[0],device_enabled=bool(u(rider+0x87c)),provenance=dict(ee_sha256=hashlib.sha256(memory).hexdigest(),rider=hex(rider),path_base=hex(base),entry='116120 /12F230 /12F398 /112D58 /11D660',event_mode_note='Variant0 uses200cm regardless of the14DD58 predicate'))
