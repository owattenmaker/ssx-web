"""Export the PS2's online screens for the time-attack sessions (read-only; docs/big-multiplayer.md "UI", pv onlineTimeAttack).

Source (user's own disc, never modified): DATA/UI/FE.LUI and OV.LUI (their sprite pages already exported by web/prepare-ui.py),
DATA/LOCALE/*.LOC, SLUS_207.72 (the widget names). The screens and the code that sets each one up (the screen name's string is
loaded there):

  FE.LUI 230online_main_menu  0x1BFBB8  Quick Matchup / Lobby / Stats / Setup Character / Online Agreement / Logoff
  FE.LUI 231lobby_menu        0x1BCB58  the channels and their room lists, Create Room
  FE.LUI 232CreateRoomPopup   0x1BE7D8  Channel Name, Room Name, Password, Done
  FE.LUI 235chat_room         0x1B1F88  the room's player list (name, games, win ratio, disconnects, ranking) and the chat window
  FE.LUI 240challenge_popup   -         the event rows (peak, event, course) and the list-box rows of a challenge
  FE.LUI 249Stats             0x1C2D18  the ranked list: Rank / User / Time-Points, ten rows and a slider, event and course selectors
  OV.LUI 109MultiplayerStandings 0x24AE90  Current standings / Final Round, Event / Mode / Winner, Continue / Replay / Quit

It runs the two existing exporters' own decoders (tools/export_fe_menus.py for FE.LUI, tools/export_results_screens.py for OV.LUI)
on these screens only, so the files have the layouts web/lui-player.js already plays:

  python3 tools/export_online_screens.py [--fe OUT] [--ov OUT]
  (defaults web/public/assets/UI/online-screens.json and web/public/assets/UI/online-overlays.json)
"""
import argparse, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
import export_fe_menus as fe  # noqa: E402
import export_results_screens as ov  # noqa: E402

FE_SCREENS = ['230online_main_menu', '231lobby_menu', '232CreateRoomPopup', '235chat_room', '240challenge_popup', '249Stats']
OV_SCREENS = ['109MultiplayerStandings']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--fe', default=str(ROOT / 'web/public/assets/UI/online-screens.json'))
    ap.add_argument('--ov', default=str(ROOT / 'web/public/assets/UI/online-overlays.json'))
    args = ap.parse_args()
    fe.SCREENS = {name: fe.X.H(name) for name in FE_SCREENS}
    fe.OUT = Path(args.fe).resolve()
    fe.OUT.parent.mkdir(parents=True, exist_ok=True)
    try:
        fe.main()
    except ValueError:
        # the exporter prints its output path relative to the repository: an output outside it is written all the same
        print(f'online screens: {", ".join(FE_SCREENS)} -> {fe.OUT}')
    ov.SCREENS = OV_SCREENS
    Path(args.ov).resolve().parent.mkdir(parents=True, exist_ok=True)
    sys.argv = [sys.argv[0], '--out', str(Path(args.ov).resolve())]
    ov.main()


if __name__ == '__main__':
    main()
