"""Authored311710 lookup leaves and eligibility inputs for native playback."""
import struct


def extract_animation_variants(memory, semantics=range(437)):
    def u(at): return struct.unpack_from('<I', memory, at)[0]
    table=u(0x4a30f0+0xd0c)
    packed=u(0x4a30f0+0xd8c)+0x1030
    result={}
    for semantic in semantics:
        first,count=struct.unpack_from('<2h',memory,table+semantic*4)
        if first<0 or count<0: raise ValueError('Negative authored variant range')
        variants=[]
        for i in range(first,first+count):
            leaf,weight,flags=struct.unpack_from('<3I',memory,0x449960+i*12)
            variants.append(dict(leaf=leaf,weight=weight,allowed_flags=flags,
                clip=None if leaf==519 else u(packed+leaf*4)))
        result[str(semantic)]=variants
    return result


def extract_animation_variant_flags(memory,rider):
    def u(at): return struct.unpack_from('<I',memory,at)[0]
    animator=u(rider+0x784)
    alternate=u(animator+0x64)
    primary=u(animator+0x60)
    return u(alternate+0xcd8) if alternate else u(primary+0x364) if primary else 0
