"""Data-only initial state for the native controller5 animation selector."""
import math,struct

def extract_air_animation_state(memory,rider):
    def read(fmt,at):
        if not 0<=at<=len(memory)-struct.calcsize('<'+fmt):raise ValueError('Air animation pointer outside memory')
        return struct.unpack_from('<'+fmt,memory,at)[0]
    def f(at):
        value=read('f',at)
        if not math.isfinite(value):raise ValueError('Nonfinite air animation state')
        return value
    def triplet(at):return dict(current=f(at),rate=f(at+4),target=f(at+8))
    animation=read('I',rider+0x784)
    return dict(adjustment28c=triplet(rider+0x28c),adjustment298=triplet(rider+0x298),next_rate=f(animation+0x1c),
        provenance=dict(selector='13437C..134C6C',animation_address=hex(animation),
            next_rate_contract='Preserved when semantic unchanged; consumed by play and reset to1 only after play'))
