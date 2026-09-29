// SPDX-License-Identifier: GPL-3.0-only
// Uses GlitcherOG/SSX-Library (GPL-3.0; docs/asset-formats.md, licenses/GPL-3.0.txt).
using System.Text.Json;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;
using SSX_Library.EATextureLibrary;

static class SamRosterTexture
{
    public static void Build(string root)
    {
        string folder=Path.Combine(root,"local/sam-ps2/roster");
        using var source=Image.Load<Rgba32>(Path.Combine(root,"sam_character/design/sam-roster-silhouette-mask.png"));
        int left=source.Width,top=source.Height,right=0,bottom=0;
        using var mask=new Image<Rgba32>(source.Width,source.Height);
        for(int y=0;y<source.Height;y++)for(int x=0;x<source.Width;x++) {
            var p=source[x,y];bool covered=p.A>128&&p.R>128&&p.G>128&&p.B>128;
            mask[x,y]=new Rgba32(255,255,255,covered?(byte)255:(byte)0);
            if(covered){left=Math.Min(left,x);right=Math.Max(right,x);top=Math.Min(top,y);bottom=Math.Max(bottom,y);}
        }
        if(right<=left||bottom<=top)throw new Exception("Empty silhouette mask");
        mask.Mutate(x=>x.Crop(new Rectangle(left,top,right-left+1,bottom-top+1)).Resize(28,74));
        var stock=new OldShapeHandler();stock.LoadShape(Path.Combine(root,"local/sam-ps2/original/FE_1.SSH"));
        if(stock.ShapeImages.Count!=22)throw new Exception("Unexpected original UI texture count");
        // Match the dominant color in the original Mac highlight sprite.
        var histogram=new Dictionary<(byte,byte,byte),int>();var sheet=stock.ShapeImages[20].Image;
        for(int y=434;y<506;y++)for(int x=237;x<261;x++) {
            var p=sheet[x,y];if(p.A<32||p.R<128||p.G>220||p.B>80)continue;
            var key=(p.R,p.G,p.B);histogram[key]=histogram.GetValueOrDefault(key)+1;
        }
        if(histogram.Count==0)throw new Exception("Original roster highlight color not found");
        var color=histogram.MaxBy(x=>x.Value).Key;
        using var atlas=new Image<Rgba32>(64,128);
        for(int y=0;y<74;y++)for(int x=0;x<28;x++) {
            byte alpha=(byte)((mask[x,y].A*128+127)/255);
            atlas[x+1,y+1]=new Rgba32(255,255,255,alpha);
            atlas[x+33,y+1]=new Rgba32(color.Item1,color.Item2,color.Item3,alpha);
        }
        string png=Path.Combine(folder,"sam-roster-atlas.png");atlas.Save(png);
        int clear=0,solid=0;
        for(int y=1;y<75;y++)for(int x=1;x<29;x++){if(atlas[x,y].A==0)clear++;if(atlas[x,y].A==128)solid++;}
        if(clear<200||solid<200)throw new Exception("Silhouette coverage lost during import");
        var pack=new OldShapeHandler{Format="GIMX",EndingString=""};
        pack.AddImage(OldShapeHandler.MatrixType.FullColor,"samr",png);
        var item=pack.ShapeImages[0];item.Longname="";pack.ShapeImages[0]=item;
        pack.SaveShape(Path.Combine(folder,"sam-roster-atlas.ssh"));
        File.WriteAllText(Path.Combine(folder,"sam-roster-texture.json"),JsonSerializer.Serialize(new {
            texture_index=22,width=64,height=128,icon_width=28,icon_height=74,
            white=new[]{1,1,28,74},orange=new[]{33,1,28,74},
            highlight_rgb=new[]{(int)color.Item1,(int)color.Item2,(int)color.Item3},mask_crop=new[]{left,top,right,bottom},
            transparent_icon_pixels=clear,opaque_icon_pixels=solid
        },new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine($"Roster mask imported; highlight RGB {color}, atlas64x128 with GS alpha");
    }
}
