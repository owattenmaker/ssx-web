"""Write a file by writing a temporary sibling and renaming it over the target.

The asset tree's files can be hard-linked (the staged deploy links unchanged assets, deploy/deploy-staged.sh): writing
into an existing file changes every link. A rename gives the target a new inode instead.
SPDX-License-Identifier: GPL-3.0
"""
import os
from pathlib import Path


def write_bytes(path, data):
    path = Path(path)
    tmp = path.with_name(f'.{path.name}.{os.getpid()}.tmp')
    tmp.write_bytes(data)
    os.replace(tmp, path)


def write_text(path, text):
    write_bytes(path, text.encode('utf-8'))
