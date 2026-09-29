// SPDX-License-Identifier: GPL-3.0-only
// Uses GlitcherOG/SSX-Library (GPL-3.0; docs/asset-formats.md, licenses/GPL-3.0.txt).
using SSXLibrary.FileHandlers.Models.SSX3;
using SSX_Library.EATextureLibrary;
using System.Numerics;
using System.Text.Json;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;
using M=SSXLibrary.FileHandlers.Models.SSX3.SSX3PS2MPF;
var root=Path.GetFullPath(args.Length>0?args[0]:".");
void Wrap(string path) {
 var raw=File.ReadAllBytes(path);int count=BitConverter.ToUInt16(raw,4),start=(int)BitConverter.ToUInt32(raw,8);using var result=new MemoryStream();result.Write(raw,0,start);
 for(int i=0;i<count;i++) {int h=12+i*96,at=start+(int)BitConverter.ToUInt32(raw,h+16),size=(int)BitConverter.ToUInt32(raw,h+20);if(raw[at]==0x10&&raw[at+1]==0xfb)throw new Exception("Already wrapped");
  BitConverter.GetBytes((int)result.Position-start).CopyTo(raw,h+16);
  result.Write(new byte[]{0x10,0xfb,(byte)(size>>16),(byte)(size>>8),(byte)size});int n=0;
  while(size-n>=4){int len=Math.Min(112,(size-n)/4*4);result.WriteByte((byte)(0xe0+len/4-1));result.Write(raw,at+n,len);n+=len;}
  result.WriteByte((byte)(0xfc+size-n));result.Write(raw,at+n,size-n);while(result.Position%16!=0)result.WriteByte(0);
 }
 result.Position=0;result.Write(raw,0,start);File.WriteAllBytes(path,result.ToArray());
}
if(args.Contains("--export-browser-ui")) {
 var dest=Path.Combine(root,"web/public/assets/UI");Directory.CreateDirectory(dest);
 foreach(var path in Directory.GetFiles(Path.Combine(root,"local/browser-ui"),"*.SSH")) {
  var pack=new OldShapeHandler();pack.LoadShape(path);var rows=new List<object>();
  for(int i=0;i<pack.ShapeImages.Count;i++){var entry=pack.ShapeImages[i];using var im=entry.Image.Clone();im.ProcessPixelRows(a=>{for(int y=0;y<a.Height;y++){var row=a.GetRowSpan(y);for(int x=0;x<row.Length;x++)row[x].A=Path.GetFileName(path).Contains("FONT")?(byte)Math.Max(row[x].R,Math.Max(row[x].G,row[x].B)):(entry.AlphaFix?row[x].A:(byte)Math.Min(255,row[x].A*2))/* PS2 alpha unity 0x80 -> 255: OldShapeHandler already doubles a palette whose alphas are all <= 0x80 (AlphaFix: FE_1 / OV_1 / SU_1); doubling those again made every translucent UI texel 2x too opaque (docs/visual-parity.md) */;}});var name=Path.GetFileNameWithoutExtension(path)+"-"+i+".png";im.SaveAsPng(Path.Combine(dest,name));rows.Add(new {index=i,name=entry.Shortname,file=name,width=im.Width,height=im.Height});}
  File.WriteAllText(Path.Combine(dest,Path.GetFileNameWithoutExtension(path)+".json"),JsonSerializer.Serialize(rows));
 }
 return;
}
if(args.Contains("--inspect-lods")) {
 // Triangle counts of each LOD model in the original templates Sam's parts are written into.
 foreach(var name in new[]{"mac_TopA.mpf","mac_BottomA.mpf","mac_BootsA.mpf","mac_HandsA.mpf","mac_HeadA.mpf","mac_HeadA_NIS.mpf","mac_Dangle.mpf","board_BindingsA.mpf","board_BoardFlexA.mpf"}) {
  var m=new M();m.load(Path.Combine(root,"local/sam-ps2/original",name));
  Console.WriteLine(name+": "+string.Join(", ",m.ModelList.Select(h=>{int tris=0,verts=0;foreach(var g in h.MaterialGroupList)foreach(var w in g.WeightRefList)foreach(var mg in w.MorphMeshGroupList)foreach(var ch in mg.MeshChunkList){verts+=ch.Vertices.Count;foreach(var s in ch.Strips)tris+=Math.Max(0,s-2);}return $"{h.ModelName} {tris} tris/{verts} verts";})));
 }
 return;
}
if(args.Contains("--parts")) { SamParts.Build(root,Wrap); SamTopVariant.Build(root,Wrap); return; }
if(args.Contains("--uphill-model")) { SamTopVariant.Build(root,Wrap); return; }
if(args.Contains("--wardrobe-parts")) { SamWardrobeParts.Build(root,Wrap); return; }
if(args.Contains("--wardrobe")) { SamWardrobe.Build(root); return; }
if(args.Contains("--roster-texture")) { SamRosterTexture.Build(root); return; }
if(args.Contains("--sam-textures")) { SamTextures.Build(root); return; }
var original=Path.Combine(root,"local/sam-ps2/original");
var output=Path.Combine(root,"local/sam-ps2/replacements");Directory.CreateDirectory(output);
if(args.Contains("--audit-wardrobe")) {
 var bolts=new SSXLibrary.FileHandlers.BoltPS2Handler();bolts.load(Path.Combine(original,"BOLTPS2.DAT"));
 File.WriteAllText(Path.Combine(root,"local/sam-ps2/roster/mac-wardrobe.json"),JsonSerializer.Serialize(bolts.characters[3],new JsonSerializerOptions{IncludeFields=true,WriteIndented=true}));
 Console.WriteLine($"Characters {bolts.characters.Count}, Mac entries {bolts.characters[3].entries.Count}, compatibility rows {bolts.characters[3].unkown2s.Count}");return;
}
if(args.Contains("--audit-ui")) {
 var ui=new SSXLibrary.FileHandlers.SSX3.LUIHandler();
 ui.LoadLUIFile(Path.Combine(original,"FE.LUI"));
 var audit=Path.Combine(root,"local/sam-ps2/roster");Directory.CreateDirectory(audit);
 File.WriteAllText(Path.Combine(audit,"frontend-ui.json"),JsonSerializer.Serialize(ui,new JsonSerializerOptions {IncludeFields=true,WriteIndented=true}));
 Console.WriteLine($"Front-end: {ui.ScreenTables.Count} screens, {ui.ObjectTables.Count} objects, {ui.FontTables.Count} fonts");
 return;
}
if(args.Contains("--validate")) {
 foreach(var path in Directory.GetFiles(output,"*.mpf")) {
  var checkModel=new M();checkModel.load(path);
  foreach(var h in checkModel.ModelList)foreach(var g in h.MaterialGroupList)foreach(var w in g.WeightRefList)foreach(var mg in w.MorphMeshGroupList)foreach(var ch in mg.MeshChunkList)foreach(var v in ch.Vertices)if(!float.IsFinite(v.X)||!float.IsFinite(v.Y)||!float.IsFinite(v.Z))throw new Exception("Nonfinite vertex");
  Console.WriteLine($"Decoded {Path.GetFileName(path)}: {checkModel.ModelList.Count} models");
 }
 return;
}
if(args.Contains("--portraits")) {
 foreach(var name in new[]{"mac_flat.ssh","mac_small_flat.ssh"}) {
  var portraitShape=new OldShapeHandler();portraitShape.LoadShape(Path.Combine(original,name));
  for(int i=0;i<portraitShape.ShapeImages.Count;i++){
   var im=portraitShape.ShapeImages[i];Console.WriteLine($"{name}: {im.Shortname} {im.Image.Width}x{im.Image.Height}");
   var image=Image.Load<Rgba32>(Path.Combine(root,"sam_character/design/sam-ps2-roster.png"));
   image.Mutate(x=>x.Resize(im.Image.Width,im.Image.Height));
   image.ProcessPixelRows(a=>{for(int y=0;y<a.Height;y++){var row=a.GetRowSpan(y);for(int x=0;x<row.Length;x++)row[x].A=(byte)((row[x].A*128+127)/255);}});
   im.Image=image;im.MatrixType=OldShapeHandler.MatrixType.FullColor;im.Longname??="";
   portraitShape.ShapeImages[i]=im;
  }
  portraitShape.EndingString??="";portraitShape.SaveShape(Path.Combine(output,name));
  var portraitCheck=new OldShapeHandler();portraitCheck.LoadShape(Path.Combine(output,name));
 }
 return;
}
if(args.Contains("--hide-original")) {
 foreach(var name in new[]{"mac_HeadA.mpf","mac_HeadA_NIS.mpf","mac_BottomA.mpf","mac_BootsA.mpf","mac_HandsA.mpf","mac_Dangle.mpf"}) {
  var path=Path.Combine(original,name);if(!File.Exists(path))continue;
  var old=new M();old.load(path);
  foreach(var h in old.ModelList) {
   foreach(var w in h.BoneWeightHeaderList) {w.BoneWeightList.Clear();w.BoneWeightList.Add(new M.BoneWeight{Weight=100,BoneID=0,FileID=0});}
   foreach(var g in h.MaterialGroupList)foreach(var w in g.WeightRefList)foreach(var mg in w.MorphMeshGroupList)foreach(var ch in mg.MeshChunkList)for(int i=0;i<ch.Vertices.Count;i++)ch.Vertices[i]=Vector3.Zero;
  }
  old.Save(Path.Combine(output,name),false);Wrap(Path.Combine(output,name));Console.WriteLine("Hidden original "+name);
 }
 return;
}
var native=SamSource.Native(root);int textureCount=SamSource.TextureCount(root);
var rig=JsonDocument.Parse(File.ReadAllText(Path.Combine(native,"rider.json"))).RootElement;
var vertices=File.ReadAllBytes(Path.Combine(native,"vertices.bin"));
var indices=File.ReadAllBytes(Path.Combine(native,"indices.bin"));
var model=new M();model.load(Path.Combine(original,"mac_TopA.mpf"));
var combiner=new SSX3PS2ModelCombiner();combiner.AddFile(model);
Vector3 Pos(int i){int p=i*40;return new Vector3(BitConverter.ToSingle(vertices,p),-BitConverter.ToSingle(vertices,p+8),BitConverter.ToSingle(vertices,p+4))*100;}
Vector3 Normal(int i){int p=i*40+12;return new Vector3(BitConverter.ToSingle(vertices,p),-BitConverter.ToSingle(vertices,p+8),BitConverter.ToSingle(vertices,p+4));}
Vector4 UV(int i){int p=i*40+24;return new Vector4(BitConverter.ToSingle(vertices,p),BitConverter.ToSingle(vertices,p+4),0,0);}
M.BoneWeightHeader Weight(int i){var list=new List<M.BoneWeight>();foreach(var w in rig.GetProperty("skin")[i].EnumerateArray()) {int b=w[0].GetInt32();var bone=rig.GetProperty("bones")[b];if(bone.GetProperty("file").GetInt32()!=0)throw new Exception("Non-body bone");list.Add(new M.BoneWeight {BoneID=bone.GetProperty("index").GetInt32(),FileID=0,boneName=bone.GetProperty("name").GetString(),Weight=(int)Math.Round(w[1].GetDouble()*100)});}list[0]=new M.BoneWeight {BoneID=list[0].BoneID,FileID=0,boneName=list[0].boneName,Weight=list[0].Weight+100-list.Sum(x=>x.Weight)};return new M.BoneWeightHeader{WeightCount=list.Count,BoneWeightList=list};}
if(!args.Contains("--finish")&&!args.Contains("--textures")) {
for(int lod=0;lod<model.ModelList.Count;lod++) {
 if(lod==3)continue; // retain original shadow mesh
 var incoming=new SSX3PS2ModelCombiner();incoming.boneDatas=combiner.boneDatasOrg;
 for(int mat=0;mat<textureCount;mat++){var m=model.ModelList[lod].MaterialList[0];m.MainTexture=$"s{mat:000}";m.Texture1="";m.Texture2="";m.Texture3="";m.Texture4="";incoming.materials.Add(m);}
 var faces=new List<M.Face>();
 foreach(var part in rig.GetProperty("parts").EnumerateArray()) {
   int first=part.GetProperty("first_index").GetInt32(),count=part.GetProperty("index_count").GetInt32();
   var ids=Enumerable.Range(first,count).Select(i=>(int)BitConverter.ToUInt32(indices,i*4)).ToArray();
   if(ids.Any(i=>rig.GetProperty("skin")[i].EnumerateArray().Any(w=>rig.GetProperty("bones")[w[0].GetInt32()].GetProperty("file").GetInt32()!=0)))continue;
   for(int f=0;f<ids.Length;f+=3){int a=ids[f],b=ids[f+1],c=ids[f+2];int morph=model.ModelList[lod].MorphKeyCount;
    faces.Add(new M.Face{V1=Pos(a),V2=Pos(b),V3=Pos(c),Normal1=Normal(a),Normal2=Normal(b),Normal3=Normal(c),UV1=UV(a),UV2=UV(b),UV3=UV(c),Weight1=Weight(a),Weight2=Weight(b),Weight3=Weight(c),MorphPoint1=Enumerable.Repeat(Vector3.Zero,morph).ToList(),MorphPoint2=Enumerable.Repeat(Vector3.Zero,morph).ToList(),MorphPoint3=Enumerable.Repeat(Vector3.Zero,morph).ToList(),MaterialID=part.GetProperty("material").GetInt32()});
   }
 }
 incoming.reassignedMesh.Add(new SSX3PS2ModelCombiner.ReassignedMesh {faces=faces,MeshName=model.ModelList[lod].ModelName});
 combiner.StartRegenMesh(incoming,lod);Console.WriteLine($"Sam LOD{lod}: {faces.Count} triangles");
}
model.Save(Path.Combine(output,"mac_TopA.mpf"),false);
}
if(!args.Contains("--textures"))Wrap(Path.Combine(output,"mac_TopA.mpf"));
var reread=new M();reread.load(Path.Combine(output,"mac_TopA.mpf"));Console.WriteLine($"Read-back: {reread.ModelList.Count} models");
var shape=new OldShapeHandler();shape.Format="GIMX";shape.EndingString="";
for(int i=0;i<textureCount;i++) {
 var path=SamSource.Material(root,i);
 using var image=Image.Load<Rgba32>(path);
 // PS2 GS alpha is 0..128.
 image.ProcessPixelRows(accessor=>{for(int y=0;y<accessor.Height;y++){var row=accessor.GetRowSpan(y);for(int x=0;x<row.Length;x++)row[x].A=(byte)((row[x].A*128+127)/255);}});
 var temp=Path.Combine(root,$"local/sam-ps2/material-{i}.png");image.Save(temp);shape.AddImage(OldShapeHandler.MatrixType.FullColor,$"s{i:000}",temp);
}
for(int i=0;i<shape.ShapeImages.Count;i++){var im=shape.ShapeImages[i];im.Longname="";shape.ShapeImages[i]=im;}
shape.SaveShape(Path.Combine(output,"sam.ssh"));
var check=new OldShapeHandler();check.LoadShape(Path.Combine(output,"sam.ssh"));Console.WriteLine($"Textures read back: {check.ShapeImages.Count}");
