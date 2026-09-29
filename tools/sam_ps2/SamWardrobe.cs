// SPDX-License-Identifier: GPL-3.0-only
// Uses GlitcherOG/SSX-Library (GPL-3.0; docs/asset-formats.md, licenses/GPL-3.0.txt).
using System.Text.Json;
using System.Text.RegularExpressions;
using SSXLibrary.FileHandlers;

static class SamWardrobe
{
    public static void Build(string root)
    {
        var folder=Path.Combine(root,"local/sam-ps2/roster/sam-assets");
        using var doc=JsonDocument.Parse(File.ReadAllText(Path.Combine(folder,"parts-manifest.json")));
        var models=doc.RootElement.GetProperty("models").EnumerateArray().ToDictionary(m=>m.GetProperty("family").GetString()!);
        var bolts=new BoltPS2Handler();bolts.load(Path.Combine(root,"local/sam-ps2/original/BOLTPS2.DAT"));
        if(bolts.characters.Count!=30)throw new Exception("Unexpected original wardrobe size");
        var source=bolts.characters[3];
        using var namesDoc=JsonDocument.Parse(File.ReadAllText(Path.Combine(root,"sam_character/gear-names.json")));
        var namesById=namesDoc.RootElement.EnumerateArray().ToDictionary(x=>x.GetProperty("item_id").GetInt32());
        var sam=new Character {entries=source.entries.ToList(),unkown2s=source.unkown2s.ToList()};
        // Equip Gear parts (tools/sam_wardrobe.py -> local/sam-model/wardrobe-spec.json, SamWardrobeParts.cs -> w/manifest.json):
        // every row takes the browser's model/texture: Sam's own parts, Mac accessories moved to Sam's head (samfit_),
        // Mac's body-worn accessories as they are; the spec's rules replace the bucket's.
        string specPath=Path.Combine(root,"local/sam-model/wardrobe-spec.json"),manifestPath=Path.Combine(folder,"w/manifest.json");
        JsonDocument? specDoc=File.Exists(specPath)&&File.Exists(manifestPath)?JsonDocument.Parse(File.ReadAllText(specPath)):null;
        JsonDocument? manifestDoc=specDoc!=null?JsonDocument.Parse(File.ReadAllText(manifestPath)):null;
        int mapped=0,disabled=0,renamed=0;
        for(int i=0;i<sam.entries.Count;i++) {
            var entry=sam.entries[i];entry.CharacterID=30;
            if(entry.ItemID is 49 or 55 or 59)entry.SmallIcon="su01";
            if(namesById.TryGetValue(entry.ItemID,out var naming)) {
                if(entry.itemName!=naming.GetProperty("original_name").GetString())throw new Exception($"Unexpected source gear name for item {entry.ItemID}");
                entry.itemName=naming.GetProperty("name").GetString();renamed++;
                // Sam's own outfits (Equip Gear): owned from a fresh profile (price field 0, 0x1513B8) and out of
                // the award pools (+0x34 flags 0x100/0x200/0x400) when gear-names.json says so
                if(naming.TryGetProperty("cost",out var cost))entry.Cost=cost.GetInt32();
                if(naming.TryGetProperty("unlock",out var unlock))entry.Unlock=unlock.GetInt32();
                if(naming.TryGetProperty("clear_flags",out var clear))entry.unkownInt6&=~clear.GetInt32();
                if(naming.TryGetProperty("icon",out var icon))entry.SmallIcon=icon.GetString();   // sam_icons.ssh (SamTextures.cs)
            }
            if(specDoc!=null) {
                var src=source.entries[i];string? model=null,texture=null;int slot=src.FileID,group=src.unkownInt2;
                if(specDoc.RootElement.GetProperty("items").TryGetProperty(entry.ItemID.ToString(),out var it)) {
                    model=it.GetProperty("model").ValueKind==JsonValueKind.String?it.GetProperty("model").GetString():null;
                    texture=it.GetProperty("texture").ValueKind==JsonValueKind.String?it.GetProperty("texture").GetString():null;
                    slot=it.GetProperty("slot").GetInt32();group=it.GetProperty("group").GetInt32()&0xFF;
                }
                if(model==null) {
                    if(!string.IsNullOrEmpty(src.ModelPath)){entry.ModelPath=null;entry.ModelID=null;entry.ModelID2=null;entry.ModelID3=null;entry.ModelID4=null;entry.buyable=0;disabled++;}
                } else if(model.StartsWith("sam_")||model.StartsWith("samfit_")) {
                    var mf=manifestDoc!.RootElement.GetProperty("models").GetProperty(model);var names=mf.GetProperty("models").EnumerateArray().Select(n=>n.GetString()).ToArray();
                    entry.ModelPath="data/char/mdlps2.big|"+mf.GetProperty("file").GetString();
                    entry.ModelID=names[0];entry.ModelID2=names.Length>1?names[1]:null;entry.ModelID3=names.Length>2?names[2]:null;entry.ModelID4=names.Length>3?names[3]:null;
                    entry.FileID=slot;entry.buyable=src.buyable;mapped++;
                } else {entry.ModelPath=src.ModelPath;entry.ModelID=src.ModelID;entry.ModelID2=src.ModelID2;entry.ModelID3=src.ModelID3;entry.ModelID4=src.ModelID4;entry.FileID=src.FileID;entry.buyable=src.buyable;mapped++;}
                entry.TexturePath=texture==null?null:texture.StartsWith("sam_")?"data/char/mactxp.big|"+texture+".ssh":src.TexturePath;
                entry.unkownInt2=group;
                sam.entries[i]=entry;continue;
            }
            if(!string.IsNullOrEmpty(entry.TexturePath))entry.TexturePath="data/char/mactxp.big|sam_textures.ssh";
            if(!string.IsNullOrEmpty(entry.ModelPath)) {
                string file=entry.ModelPath.Split('|').Last().ToLowerInvariant();string family=null;
                if(Regex.IsMatch(file,@"^mac_head[a-z]_nis\.mpf$"))family="head_nis";
                else if(Regex.IsMatch(file,@"^mac_head[a-z]\.mpf$"))family="head";
                else if(Regex.IsMatch(file,@"^mac_top[a-z]\.mpf$"))family="top";
                else if(Regex.IsMatch(file,@"^mac_bottom[a-z]\.mpf$"))family="bottom";
                else if(Regex.IsMatch(file,@"^mac_boots[a-z]\.mpf$"))family="boots";
                else if(Regex.IsMatch(file,@"^mac_hands[a-z]_nis\.mpf$"))family="hands_nis";   // Select/Setup preview gloves
                else if(Regex.IsMatch(file,@"^mac_hands[a-z]\.mpf$"))family="hands";
                else if(file=="mac_dangle.mpf")family="hair";
                else if(Regex.IsMatch(file,@"^board_bindings[a-z]\.mpf$"))family="bindings";
                else if(Regex.IsMatch(file,@"^board_boardflex[a-z]\.mpf$"))family="board";
                if(family!=null) {
                    var model=models[family];var names=model.GetProperty("models").EnumerateArray().Select(n=>n.GetString()).ToArray();
                    entry.ModelPath="data/char/mdlps2.big|"+model.GetProperty("filename").GetString();
                    entry.ModelID=names[0];entry.ModelID2=names.Length>1?names[1]:null;
                    entry.ModelID3=names.Length>2?names[2]:null;entry.ModelID4=names.Length>3?names[3]:null;
                    entry.TexturePath="data/char/mactxp.big|sam_textures.ssh";mapped++;
                } else {
                    // Unconverted accessories must not bring original Mac meshes
                    // back onto Sam. Keep hierarchy IDs for compatibility rules.
                    entry.ModelPath=null;entry.ModelID=null;entry.ModelID2=null;entry.ModelID3=null;entry.ModelID4=null;
                    entry.TexturePath=null;entry.buyable=0;disabled++;
                }
            }
            // Sam's cap is part of his head model, so the headwear slot Mac's default outfit equips (150
            // "Beanie Cap w Peak", file 50, no Sam conversion) carries Sam's hair instead: the Dangle model
            // in the Dangle file slot (28, secondary-motion bones sec_dangle_l/r). Without it the default
            // outfit showed no hair in the Select Character preview or the race (143 is not equipped).
            if(specDoc==null&&entry.ItemID==150) {
                var hair=models["hair"];var names=hair.GetProperty("models").EnumerateArray().Select(n=>n.GetString()).ToArray();
                var original=source.entries[i];
                entry.ModelPath="data/char/mdlps2.big|"+hair.GetProperty("filename").GetString();
                entry.ModelID=names[0];entry.ModelID2=names.Length>1?names[1]:null;entry.ModelID3=names.Length>2?names[2]:null;entry.ModelID4=names.Length>3?names[3]:null;
                entry.TexturePath="data/char/mactxp.big|sam_textures.ssh";entry.FileID=28;entry.buyable=original.buyable;disabled--;mapped++;
            }
            // The selectable top and its model dependency must use the same
            // palette; other equipment retains its original Sam texture pack.
            if(specDoc==null&&entry.ItemID is 52 or 55)entry.TexturePath="data/char/mactxp.big|sam_uphill.ssh";
            if(specDoc==null&&entry.ItemID==52)entry.ModelPath="data/char/mdlps2.big|sam_top_uphill.mpf";
            if(entry.ItemID==55)entry.SmallIcon="su02";
            sam.entries[i]=entry;
        }
        for(int i=0;i<sam.unkown2s.Count;i++){var rule=sam.unkown2s[i];rule.CharacterID=30;sam.unkown2s[i]=rule;}
        if(specDoc!=null) {
            // the browser's rule list (Mac's, less the hood rules, plus Sam's hair under hats), bytes 4/8 kept from the source rule
            var rules=new List<Unkown2>();
            foreach(var r in specDoc.RootElement.GetProperty("rules").EnumerateArray()) {
                int on=r[0].GetInt32(),item=r[1].GetInt32(),cond=r[2].GetInt32(),state=r[3].GetInt32(),target=r[4].GetInt32(),desired=r[5].GetInt32();
                var src=sam.unkown2s.FirstOrDefault(u=>u.BoolInt==on&&u.UnkownInt==item&&u.UnkownInt4==cond&&(sbyte)u.UnkownInt3==state&&u.UnkownInt7==target&&u.BoolInt2==desired,new Unkown2{CharacterID=30,BoolInt=-1});
                if(src.BoolInt==-1)src=new Unkown2{CharacterID=30,BoolInt=on,UnkownInt=item,UnkownInt2=0,UnkownInt3=state&0xFF,UnkownInt4=cond,UnkownInt5=0,BoolInt2=desired,UnkownInt7=target};
                rules.Add(src);
            }
            sam.unkown2s=rules;
        }
        bolts.characters.Add(sam);
        if(renamed!=namesById.Count)throw new Exception("Some Sam gear names did not match a wardrobe row");
        foreach(var sourceRule in bolts.unkown3.Where(r=>r.UnkownInt==3).ToArray()){var rule=sourceRule;rule.UnkownInt=30;bolts.unkown3.Add(rule);}
        foreach(var sourceRule in bolts.unkown4.Where(r=>r.UnkownInt==3).ToArray()){var rule=sourceRule;rule.UnkownInt=30;bolts.unkown4.Add(rule);}
        var path=Path.Combine(folder,"BOLTPS2.DAT");bolts.Save(path);
        var check=new BoltPS2Handler();check.load(path);
        if(check.characters.Count!=31 || check.characters[30].entries.Count!=source.entries.Count)throw new Exception("Wardrobe roundtrip failed");
        File.WriteAllText(Path.Combine(folder,"wardrobe-manifest.json"),JsonSerializer.Serialize(new {character_id=30,mapped_model_entries=mapped,disabled_unconverted_accessories=disabled,renamed_items=renamed,item_rows=sam.entries.Count,compatibility_rows=sam.unkown2s.Count,note="Sam-specific starting gear names; visual variants and thumbnails remain unfinished."},new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine($"Sam wardrobe: {mapped} model references, {disabled} unconverted accessories disabled; 31 characters read back");
    }
}
