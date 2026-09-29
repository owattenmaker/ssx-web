#!/usr/bin/env python3
"""Condense the Conquer the Mountain PS2 flow captures into the trace web/test-ctm-flow.mjs compares the port with.

Input: local/ps2-capture/ctm-parity/runs/<run>/navigate.json (tools/ctm_flow_capture.py, or the earlier scratch runner with the
same fields). For every run of RUNS it keeps the world-state changes, the NIS steps queued and the scripts that played (in
order), the course at each world state 4 (the ride), and the career fields as they change. Screens that only show in the
frames (prompt texts, the focused item, the results labels) are the OBSERVED entries below, each with its frame.

  python3 tools/ctm_flow_trace.py            -> web/ctm-flow-ps2.json
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / 'local/ps2-capture/ctm-parity/runs'
OUT = ROOT / 'web/ctm-flow-ps2.json'

# run -> what it shows (every state is derived; the base of each run is the named state of the run before it)
RUNS_USED = {
    'new-career': 'fresh profile, Select Character (Zoe) -> Cross, neutral pad',
    'reenter': 'title -> CTM -> Zoe again (new-career flag still set, Happiness visited)',
    'start-lodge': 'the same Select Character with the new-career flag poked to 0 (last lodge 17)',
    'quit-ctm': 'free ride -> MCOMM -> Quit -> Yes',
    'race-q': 'Snow Jam qualifier from the card, computer riders poked DNF (+0x480) -> 1st',
    'to-final': 'results Next heat with GMM+0x70 poked 3',
    'race-f': 'Snow Jam final, computer riders DNF -> 1st, first gold',
    'race-f95': 'the same final with earned/cash poked to 95,000 (earnings goal)',
    'f95-after': 'rewards -> results -> Transport -> Peak 2 -> Go to this peak now? Yes',
    'peak2-arr': 'the Peak 2 first arrival (continues f95-after)',
    'lodge-return': 'the lodge door prompt Yes -> lodge -> Return to Game -> Save progress? Yes',
    'lr-tri': 'no memory card -> Triangle -> the lodge exit load -> the station',
    'lodge-quit': 'lodge Quit -> Yes',
    # collectibles and Big Challenges (2026-09-26)
    'explore-info': 'free ride MCOMM (menus/ctm/state-mcomm, Zoe) with 41 Happiness collectibles and 22 Peak 1 Big Challenges poked '
                    'completed -> Transport: Peak 1 / All Mountain / Freeride goal / freeride list INFO (Square)',
    'sd-goal': 'Speed Demon at its last gate (peak1/bc-sd-lastgate) with 11 more Peak 1 challenges and 40 Happiness collectibles poked: '
               'the completion is the 12th challenge (Big Challenge bronze) and completes the Freeride goal',
    'pc75': 'Snow Jam free ride (peak1/bc-sj-arrival), the Point Challenge 75,000 offer queued in C+0x1C4 -> Yes, neutral pad',
    'dizzy': 'Happiness free ride (menus/ctm/state-freeride-peak1), the Dizzy Spells offer queued -> Yes, neutral pad',
}

# Frame-only facts (text on screen), with the frame they come from.
OBSERVED = [
    dict(step='ctm-select', frame='new-career/sample00100.png', screen='load', note='Cross on Select Character -> Basic Controls load screen (no Setup Character)'),
    dict(step='movie', frame='new-career/sample00650.png', screen='movie', letterbox=True, skip='Press X to skip'),
    dict(step='quit', frame='quit-ctm/sample00360.png', prompt='Quit Game', items=['Yes', 'No'], focus='No'),
    dict(step='quit-save', frame='quit-ctm/sample00480.png', prompt='Save progress before quitting?', items=['Yes', 'No'], focus='Yes'),
    dict(step='quit-end', frame='quit-save-tri/sample00300.png', screen='title', note='after the save: Hints load, then the title screen'),
    dict(step='qualifier-results', frame='race-q/sample14400.png', title='Snow Jam - Race', sub='Qualifier Results',
         message="Congratulations!  You've qualified for the semi final round.", items=['Next heat', 'Restart', 'Replay', 'Records', 'Quit']),
    dict(step='final-card', frame='to-final/sample01100.png', title='Snow Jam - Race', sub='Final Round'),
    dict(step='final-rewards', frame='race-f/sample15600.png', title='Snow Jam - Race', sub='Rewards', head="Congratulations!  You've been awarded:",
         lines=[['Cash: $10,000', 0], ['Gold medal earned', 0], ['Accessory', 1]]),
    dict(step='final-rewards-goal', frame='race-f95/sample15800.png', lines=[['Cash: $10,000', 0], ['Earnings goal complete!', 0], ['Peak 2 pass', 1],
         ['Poster', 1], ['Trading card', 1], ['Trading card', 1], ['Trading card', 1], ['Trading card', 1], ['Gold medal earned', 0]]),
    dict(step='final-results', frame='after-final/sample00165.png', sub='Final Results', message="Congratulations!  You've received a Gold medal.",
         items=['Transport', 'Restart', 'Replay', 'Records', 'Quit'], medal_icon=True),
    dict(step='go-peak', frame='f95-after/sample00765.png', prompt='Go to this peak now?', items=['Yes', 'No'], focus='Yes'),
    dict(step='lodge-return', frame='lodge-return/sample00980.png', prompt='Save progress?', items=['Yes', 'No'], focus='Yes'),
    dict(step='lodge-quit', frame='lodge-quit/sample00140.png', prompt='Quit to Title screen?', items=['Yes', 'No'], focus='Yes'),
    dict(step='lodge-quit-save', frame='lodge-quit/sample00260.png', prompt='Save progress before quitting?', items=['Yes', 'No'], focus='Yes'),
    # Transport INFO with 41 Happiness collectibles and 22 Peak 1 challenges (0x205098 / 0x204D40 / 0x207430)
    dict(step='explore-peak-goals', frame='explore-info/sample00255.png', title='Peak 1 Goals',
         rows=[['Race', '0 / 4', False], ['Freestyle', '0 / 5', False], ['Freeride', '2 / 2', True], ['Earnings', '0 / 1', False]]),
    dict(step='explore-all-mountain', frame='explore-info/sample00330.png', percent='6%',
         rows=[['Gold Medals:', '0 / 14'], ['Rival Challenges:', '0 / 12'], ['Big Challenges:', '22 / 88'], ['Collectibles:', '41 / 425'], ['Career Highlights:', '0 / 24']]),
    dict(step='explore-goal-list', frame='explore-info/sample00540.png', goals=['Race', 'Freestyle', 'Freeride', 'Earnings'], ticked=[False, False, True, False]),
    dict(step='explore-freeride-goal', frame='explore-info/sample00660.png', title='Peak 1 Freeride Goals', complete='2/2 Complete',
         rows=[['Challenges Complete: 22/40', 'Gold medal at 32', True], ['Collectibles: 41/155', 'Silver Medal at 70', True]]),
    dict(step='explore-freeride-list', frame='explore-info/sample00945.png', entries=[
        dict(name='Happiness', run='Happiness', event='Freeride', collectibles='41 / 44', challenges='5 / 5'),
        dict(name='Green Station', run='Green Base Station', event='Freeride', collectibles='0 / 5', challenges='N/A'),
        dict(name='R&B', run='R&B', event='Slopestyle', collectibles='0 / 30', challenges='1 / 8')]),
    # Big Challenges
    dict(step='bc-offer', frame='pc75/sample00100.png', title='Big Challenge', name='Point Challenge', text='Get 75,000 points on this race track',
         question='Accept challenge?', items=['Yes', 'No'], focus='Yes'),
    dict(step='bc-goal-hud', frame='pc75/sample00140.png', badge=15, goal='GOAL: 75000', score='0', cash=None, go=True),
    dict(step='bc-called-hud', frame='dizzy/sample00300.png', badge=9, clock='00:00:57.xx', count='COUNT: 0 / 5', called='180',
         called_rgb=[200, 0, 0], called_box=[299, 381, 336, 395], cash='$ 0'),
    dict(step='bc-success-goal', frame='sd-goal/sample00150.png', popup=['MISSION SUCCESS', '$ 2,000'], cash='$ 2,000', mail_icon=True, reward_list=False),
]

# Memory facts of named states (stage-script context C = *(gp+0xCE8) = 0xAD0370 in these runs; the career block of Zoe 0x4AAAC8).
def mission_facts(state):
    import struct, zipfile
    m = zipfile.ZipFile(state).read('eeMemory.bin')
    i = lambda a: struct.unpack_from('<i', m, a)[0]
    C = struct.unpack_from('<I', m, 0x4A3DD8)[0]
    ci = m[0x534FE0 + 0x11]; B = 0x4A6CA8 + ci * 0xF88
    return dict(hud=[i(C + 4 * k) for k in range(16)], active=i(C + 0x2A0), status=[i(B + 0x118 + 4 * k) for k in range(40)],
                awards=m[B + 0xF28:B + 0xF30].hex(), locks=hex(struct.unpack_from('<Q', m, B + 0x278)[0]), cash=i(B + 0xAC4),
                inbox=[list(struct.unpack_from('<ii', m, B + 0xE38 + 8 * k)) for k in range(max(0, min(25, i(B + 0xE38 + 0xCC))))],
                collect={c: [m[B + 4 + 12 * c], *struct.unpack_from('<II', m, B + 8 + 12 * c)] for c in range(22) if m[B + 4 + 12 * c]})


STATES = {'dizzy-s200': 'dizzy/s200.p2s', 'dizzy-end': 'dizzy/end.p2s', 'sd-goal-end': 'sd-goal/end.p2s', 'pc75-start': 'pc75/start.p2s'}
BASES = {'sd-goal-start': 'local/ps2-capture/ctm-parity/states/sd-goal.p2s', 'explore-info-start': 'local/ps2-capture/ctm-parity/states/mcomm-explore.p2s'}


def playing_ids(nis):
    return [p[0] for p in nis.get('playing', [])]


def queued(nis):
    out = []
    for L in nis.get('lists', []):
        steps = L.get('steps', L) if isinstance(L, dict) else L
        out.append([s['list'] for s in steps])
    return out


def condense(run):
    nav = json.loads((RUNS / run / 'navigate.json').read_text())
    res = sorted(nav['results'], key=lambda e: e['sample'])
    ws_log = [[w['sample'], w['ws']] for w in nav.get('ws_log', []) if w['ws'] is not None and -1 <= w['ws'] <= 20]
    scripts, lists, rides, career, prev_career = [], [], [], [], None
    for e in res:
        n = e['nis']
        for s in playing_ids(n):
            if not scripts or scripts[-1][1] != s: scripts.append([e['sample'], s])
        q = queued(n)
        if q and any(q) and (not lists or lists[-1][1] != q): lists.append([e['sample'], q])
        if n.get('ws') == 4 and (not rides or rides[-1][1] != n.get('course')): rides.append([e['sample'], n.get('course')])
        c = n.get('career')
        if isinstance(c, dict):
            c = {k: c[k] for k in ('new', 'lodge', 'locks', 'visited', 'cash', 'earned') if k in c}
            if c != prev_career: career.append([e['sample'], c]); prev_career = c
    return dict(what=RUNS_USED[run], ws=ws_log, scripts=scripts, queued=lists, rides=rides, career=career)


def main():
    out = dict(source='tools/ctm_flow_trace.py from local/ps2-capture/ctm-parity/runs (ARMSX2, SLUS_207.72, derived states)',
               runs={run: condense(run) for run in RUNS_USED}, observed=OBSERVED,
               memory={**{k: mission_facts(RUNS / v) for k, v in STATES.items()}, **{k: mission_facts(ROOT / v) for k, v in BASES.items()}})
    OUT.write_text(json.dumps(out, indent=1) + '\n')
    print(OUT, sum(len(r['scripts']) for r in out['runs'].values()), 'script steps')


if __name__ == '__main__':
    main()
