"""Recover original rider rail-query anchors and control triplets."""
import struct

def extract_memory(memory,rider=0x14701a0):
 u=lambda offset:struct.unpack_from('<I',memory,rider+offset)[0]
 vector=lambda offset,n:list(struct.unpack_from('<'+str(n)+'f',memory,rider+offset))
 bone=u(0x8a0)
 if bone!=22:raise ValueError('Unverified original rail query bone')
 return dict(board_root_bone=bone,offset9d0=vector(0x9d0,3),steer=vector(0x22c,3),balance=vector(0x238,3),tolerance=vector(0x25c,3),provenance=dict(rider=rider,board_accessor='108A48/1086B8 rider+8A0',triplet_order='current,rate,target'))
