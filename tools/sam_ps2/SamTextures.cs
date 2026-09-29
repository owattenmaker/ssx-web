// SPDX-License-Identifier: GPL-3.0-only
// Uses GlitcherOG/SSX-Library (GPL-3.0; docs/asset-formats.md, licenses/GPL-3.0.txt).
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;
using SSX_Library.EATextureLibrary;

static class SamTextures
{
    // PS2 rider texel domain: RGB (v+1)>>1 (0..128 = the GameCube 0..255), alpha 0..128 GS.
    static void HalveToPs2(Image<Rgba32> image) => image.ProcessPixelRows(accessor=> {
        for(int y=0;y<accessor.Height;y++) {
            var row=accessor.GetRowSpan(y);
            for(int x=0;x<row.Length;x++){var p=row[x];row[x]=new Rgba32((byte)((p.R+1)>>1),(byte)((p.G+1)>>1),(byte)((p.B+1)>>1),(byte)((p.A*128+127)/255));}
        }
    });

    public static void Build(string root)
    {
        string folder=Path.Combine(root,"local/sam-ps2/roster/sam-assets");Directory.CreateDirectory(folder);
        var pack=new OldShapeHandler {Format="GIMX",EndingString=""};
        var icons=new OldShapeHandler {Format="GIMX",EndingString=""};
        int count=SamSource.TextureCount(root);
        for(int i=0;i<count;i++) {
            // Runtime maps at their authored roster size (256 suit, 128 or 128x256 others; tools/sam_textures.py).
            using var image=Image.Load<Rgba32>(SamSource.Material(root,i));
            if(image.Width>256||image.Height>256||(image.Width&(image.Width-1))!=0||(image.Height&(image.Height-1))!=0)throw new Exception($"material-{i} is not a PS2 texture size");
            // Rider texels on the PS2 are half the GameCube-domain range (the originals' PS2 textures; tools/
            // export_characters.py ps2_texel_rgba halves a GameCube-only map the same way): Sam's authored maps are
            // GameCube domain (255 = 1.0), so full-range texels drew twice as bright on the PS2.
            HalveToPs2(image);
            string file=Path.Combine(folder,$"material-{i}.png");image.Save(file);
            pack.AddImage(OldShapeHandler.MatrixType.FullColor,$"s{i:000}",file);
        }
        // su01/su02 are the design thumbnails; su03..su06 the outfit items rendered from Sam's packages
        // (tools/sam_icons/render.mjs -> local/sam-model/icons): Sunday Unc, Lodge Legend, Catch & Release, Packed for the Creek
        var iconSources=new List<(string,string)>{(Path.Combine(root,"sam_character/design/gear-top-midwest-unc-v1.png"),"su01"),(Path.Combine(root,"sam_character/design/uphill-club-thumbnail-v1.png"),"su02")};
        foreach(var n in new[]{"su03","su04","su05","su06"})iconSources.Add((Path.Combine(root,"local/sam-model/icons",n+".png"),n));
        foreach(var (source,name) in iconSources)
        using(var icon=Image.Load<Rgba32>(source)) {
            icon.Mutate(x=>x.Resize(64,64));
            icon.ProcessPixelRows(accessor=> {
                for(int y=0;y<accessor.Height;y++) {
                    var row=accessor.GetRowSpan(y);
                    for(int x=0;x<row.Length;x++)row[x].A=(byte)((row[x].A*128+127)/255);
                }
            });
            string iconPath=Path.Combine(folder,$"sam-top-icon-{name}.png");
            icon.SaveAsPng(iconPath,new SixLabors.ImageSharp.Formats.Png.PngEncoder {
                ColorType=SixLabors.ImageSharp.Formats.Png.PngColorType.RgbWithAlpha
            });
            icons.AddImage(OldShapeHandler.MatrixType.FullColor,name,iconPath);
        }
        for(int i=0;i<pack.ShapeImages.Count;i++){var image=pack.ShapeImages[i];image.Longname="";pack.ShapeImages[i]=image;}
        string path=Path.Combine(folder,"sam_textures.ssh");pack.SaveShape(path);
        var check=new OldShapeHandler();check.LoadShape(path);
        if(check.ShapeImages.Count!=count)throw new Exception("Texture count mismatch");
        for(int i=0;i<count;i++)if(check.ShapeImages[i].Shortname!=$"s{i:000}")throw new Exception("Texture identity mismatch");
        // Uphill Club: the same pack with the suit map (s000 -> u000) replaced by the cobalt/burnt-orange
        // colorway painted in the same layout (tools/sam_textures.py 'uphill_club'); the other maps are identical.
        using(var suit=Image.Load<Rgba32>(Path.Combine(SamSource.Exports(root),"uphill-suit.png"))) {
            HalveToPs2(suit);
            string suitPath=Path.Combine(folder,"sam-uphill-suit.png");
            suit.SaveAsPng(suitPath,new SixLabors.ImageSharp.Formats.Png.PngEncoder {ColorType=SixLabors.ImageSharp.Formats.Png.PngColorType.RgbWithAlpha});
            var uphill=new OldShapeHandler();uphill.LoadShape(path);uphill.LoadSingleImage(suitPath,0);
            for(int i=0;i<uphill.ShapeImages.Count;i++){var entry=uphill.ShapeImages[i];entry.Longname="";entry.Shortname=$"u{i:000}";uphill.ShapeImages[i]=entry;}
            uphill.EndingString="";
            uphill.SaveShape(Path.Combine(folder,"sam_uphill.ssh"));
            var uphillCheck=new OldShapeHandler();uphillCheck.LoadShape(Path.Combine(folder,"sam_uphill.ssh"));
            if(uphillCheck.ShapeImages.Count!=count)throw new Exception("Uphill pack count mismatch");
        }
        for(int i=0;i<icons.ShapeImages.Count;i++){var iconEntry=icons.ShapeImages[i];iconEntry.Longname="";icons.ShapeImages[i]=iconEntry;}
        string iconArchive=Path.Combine(folder,"sam_icons.ssh");icons.SaveShape(iconArchive);
        var iconCheck=new OldShapeHandler();iconCheck.LoadShape(iconArchive);
        if(iconCheck.ShapeImages.Count!=iconSources.Count)throw new Exception("Gear thumbnail count mismatch");
        for(int i=0;i<iconSources.Count;i++)if(iconCheck.ShapeImages[i].Shortname!=$"su0{i+1}" || iconCheck.ShapeImages[i].Image.Width!=64 || iconCheck.ShapeImages[i].Image.Height!=64)throw new Exception("Gear thumbnail mismatch");
        Console.WriteLine("Sam packs decoded: base and Uphill Club model textures; six 64x64 gear thumbnails; GS alpha encoded");
    }
}
