// SPDX-License-Identifier: GPL-3.0-only
// Uses GlitcherOG/SSX-Library (GPL-3.0; docs/asset-formats.md, licenses/GPL-3.0.txt).
using System.Text.Json;
using M=SSXLibrary.FileHandlers.Models.SSX3.SSX3PS2MPF;

static class SamTopVariant
{
    static string Geometry(M model) => JsonSerializer.Serialize(model.ModelList.Select(h=>new {
        h.ModelName,h.BoneList,h.MorphKeyIDList,h.BoneWeightHeaderList,h.WeightRefrenceLists,
        groups=h.MaterialGroupList.Select(g=>g.WeightRefList.Select(w=>w.MorphMeshGroupList.Select(m=>new {
            m.MorphDataList,chunks=m.MeshChunkList.Select(c=>new {c.Strips,c.UV,c.UVNormals,c.Vertices,c.Weights,c.MatieralID})
        })))
    }),new JsonSerializerOptions {IncludeFields=true});

    public static void Build(string root,Action<string> wrap)
    {
        string folder=Path.Combine(root,"local/sam-ps2/roster/sam-assets");int textureCount=SamSource.TextureCount(root);
        var model=new M();model.load(Path.Combine(folder,"sam_top.mpf"));string before=Geometry(model);
        foreach(var header in model.ModelList) {
            for(int i=0;i<header.MaterialList.Count;i++) {
                var material=header.MaterialList[i];
                if(material.MainTexture.Length!=4 || material.MainTexture[0]!='s' || !int.TryParse(material.MainTexture[1..],out int id) || id>=textureCount)
                    throw new Exception("Unexpected top material namespace");
                material.MainTexture="u"+material.MainTexture[1..];header.MaterialList[i]=material;
            }
        }
        string output=Path.Combine(folder,"sam_top_uphill.mpf");model.Save(output,false);wrap(output);
        var check=new M();check.load(output);
        if(Geometry(check)!=before)throw new Exception("Variant altered geometry or skinning");
        if(check.ModelList.SelectMany(h=>h.MaterialList).Any(m=>!m.MainTexture.StartsWith("u")))throw new Exception("Variant material namespace missing");
        File.WriteAllText(Path.Combine(folder,"uphill-model-verification.json"),JsonSerializer.Serialize(new {
            source="sam_top.mpf",output="sam_top_uphill.mpf",geometry_and_skinning_unchanged=true,
            geometry_sha256=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(before))).ToLowerInvariant(),
            textures=check.ModelList.SelectMany(h=>h.MaterialList).Select(m=>m.MainTexture).Distinct().ToArray()
        },new JsonSerializerOptions {WriteIndented=true}));
        Console.WriteLine("Uphill top: unique texture names; geometry, UVs, skinning, morph data and model names preserved");
    }
}
