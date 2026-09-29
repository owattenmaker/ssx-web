# Browser visual fidelity: current evidence

The current implementation, rather than a demonstrated browser ceiling, contains concrete fidelity gaps:

- web/main.js uses MeshBasicMaterial for terrain, scenery and the skinned rider, with vertex colours, a fixed1.75 light-map multiplier and generic instance alphaTest. This is not a verified reproduction of the original material/lighting pipeline.
- Render resolution is capped at devicePixelRatio1.5; anisotropy is set to4. These are prototype settings, not values recovered from original rendering. Their actual visual/performance impact needs controlled comparisons.
- web/prepare.py encodes textures to PNG at their imported dimensions; no additional resizing is done there. Therefore do not assume that browser packaging alone lowered texture resolution. Source format decoding, mip/filter choice and colour/alpha math still need an asset-by-asset audit.
- tools/import_world.py defaults to GameCube lighting with verified surface/UV correspondence. That does not establish pixel parity with the PS2 reference, and platform lighting differences must be separated from rendering bugs.
- Sky transparency and scene ordering, original particle textures and emission routines have been improved, but original blend/fog/streaming, view clamps, specular/environment effects and continuous original framebuffer comparisons remain incomplete.

Next compare fixed original/gameplay views with matched framing, measure source/imported texture dimensions and pixels, then recover the original material stages. Do not add generic PBR lighting or sharpen/filter settings simply to make a different look. Keep the user’s faithful-SSX3 target.
