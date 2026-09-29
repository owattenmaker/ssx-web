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
