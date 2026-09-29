#!/usr/bin/env python3
"""Where the tools find the user's disc images (tools/setup_from_iso.py sets these for every step).

    SSX3_ISO              the PS2 disc image (NTSC-U SLUS_207.72); default ~/Downloads/SSX 3 (USA).iso
    SSX3_GAMECUBE_IMAGE   the GameCube disc image (GXBE69, .rvz/.iso), only for tools/setup_from_iso.py --gamecube

The GameCube files themselves are read from local/gamecube/disc/files (tools/extract_gamecube.py), the Xbox files from
local/xbox (tools/xdvdfs.py, docs/xbox-textures.md).
"""
import os
from pathlib import Path


def ps2_iso():
    """The PS2 disc image: $SSX3_ISO, else the historical default."""
    return Path(os.environ.get('SSX3_ISO') or Path.home() / 'Downloads/SSX 3 (USA).iso')


def repo_relative(path):
    """A path as written into generated data: relative to the checkout when inside it (no machine-specific prefix)."""
    p = Path(path)
    if not p.is_absolute():
        return str(path)
    root = Path(__file__).resolve().parents[1]
    for base in (root, root.resolve()):
        try:
            return p.relative_to(base).as_posix()
        except ValueError:
            pass
    try:
        return p.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return str(path)


def test_data(path):
    """Where a test-only export goes: web/public/test-data, laid out like web/public/assets (not deployed with the game).
    A path outside web/public/assets is returned unchanged."""
    p = Path(path)
    assets = Path(__file__).resolve().parents[1] / 'web/public/assets'
    try:
        rel = p.resolve().relative_to(assets.resolve()) if p.is_absolute() else p.relative_to('web/public/assets')
    except ValueError:
        return p
    out = assets.parent / 'test-data' / rel
    out.parent.mkdir(parents=True, exist_ok=True)
    return out
