// SPDX-License-Identifier: GPL-3.0-only
// Uses GlitcherOG/SSX-Library (GPL-3.0; docs/asset-formats.md, licenses/GPL-3.0.txt).
using System.Text.Json;

// Where the PS2 tools read Sam's authored model (tools/sam_mesh.py). The native package holds the
// geometry, skin, part families (rider.json parts[].ps2_family) and distance LODs (lods.json); the
// derived authoring exports (material-N.png, uphill-suit.png) live in the git-ignored local/sam-model.
static class SamSource
{
    public static string Native(string root) => Path.Combine(root, "local/assets/native/RIDER_SAM");
    public static string Exports(string root) => Path.Combine(root, "local/sam-model/RIDER_SAM");
    // Number of runtime maps (s000..): the native world.json texture table.
    public static int TextureCount(string root)
    {
        using var world = JsonDocument.Parse(File.ReadAllText(Path.Combine(Native(root), "world.json")));
        int count = world.RootElement.GetProperty("textures").EnumerateObject().Count();
        for (int i = 0; i < count; i++)
            if (!File.Exists(Path.Combine(Exports(root), $"material-{i}.png"))) throw new Exception($"Missing Sam export material-{i}.png (run tools/sam_mesh.py --all)");
        return count;
    }
    public static string Material(string root, int i) => Path.Combine(Exports(root), $"material-{i}.png");
}
