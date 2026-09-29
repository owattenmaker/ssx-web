"""tools/ps2_menu_capture.py driving a derived Sam playtest disc instead of the original disc.

  python3 tools/sam_ps2/menu_capture_disc.py DISC.iso session|run|send ...   (same arguments as ps2_menu_capture.py)
The BASE savestate must come from the same disc (tools/sam_ps2/boot_states.py).
"""
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import ps2_menu_capture
disc = Path(sys.argv.pop(1)).resolve()
assert disc.exists() and 'Downloads' not in str(disc)
ps2_menu_capture.ISO = disc
ps2_menu_capture.main()
