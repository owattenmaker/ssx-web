#!/usr/bin/env python3
"""Offline SSX3 world decoding. No console code executes in the native engine.

SPDX-License-Identifier: GPL-3.0-only
Format references: GlitcherOG/SSX-Library (see docs/asset-formats.md).
"""
import struct
import math
from pathlib import Path


def refpack(data, limit=64 * 1024 * 1024):
    if len(data) < 5 or data[1] != 0xfb or data[0] & 0x7e != 0x10:
        raise ValueError('Unsupported RefPack header')
    width = 4 if data[0] & 0x80 else 3
    size = int.from_bytes(data[2:2 + width], 'big')
    if size > limit:
        raise ValueError('RefPack output exceeds limit')
    pos = 2 + width * (2 if data[0] & 1 else 1)
    out = bytearray()

    def read(n):
        nonlocal pos
        if pos + n > len(data):
            raise ValueError('Truncated RefPack command')
        result = data[pos:pos + n]
        pos += n
        return result

    while len(out) < size:
        c = read(1)[0]
        distance = count = 0
        if c < 0x80:
            b = read(1)[0]
            literal, count, distance = c & 3, ((c >> 2) & 7) + 3, ((c & 0x60) << 3) + b + 1
        elif c < 0xc0:
            b, d = read(2)
            literal, count, distance = b >> 6, (c & 63) + 4, ((b & 63) << 8) + d + 1
        elif c < 0xe0:
            b, d, e = read(3)
            literal, count, distance = c & 3, ((c & 12) << 6) + e + 5, ((c & 16) << 12) + (b << 8) + d + 1
        else:
            literal = (c & 3) if c >= 0xfc else ((c & 31) + 1) * 4
        if len(out) + literal + count > size:
            raise ValueError('RefPack command exceeds declared size')
        out.extend(read(literal))
        if count:
            if distance > len(out):
                raise ValueError('RefPack match before output')
            for _ in range(count):
                out.append(out[-distance])
        if c >= 0xfc:
            break
    if len(out) != size:
        raise ValueError('RefPack size mismatch')
    return bytes(out)


def world_chunks(path):
    """SSB compressed blocks concatenate until CEND, including split records."""
    assembled = bytearray()
    with Path(path).open('rb') as stream:
        while header := stream.read(8):
            if len(header) != 8 or header[:4] not in (b'CBXS', b'CEND'):
                raise ValueError(f'Invalid world block at {stream.tell() - 8}')
            size, = struct.unpack_from('<I', header, 4)
            if not 8 < size <= 16 * 1024 * 1024:
                raise ValueError('Invalid world block size')
            block = stream.read(size - 8)
            if len(block) != size - 8:
                raise ValueError('Truncated world block')
            assembled.extend(refpack(block))
            if header[:4] == b'CEND':
                yield bytes(assembled)
                assembled.clear()
    if assembled:
        raise ValueError('Unterminated world chunk')


def records(chunk, byteorder='little'):
    pos = 0
    while pos < len(chunk):
        if pos + 8 > len(chunk):
            raise ValueError('Truncated world record')
        kind, track = chunk[pos], chunk[pos + 4]
        size = int.from_bytes(chunk[pos + 1:pos + 4], byteorder)
        rid = int.from_bytes(chunk[pos + 5:pos + 8], byteorder)
        pos += 8
        if pos + size > len(chunk):
            raise ValueError('World record extends past chunk')
        yield kind, track, rid, chunk[pos:pos + size]
        pos += size


def locations(path, endian='<'):
    data = Path(path).read_bytes()
    count, = struct.unpack_from(endian+'I', data, 8)
    if len(data) < 80 + count * 88:
        raise ValueError('Truncated SDB locations')
    result = []
    for i in range(count):
        p = 80 + i * 88
        name = data[p:p + 16].split(b'\0')[0].decode('ascii')
        sub_count, chunk_count, chunk_end, sub_start = struct.unpack_from(endian+'4I', data, p + 16)
        result.append(dict(name=name, sub_count=sub_count, chunk_count=chunk_count,
                           chunk_end=chunk_end, sub_start=sub_start))
    return result


def event_locations(locs, name):
    """Locations resident in the race event of course `name`, in SDB (= load) order.

    The original loads the event course plus its two connector locations (ELF location
    table 0x43E250 kind 2, e.g. A_ARA1 and ARA1_B for ARA1); the streaming table
    0x442168 of every Snow Jam event savestate holds exactly these (plus TRANSP and the
    sky location) in state 2 (tools/export_event_membership.py). Returns
    [(index, name, begin_chunk, end_chunk)]; the resource track of a location is its
    SDB index."""
    selected = next((i for i, l in enumerate(locs) if l['name'] == name), None)
    if selected is None:
        raise ValueError(f'Unknown location {name}')
    wanted = [i for i, l in enumerate(locs) if i == selected or ('_' in l['name'] and name in l['name'].split('_'))]
    return [(i, locs[i]['name'], locs[i - 1]['chunk_end'] + 1 if i else 0, locs[i]['chunk_end']) for i in wanted]


def texture_rgba(data):
    """PS2 SHAPE records: decode palette and texel layout during import only."""
    if len(data) < 128:
        raise ValueError('Truncated texture header')
    fmt = data[0]
    size = int.from_bytes(data[1:4], 'little')
    w, h = struct.unpack_from('<HH', data, 4)
    if not w or not h or max(w, h) > 4096:
        raise ValueError('Invalid texture dimensions')
    if fmt == 5:
        rgba = bytearray(data[128:128 + w * h * 4])
        if len(rgba) != w * h * 4:
            raise ValueError('Truncated RGBA texture')
        return w, h, bytes(rgba)
    if fmt not in (1, 2):
        raise ValueError(f'Unsupported texture format {fmt}')
    if size < 128 or size + 128 > len(data):
        raise ValueError('Invalid palette offset')
    count, = struct.unpack_from('<H', data, size + 8)
    if not 1 <= count <= 256 or size + 128 + count * 4 > len(data):
        raise ValueError('Invalid palette extent')
    palette = []
    for i in range(256 if fmt == 2 else 16):
        source = (i & 0xe7) | ((i & 8) << 1) | ((i & 16) >> 1) if fmt == 2 else i
        palette.append(data[size + 128 + source * 4:size + 132 + source * 4] if size + 132 + source * 4 <= len(data) else b'\0\0\0\0')
    if max(p[3] for p in palette) <= 128:
        palette = [p[:3] + bytes([min(255, p[3] * 2)]) for p in palette]
    texels = data[128:size]
    # A uniform 4-bit tile is invariant under any texel permutation. This also
    # covers the game's 16x16 single-color shapes, whose compact storage omits
    # the padding addressed by the general large-page swizzle layout.
    if fmt==1 and texels and len(set(texels))==1 and texels[0]&15==texels[0]>>4:
        index=texels[0]&15
        if size+132+index*4>len(data): raise ValueError('Palette index outside table')
        return w,h,palette[index]*(w*h)
    out = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            swap = (((y + 2) >> 2) & 1) * 4
            row = (((y & ~3) >> 1) + (y & 1)) & 7
            if fmt == 2:
                address = ((y & ~15) * w + (x & ~15) * 2 + row * w * 2
                           + ((x + swap) & 7) * 4 + ((y >> 1) & 1) + ((x >> 2) & 2))
                shift = 0
            else:
                pages_v = (h + 127) // 128
                page = (y // 128) * ((w + 127) // 128) + x // 128
                address = ((page // pages_v) * 32 * h * 2 + (page % pages_v) * 64 * 4
                           + (((x & 127) & ~31) >> 1) * h + ((y & 127) & ~15) * 2
                           + row * h * 2 + ((x + swap) & 7) * 4 + ((x >> 3) & 3))
                shift = ((y >> 1) & 1) * 4
            if address >= len(texels):
                raise ValueError('Swizzled texel outside image')
            index = (texels[address] >> shift) & (255 if fmt == 2 else 15)
            if size + 132 + (((index & 0xe7) | ((index & 8) << 1) | ((index & 16) >> 1)) if fmt == 2 else index) * 4 > len(data):
                raise ValueError('Palette index outside table')
            at = (y * w + x) * 4
            out[at:at + 4] = palette[index]
    return w, h, bytes(out)


def gamecube_cmpr(data, header_size=32):
    """Decode native CMPR tiled BC1 into ordinary RGBA during asset preparation."""
    if header_size not in (16,32) or len(data) < header_size or data[0] != 30:
        raise ValueError('Expected GameCube CMPR shape')
    w,h = struct.unpack_from('>HH', data, 4)
    if not w or not h or max(w,h)>4096 or w%8 or h%8 or len(data)<header_size+w*h//2:
        raise ValueError('Invalid CMPR extent')
    out=bytearray(w*h*4)
    at=header_size
    def rgb565(c):
        r,g,b=(c>>11)&31,(c>>5)&63,c&31
        return ((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2),255)
    for ty in range(0,h,8):
        for tx in range(0,w,8):
            for dy,dx in ((0,0),(0,4),(4,0),(4,4)):
                c0,c1=struct.unpack_from('>HH',data,at)
                a,b=rgb565(c0),rgb565(c1)
                if c0>c1:
                    palette=(a,b,tuple((2*a[k]+b[k])//3 for k in range(4)),tuple((a[k]+2*b[k])//3 for k in range(4)))
                else:
                    palette=(a,b,tuple((a[k]+b[k])//2 for k in range(4)),(0,0,0,0))
                for y in range(4):
                    for x in range(4):
                        index=(data[at+4+y]>>(6-x*2))&3
                        target=((ty+dy+y)*w+tx+dx+x)*4
                        out[target:target+4]=bytes(palette[index])
                at+=8
    return w,h,bytes(out)


def world_resource_names(phm,psm):
    """Bounded PS2 PHM/PSM lookup, following SSX-Library PHM/PSMHandler.

    Arrays: terrain0, instances1, models2, splines3, collision4. Names retain
    authored helper identities that must not become physical scenery by accident.
    """
    def read(data,fmt,offset):
        size=struct.calcsize('<'+fmt)
        if offset<0 or offset+size>len(data):raise ValueError('World name-table extent')
        return struct.unpack_from('<'+fmt,data,offset)
    count=read(phm,'I',8)[0]
    if count!=read(psm,'I',8)[0] or not 1<=count<=32:raise ValueError('World name-array count mismatch')
    hp=sp=12;groups=[]
    for _ in range(count):
        _,entries=read(phm,'2I',hp);hp+=8
        _,strings=read(psm,'2I',sp);sp+=8
        if entries!=strings or entries>1000000:raise ValueError('World resource-name count mismatch')
        group={}
        for _ in range(entries):
            _,_,resource,_=read(phm,'4I',hp);hp+=16
            end=psm.find(b'\0',sp)
            if end<0:raise ValueError('Unterminated world resource name')
            name=psm[sp:end].decode('utf-8');sp=end+1
            key=resource&255,resource>>8
            if key in group and group[key]!=name:raise ValueError('Ambiguous world resource name')
            group[key]=name
        sp=(sp+3)&~3
        if sp>len(psm):raise ValueError('World name alignment outside file')
        groups.append(group)
    return groups


def world_model_role(name):
    # Original authored names explicitly identify trigger-helper meshes. This is
    # deliberately not a geometric size/color heuristic or collision-material guess.
    return 'trigger_volume' if name.startswith('mdl_') and 'trig' in name.lower() else 'scenery'


def world_vertex_to_native(vertex):
    """Convert completed source Z-up world geometry to native Y-up once.

    Use only after original model-node and world-instance transforms. Positions
    already use meters here; both position and normal receive the same proper
    rotation (x,y,z)->(x,z,-y), preserving winding, UVs and lighting coordinates.
    """
    if len(vertex)!=10 or not all(math.isfinite(v) for v in vertex):
        raise ValueError('Invalid complete world vertex')
    x,y,z,nx,ny,nz,*uv=vertex
    return [x,z,-y,nx,nz,-ny,*uv]


def patch_texture_uv(corners, u, v):
    """Original2EDB20 corner order: (0,0), (0,1), (1,0), (1,1).

    Same polynomial parameters as the terrain power basis; leave repetition
    to the sampler instead of wrapping the offline vertex coordinates.
    """
    return [corners[0][k]*(1-u)*(1-v)+corners[2][k]*u*(1-v)+corners[1][k]*(1-u)*v+corners[3][k]*u*v for k in range(2)]


def patch_mesh(data, subdivisions=8):
    """Evaluate bicubic power basis in SOURCE Z-up coordinates; positions in meters."""
    if len(data) != 432 or not 1 <= subdivisions <= 64:
        raise ValueError('Unexpected terrain patch layout or resolution')
    coefficients = [struct.unpack_from('<3f', data, 64 + i * 16) for i in reversed(range(16))]
    uv = [struct.unpack_from('<2f', data, 32 + i * 8) for i in range(4)]
    light = struct.unpack_from('<4f', data, 16)
    texture, lightmap = struct.unpack_from('<2h', data, 416)
    vertices = []
    for y in range(subdivisions + 1):
        v = y / subdivisions
        for x in range(subdivisions + 1):
            u = x / subdivisions
            up, vp = (1, u, u*u, u*u*u), (1, v, v*v, v*v*v)
            p = [sum(coefficients[j*4+i][k]*up[i]*vp[j] for j in range(4) for i in range(4)) / 100 for k in range(3)]
            du = [sum(coefficients[j*4+i][k]*i*up[i-1]*vp[j] for j in range(4) for i in range(1, 4)) for k in range(3)]
            dv = [sum(coefficients[j*4+i][k]*up[i]*j*vp[j-1] for j in range(1, 4) for i in range(4)) for k in range(3)]
            normal = [du[1]*dv[2]-du[2]*dv[1], du[2]*dv[0]-du[0]*dv[2], du[0]*dv[1]-du[1]*dv[0]]
            length = math.sqrt(sum(a*a for a in normal))
            normal = [a / length for a in normal] if length > 1e-10 else [0, 0, 1]
            tex = patch_texture_uv(uv, u, v)
            vertex = p + normal + tex + [light[0]+u*light[2], light[1]+v*light[3]]
            if not all(math.isfinite(a) for a in vertex):
                raise ValueError('Nonfinite terrain vertex')
            vertices.append(vertex)
    indices = []
    for y in range(subdivisions):
        for x in range(subdivisions):
            a = y*(subdivisions+1)+x
            b, c, d = a+1, a+subdivisions+1, a+subdivisions+2
            indices.extend((a, b, c, b, d, c))
    return vertices, indices, texture, lightmap
