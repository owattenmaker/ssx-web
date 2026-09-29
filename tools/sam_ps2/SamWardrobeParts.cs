// SPDX-License-Identifier: GPL-3.0-only
// Uses GlitcherOG/SSX-Library (GPL-3.0; docs/asset-formats.md, licenses/GPL-3.0.txt).
using System.Numerics;
using System.Text.Json;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SSX_Library.EATextureLibrary;
using SSXLibrary.FileHandlers.Models.SSX3;
using M = SSXLibrary.FileHandlers.Models.SSX3.SSX3PS2MPF;

// Sam's Equip Gear parts and textures for the PS2 bucket (tools/sam_wardrobe.py writes the browser WARDROBE/SAM
// parts, local/sam-model/wardrobe-spec.json and wardrobe-lods.json):
//   sam_*    Sam-authored parts -> MPFs on the matching Mac template, H from the part, M/L/Shdw from the LOD sidecar
//   samfit_* Mac head-worn accessories moved to Sam's head: the Mac MPF with every LOD's vertices transformed
//   textures every sam_* texture stem -> its own SSH pack holding one image named after the part material
// Output: local/sam-ps2/roster/sam-assets/w/*.mpf|*.ssh and w/manifest.json (model names per file for the rows).
static class SamWardrobeParts
{
    static readonly Dictionary<string,string> Templates = new() {
        ["sam_topa"]="mac_TopA.mpf", ["sam_topd"]="mac_TopD.mpf", ["sam_bottomb"]="mac_BottomB.mpf", ["sam_bootsa"]="mac_BootsA.mpf",
        ["sam_handsa"]="mac_HandsA.mpf", ["sam_handsa_nis"]="mac_HandsA_NIS.mpf", ["sam_heada"]="mac_HeadA.mpf", ["sam_heada_nis"]="mac_HeadA_NIS.mpf",
        ["sam_hair"]="mac_Dangle.mpf", ["sam_cap"]="mac_HeadBoltB1.mpf", ["sam_hair_side"]="mac_Elephant6.mpf", ["sam_afro"]="mac_Antenna3.mpf",
        ["sam_helmet"]="mac_HeadBoltB14.mpf", ["sam_kit_net"]="mac_Backpack.mpf", ["sam_kit_tube"]="mac_Backpack2.mpf",
        ["sam_bindingsa"]="board_BindingsA.mpf", ["sam_boardflexa"]="board_BoardFlexA.mpf",
    };

    public static void Build(string root, Action<string> wrap)
    {
        var original = Path.Combine(root, "local/sam-ps2/original");
        var output = Path.Combine(root, "local/sam-ps2/roster/sam-assets/w"); Directory.CreateDirectory(output);
        foreach (var old in Directory.GetFiles(output)) File.Delete(old);
        var web = Path.Combine(root, "web/public/assets/WARDROBE/SAM");
        using var wdoc = JsonDocument.Parse(File.ReadAllText(Path.Combine(web, "wardrobe.json")));
        using var ldoc = JsonDocument.Parse(File.ReadAllText(Path.Combine(root, "local/sam-model/wardrobe-lods.json")));
        using var sdoc = JsonDocument.Parse(File.ReadAllText(Path.Combine(root, "local/sam-model/wardrobe-spec.json")));
        var bin = File.ReadAllBytes(Path.Combine(web, "parts.bin"));
        var parts = wdoc.RootElement.GetProperty("parts");
        M Load(string name) { var m = new M(); m.load(Path.Combine(original, name)); return m; }
        // bone records of every template (unique by name): the body, board, and the secondary-bone parts
        var bones = new List<M.BoneData>();
        foreach (var name in new[] { "mac_TopA.mpf", "board_BindingsA.mpf", "mac_Dangle.mpf", "mac_Elephant6.mpf", "mac_Antenna3.mpf", "mac_Backpack.mpf" })
            foreach (var b in Load(name).ModelList[0].BoneList) if (!bones.Any(x => x.BoneName == b.BoneName)) bones.Add(b);
        var boneName = new Dictionary<(int, int), string>();
        foreach (var p in parts.EnumerateObject()) foreach (var b in p.Value.GetProperty("bones").EnumerateArray())
            boneName[(b.GetProperty("file").GetInt32(), b.GetProperty("index").GetInt32())] = b.GetProperty("name").GetString()!;
        var manifest = new Dictionary<string, object>();
        // short member names (sm00.mpf ..): MDLPS2.BIG's directory padding holds 41 of them without moving any original payload
        int serial = 0; string Short() => $"sm{serial++:00}.mpf";

        foreach (var p in parts.EnumerateObject())
        {
            string key = p.Name, stem = key.Replace(".mnf", "");
            if (key.StartsWith("sam_"))
            {
                var part = p.Value; var model = Load(Templates[stem]);
                int vo = part.GetProperty("vertex_offset").GetInt32(), vc = part.GetProperty("vertex_count").GetInt32();
                int io = part.GetProperty("index_offset").GetInt32(), so = part.GetProperty("skin_offset").GetInt32();
                Vector3 Pos(int i) { int a = vo + i * 40; return new(BitConverter.ToSingle(bin, a) * 100, -BitConverter.ToSingle(bin, a + 8) * 100, BitConverter.ToSingle(bin, a + 4) * 100); }
                Vector3 Nrm(int i) { int a = vo + i * 40 + 12; return new(BitConverter.ToSingle(bin, a), -BitConverter.ToSingle(bin, a + 8), BitConverter.ToSingle(bin, a + 4)); }
                Vector4 UV(int i) { int a = vo + i * 40 + 24; return new(BitConverter.ToSingle(bin, a), BitConverter.ToSingle(bin, a + 4), 0, 0); }
                M.BoneWeightHeader Wt(int i)
                {
                    int a = so + i * 20, n = bin[a]; var list = new List<M.BoneWeight>();
                    for (int k = 0; k < n; k++) { int f = bin[a + 4 + k * 4], b = bin[a + 5 + k * 4], w = BitConverter.ToInt16(bin, a + 6 + k * 4);
                        list.Add(new M.BoneWeight { BoneID = b, FileID = f, boneName = boneName[(f, b)], Weight = w }); }
                    var first = list[0]; first.Weight += 100 - list.Sum(x => x.Weight); list[0] = first;
                    return new M.BoneWeightHeader { WeightCount = list.Count, BoneWeightList = list };
                }
                var materials = part.GetProperty("batches").EnumerateArray().Select(b => b.GetProperty("material").GetString()!).Distinct().ToList();
                var combiner = new SSX3PS2ModelCombiner(); combiner.AddFile(model); combiner.boneDatasOrg = bones;
                var lodCounts = new List<string>();
                for (int lod = 0; lod < model.ModelList.Count; lod++)
                {
                    var incoming = new SSX3PS2ModelCombiner { boneDatas = bones };
                    foreach (var mat in materials) { var m = model.ModelList[lod].MaterialList[0]; m.MainTexture = mat; m.Texture1 = ""; m.Texture2 = ""; m.Texture3 = ""; m.Texture4 = ""; incoming.materials.Add(m); }
                    int morph = model.ModelList[lod].MorphKeyCount;
                    List<Vector3> Morph() => Enumerable.Repeat(Vector3.Zero, morph).ToList();
                    var faces = new List<M.Face>();
                    void Add(int a, int b, int c, string mat) => faces.Add(new M.Face { V1 = Pos(a), V2 = Pos(b), V3 = Pos(c), Normal1 = Nrm(a), Normal2 = Nrm(b), Normal3 = Nrm(c), UV1 = UV(a), UV2 = UV(b), UV3 = UV(c), Weight1 = Wt(a), Weight2 = Wt(b), Weight3 = Wt(c), MorphPoint1 = Morph(), MorphPoint2 = Morph(), MorphPoint3 = Morph(), MaterialID = materials.IndexOf(mat) });
                    string name = model.ModelList[lod].ModelName, level = name.EndsWith("_M") ? "M" : name.EndsWith("_L") ? "L" : name.EndsWith("_Shdw") ? "Shdw" : "H";
                    if (level == "H")
                        foreach (var b in part.GetProperty("batches").EnumerateArray())
                        {
                            int first = b.GetProperty("first").GetInt32(), count = b.GetProperty("count").GetInt32(); string mat = b.GetProperty("material").GetString()!;
                            for (int f = first; f < first + count; f += 3)
                                Add((int)BitConverter.ToUInt32(bin, io + f * 4), (int)BitConverter.ToUInt32(bin, io + (f + 1) * 4), (int)BitConverter.ToUInt32(bin, io + (f + 2) * 4), mat);
                        }
                    else
                        foreach (var b in ldoc.RootElement.GetProperty(key).GetProperty(level).EnumerateArray())
                        {
                            var ids = b.GetProperty("indices").EnumerateArray().Select(x => x.GetInt32()).ToArray(); string mat = b.GetProperty("material").GetString()!;
                            for (int f = 0; f < ids.Length; f += 3) Add(ids[f], ids[f + 1], ids[f + 2], mat);
                        }
                    if (faces.Count == 0) throw new Exception($"{key} {level}: no faces");
                    incoming.reassignedMesh.Add(new SSX3PS2ModelCombiner.ReassignedMesh { faces = faces, MeshName = name });
                    combiner.StartRegenMesh(incoming, lod); lodCounts.Add($"{name}:{faces.Count}");
                }
                string file = Short(), path = Path.Combine(output, file);
                model.Save(path, false); wrap(path);
                var check = new M(); check.load(path);
                foreach (var h in check.ModelList) foreach (var g in h.MaterialGroupList) foreach (var w in g.WeightRefList) foreach (var mg in w.MorphMeshGroupList) foreach (var ch in mg.MeshChunkList) foreach (var v in ch.Vertices)
                    if (!float.IsFinite(v.X) || !float.IsFinite(v.Y) || !float.IsFinite(v.Z)) throw new Exception("Nonfinite vertex in " + file);
                manifest[key] = new { file, models = check.ModelList.Select(h => h.ModelName).ToArray(), lods = lodCounts, template = Templates[stem] };
            }
            else if (key.StartsWith("samfit_"))
            {
                // the Mac MPF of this accessory, every LOD moved from Mac's head to Sam's (PS2 = (x, -z, y) * 100 of Y-up m)
                var fit = sdoc.RootElement.GetProperty("head_fit");
                float sx = fit.GetProperty("sx").GetSingle(), dy = fit.GetProperty("dy").GetSingle() * 100, dz = fit.GetProperty("dz").GetSingle() * 100;
                string resource = p.Value.GetProperty("source_resource").GetString()!;   // mac_HeadBoltB6.mnf
                string template = resource.Replace(".mnf", ".mpf");
                var model = Load(template);
                foreach (var h in model.ModelList) foreach (var g in h.MaterialGroupList) foreach (var w in g.WeightRefList) foreach (var mg in w.MorphMeshGroupList) foreach (var ch in mg.MeshChunkList)
                    for (int i = 0; i < ch.Vertices.Count; i++) { var v = ch.Vertices[i]; ch.Vertices[i] = new Vector3(v.X * sx, v.Y - dz, v.Z + dy); }
                string file = Short(), path = Path.Combine(output, file);
                model.Save(path, false); wrap(path);
                var check = new M(); check.load(path);
                manifest[key] = new { file, models = check.ModelList.Select(h => h.ModelName).ToArray(), template };
            }
        }
        // textures: one pack per Sam stem, the image named after the part material, PS2 texel domain
        var textures = new List<string>();
        foreach (var t in wdoc.RootElement.GetProperty("textures").EnumerateObject())
        {
            if (!t.Name.StartsWith("sam_")) continue;
            string material = t.Value.GetProperty("name").GetString()!;
            if (material.Length > 4) throw new Exception($"{t.Name}: SSH image name {material} is longer than 4 characters");
            using var image = Image.Load<Rgba32>(Path.Combine(web, "textures", t.Name + ".png"));
            image.ProcessPixelRows(a => { for (int y = 0; y < a.Height; y++) { var row = a.GetRowSpan(y); for (int x = 0; x < row.Length; x++) { var q = row[x]; row[x] = new Rgba32((byte)((q.R + 1) >> 1), (byte)((q.G + 1) >> 1), (byte)((q.B + 1) >> 1), (byte)((q.A * 128 + 127) / 255)); } } });
            string png = Path.Combine(output, t.Name + ".png");
            image.SaveAsPng(png, new SixLabors.ImageSharp.Formats.Png.PngEncoder { ColorType = SixLabors.ImageSharp.Formats.Png.PngColorType.RgbWithAlpha });
            var pack = new OldShapeHandler { Format = "GIMX", EndingString = "" };
            pack.AddImage(OldShapeHandler.MatrixType.FullColor, material, png);
            for (int i = 0; i < pack.ShapeImages.Count; i++) { var e = pack.ShapeImages[i]; e.Longname = ""; pack.ShapeImages[i] = e; }
            string ssh = Path.Combine(output, t.Name + ".ssh"); pack.SaveShape(ssh); File.Delete(png);
            var check = new OldShapeHandler(); check.LoadShape(ssh);
            if (check.ShapeImages.Count != 1 || check.ShapeImages[0].Shortname != material) throw new Exception("Pack check " + t.Name);
            textures.Add(t.Name);
        }
        File.WriteAllText(Path.Combine(output, "manifest.json"), JsonSerializer.Serialize(new { models = manifest, textures }, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"Sam wardrobe PS2: {manifest.Count} model files, {textures.Count} texture packs");
    }
}
