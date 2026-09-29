"""Original115D48 persistent inputs; no expected animation or pose output."""
import struct


def extract_upper_reaction(memory, rider):
    def u(at): return struct.unpack_from('<I', memory, at)[0]
    def f(at): return struct.unpack_from('<f', memory, at)[0]
    def q(at): return struct.unpack_from('<Q', memory, at)[0]
    #1298C8 reads the race's integer logic tick, not milliseconds. Consecutive
    #glide states338->339 confirm the unit; the600 threshold is600logic ticks.
    race = u(u(u(0x4a30f0-0x848)+0x84)+12)
    return dict(idle_seconds=f(rider+0x35c), clock_tick=u(race+8),
        reaction_mask=q(rider+0x8c0), lookback_mask=q(rider+0x8d0),
        peers=[dict(enabled=bool(u(rider+i*36)),
            distance_cm=f(rider+i*36+8), bearing=f(rider+i*36+12),
            attack_eligible=bool(u(rider+i*36+28)),
            last_reaction_tick=u(rider+i*36+32)) for i in range(6)])
