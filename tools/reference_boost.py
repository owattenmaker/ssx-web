"""Read original boost-controller data without inventing meter/threshold values."""
import math,struct

def extract_boost(memory,base):
    def read(fmt,address):
        if not 0<=address<=len(memory)-struct.calcsize(fmt):raise ValueError('Boost pointer outside EE memory')
        return struct.unpack_from(fmt,memory,address)[0]
    def f(address):
        value=read('<f',address)
        if not math.isfinite(value):raise ValueError('Nonfinite boost field')
        return value
    owner=read('<I',base+0x77c);gp=0x4a30f0
    return dict(award_context=dict(reward_mask=read('<I',base+0xb28),enable_tricky=bool(read('<I',base+0xb2c))),profile=dict(full_threshold=f(gp-0x7bfc),medium_threshold=f(gp-0x7bf8),drain_per_tick=f(gp-0x7bf4),tick_seconds=f(gp-0x7958),modifier_threshold=f(gp-0x7954),super_timer_floor=f(gp-0x7950),normal_decay=f(gp-0x794c),fast_decay=f(gp-0x7948)),
        state=dict(meter=f(base+0x2f8),amount=f(base+0x2fc),window=f(base+0x2e8),tier=read('<i',base+0x2f4),
                   drain_enabled=read('<i',base+0x304),feedback_flags=read('<B',owner+0xd27),modifier=f(base+0x2ec),super_time=f(base+0x2f0)),
        provenance=dict(controller='114130',request_flags='2F6AC8(owner+D20,0)',callbacks=['298D90','299368','2992D8'],
                        scope='Boost control only; meter rewards, timer advancement and callback systems are separate'))
