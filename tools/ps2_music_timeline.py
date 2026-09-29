#!/usr/bin/env python3
"""ps2_music_timeline.py [RUN ...]: the ps2_music_drive.py call logs (local/ps2-capture/music/runs/NAME/events.jsonl) in the
director timeline vocabulary of web/game-audio.js `tl` ([ms, kind, ...args], ms = pad samples x 1000 / 59.94).
With no argument: writes web/ps2-audio-timelines.json, the scenarios web/test-audio-timeline.mjs compares
(docs/audio-logic.md 9.11). Development reference only."""
import json, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / 'local/ps2-capture/music/runs'


def reduce(run):
    ev = [json.loads(l) for l in open(RUNS / run / 'events.jsonl')]
    out = []; i = 0
    ms = lambda e: round(e['sample'] * 1000 / 59.94)
    while i < len(ev):
        e = ev[i]; n = e['name']; ra = int(e['ra'], 16)
        if n == 'listener_play':
            # 28F478: PlaySong (+ HUD 5); the named / playlist song, then its start event (forced send in the same frame)
            j = i + 1; name = None
            while j < len(ev) and ev[j]['sample'] - e['sample'] <= 1 and ev[j]['name'] in ('play_song_named', 'play_song', 'stop', 'resume'):
                name = name or ev[j].get('str'); j += 1
            evn = 0
            if j < len(ev) and ev[j]['name'] == 'send_event' and ev[j]['sample'] - e['sample'] <= 1:
                evn = ev[j]['a1']; j += 1
                if j < len(ev) and ev[j]['name'] == 'send_forced': j += 1
            out.append([ms(e), 'play', name or '*playlist', evn]); out.append([ms(e), 'nowplaying?', name or '*playlist']); i = j; continue
        if n == 'send_event':
            out.append([ms(e), 'event', e['a1']])
            if i + 1 < len(ev) and ev[i + 1]['name'] == 'send_forced': i += 1
        elif n == 'send_forced': out.append([ms(e), 'event', e['a1']])
        elif n == 'pick_next': out.append([ms(e), 'pick'])
        elif n == 'fade_out': out.append([ms(e), 'fade', round(e['f12'], 3)])
        elif n == 'stop' and ra != 0x2B3878: out.append([ms(e), 'stop'])           # not the Stop inside PlaySong
        elif n == 'pause' and ra == 0x289B9C: out.append([ms(e), 'pause'])        # 289B70 game pause
        elif n == 'resume' and ra == 0x289BDC: out.append([ms(e), 'resume'])      # 289BB8 game resume
        elif n == 'dj_timer': out.append([ms(e), 'dj', e.get('x0')])
        elif n == 'request_cb': out.append([ms(e), 'request', e.get('x0')])
        elif n == 'music_code': out.append([ms(e), 'code', e['a1']])
        elif n == 'speech' and e['t0'] in (10, 11): out.append([ms(e), 'speech', format(e['a2'], 'x')])   # DJ / PA speakers
        elif n == 'speech_bus':
            banks = [e.get('str')]; j = i + 1
            while j < len(ev) and ev[j]['name'] == 'speech_bus' and ev[j]['sample'] - e['sample'] <= 1: banks.append(ev[j].get('str')); j += 1
            if banks[0] and (banks[0].startswith('DJ_') or banks[0].startswith('PA_')): out.append([ms(e), 'line', '+'.join(banks)])
            i = j; continue
        elif n == 'fe_state': out.append([ms(e), 'fe', e['a1']])
        elif n == 'loading_on': out.append([ms(e), 'loading', 1])
        elif n == 'loading_off': out.append([ms(e), 'loading', 0])
        elif n == 'world_load': out.append([ms(e), 'worldload'])
        elif n == 'world_leave': out.append([ms(e), 'leave'])
        i += 1
    return out


def rel(rows, t0, lo=None, hi=None, drop=('nowplaying?',)):
    out = []
    for r in rows:
        if lo is not None and r[0] < lo or hi is not None and r[0] > hi or r[1] in drop: continue
        if out and r[1] == 'event' and out[-1][1] == 'event' and out[-1][2] == r[2]: continue   # 2B3BC0 repeats (filtered by the song)
        out.append([r[0] - t0] + r[1:])
    return out


def merge_play_event(rows, window=200):
    """PlaySong then its start event a few frames later (pktrans: 28CD48 sends 1 seven frames after) -> play X e."""
    out = []
    for r in rows:
        prev = next((o for o in reversed(out) if o[1] != 'nowplaying?'), None)
        if r[1] == 'event' and prev and prev[1] == 'play' and prev[3] == 0 and r[0] - prev[0] <= window and prev[2] != 'charsel':
            prev[3] = r[2]; continue
        out.append(r)
    return out


HUB = {'20c8', '20c9', '20ca', '20e7', '20e8', '20e9', '210b'}
def hubs(rows, also=()):
    return [[r[0], r[1], 'hub' if r[2] in HUB or r[2] in also else r[2]] if r[1] == 'speech' else r for r in rows]


def build():
    ref = {'provenance': 'ARMSX2 headless, stock USA disc, derived savestates only: tools/ps2_music_drive.py + ps2_music_log.py, plans in '
                         'tools/ps2_music_plans, logs in local/ps2-capture/music/runs (docs/audio-logic.md 9.11). ms = pad samples x 1000 / 59.94 '
                         'from the anchor event. hub = a random hub-chatter category (2A2E50); *playlist = the PickNextSong song (random).', 'scenarios': {}}
    S = ref['scenarios']
    nc = merge_play_event(reduce('newcareer'))
    c19 = next(r[0] for r in nc if r[1] == 'code' and r[2] == 19)
    sa = [[r[0] + round(4604 * 1000 / 59.94)] + r[1:] for r in reduce('stationA')]   # stationA starts at newcareer's sample 4604 save
    faq = round((4604 + 650) * 1000 / 59.94)   # stall run: the Message Center overlay (tick 2003) ~650 samples after that save
    S['new-career'] = dict(source='newcareer (state-select-character-zoe.p2s, Cross), stationA / stall (its A-crossing save)',
        anchor='code 19 (28E888 -> 28E8C0(19, 1): the plane intro ends)',
        events=rel(nc, c19) + rel([r for r in sa if r[1] == 'speech'], c19) + [[faq - c19, 'faq']])
    lo = reduce('lodge')
    t_leave = next(r[0] for r in lo if r[1] == 'leave'); t_fade = next(r[0] for r in lo if r[1] == 'fade')
    S['lodge-enter'] = dict(source='lodge (state-lodge-prompt.p2s, Yes)', anchor='leave (286A80)', events=rel(lo, t_leave, hi=t_fade - 1))
    S['lodge-exit'] = dict(source='lodge (Return to Game, No)', anchor='fade (286200)', events=hubs(rel(lo, t_fade, lo=t_fade, hi=t_fade + 15000)))
    tr = reduce('transport')
    t_stop = next(r[0] for r in tr if r[1] == 'stop'); t18 = next(r[0] for r in tr if r[1] == 'event' and r[2] == 18)
    S['transport'] = dict(source='transport (state-transport-confirm.p2s: MCOMM Transport, Happiness -> Snow Jam)', anchor='stop (28F520)',
        events=hubs(rel(tr, t_stop, hi=t18 - 1), also=('212c',)))
    sj = merge_play_event(reduce('sjB'))
    t11 = next(r[0] for r in sj if r[1] == 'event' and r[2] == 11); t18 = next(r[0] for r in sj if r[1] == 'event' and r[2] == 18)
    S['change-song'] = dict(source='sjB (Snow Jam MusicTrigger 18 in free ride, first visit; also transport)', anchor='event 18 (28DF18)',
        events=[r for r in rel(sj, t18, lo=t18, hi=t11 - 1) if not (r[1] == 'event' and r[2] in (1, 2, 9))])
    S['spoke'] = dict(source='sjB (state-snowjam-arrival.p2s: ARA1_B MusicTrigger 11 into Blue Base Station, +0x5790 = 0)', anchor='event 11 (28D988)',
        events=[r for r in rel(sj, t11, lo=t11) if not (r[1] == 'event' and r[2] in (1, 2, 9))])
    for name, run, src in (('mcomm', 'mcomm', 'mcomm (state-freeride-peak1.p2s: Start, Messages, back, Return)'),
                           ('transport-map', 'map', 'map (MCOMM Transport map opened and closed; 28F5B8 / 28F678 change nothing in free ride)')):
        m = reduce(run); t_p = next(r[0] for r in m if r[1] == 'pause')
        S[name] = dict(source=src, anchor='pause (289B70)', events=rel(m, t_p, lo=t_p))
    si = merge_play_event(reduce('single'))
    t_f = next(r[0] for r in si if r[1] == 'fade')
    S['single-event'] = dict(source='single (snow-jam-rules.p2s: Cross, the card Continue at 1600, GO at 1783)', anchor='fade (286200)', events=rel(si, t_f, lo=t_f))
    (ROOT / 'web/ps2-audio-timelines.json').write_text(json.dumps(ref, indent=1) + '\n')
    return ref


if __name__ == '__main__':
    if len(sys.argv) > 1:
        for r in sys.argv[1:]:
            print('==', r)
            for row in reduce(r): print(*row)
    else:
        print(json.dumps({k: len(v['events']) for k, v in build()['scenarios'].items()}))
