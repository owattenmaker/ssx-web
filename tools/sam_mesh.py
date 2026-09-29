#!/usr/bin/env python3
"""Author Sam's skinned mesh and export directly to the native SSX mesh format.

Uses the recovered body/board rig and derived shirt, trouser, glove, boot and
binding geometry from the user's Mac assets, keeping those parts' original UV
layout and skin weights so they deform like the roster. The head is Mac's original
HeadA_NIS + eyes + scalp warped to Sam's photo-fitted landmarks with its own painted
map (tools/sam_head.py); cap, hair clumps, collar, board and accessories are authored
here, fitted around that head. All runtime textures are the five-map set of an
original rider (256 suit + 128 boots/gloves, 128x256 head, board, gear) painted at
the roster's texel density. No console runtime is used by the asset.

  python3 tools/sam_mesh.py --all      # four native packages (+ GLB/OBJ authoring exports)
  python3 tools/build_sam_web.py       # package RIDER_SAM for the browser and compile its skin

Requires numpy and Pillow.
"""
import argparse, hashlib, json, math, os, shutil, struct
from pathlib import Path
import numpy as np
from PIL import Image
from sam_model_details import GarmentSurface, cap, beanie, soft_box, hair_skirt, collar, glasses
from sam_textures import (GEAR, BOARD, HAIR, paint_suit, paint_boots, paint_face, paint_board, paint_gear,
                          skin_tone, region_uv, packers_decal)
import sam_head
from sam_lod import build_lods

ROOT=Path(os.environ.get('SSX_ROOT') or Path(__file__).resolve().parents[1])
SOURCE=ROOT/'local/assets/native/RIDER_MAC'
ART=ROOT/'sam_character/model'          # authored source artwork (face.png) and docs
OUT=Path(os.environ.get('SAM_MODEL_OUT') or ROOT/'local/sam-model')   # derived authoring exports (GLB/OBJ/PNG): git-ignored, they contain derived game data
NATIVE=Path(os.environ.get('SAM_NATIVE_ROOT') or ROOT/'local/assets/native')   # native packages (SAM_NATIVE_ROOT: a scratch build)
STAGE='roster_head_pass_4'

def unit(v):
    v=np.asarray(v,dtype=float);length=np.linalg.norm(v)
    return v/length if length>1e-12 else np.array([0.,1.,0.])

class Region:
    """A painted swatch of one shared runtime texture. Local UVs (0..1) map into its box."""
    def __init__(self,tex,table,name):self.tex=tex;self.table=table;self.name=name
    def uv(self,local):return region_uv(self.table,self.name,local)

class Mesh:
    def __init__(self):self.vertices=[];self.indices=[];self.skin=[];self.batches=[];self.parts=[];self.colors=[]
    def add(self,name,positions,faces,material,uv=None,weights=None,normals=None,tint=(1,1,1,1)):
        p=np.asarray(positions,dtype=float);f=np.asarray(faces,dtype=np.uint32).reshape(-1,3)
        if not len(f):return
        if normals is None:
            normals=np.zeros_like(p)
            for a,b,c in f:
                n=np.cross(p[b]-p[a],p[c]-p[a]);normals[[a,b,c]]+=n
            normals=np.array([unit(n) for n in normals])
        if isinstance(material,Region):
            uv=material.uv(np.full((len(p),2),.5) if uv is None else uv);material=material.tex
        if uv is None:uv=np.full((len(p),2),.5)
        if weights is None:weights=[[[3,1.]]] * len(p)
        used=np.unique(f);remap=np.zeros(len(p),dtype=np.uint32);remap[used]=np.arange(len(used))
        f=remap[f];p=p[used];normals=np.asarray(normals)[used];uv=np.asarray(uv)[used];weights=[weights[i] for i in used]
        base=len(self.vertices);first=len(self.indices)
        self.vertices.extend(np.column_stack([p,normals,uv,np.zeros((len(p),2))]).tolist())
        self.indices.extend((f+base).reshape(-1).tolist());self.skin.extend(weights);self.colors.extend([tint]*len(p))
        self.batches.append(dict(first_index=first,index_count=f.size,texture=material,lightmap=-1,instance=True))
        self.parts.append(dict(name=name,first_vertex=base,vertex_count=len(p),first_index=first,index_count=f.size,material=material))
    def ellipsoid(self,name,center,radii,material,bone=5,rings=8,sides=16):
        p=[];uv=[];faces=[]
        for j in range(rings+1):
            phi=math.pi*(j+.001)/(rings+.002)
            for i in range(sides+1):
                a=2*math.pi*i/sides
                p.append(np.array(center)+np.array(radii)*[math.sin(phi)*math.sin(a),math.cos(phi),math.sin(phi)*math.cos(a)])
                uv.append([i/sides,j/rings])
        for j in range(rings):
            for i in range(sides):
                a=j*(sides+1)+i;b=a+sides+1;faces.extend([[a,b,a+1],[a+1,b,b+1]])
        self.add(name,p,faces,material,uv,[[[bone,1.]]] * len(p))
    def tube(self,name,a,b,radius,material,bone=3,sides=8,cap_ends=False):
        axis=unit(np.array(b)-a);u=unit(np.cross(axis,[0,0,1] if abs(axis[2])<.9 else [0,1,0]));v=np.cross(axis,u)
        p=[np.array(c)+radius*(math.cos(i*2*math.pi/sides)*u+math.sin(i*2*math.pi/sides)*v) for c in [a,b] for i in range(sides)]
        faces=[]
        for i in range(sides):
            k=(i+1)%sides;faces.extend([[i,k,i+sides],[k,k+sides,i+sides]])
        if cap_ends:
            ca=len(p);p.extend([np.asarray(a),np.asarray(b)])
            for i in range(sides):
                k=(i+1)%sides;faces.extend([[ca,k,i],[ca+1,i+sides,k+sides]])
        self.add(name,p,faces,material,weights=[[[bone,1.]]] * len(p))

def ps2_family(name):
    """PS2 equipment family (tools/sam_ps2/SamParts.cs model file) of an authored part."""
    fixed={'derived_TopB':'top','derived_BottomA':'bottom','derived_BootsA':'boots','derived_HandsA':'hands','derived_BindingsA':'bindings','hair_clumps':'hair'}
    if name in fixed:return fixed[name]
    if name.startswith('K2_'):return 'board'
    if any(k in name for k in ('pack','fly_box','retractor','rod_','net_')):return 'back'   # fishing kits (Backpacks items)
    if name.startswith('Sam_') or any(k in name for k in ('cap','beanie','cuff_knit','glass','lens')):return 'head'
    return 'top'

def load_source():
    """The derived body's geometry, part triangles, UVs and source texture maps."""
    rig=json.loads((SOURCE/'rider.json').read_text());world=json.loads((SOURCE/'world.json').read_text())
    original=np.fromfile(SOURCE/'vertices.bin',dtype='<f4').reshape(-1,10).astype(float);indices=np.fromfile(SOURCE/'indices.bin',dtype='<u4')
    parts={}
    for part,batch in zip(rig['parts'],world['batches']):
        parts[part['part']]=indices[batch['first_index']:batch['first_index']+batch['index_count']].reshape(-1,3).astype(int)
    textures=[]
    for key in sorted(world['textures'],key=lambda k:int(k.split('-')[1])):
        t=world['textures'][key];textures.append(np.frombuffer((SOURCE/t['path']).read_bytes(),np.uint8).reshape(t['height'],t['width'],4))
    return rig,original,parts,dict(uv=original[:,6:8],pos=original[:,:3],nrm=original[:,3:6],parts=parts,textures=textures)

# Whole-outfit packages for Equip Gear (docs/characters.md "Sam's Equip Gear"): a top (the Sam build's Tops item)
# times a back kit (its Backpacks item). The fishing kits carry the chest pack; the rod kit also the fly box and the
# amber sunglasses, as the original fishing presets. Lodge Legend wears its amber sunglasses on its own.
TOPS={'rope_tow_regular':'RIDER_SAM','sunday_unc':'RIDER_SAM_PACKERS','uphill_club':'RIDER_SAM_UPHILL','lodge_legend':'RIDER_SAM_LODGE'}
BACKS={None:('',[]),'net':('_FISHING',['creek_kit','catch_and_release']),'tube':('_FISHING_TUBE',['creek_kit','one_more_cast','packed_for_the_creek','reading_the_water'])}
def combinations():
    for top in TOPS:
        for back in BACKS:yield top,back
def package_name(top,back):return TOPS[top]+BACKS[back][0]

def build(outfit='rope_tow_regular',accessories=(),back=None):
    # legacy call: build('creekside_unc',[...]) = the default top with a back kit
    if outfit=='creekside_unc':
        outfit='rope_tow_regular';back='tube' if 'packed_for_the_creek' in accessories else 'net'
    if back is not None:accessories=BACKS[back][1]
    if outfit=='lodge_legend' and 'reading_the_water' not in accessories:accessories=[*accessories,'reading_the_water']
    package=package_name(outfit,back)
    folder=NATIVE/package;folder.mkdir(parents=True,exist_ok=True)
    if (folder/'textures').exists():shutil.rmtree(folder/'textures')
    (folder/'textures').mkdir()
    rig,original,parts,source=load_source()
    packers=outfit=='sunday_unc'
    textures={};mesh=Mesh()
    def texture(image,note,path=None):
        image=image.convert('RGBA');i=len(textures);name=f'textures/9-{i}.rgba';(folder/name).write_bytes(image.tobytes())
        entry=dict(width=image.width,height=image.height,path=name,source=note)
        if path:entry['source_sha256']=hashlib.sha256(Path(path).read_bytes()).hexdigest()
        textures[f'9-{i}']=entry;return i
    decal=packers_decal(Image.open(ROOT/'sam_character/design/wardrobe/sam-wardrobe-options.png')) if packers else None
    suit=texture(paint_suit(source,outfit,decal),'Sam suit map painted over the derived body UV layout (tools/sam_textures.py)')
    variants={'uphill-suit.png':paint_suit(source,'uphill_club')} if outfit=='rope_tow_regular' and back is None else {}
    boots=texture(paint_boots(source),'Sam boots and work gloves painted over the derived layout')
    face_art=Image.open(ART/'textures/face.png');skin=skin_tone(paint_face(face_art))
    face=texture(sam_head.paint(),'Sam head map painted in the roster HeadA layout (Mac topology warped to Sam), scalp below (tools/sam_head.py)')
    board=texture(paint_board(source,Image.open(ROOT/'sam_character/design/equipment/k2-alchemist-user-reference.png')),
                  'K2 Alchemist top/base from the user reference plus white bindings',ROOT/'sam_character/design/equipment/k2-alchemist-user-reference.png')
    gear=texture(paint_gear(skin,outfit),'Sam cap, patch, hair, skin and accessory swatches')
    G=lambda name:Region(gear,GEAR,name)
    surface=GarmentSurface(original[:,:3],parts['TopB'],original[:,3:6],rig['skin'])
    # Derived parts keep their original UVs, normals and weights: they deform exactly like the source body.
    material={'TopB':suit,'BottomA':suit,'BootsA':boots,'HandsA':boots,'BindingsA':board}
    for name,ix in parts.items():
        if name not in material:continue
        used=np.unique(ix);remap={int(v):i for i,v in enumerate(used)}
        faces=[[remap[int(i)] for i in tri] for tri in ix]
        mesh.add('derived_'+name,original[used,:3],faces,material[name],original[used,6:8],[rig['skin'][i] for i in used],original[used,3:6])
    # Head: Mac's original HeadA_NIS + eyes + scalp warped to Sam's photo-fitted landmarks (tools/sam_head.py);
    # the collar, hair, cap/beanie and glasses are fitted around its measured sections.
    sam_head.build(mesh,face);H=sam_head.head()
    collar(mesh,G('knit') if packers else G('flannel'),H)
    hair_skirt(mesh,G('hair'),H)
    if packers:
        beanie(mesh,G('knit'),G('knit'),H)
        mesh.ellipsoid('black_hood_collar',[0,.392,-.035],[.089,.032,.073],G('knit'),4,5,16)
    else:
        cap(mesh,G('cap'),G('bill'),G('patch'),H)
    # Authored directional K2 board at the roster's board size (original boards span
    # 1.93m x 0.455m in rig units). The nose is +x, the lightly notched tail -x.
    SX,SZ=1.095,1.2
    stations=[(-.87,.11),(-.84,.16),(-.76,.179),(-.60,.17),(-.35,.149),(0,.14),(.35,.152),(.60,.174),(.76,.19),(.84,.158),(.88,.075)]
    outline=[[-.858*SX,0]]+[[x*SX,-z*SZ] for x,z in stations]+[[.889*SX,0]]+[[x*SX,z*SZ] for x,z in reversed(stations)]+[[-.858*SX,0]]
    perimeter=[[x,.010+max(0,abs(x)-.74)**2*.9,z] for x,z in outline]
    XMAX,ZMAX=.889*SX,.19*SZ
    for top in [True,False]:
        pos=[[0,.010 if top else -.015,0]]+[[x,y if top else y-.025,z] for x,y,z in perimeter]
        u0,_,u1,_=BOARD['top' if top else 'base']
        uv=[[u0+(z/(2*ZMAX)+.5)*(u1-u0),.5-x/(2*XMAX)] for x,y,z in pos]
        faces=[[0,i+1,i+2] if top else [0,i+2,i+1] for i in range(len(perimeter)-1)]
        mesh.add('K2_Alchemist_top' if top else 'K2_Alchemist_base',pos,faces,board,uv,[[[23,1.]]] * len(pos))
    wall=[];faces=[];wall_uv=[]
    for x,y,z in perimeter:wall.extend([[x,y,z],[x,y-.025,z]]);wall_uv.extend([[(BOARD['sidewall'][0]+BOARD['sidewall'][2])/2,.5]]*2)
    for i in range(len(perimeter)-1):
        a=i*2;faces.extend([[a,a+1,a+2],[a+1,a+3,a+2]])
    mesh.add('K2_sidewall',wall,faces,board,wall_uv,[[[23,1.]]] * len(wall))
    # Optional fishing gear is actual rig-bound geometry.
    if 'creek_kit' in accessories:
        soft_box(mesh,'fly_fishing_chest_pack',[0,.16,.200],[.18,.145,.060],G('canvas'),3,None,.010)
        soft_box(mesh,'pack_front_pocket',[0,.15,.235],[.135,.085,.025],G('canvas'),3,None,.007)
        mesh.tube('pack_zipper',[-.054,.184,.250],[.054,.184,.250],.002,G('dark'))
        for side in [-1,1]:surface.ribbon(mesh,'fitted_pack_strap',[(side*(.075+t*.06),.12+t*.235) for t in np.linspace(0,1,10)],.014,G('dark'),.013)
    if 'one_more_cast' in accessories:
        if 'creek_kit' not in accessories:raise ValueError('Fly box requires chest pack')
        soft_box(mesh,'closed_fly_box',[.042,.10,.256],[.075,.051,.018],G('green'))
        mesh.tube('tool_retractor',[.078,.16,.241],[.078,.095,.247],.003,G('dark'))
    if 'packed_for_the_creek' in accessories:
        if 'catch_and_release' in accessories:raise ValueError('Net and rod tube share one back slot')
        mesh.tube('packed_rod_tube',[-.09,-.13,-.182],[.12,.52,-.122],.031,G('dark'),sides=12,cap_ends=True)
        for y,z in [(.08,-.131),(.33,-.114)]:mesh.tube('rod_case_strap',[-.12,y,z],[.10,y+.05,z-.004],.009,G('green'))
    if 'reading_the_water' in accessories:glasses(mesh,G('dark'),G('amber'),H,unit)
    if 'catch_and_release' in accessories:
        # Clipped flat against the upper back: the frame follows the vest's back surface.
        back_z=lambda y:-.131+max(0,y-.25)*.2
        for i in range(24):
            a=i*2*math.pi/24;b=(i+1)*2*math.pi/24;ya,yb=.34+.16*math.cos(a),.34+.16*math.cos(b)
            mesh.tube('net_frame',[.115*math.sin(a),ya,back_z(ya)],[.115*math.sin(b),yb,back_z(yb)],.007,G('wood'))
        mesh.tube('net_handle',[0,.18,back_z(.18)],[0,.04,back_z(.04)],.012,G('wood'))
        for offset in [-.08,-.04,0,.04,.08]:
            span=.16*math.sqrt(max(0,1-(offset/.115)**2));mesh.tube('net_mesh',[offset,.34-span,back_z(.34-span)-.004],[offset,.34+span,back_z(.34+span)-.004],.0018,G('dark'),sides=4)
        for offset in [-.12,-.06,0,.06,.12]:
            span=.115*math.sqrt(max(0,1-(offset/.16)**2));y=.34+offset;mesh.tube('net_mesh',[-span,y,back_z(y)-.005],[span,y,back_z(y)-.005],.0018,G('dark'),sides=4)
    return export(mesh,rig,textures,folder,package,outfit,list(accessories),variants,back)

def export(mesh,rig,textures,folder,package,outfit,accessories,variants={},back=None):
    v=np.array(mesh.vertices,dtype='<f4');ix=np.array(mesh.indices,dtype='<u4')
    assert np.isfinite(v).all() and len(mesh.skin)==len(v) and ix.max()<len(v)
    for influences in mesh.skin:assert abs(sum(w for b,w in influences)-1)<1e-4 and all(0<=b<len(rig['bones']) for b,w in influences)
    (folder/'vertices.bin').write_bytes(v.tobytes());(folder/'indices.bin').write_bytes(ix.tobytes());(folder/'colors.bin').write_bytes(np.asarray(mesh.colors,dtype='<f4').tobytes())
    # Keep the gameplay rig exactly as before: drop the source_* bone capture fields a newer
    # source export carries (Sam's bind is compiled by tools/compile_sam_skin.py).
    bones=[{k:v for k,v in b.items() if not k.startswith('source_')} for b in rig['bones']]
    for part in mesh.parts:part['ps2_family']=ps2_family(part['name'])
    rider=dict(bones=bones,skin=mesh.skin,parts=mesh.parts,units='meters',up_axis='Y',character_id='custom.sam',note='Authored Sam mesh; derived Mac shirt/trouser/glove/boot/binding geometry with original UVs and weights, custom head, cap, hair, collar, K2 board and fishing accessories. Five roster-density painted maps.')
    (folder/'rider.json').write_text(json.dumps(rider,separators=(',',':'))+'\n')
    if not (folder/'animation-samples.json').exists():shutil.copy2(SOURCE/'animation-samples.json',folder/'animation-samples.json')
    bounds=[v[:,:3].min(0).tolist(),v[:,:3].max(0).tolist()]
    world=dict(version=1,location=package,vertex_stride=40,vertex_count=len(v),index_count=len(ix),bounds=bounds,batches=mesh.batches,textures=textures,lighting_verified=False,source='Sam authored mesh and user-owned SSX rig/derived clothing',missing_textures=[])
    (folder/'world.json').write_text(json.dumps(world,indent=2)+'\n')
    lods=build_lods(v,ix,mesh.parts,ps2_family)
    (folder/'lods.json').write_text(json.dumps(dict(note='PS2 distance LODs (tools/sam_lod.py); indices reference this package vertices.bin. LOD0 is world.json.',levels=lods),separators=(',',':'))+'\n')
    (folder/'character.json').write_text(json.dumps(dict(character_id='custom.sam',outfit_id=outfit,back_kit=back,accessory_ids=accessories,stage=STAGE),indent=2)+'\n')
    source=OUT/package;source.mkdir(parents=True,exist_ok=True)
    for old in source.glob('material-*.png'):old.unlink()
    lines=['# Sam bind-pose mesh. Native rider.json carries skeleton and skin weights.','mtllib sam.mtl']
    lines.extend(f'v {p[0]} {p[1]} {p[2]}' for p in v)
    lines.extend(f'vt {p[6]} {1-p[7]}' for p in v)
    lines.extend(f'vn {p[3]} {p[4]} {p[5]}' for p in v)
    for part in mesh.parts:
        lines.append('g '+part['name']);lines.append('usemtl material_'+str(part['material']))
        for tri in ix[part['first_index']:part['first_index']+part['index_count']].reshape(-1,3):lines.append('f '+' '.join(f'{i+1}/{i+1}/{i+1}' for i in tri))
    (source/'sam.obj').write_text('\n'.join(lines)+'\n');mtl=[]
    for key,t in textures.items():
        identifier=key.split('-')[1];name=f'material-{identifier}.png'
        Image.frombytes('RGBA',(t['width'],t['height']),(folder/t['path']).read_bytes()).save(source/name)
        mtl.extend([f'newmtl material_{identifier}','Kd 1 1 1',f'map_Kd {name}',''])
    for name,image in variants.items():image.save(source/name)
    (source/'sam.mtl').write_text('\n'.join(mtl));shutil.copy2(folder/'rider.json',source/'rider.json')
    export_glb(mesh,rig,source,textures)
    summary=dict(package=package,vertices=len(v),triangles=len(ix)//3,lod_triangles={l['name']:sum(t['lod'] for t in l['triangles'].values()) for l in lods},bones=len(rig['bones']),outfit=outfit,accessories=accessories,stage=STAGE,
                 textures={k:[t['width'],t['height']] for k,t in textures.items()},source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    summary['authoring_sources_sha256']={name:hashlib.sha256((ROOT/'tools'/name).read_bytes()).hexdigest() for name in ['sam_mesh.py','sam_model_details.py','sam_textures.py','sam_head.py','sam_lod.py']}
    (source/'build.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary));return summary

def export_glb(mesh,rig,source,textures):
    """Editable skinned glTF2 asset. Original clip streams remain in native package."""
    binary=bytearray();views=[];accessors=[]
    def view(data):
        while len(binary)%4:binary.append(0)
        result=len(views);views.append(dict(buffer=0,byteOffset=len(binary),byteLength=len(data)));binary.extend(data);return result
    def array(data,kind,component=5126,bounds=False):
        a=np.asarray(data,dtype='<u4' if component==5125 else '<u2' if component==5123 else '<f4')
        info=dict(bufferView=view(a.tobytes()),componentType=component,count=len(a),type=kind)
        if bounds:info.update(min=a.min(0).tolist(),max=a.max(0).tolist())
        result=len(accessors);accessors.append(info);return result
    v=np.asarray(mesh.vertices);joints=np.zeros((len(v),4),dtype='<u2');weights=np.zeros((len(v),4),dtype='<f4')
    for i,skin in enumerate(mesh.skin):
        for k,(bone,w) in enumerate(skin):joints[i,k]=bone;weights[i,k]=w
    attributes=dict(POSITION=array(v[:,:3],'VEC3',bounds=True),NORMAL=array(v[:,3:6],'VEC3'),TEXCOORD_0=array(v[:,6:8],'VEC2'),COLOR_0=array(mesh.colors,'VEC4'),JOINTS_0=array(joints,'VEC4',5123),WEIGHTS_0=array(weights,'VEC4'))
    nodes=[];globals=[];inverse=[];roots=[]
    for i,b in enumerate(rig['bones']):
        rotation=np.array(b['rotation'],dtype=float);rotation/=np.linalg.norm(rotation)
        x,y,z,w=rotation;q=np.eye(4)
        q[:3,:3]=[[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]];q[:3,3]=b['translation']
        parent=b['parent'];g=globals[parent]@q if parent>=0 else q;globals.append(g);inverse.append(np.linalg.inv(g).T.reshape(16))
        nodes.append(dict(name=b['name'],translation=b['translation'],rotation=rotation.tolist()))
        if parent>=0:nodes[parent].setdefault('children',[]).append(i)
        else:roots.append(i)
    skins=[dict(joints=list(range(len(nodes))),inverseBindMatrices=array(inverse,'MAT4'))]
    images=[];materials=[]
    for i in range(len(textures)):
        images.append(dict(bufferView=view((source/f'material-{i}.png').read_bytes()),mimeType='image/png'))
        materials.append(dict(name=f'material_{i}',doubleSided=True,pbrMetallicRoughness=dict(baseColorTexture=dict(index=i),metallicFactor=0,roughnessFactor=1)))
    primitives=[];ix=np.asarray(mesh.indices)
    for batch in mesh.batches:
        primitives.append(dict(attributes=attributes,indices=array(ix[batch['first_index']:batch['first_index']+batch['index_count']],'SCALAR',5125),material=batch['texture'],mode=4))
    mesh_node=len(nodes);nodes.append(dict(name='Sam',mesh=0,skin=0));roots.append(mesh_node)
    doc=dict(asset=dict(version='2.0',generator='SSX3 native Sam mesh authoring'),scene=0,scenes=[dict(nodes=roots)],nodes=nodes,meshes=[dict(name='Sam',primitives=primitives)],skins=skins,materials=materials,images=images,textures=[dict(source=i) for i in range(len(images))],buffers=[dict(byteLength=len(binary))],bufferViews=views,accessors=accessors)
    content=json.dumps(doc,separators=(',',':')).encode();content+=b' '*((-len(content))%4);binary+=b'\0'*((-len(binary))%4)
    data=struct.pack('<III',0x46546c67,2,12+8+len(content)+8+len(binary))+struct.pack('<I4s',len(content),b'JSON')+content+struct.pack('<I4s',len(binary),b'BIN\0')+binary
    (source/'sam.glb').write_bytes(data)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--all',action='store_true');args=parser.parse_args()
    build()
    if args.all:
        for top,back in combinations():
            if (top,back)!=('rope_tow_regular',None):build(top,back=back)
