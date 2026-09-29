"""Fresh-boot a derived Sam playtest disc in ARMSX2 (no savestate) and keep timed savestates + screenshots.

The menu capture tool (tools/ps2_menu_capture.py) needs a BASE savestate of the disc it drives; a state
made on the original disc carries the original ELF, so a new playtest disc needs its own boot state.
  python3 tools/sam_ps2/boot_states.py DISC.iso NAME [--at 20,40,60]
writes local/ps2-capture/menus/NAME/boot.tNNN.p2s/.png. The emulator runs isolated (own data path/PINE slot).
"""
from pathlib import Path
import random, shutil, subprocess, sys, time, zipfile
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from ps2_capture import Pine, prepare_datapath, PCSX2
from ps2_menu_capture import _wait_state

disc, name = Path(sys.argv[1]).resolve(), sys.argv[2]
assert 'Downloads' not in str(disc)
at = [int(x) for x in (sys.argv[sys.argv.index('--at') + 1] if '--at' in sys.argv else '20,40,60').split(',')]
out = ROOT / 'local/ps2-capture/menus' / name; out.mkdir(parents=True, exist_ok=True)
slot = random.randint(29100, 29899); datapath = ROOT / f'local/ps2-capture/pcsx2-boot-{slot}'
prepare_datapath(datapath, slot)
log = open(out / 'boot.pcsx2.log', 'w')
proc = subprocess.Popen([str(PCSX2), '-datapath', str(datapath), '-batch', '-nogui', '--', str(disc)], stdout=log, stderr=subprocess.STDOUT)
start = time.time(); pine = None
try:
    for t in at:
        while time.time() - start < t:
            if proc.poll() is not None: raise RuntimeError(f'ARMSX2 exited early ({proc.returncode})')
            time.sleep(.2)
        if pine is None:
            deadline = time.time() + 30
            while pine is None:
                try: pine = Pine(slot)
                except OSError:
                    if time.time() > deadline: raise
                    time.sleep(.3)
        known = set(datapath.rglob('*.p2s'))
        pine.request(b'\x09' + bytes([1]))
        f = _wait_state(datapath, known)
        state = out / f'boot.t{t:03d}.p2s'; shutil.move(str(f), state)
        with zipfile.ZipFile(state) as z: (out / f'boot.t{t:03d}.png').write_bytes(z.read('Screenshot.png'))
        print(state, flush=True)
finally:
    if pine: pine.close()
    proc.terminate()
    try: proc.wait(10)
    except subprocess.TimeoutExpired: proc.kill()
    log.close(); shutil.rmtree(datapath, ignore_errors=True)
