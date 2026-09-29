"""Offline PS2 MDR world models and instance transforms.

All intermediate geometry and matrices retain the original Z-up basis. The world
package writer converts completed world-space vertices after instance transforms.

SPDX-License-Identifier: GPL-3.0-only
Reference: SSX-Library WorldMDR/WorldInstance; all accesses are extent checked.
"""
import math
import struct

IDENTITY=(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)


class Reader:
    def __init__(self,data,endian='<'): self.data,self.endian=data,endian
    def read(self,fmt,p):
        if p<0 or p+struct.calcsize('<'+fmt)>len(self.data):
            raise ValueError(f'MDR extent outside resource: {p} {fmt} / {len(self.data)}')
        return struct.unpack_from(self.endian+fmt,self.data,p)
    def u32(self,p): return self.read('I',p)[0]
    def object_id(self,p):
        value=self.u32(p)
        return value&255,value>>8


def align16(value): return (value+15)&~15


def multiply(a,b):
    return tuple(sum(a[r*4+k]*b[k*4+c] for k in range(4)) for r in range(4) for c in range(4))


def transform(p,m,normal=False):
    if normal:
        a,b,c,d,e,f,g,h,i=(m[k] for k in (0,1,2,4,5,6,8,9,10))
        cof=(e*i-f*h,f*g-d*i,d*h-e*g,c*h-b*i,a*i-c*g,b*g-a*h,b*f-c*e,c*d-a*f,a*e-b*d)
        determinant=a*cof[0]+b*cof[1]+c*cof[2]
        if abs(determinant)<1e-12: raise ValueError('Singular instance transform')
        result=[sum(p[k]*cof[k*3+j] for k in range(3))/determinant for j in range(3)]
        length=math.sqrt(sum(v*v for v in result))
        return [v/length for v in result] if length else [0,1,0]
    return [sum(p[k]*m[k*4+j] for k in range(3))+m[12+j] for j in range(3)]


def decode_model(data):
    r=Reader(data)
    node_count=r.u32(4)
    node_offset=r.u32(8)
    scale=r.read('3f',24)
    data_base=r.u32(36)
    material_count=r.u32(40)
    if node_count>4096 or material_count>4096:
        raise ValueError('Unreasonable MDR node/material count')
    materials=[r.object_id(44+i*4) for i in range(material_count)]
    nodes=[]
    for i in range(node_count):
        parent,mesh_header,_,matrix_at=r.read('4I',node_offset+i*16)
        matrix=r.read('16f',matrix_at) if matrix_at not in (0,0xffffffff) else IDENTITY
        nodes.append((parent,mesh_header,matrix))
    matrices={}
    def matrix_for(i,visiting=None):
        if i in matrices: return matrices[i]
        visiting=set() if visiting is None else visiting
        if i>=len(nodes) or i in visiting: raise ValueError('Invalid MDR node hierarchy')
        visiting.add(i)
        parent,_,matrix=nodes[i]
        if parent!=0xffffffff: matrix=multiply(matrix,matrix_for(parent,visiting))
        matrices[i]=matrix
        return matrix
    meshes=[]
    color_offset=0
    for node,(_,mesh_header,_) in enumerate(nodes):
        if mesh_header in (0,0xffffffff): continue
        matrix=matrix_for(node)
        group_count,groups_at=r.read('2I',mesh_header+28)
        if group_count>4096: raise ValueError('Unreasonable MDR mesh group count')
        for g in range(group_count):
            group_at=r.u32(groups_at+g*4)
            material,flags=r.read('2H',group_at)
            chain_at=(r.u32(group_at+4)&0xffffff)+data_base
            entries=[]
            for _ in range(4096):
                tag,address=r.read('2I',chain_at)
                if address>>24 not in (0,0x80): raise ValueError(f'Unsupported MDR address flags {tag:08x} {address:08x} at {chain_at:x}')
                chain_at=align16(chain_at+8)
                if tag>>24==0x60: break
                if address&0xffffff: entries.append(data_base+(address&0xffffff))
            else: raise ValueError('Unterminated MDR data chain')
            pairs=[]
            pending=None
            seen_normals=set()
            for at in entries:
                is_normal=r.read('2I',at)==(0x20000000,0x40404040)
                if is_normal:
                    if pending is not None:
                        pairs.append((pending,at));pending=None
                    elif at not in seen_normals:
                        raise ValueError('MDR normal block has no vertex block')
                    # Some material chains re-emit existing normal blocks for
                    # another draw pass. Do not interpret them as vertices.
                    seen_normals.add(at)
                else:
                    if pending is not None: raise ValueError('MDR vertices missing normals')
                    pending=at
            if pending is not None: raise ValueError('Unpaired MDR vertex block')
            first=True
            for vertex_at,normal_at in pairs:
                at=vertex_at+(32 if first else 0)
                first=False
                strip_count=r.u32(at+32)
                count=r.u32(at+40)
                if not 1<=count<=256 or strip_count>256:
                    raise ValueError(f'Unexpected MDR vertex block: {count}, {strip_count}')
                strips=r.read(f'{strip_count}H',at+64)
                if sum(strips)!=count: raise ValueError('MDR strip lengths do not match vertices')
                uv_at=align16(at+64+strip_count*2)+16
                pos_at=align16(uv_at+count*4)+16
                norm_at=normal_at
                normal_count=r.read('B',norm_at+14)[0] or 256
                if normal_count!=count: raise ValueError('MDR normal count mismatch')
                vertices=[]
                for v in range(count):
                    uv=[a/4096 for a in r.read('2h',uv_at+v*4)]
                    pos=[a/32768*scale[k] for k,a in enumerate(r.read('3h',pos_at+v*6))]
                    normal=[a/32768 for a in r.read('3h',norm_at+16+v*6)]
                    # Mesh node transforms are composed before the instance transform.
                    vertices.append(transform(pos,matrix)+transform(normal,matrix,True)+uv+[0,0])
                indices=[]
                start=0
                for length in strips:
                    for v in range(2,length):
                        tri=(start+v-2,start+v-1,start+v)
                        if v&1: tri=tri[::-1]
                        indices.extend(tri)
                    start+=length
                if material>=len(materials): raise ValueError('MDR material outside list')
                meshes.append(dict(vertices=vertices,indices=indices,material=materials[material],
                                   color_offset=color_offset,group_flags=flags))
                color_offset+=count
    return meshes


def decode_instance(data):
    r=Reader(data)
    matrix=r.read('16f',16)
    if not all(math.isfinite(v) for v in matrix): raise ValueError('Nonfinite instance matrix')
    model_id=r.object_id(128)
    scale=r.read('f',132)[0]
    if not math.isfinite(scale): raise ValueError('Invalid instance uniform scale')
    colors=[]
    at=160
    while at+4<=len(data):
        word=r.u32(at);at+=4
        if word>>24 not in (0x6f,0x7f): continue
        count=((word>>16)&255) or 256
        for value in r.read(f'{count}H',at):
            colors.append(((value&31)/31,((value>>5)&31)/31,((value>>10)&31)/31,1))
        at=(at+count*2+3)&~3
    return model_id,matrix,colors,scale
