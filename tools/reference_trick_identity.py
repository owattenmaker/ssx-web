"""Extract 11A8C8 identity-builder inputs and its two authored rotation tables."""
import struct, math

def extract_trick_identity(memory,rider):
    def u(at):return struct.unpack_from('<I',memory,at)[0]
    def i(at):return struct.unpack_from('<i',memory,at)[0]
    def f(at):
        value=struct.unpack_from('<f',memory,at)[0]
        if not math.isfinite(value):raise ValueError('Nonfinite trick identity input')
        return value
    base=u(rider+0x790)
    if base<0 or base+0x1b0>len(memory) or u(base+0x1ac)!=rider:raise ValueError('Invalid scoring owner')
    state={k:i(base+off) for k,off in [('stance00',0),('field04',4),('field08',8),('style0c',12),('flag10',16),('style20',0x20),('flag28',0x28),('active70',0x70),('field7c',0x7c)]}
    state.update({k:f(base+off) for k,off in [('time24',0x24),('time2c',0x2c),('spin34',0x34),('flip38',0x38)]})
    state['grabs60']=[i(base+0x60+n*4) for n in range(3)]
    named=[dict(id=u(0x43d608+n*16),points=i(0x43d60c+n*16),identity_fields=list(memory[0x43d610+n*16:0x43d617+n*16])) for n in range(24)]
    return dict(commit_profile=dict(named_point_scale=f(0x49b6fc),score_scale=f(0x49b5ec),spin_scale=f(0x49b6dc),flip_scale=f(0x49b6e0)),named_tricks=named,state=state,profile=dict(spin_degrees=f(0x4a30f0-0x79e4),flip_degrees=f(0x4a30f0-0x79e0),ordinary=list(memory[0x43d388:0x43d388+319]),alternate=list(memory[0x43d4c8:0x43d4c8+319])),rider_stance=bool(i(rider+0x324)),provenance=dict(function='11A8C8',tables=['43D388','43D4C8'],scope='Identity construction; landing-call inputs remain caller-owned'))
