// SPDX-License-Identifier: GPL-3.0-only
// Uses GlitcherOG/SSX-Library (GPL-3.0; docs/asset-formats.md, licenses/GPL-3.0.txt).
using System.Numerics;
using System.Text.Json;
using SSXLibrary.FileHandlers.Models.SSX3;
using M = SSXLibrary.FileHandlers.Models.SSX3.SSX3PS2MPF;

// Export independent equipment models. Never overwrite a stock Mac resource.
static class SamParts
{
    // Part -> equipment family comes from the authored package (rider.json parts[].ps2_family, tools/sam_mesh.py).
    static string Family(JsonElement part) => part.GetProperty("ps2_family").GetString()!;

    public static void Build(string root, Action<string> wrap)
    {
        var original=Path.Combine(root,"local/sam-ps2/original");
        var output=Path.Combine(root,"local/sam-ps2/roster/sam-assets");Directory.CreateDirectory(output);
        var native=SamSource.Native(root);int textureCount=SamSource.TextureCount(root);
        using var lodDocument=JsonDocument.Parse(File.ReadAllText(Path.Combine(native,"lods.json")));
        var lodLevels=lodDocument.RootElement.GetProperty("levels").EnumerateArray().ToDictionary(l=>l.GetProperty("name").GetString()!,l=>l.GetProperty("parts").EnumerateArray().ToArray());
        using var document=JsonDocument.Parse(File.ReadAllText(Path.Combine(native,"rider.json")));
        var rig=document.RootElement;
        var vertices=File.ReadAllBytes(Path.Combine(native,"vertices.bin"));
        var indices=File.ReadAllBytes(Path.Combine(native,"indices.bin"));
        var parts=rig.GetProperty("parts").EnumerateArray().ToArray();
        M Load(string name) {var m=new M();m.load(Path.Combine(original,name));return m;}
        var bones=new List<M.BoneData>();
        foreach(var name in new[]{"mac_TopA.mpf","board_BindingsA.mpf","mac_Dangle.mpf"})
            bones.AddRange(Load(name).ModelList[0].BoneList);
        if(bones.Select(b=>b.BoneName).Distinct().Count()!=bones.Count)throw new Exception("Ambiguous bone names");
        Console.WriteLine(JsonSerializer.Serialize(bones.Select(b=>new {b.BoneName,b.FileID,b.BonePos})));
        foreach(var b in rig.GetProperty("bones").EnumerateArray()) {
            var matches=bones.Where(x=>x.BoneName==b.GetProperty("name").GetString()).ToArray();
            if(matches.Length!=1 || matches[0].FileID!=b.GetProperty("file").GetInt32() || matches[0].BonePos!=b.GetProperty("index").GetInt32())
                throw new Exception("PS2/native skeleton mismatch: "+b.GetProperty("name")+" matches="+JsonSerializer.Serialize(matches,new JsonSerializerOptions{IncludeFields=true}));
        }
        Vector3 Position(int i) {int p=i*40;return new(BitConverter.ToSingle(vertices,p)*100,-BitConverter.ToSingle(vertices,p+8)*100,BitConverter.ToSingle(vertices,p+4)*100);}
        Vector3 Normal(int i) {int p=i*40+12;return new(BitConverter.ToSingle(vertices,p),-BitConverter.ToSingle(vertices,p+8),BitConverter.ToSingle(vertices,p+4));}
        Vector4 UV(int i) {int p=i*40+24;return new(BitConverter.ToSingle(vertices,p),BitConverter.ToSingle(vertices,p+4),0,0);}
        M.BoneWeightHeader Weight(int i) {
            var list=new List<M.BoneWeight>();
            foreach(var w in rig.GetProperty("skin")[i].EnumerateArray()) {
                var b=rig.GetProperty("bones")[w[0].GetInt32()];
                list.Add(new M.BoneWeight {BoneID=b.GetProperty("index").GetInt32(),FileID=b.GetProperty("file").GetInt32(),boneName=b.GetProperty("name").GetString(),Weight=(int)Math.Round(w[1].GetDouble()*100)});
            }
            var first=list[0];first.Weight+=100-list.Sum(w=>w.Weight);list[0]=first;
            if(list.Any(w=>w.Weight<0)||list.Sum(w=>w.Weight)!=100)throw new Exception("Invalid quantized skin weights");
            return new M.BoneWeightHeader {WeightCount=list.Count,BoneWeightList=list};
        }
        var templates=new Dictionary<string,string> {
            ["top"]="mac_TopA.mpf",["bottom"]="mac_BottomA.mpf",["boots"]="mac_BootsA.mpf",
            ["hands"]="mac_HandsA.mpf",["head"]="mac_HeadA.mpf",["hair"]="mac_Dangle.mpf",
            ["bindings"]="board_BindingsA.mpf",["board"]="board_BoardFlexA.mpf",["head_nis"]="mac_HeadA_NIS.mpf",
            // FE/NIS gloves: a distinct model for the Hands NIS item (the preview hides the race hands part)
            ["hands_nis"]="mac_HandsA_NIS.mpf"
        };
        var manifest=new List<object>();int uniqueTriangles=0;
        foreach(var (family,template) in templates) {
            string sourceFamily=family=="head_nis"?"head":family=="hands_nis"?"hands":family;
            var selected=parts.Where(p=>Family(p)==sourceFamily).ToArray();
            if(selected.Length==0)throw new Exception("Empty equipment family "+family);
            int triangleCount=selected.Sum(p=>p.GetProperty("index_count").GetInt32()/3);
            if(family!="head_nis"&&family!="hands_nis")uniqueTriangles+=triangleCount;
            var model=Load(template);var lodTriangles=new List<string>();
            var combiner=new SSX3PS2ModelCombiner();combiner.AddFile(model);combiner.boneDatasOrg=bones;
            for(int lod=0;lod<model.ModelList.Count;lod++) {
                var incoming=new SSX3PS2ModelCombiner {boneDatas=bones};
                for(int mat=0;mat<textureCount;mat++) {
                    var m=model.ModelList[lod].MaterialList[0];m.MainTexture=$"s{mat:000}";
                    m.Texture1="";m.Texture2="";m.Texture3="";m.Texture4="";incoming.materials.Add(m);
                }
                int morph=model.ModelList[lod].MorphKeyCount;
                List<Vector3> Morph() => Enumerable.Repeat(Vector3.Zero,morph).ToList();
                var faces=new List<M.Face>();
                void Add(int a,int b,int c,int material)=>faces.Add(new M.Face {V1=Position(a),V2=Position(b),V3=Position(c),Normal1=Normal(a),Normal2=Normal(b),Normal3=Normal(c),UV1=UV(a),UV2=UV(b),UV3=UV(c),Weight1=Weight(a),Weight2=Weight(b),Weight3=Weight(c),MorphPoint1=Morph(),MorphPoint2=Morph(),MorphPoint3=Morph(),MaterialID=material});
                // The template's model names end in _H/_M/_L/_Shdw (the NIS head has one model): H is the authored
                // mesh, the others are Sam's clustered LODs (tools/sam_lod.py), sized like the original templates.
                string modelName=model.ModelList[lod].ModelName,level=modelName.EndsWith("_M")?"M":modelName.EndsWith("_L")?"L":modelName.EndsWith("_Shdw")?"Shdw":"H";
                if(level=="H") {
                    foreach(var part in selected) {
                        int first=part.GetProperty("first_index").GetInt32(),count=part.GetProperty("index_count").GetInt32();
                        for(int f=first;f<first+count;f+=3)
                            Add((int)BitConverter.ToUInt32(indices,f*4),(int)BitConverter.ToUInt32(indices,(f+1)*4),(int)BitConverter.ToUInt32(indices,(f+2)*4),part.GetProperty("material").GetInt32());
                    }
                } else {
                    foreach(var part in lodLevels[level].Where(p=>p.GetProperty("family").GetString()==sourceFamily)) {
                        var ids=part.GetProperty("indices").EnumerateArray().Select(x=>x.GetInt32()).ToArray();
                        for(int f=0;f<ids.Length;f+=3)Add(ids[f],ids[f+1],ids[f+2],part.GetProperty("material").GetInt32());
                    }
                    if(faces.Count==0)throw new Exception($"Empty {level} LOD for {family}");
                }
                lodTriangles.Add($"{modelName}:{faces.Count}");
                incoming.reassignedMesh.Add(new SSX3PS2ModelCombiner.ReassignedMesh {faces=faces,MeshName=model.ModelList[lod].ModelName});
                combiner.StartRegenMesh(incoming,lod);
            }
            string filename=$"sam_{family}.mpf",path=Path.Combine(output,filename);
            model.Save(path,false);wrap(path);
            var check=new M();check.load(path);
            foreach(var h in check.ModelList)foreach(var g in h.MaterialGroupList)foreach(var w in g.WeightRefList)foreach(var mg in w.MorphMeshGroupList)foreach(var ch in mg.MeshChunkList)foreach(var v in ch.Vertices)
                if(!float.IsFinite(v.X)||!float.IsFinite(v.Y)||!float.IsFinite(v.Z))throw new Exception("Nonfinite PS2 vertex");
            manifest.Add(new {family,filename,template,triangles=triangleCount,lods=lodTriangles,models=check.ModelList.Select(h=>h.ModelName).ToArray(),parts=selected.Select(p=>p.GetProperty("name").GetString()).ToArray()});
            Console.WriteLine($"{filename}: {triangleCount} authored triangles, {check.ModelList.Count} decoded models");
        }
        if(uniqueTriangles!=indices.Length/12)throw new Exception("Some authored triangles omitted or duplicated");
        File.WriteAllText(Path.Combine(output,"parts-manifest.json"),JsonSerializer.Serialize(new {uniqueTriangles,models=manifest,note="Morph deltas are zero placeholders; original bone animations drive the rig. _M/_L/_Shdw models are Sam's clustered LODs (tools/sam_lod.py)."},new JsonSerializerOptions{WriteIndented=true}));
    }
}
