"""Spatial draw batching with retained ownership for dynamic instances."""
import array

def spatial_batches(world,vertices,indices,dynamic_resources=(),event_dead_resources=(),triangle_blend=None,moving_resources=(),hidden_resources=(),scroll_groups=None,livecomp_nodes=None,instance_chunks=None,script_resources=(),meshanim_nodes=None,triangle_wrap=None,triangle_env=None,lit_resources=None):
 # lit_resources: resource -> lighting of a lit instance (runtime flag 0x4000, tools/export_lit_instances.py: its light-cache rows,
 # bank, relight): its triangles get their own batches (per instance: each one's rows differ), tagged lighting (web/world-material.js).
 # triangle_env: source triangle -> (second texture, 0x200000 / 0x600000) of its static-model material (the env-map second pass,
 # 37F2A4..37FD2C, web/world-material.js envPassMaterial; pv envMap): split into batches tagged env = [texture, mode].
 # triangle_wrap: source triangle -> GS CLAMP_1 bits of its static-model material (web/world-material.js originalModelWrap;
 # 1 = u clamp, 2 = v clamp; material word+12 & 0x180000, 37F2BC..37F354): split into batches tagged wrap.
 # meshanim_nodes: (resource, collision-source mesh index) -> node of a builtin13 MeshAnim target: its triangles keep
 # their static batches and are ALSO emitted once more per node (hidden batches tagged meshanim_resource/node, appended
 # after the static ones), which the renderer shows only while the pieces fly (stage_world_meshanims).
 # script_resources: instances whose flags the stage programs change at run time (web/stage_world.inc
 # stage_world_instances; tools/export_stage_world.py): own batches tagged script_resource (hidden ones stay hidden).
 # instance_chunks: resource -> texture chunk (instance +0x7E; -1 when +0x7D == 255, always drawn). The static
 # collector 0x22A5A0 skips instances whose chunk is not resident (tools/export_sections.py, web/section_gameplay.inc).
 # moving_resources: instances whose draw follows a runtime entity matrix (RollerModifier crashbags).
 # livecomp_nodes: (resource, collision-source mesh index) -> animated node of a LiveComp player
 # (builtin3, web/livecomp-animation.js); those batches are split per node.
 # scroll_groups: resource -> UVScrollModifier group (builtin21, web/uv-scroll.js); batches of one group share a
 # texture offset. hidden_resources: instances the original never draws (runtime flag bit0 clear: trigger/reset
 # volumes, emitter placeholders); they may own no render triangles.
 pickups=set(dynamic_resources);dead=set(event_dead_resources);moving=set(moving_resources);hidden=set(hidden_resources);script=set(script_resources)-pickups-dead-moving;wanted=pickups|dead|moving|hidden|script;owners={};found=set()
 for source in world.get('collision_sources',[]):
  if source['kind']!='instance':continue
  resource=(source['rid']<<8)|source['track']
  if resource not in wanted:continue
  for tri in range(source['first_triangle'],source['first_triangle']+source['triangle_count']):
   if tri in owners:raise ValueError('Overlapping dynamic instance triangle ownership')
   owners[tri]=resource
  found.add(resource)
 if found|hidden|script!=wanted:raise ValueError(f'Missing dynamic instance geometry: {wanted-found-hidden-script}')
 source_owner={}
 live_owner={}
 chunk_owner={}
 lit_owner={}
 if scroll_groups or livecomp_nodes or instance_chunks or lit_resources:
  for source in world.get('collision_sources',[]):
   if source['kind']!='instance':continue
   r=(source['rid']<<8)|source['track']
   if instance_chunks is not None and instance_chunks.get(r,-1)>=0:
    for tri in range(source['first_triangle'],source['first_triangle']+source['triangle_count']):chunk_owner[tri]=instance_chunks[r]
   if scroll_groups and r in scroll_groups:
    for tri in range(source['first_triangle'],source['first_triangle']+source['triangle_count']):source_owner[tri]=r
   if lit_resources and r in lit_resources:
    for tri in range(source['first_triangle'],source['first_triangle']+source['triangle_count']):lit_owner[tri]=r
   if livecomp_nodes and (r,source['mesh']) in livecomp_nodes:
    for tri in range(source['first_triangle'],source['first_triangle']+source['triangle_count']):live_owner[tri]=(r,livecomp_nodes[(r,source['mesh'])])
 mesh_owner={}
 if meshanim_nodes:
  for source in world.get('collision_sources',[]):
   if source['kind']!='instance':continue
   r=(source['rid']<<8)|source['track']
   if (r,source['mesh']) in meshanim_nodes:
    for tri in range(source['first_triangle'],source['first_triangle']+source['triangle_count']):mesh_owner[tri]=(r,meshanim_nodes[(r,source['mesh'])])
 groups={};pieces={}
 for batch in world['batches']:
  for at in range(batch['first_index'],batch['first_index']+batch['index_count'],3):
   triangle=indices[at:at+3]
   if len(triangle)!=3:raise ValueError('Partial source triangle')
   cx=sum(vertices[i*10] for i in triangle)/3;cz=sum(vertices[i*10+2] for i in triangle)/3
   resource=owners.get(at//3)
   blend=triangle_blend.get(at//3,0) if triangle_blend else 0
   wrap=triangle_wrap.get(at//3,0) if triangle_wrap else 0
   env=triangle_env.get(at//3,0) if triangle_env else 0
   scroll=scroll_groups.get(source_owner.get(at//3)) if scroll_groups else None
   live=live_owner.get(at//3)
   chunk=chunk_owner.get(at//3,-1)
   lit=lit_owner.get(at//3)
   key=(batch['texture'],batch['lightmap'],batch['instance'],int(cx//80),int(cz//80),resource,blend,scroll,live,chunk,wrap,env,lit)
   groups.setdefault(key,array.array('I')).extend(triangle)
   if at//3 in mesh_owner:pieces.setdefault((batch['texture'],batch['lightmap'],batch['instance'],blend,scroll,mesh_owner[at//3],wrap,env,lit),array.array('I')).extend(triangle)
 output=array.array('I');batches=[]
 for (texture,lightmap,instance,_,_,resource,blend,scroll,live,chunk,wrap,env,lit),triangles in groups.items():
  batch=dict(texture=texture,lightmap=lightmap,instance=instance,first_index=len(output),index_count=len(triangles))
  if blend:batch['blend']=blend
  if wrap:batch['wrap']=wrap
  if env:batch['env']=list(env)
  if chunk>=0:batch['chunk']=chunk
  if scroll is not None:batch['uv_scroll_group']=scroll
  if live is not None:batch['livecomp_resource'],batch['livecomp_node']=live
  if resource in pickups:batch['pickup_resource']=resource
  if resource in dead:batch['event_dead_resource']=resource
  if resource in moving:batch['moving_resource']=resource
  if resource in hidden:batch['hidden_resource']=resource
  if resource in script:batch['script_resource']=resource
  if lit is not None:batch['lighting']=lit_resources[lit]
  batches.append(batch);output.extend(triangles)
 if len(output)!=len(indices):raise ValueError('Spatial batching lost source indices')
 for (texture,lightmap,instance,blend,scroll,(resource,node),wrap,env,lit),triangles in pieces.items():
  batch=dict(texture=texture,lightmap=lightmap,instance=instance,first_index=len(output),index_count=len(triangles),hidden_resource=resource,meshanim_resource=resource,meshanim_node=node)
  if blend:batch['blend']=blend
  if wrap:batch['wrap']=wrap
  if env:batch['env']=list(env)
  if scroll is not None:batch['uv_scroll_group']=scroll
  if lit is not None:batch['lighting']=lit_resources[lit]
  batches.append(batch);output.extend(triangles)
 return batches,output
