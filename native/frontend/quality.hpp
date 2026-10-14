#pragma once
// The page's quality settings (web/quality.js resolveQuality) as far as the native front end applies them: the tier defaults
// (TIER_DEFAULTS), then the overrides the page takes from its URL (?riders=, ?fastfx=, ?bc=). A desktop resolves to tier
// 'high' (detectTier); the native app is a desktop, so 'high' is its default, as on the page.
#include <stdexcept>
#include <string>

namespace ssx_native {

struct Quality {
    std::string tier = "high";
    // the core's plain-float skin palettes and snow sprites (web/presentation_fast.hpp): tier low only
    bool presentationFast = false;
    // 'xbox': the Xbox HD rider textures (textures-xbox.tex / gear-xbox.tex twins); 'ps2': the PS2 archives
    std::string riderTextures = "xbox";
    // BC blocks uploaded where the device has texture-compression-bc (false: the page's ?bc=0, the RGBA decode)
    bool bc = true;
    // the canvas's MSAA (tier medium and high); the native renderer has none yet (docs/native.md section 7)
    bool antialias = true;

    // TIER_DEFAULTS[tier] and defaultRiderTextures(desktop, tier)
    static Quality tierDefaults(const std::string& tier) {
        if (tier != "low" && tier != "medium" && tier != "high") {
            throw std::runtime_error("unknown quality tier " + tier + " (low, medium, high)");
        }
        Quality q;
        q.tier = tier;
        q.presentationFast = tier == "low";
        q.antialias = tier != "low";
        q.riderTextures = tier == "low" ? "ps2" : "xbox";
        return q;
    }
    // ?riders=ps2|xbox
    void setRiders(const std::string& set) {
        if (set != "ps2" && set != "xbox") {
            throw std::runtime_error("unknown rider texture set " + set + " (ps2, xbox)");
        }
        riderTextures = set;
    }
};

}  // namespace ssx_native
