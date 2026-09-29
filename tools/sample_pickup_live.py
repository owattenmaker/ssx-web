"""Read-only PINE lifecycle samples; preserve game ticks and torn-read markers."""
import os,sys,struct,time,json,argparse
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent));from pcsx2_pine import Pine
out=Path(__file__).resolve().parents[1]/'local/browser-pickups/live-lifecycle.json'
parser=argparse.ArgumentParser();parser.add_argument('--arm',action='store_true',help='Wait for a paused-to-running transition before sampling');parser.add_argument('--wait-seconds',type=float,default=20);args=parser.parse_args();
if not 0<args.wait_seconds<=45:raise ValueError('Wait must be within45 seconds')
rows=[]
with Pine(Path(os.environ.get('TMPDIR','/tmp'))/'pcsx2.sock.28022') as p:
 assert p.info()['game_id']=='SLUS-20772'
 if args.arm:
  if p.status()!='paused':raise RuntimeError('Arm while paused, before resuming the reference')
  deadline=time.monotonic()+args.wait_seconds
  while p.status()=='paused':
   if time.monotonic()>=deadline:raise TimeoutError('Reference did not resume; no lifecycle capture recorded')
   time.sleep(.01)
 for _ in range(240):
  tick=struct.unpack('<I',p.read(0x5bc508,4))[0];flags,entity=struct.unpack('<2I',p.read(0x10381e8,8));state={}
  if 0x100000<=entity<0x2000000-64:
   data=p.read(entity,64);state=dict(vtable=struct.unpack_from('<I',data,12)[0],kind=struct.unpack_from('<h',data,16)[0],counter20=struct.unpack_from('<i',data,32)[0],counter2C=struct.unpack_from('<i',data,44)[0])
  speed,trick=struct.unpack('<2f',p.read(0x1470488,8));end=struct.unpack('<I',p.read(0x5bc508,4))[0]
  row=dict(tick=tick,end_tick=end,coherent=tick==end,flags=flags,entity=entity,speed_counter=speed,trick_counter=trick,component=state)
  if not rows or row!=rows[-1]:rows.append(row)
  time.sleep(.01)
 status=p.status()
out.write_text(json.dumps(dict(rows=rows,status_after=status,scope='Polling samples after temporary placement, not deterministic frame replay'),indent=2)+'\n')
print('samples',len(rows),'ticks',rows[0]['tick'],rows[-1]['end_tick'],'types',sorted({r['component'].get('kind',-1) for r in rows}),'speed',rows[-1]['speed_counter'])
