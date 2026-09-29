"""Synthetic malformed-input and format fixtures; no game assets required."""
import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from world_assets import refpack, records, patch_mesh, gamecube_cmpr, world_vertex_to_native, world_resource_names, world_model_role
from world_models import transform, multiply, IDENTITY, decode_model, decode_instance


class WorldAssetsTests(unittest.TestCase):
    def test_refpack_literals_and_overlapping_match(self):
        # ABCD literal then a distance-4 match, including overlapping output.
        packed=b'\x10\xfb\x00\x00\x10\xe0ABCD\x88\x00\x03'
        self.assertEqual(refpack(packed),b'ABCD'*4)

    def test_refpack_rejects_truncation_and_bad_reference(self):
        for data in (b'\x10\xfb\x00\x00\x04\xe0ABC',
                     b'\x10\xfb\x00\x00\x03\x00\x01',
                     b'\x10\xfb\xff\xff\xff'):
            with self.assertRaises(ValueError): refpack(data,limit=1024)

    def test_record_endianness_and_bounds(self):
        for endian in ('little','big'):
            data=bytes([1])+(3).to_bytes(3,endian)+bytes([8])+(0x1234).to_bytes(3,endian)+b'abc'
            self.assertEqual(list(records(data,endian)),[(1,8,0x1234,b'abc')])
            with self.assertRaises(ValueError): list(records(data[:-1],endian))

    def test_bicubic_plane_scale_and_normals(self):
        data=bytearray(432)
        # P(u,v)=(100*u,0,100*v) cm in reverse power coefficient order.
        struct.pack_into('<3f',data,64+(15-1)*16,100,0,0)
        struct.pack_into('<3f',data,64+(15-4)*16,0,0,100)
        struct.pack_into('<8f',data,32,0,0,1,0,0,1,1,1)
        struct.pack_into('<4f',data,16,.25,.5,.125,.25)
        struct.pack_into('<2h',data,416,7,9)
        vertices,indices,tex,light=patch_mesh(data,2)
        self.assertEqual((len(vertices),len(indices),tex,light),(9,24,7,9))
        self.assertEqual(vertices[-1][:3],[1,0,1])
        self.assertEqual(vertices[4][6:],[.5,.5,.3125,.625])
        self.assertEqual(abs(vertices[4][4]),1)
        self.assertTrue(all(i<len(vertices) for i in indices))

    def test_terrain_uv_corners_follow_original_sampler(self):
        data=bytearray(432)
        struct.pack_into('<3f',data,64+(15-1)*16,100,0,0)
        struct.pack_into('<3f',data,64+(15-4)*16,0,200,0)
        for i,uv in enumerate(((0,0),(0,2),(3,0),(3,2))):
            struct.pack_into('<2f',data,32+i*8,*uv)
        vertices,_,_,_=patch_mesh(data,2)
        self.assertEqual(vertices[1][6:8],[1.5,0])
        self.assertEqual(vertices[3][6:8],[0,1])
        self.assertEqual(vertices[8][6:8],[3,2])

    def test_native_world_basis_for_flat_source_xy_terrain(self):
        data=bytearray(432)
        # Actual source convention: terrain lies in XY and elevation is Z.
        struct.pack_into('<3f',data,64+15*16,0,0,300)
        struct.pack_into('<3f',data,64+(15-1)*16,100,0,0)
        struct.pack_into('<3f',data,64+(15-4)*16,0,200,0)
        vertices,indices,_,_=patch_mesh(data,1)
        native=list(map(world_vertex_to_native,vertices))
        self.assertEqual(native[-1][:6],[1,3,-2,0,1,0])
        a,b,c=[native[i][:3] for i in indices[:3]]
        e1=[b[i]-a[i] for i in range(3)];e2=[c[i]-a[i] for i in range(3)]
        self.assertGreater(e1[2]*e2[0]-e1[0]*e2[2],0) # Winding remains upward.

    def test_native_conversion_follows_instance_transform(self):
        matrix=(2,0,0,0,0,3,0,0,0,0,4,0,10,20,30,1)
        source=transform([1,2,3],matrix)
        normal=transform([1,1,0],matrix,True)
        vertex=[v/100 for v in source]+normal+[.2,.4,.6,.8]
        converted=world_vertex_to_native(vertex)
        self.assertEqual(converted[:3],[.12,.42,-.26])
        self.assertEqual(converted[3:6],[normal[0],normal[2],-normal[1]])
        self.assertEqual(converted[6:],vertex[6:])
        self.assertNotEqual(converted[:3],[v/100 for v in transform([1,3,-2],matrix)])

    def test_world_name_tables_and_trigger_identity(self):
        phm=struct.pack('<3I',0,0,1)+struct.pack('<2I',0,2)+struct.pack('<8I',1,2,(295<<8)|8,4,1,2,(296<<8)|8,4)
        strings=b'mdl_ARA1_startfireTrig_1000\0mdl_ARA1_startdividerlong_eb_1001\0'
        psm=struct.pack('<3I',0,0,1)+struct.pack('<2I',0,2)+strings
        psm+=b'\0'*((-len(psm))%4)
        names=world_resource_names(phm,psm)[0]
        self.assertEqual(world_model_role(names[8,295]),'trigger_volume')
        self.assertEqual(world_model_role(names[8,296]),'scenery')
        with self.assertRaises(ValueError):world_resource_names(phm[:-1],psm)
        with self.assertRaises(ValueError):world_resource_names(phm,psm[:-5])

    def test_cmpr_tile_order_and_endpoint_colors(self):
        header=bytearray(32);header[0]=30;struct.pack_into('>HH',header,4,8,8)
        colors=(0xf800,0x07e0,0x001f,0xffff)
        data=header+b''.join(struct.pack('>HHI',c,0,0) for c in colors)
        w,h,pixels=gamecube_cmpr(data)
        self.assertEqual((w,h),(8,8))
        for (x,y),rgba in zip(((0,0),(4,0),(0,4),(4,4)),
                              ((255,0,0,255),(0,255,0,255),(0,0,255,255),(255,255,255,255))):
            p=(y*w+x)*4
            self.assertEqual(tuple(pixels[p:p+4]),rgba)
        with self.assertRaises(ValueError): gamecube_cmpr(data[:-1])
        compact=data[:16]+data[32:]
        self.assertEqual(gamecube_cmpr(compact,header_size=16),(w,h,pixels))

    def test_instance_transform_and_inverse_transpose_normal(self):
        m=(2,0,0,0,0,3,0,0,0,0,4,0,10,20,30,1)
        self.assertEqual(transform([1,2,3],m),[12,26,42])
        n=transform([1,1,0],m,True)
        self.assertAlmostEqual(n[0]/n[1],1.5)
        self.assertEqual(multiply(IDENTITY,m),m)
        with self.assertRaises(ValueError): transform([1,0,0],(0,)*16,True)

    def test_model_and_instance_extents(self):
        with self.assertRaises(ValueError): decode_model(b'\0'*8)
        with self.assertRaises(ValueError): decode_instance(b'\0'*80)


if __name__=='__main__': unittest.main()
