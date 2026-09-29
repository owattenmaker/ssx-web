"""Fitted detail geometry for Sam. Shared rig coordinates are native meters."""
import math
import numpy as np

def skin_blend(skins,coefficients):
    values={}
    for skin,coefficient in zip(skins,coefficients):
        for bone,weight in skin:values[bone]=values.get(bone,0)+coefficient*weight
    pairs=sorted(((b,w) for b,w in values.items() if w>1e-8),key=lambda p:-p[1])[:4]
    total=sum(w for _,w in pairs)
    return [[b,w/total] for b,w in pairs]

class GarmentSurface:
    def __init__(self,positions,triangles,normals,skin):
        self.p=positions;self.triangles=triangles;self.normals=normals;self.skin=skin
    def sample(self,x,y,offset=.004):
        best=None
        for ids in self.triangles:
            a,b,c=self.p[ids];matrix=np.column_stack([b[:2]-a[:2],c[:2]-a[:2]])
            if abs(np.linalg.det(matrix))<1e-10:continue
            u,v=np.linalg.solve(matrix,np.array([x,y])-a[:2])
            if min(u,v,1-u-v)<-1e-7:continue
            weights=np.array([1-u-v,u,v]);point=weights@self.p[ids]
            normal=weights@self.normals[ids];normal/=max(np.linalg.norm(normal),1e-12)
            if best is None or point[2]>best[0][2]:best=(point,normal,skin_blend([self.skin[i] for i in ids],weights))
        if best is None:return None
        return best[0]+best[1]*offset,best[1],best[2]
    def ribbon(self,mesh,name,path,width,material,offset=.012):
        p=[];n=[];skin=[];faces=[];previous=None
        for x,y in path:
            edge=[self.sample(x-width/2,y,offset),self.sample(x+width/2,y,offset)]
            if any(v is None for v in edge):previous=None;continue
            first=len(p)
            for point,normal,weights in edge:p.append(point);n.append(normal);skin.append(weights)
            if previous is not None:faces.extend([[previous,first,previous+1],[previous+1,first,first+1]])
            previous=first
        if faces:mesh.add(name,p,faces,material,weights=skin,normals=n)
    def snap(self,mesh,x,y,material,radius=.006):
        result=self.sample(x,y,.014)
        if result is None:return
        center,normal,weights=result;p=[center+normal*.001]
        for i in range(9):
            angle=i*math.pi/4;p.append(center+[radius*math.cos(angle),radius*math.sin(angle),.0015])
        mesh.add('jacket_snap',p,[[0,i+1,i+2] for i in range(8)],material,weights=[weights]*len(p),normals=[normal]*len(p))
    def decal(self,mesh,name,center,size,material,uvbox):
        points=[];normals=[];skin=[];uv=[];faces=[];columns=5
        for j in range(columns):
            for i in range(columns):
                u,v=i/(columns-1),j/(columns-1)
                result=self.sample(center[0]+(u-.5)*size[0],center[1]+(v-.5)*size[1],.005)
                if result is None:raise ValueError('Decal extends beyond garment')
                p,n,w=result;points.append(p);normals.append(n);skin.append(w)
                uv.append([uvbox[0]+u*(uvbox[2]-uvbox[0]),uvbox[3]-v*(uvbox[3]-uvbox[1])])
        for j in range(columns-1):
            for i in range(columns-1):
                a=j*columns+i;b=a+columns;faces.extend([[a,a+1,b],[a+1,b+1,b]])
        mesh.add(name,points,faces,material,uv,skin,normals)

def dome(mesh,name,levels,z,material,bone=5,uvbox=None,sides=24):
    p=[];uv=[];faces=[]
    for j,(y,rx,rz) in enumerate(levels):
        for i in range(sides+1):
            a=i*2*math.pi/sides;p.append([rx*math.sin(a),y,z+rz*math.cos(a)])
            u,v=i/sides,j/(len(levels)-1)
            if uvbox:u,v=uvbox[0]+u*(uvbox[2]-uvbox[0]),uvbox[1]+v*(uvbox[3]-uvbox[1])
            uv.append([u,v])
    for j in range(len(levels)-1):
        for i in range(sides):
            a=j*(sides+1)+i;b=a+sides+1;faces.extend([[a,a+1,b],[a+1,b+1,b]])
    top=len(p);p.append([0,levels[-1][0]+.001,z]);uv.append(uv[-1])
    for i in range(sides):faces.append([top,(len(levels)-1)*(sides+1)+i,(len(levels)-1)*(sides+1)+i+1])
    mesh.add(name,p,faces,material,uv,[[[bone,1.]]] * len(p))

def dome_rings(mesh,name,rings,material,bone=5,sides=24,uvbox=None,top=None):
    """Closed dome of elliptical rings [(y, rx, rz, cz)] bottom -> top."""
    p=[];uv=[];faces=[]
    for j,(y,rx,rz,cz) in enumerate(rings):
        for i in range(sides+1):
            a=i*2*math.pi/sides;p.append([rx*math.sin(a),y,cz+rz*math.cos(a)])
            u,v=i/sides,j/(len(rings)-1)
            if uvbox:u,v=uvbox[0]+u*(uvbox[2]-uvbox[0]),uvbox[1]+v*(uvbox[3]-uvbox[1])
            uv.append([u,v])
    for j in range(len(rings)-1):
        for i in range(sides):
            a=j*(sides+1)+i;b=a+sides+1;faces.extend([[a,a+1,b],[a+1,b+1,b]])
    if top is not None:
        t=len(p);p.append(top);uv.append(uv[-1])
        for i in range(sides):faces.append([t,(len(rings)-1)*(sides+1)+i,(len(rings)-1)*(sides+1)+i+1])
    mesh.add(name,p,faces,material,uv,[[[bone,1.]]]*len(p))

def cap(mesh,crown,bill,patch_region,head):
    """Fitted six-panel cap over the hair: band 3.6 cm above the eyes, rounded crown 2 cm over the skull, curved bill and
    the painted uphill-arrow patch (gear map). `head` is the fitted head's measurements (tools/sam_head.py Head)."""
    yb=head.eye[1]+.036;ytop=head.top+.020
    rx0,rz0,cz0=head.ring(yb,.010,.006,.012)
    rx1,rz1,cz1=head.ring(yb+.016,.010,.006,.012);rx1*=1.035;rz1*=1.035
    rings=[(yb,rx0,rz0,cz0),(yb+.016,rx1,rz1,cz1)]
    yc=yb+.016
    for f in (.30,.55,.74,.88,.97):
        k=math.sqrt(1-f**2.2);rings.append((yc+(ytop-yc)*f,rx1*k,rz1*k,cz1+.004*f))
    dome_rings(mesh,'fitted_cap_crown',rings,crown,sides=18,top=[0,ytop,rings[-1][3]])
    def front(x,y):
        for (ay,ax,az,ac),(by,bx,bz,bc) in zip(rings,rings[1:]):
            if ay<=y<=by:
                t=(y-ay)/(by-ay);rx=ax*(1-t)+bx*t;rz=az*(1-t)+bz*t;c=ac*(1-t)+bc*t
                return c+rz*math.sqrt(max(0,1-(x/rx)**2))
        raise ValueError('Patch extends outside crown')
    # Bill has an attached inner edge, a curved tip and actual thin sidewalls.
    points=[];uv=[];faces=[];columns=15;rows=3
    for bottom in [False,True]:
        for row in range(rows):
            t=row/(rows-1)
            for i in range(columns):
                a=-1.3+2.6*i/(columns-1)
                x=(rx0+.004+.010*t)*math.sin(a);z=cz0+(rz0+.002+.080*t)*math.cos(a)
                y=yb+.008-.012*math.sin(a)**2-.006*t-(.005 if bottom else 0)
                points.append([x,y,z]);uv.append([i/(columns-1),t])
        base=(rows*columns if bottom else 0)
        for row in range(rows-1):
            for i in range(columns-1):
                a=base+row*columns+i;b=a+columns
                triangles=[[a,b,a+1],[a+1,b,b+1]]
                faces.extend([t[::-1] if bottom else t for t in triangles])
    edge=list(range(columns))+[r*columns+columns-1 for r in range(1,rows)]+list(range(rows*columns-2,(rows-1)*columns-1,-1))+[r*columns for r in range(rows-2,0,-1)]
    shift=rows*columns
    for a,b in zip(edge,edge[1:]+edge[:1]):faces.extend([[a,b,a+shift],[b,b+shift,a+shift]])
    mesh.add('curved_cap_bill',points,faces,bill,uv,[[[5,1.]]]*len(points))
    patch=[];uv=[];faces=[]
    for j,y in enumerate([yb+.064,yb+.047,yb+.030]):
        for i,x in enumerate([-.033,-.0165,0,.0165,.033]):patch.append([x,y,front(x,y)+.0025]);uv.append([i/4,j/2])
    for row in range(2):
        for i in range(4):
            a=row*5+i;b=a+5;faces.extend([[a,b,a+1],[a+1,b,b+1]])
    mesh.add('fitted_yellow_cap_patch',patch,faces,patch_region,uv,[[[5,1.]]]*len(patch))

def hair_skirt(mesh,region,head):
    """Solid painted hair clumps from under the cap band, behind the ears, to below the ear lobes at the sides and the
    collar at the nape (the ears and face stay open), fitted around the head's measured sections.

    Like the source rider's Dangle hair, the lower rows are shared between the head and the
    two secondary-motion bones sec_dangle_l (24, +x) / sec_dangle_r (25, -x), which the
    original SH_* hair channel animates, so the clumps sway in the race.
    """
    a0,a1=math.radians(100),math.radians(260);columns=25;rows=[0,.3,.6,.85,1.]
    ytop=head.eye[1]+.034
    rx0,rz0,cz=head.ring(ytop,.007,.004,.007)
    ear_x=head.ear_top[0]+.006
    p=[];uv=[];faces=[];skin=[]
    for r,t in enumerate(rows):
        for i in range(columns):
            s=i/(columns-1);a=a0+(a1-a0)*s;back=(1-math.cos(a))/2
            ybottom=head.lobe[1]-.012-.040*max(0,back-.55)/.45
            tip=r==len(rows)-1
            if tip:ybottom+=-(.010+.008*math.sin(i*2.3)**2) if i%2==0 else .005+.003*math.cos(i*1.7)
            y=ytop+(ybottom-ytop)*t
            flare=t**1.2
            wave=1+.045*t*math.sin(i*1.9+.7)
            rx=max(rx0+.030*flare,ear_x*min(1,t*3))*wave;rz=(rz0+.014*flare)*wave
            x=rx*math.sin(a);p.append([x,y,cz+rz*math.cos(a)]);uv.append([s,t])
            sway=.5*t**1.3+(.08 if tip and i%2==0 else 0)
            left=.5+.5*max(-1,min(1,x/.04))
            w=[[5,1-sway],[24,sway*left],[25,sway*(1-left)]]
            w=[[b,v] for b,v in w if v>1e-4];total=sum(v for _,v in w);skin.append([[b,v/total] for b,v in w])
    for r in range(len(rows)-1):
        for i in range(columns-1):
            a=r*columns+i;b=a+columns;faces.extend([[a,b,a+1],[a+1,b,b+1]])
    mesh.add('hair_clumps',p,faces,region,uv,skin)

def collar(mesh,region,head):
    """Stand-up shirt collar from the neckline to under the jaw, higher at the nape than the throat, around the neck."""
    rings=[]
    for yf,yb,w,grow in [(.398,.398,[[3,.8],[4,.2]],.018),(.420,.432,[[3,.5],[4,.5]],.009),(.436,.466,[[4,.8],[3,.2]],.006)]:
        x,zf,zb=head.sec((yf+yb)/2,zmax=.118)
        rings.append((yf,yb,x+grow,(zf-zb)/2+grow,(zf+zb)/2-.002,w))
    sides=20;p=[];uv=[];skin=[];faces=[]
    for j,(yf,yb,rx,rz,cz,w) in enumerate(rings):
        for i in range(sides+1):
            a=2*math.pi*i/sides;y=yf+(yb-yf)*(1-math.cos(a))/2
            p.append([rx*math.sin(a),y,cz+rz*math.cos(a)]);uv.append([i/sides,j/(len(rings)-1)]);skin.append(w)
    for j in range(len(rings)-1):
        for i in range(sides):
            a=j*(sides+1)+i;b=a+sides+1;faces.extend([[a,a+1,b],[a+1,b+1,b]])
    mesh.add('shirt_collar',p,faces,region,uv,skin)

def beanie(mesh,dark,trim,head):
    """Taller knit dome with a folded ribbed cuff, sitting 3 cm above the eyes."""
    yb=head.eye[1]+.030;ytop=head.top+.028
    rx1,rz1,cz1=head.ring(yb+.022,.012,.008,.013)
    body=[(yb+.008,*head.ring(yb+.008,.012,.008,.013)),(yb+.022,rx1*1.02,rz1*1.02,cz1)]
    for f in (.3,.55,.74,.88,.97):
        k=math.sqrt(1-f**2.4);body.append((yb+.022+(ytop-yb-.022)*f,rx1*1.02*k,rz1*1.02*k,cz1))
    dome_rings(mesh,'soft_knit_beanie',body,dark,top=[0,ytop,cz1])
    rx,rz,cz=head.ring(yb+.012,.016,.011,.016)
    dome_rings(mesh,'folded_beanie_cuff',[(yb-.002,rx*.985,rz*.985,cz),(yb+.001,rx+.004,rz+.004,cz),(yb+.026,rx+.005,rz+.005,cz),(yb+.030,rx,rz,cz)],trim)
    # Subtle vertical knit ribs on the rolled cuff, not a visor-like disk.
    for i in range(32):
        a=i*math.pi/16
        mesh.tube('cuff_knit_rib',[(rx+.0045)*math.sin(a),yb+.003,cz+(rz+.0045)*math.cos(a)],[(rx+.0055)*math.sin(a),yb+.024,cz+(rz+.0055)*math.cos(a)],.0008,dark,5,4)

def glasses(mesh,frame,lens,head,unit):
    """Amber sunglasses: lenses in front of the eyes resting on the bridge, temples back to the ear tops."""
    e=head.eye;cx,cy,z=e[0]+.001,e[1]+.0015,e[2]+.020;te=[head.ear_top[0]-.002,head.ear_top[1]-.006,head.ear_top[2]+.004]
    for side in [-1,1]:
        for name,size,dz,region,bevel in [('sunglass_frame',[.060,.032,.008],0,frame,.009),('amber_lens',[.050,.024,.003],.006,lens,.007)]:
            start=len(mesh.vertices)
            soft_box(mesh,name,[side*cx,cy,z+dz],size,region,5,bevel=bevel)
            for vertex in mesh.vertices[start:]:
                vertex[2]-=abs(vertex[0])*.16
                vertex[3:6]=unit([vertex[3]+math.copysign(.16,vertex[0])*vertex[5],vertex[4],vertex[5]]).tolist()
        mesh.tube('glasses_temple',[side*(cx+.029),cy+.003,z-.012],[side*te[0],te[1],te[2]],.0025,frame,5)
    mesh.tube('glasses_bridge',[-.006,cy+.002,z+.002],[.006,cy+.002,z+.002],.0035,frame,5)

def soft_box(mesh,name,center,size,material,bone=3,uvbox=None,bevel=.008):
    center=np.array(center);sx,sy,sz=size;depth_bevel=min(bevel,sz*.3);bevel=min(bevel,min(sx,sy)*.3)
    ring=[(-sx/2+bevel,-sy/2), (sx/2-bevel,-sy/2),(sx/2,-sy/2+bevel),(sx/2,sy/2-bevel),(sx/2-bevel,sy/2),(-sx/2+bevel,sy/2),(-sx/2,sy/2-bevel),(-sx/2,-sy/2+bevel)]
    p=[];uv=[];faces=[]
    for z,scale in [(-sz/2,.90),(-sz/2+depth_bevel,1),(sz/2-depth_bevel,1),(sz/2,.90)]:
        for x,y in ring:
            p.append(center+[x*scale,y*scale,z]);u,v=x/sx+.5,.5-y/sy
            if uvbox:u,v=uvbox[0]+u*(uvbox[2]-uvbox[0]),uvbox[1]+v*(uvbox[3]-uvbox[1])
            uv.append([u,v])
    for row in range(3):
        for i in range(8):
            a=row*8+i;b=row*8+(i+1)%8;faces.extend([[a,b,a+8],[b,b+8,a+8]])
    for i in range(1,7):faces.extend([[0,i+1,i],[24,24+i,25+i]])
    mesh.add(name,p,faces,material,uv,[[[bone,1.]]] * len(p))
