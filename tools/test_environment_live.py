#!/usr/bin/env python3
"""Exported authored textures reproduce actual glide->glide+1 environment colour."""
import json,struct,subprocess,zipfile
from pathlib import Path
from reference_environment_lighting import extract
root=Path(__file__).resolve().parents[1];out=root/'build/environment-oracle';out.mkdir(parents=True,exist_ok=True)
source=root/'local/reference/pcsx2';before=source/'snow-jam-glide.p2s';after=source/'snow-jam-glide-1.p2s';m=zipfile.ZipFile(after).read('eeMemory.bin');u=lambda at:struct.unpack_from('<I',m,at)[0];f=lambda at:struct.unpack_from('<f',m,at)[0];old=extract(before);new=extract(after);actor=0x14701a0
case=dict(name=after.name,old_ambient=old['ambient'],old_ratio=old['ratio'],expected_ambient=new['ambient'],resource=u(actor+0x430),u=f(actor+0xaac),v=f(actor+0xab0));fixture=out/'live.json';fixture.write_text(json.dumps([case]));binary=out/'asset_test'
subprocess.run(['xcrun','clang++','-std=c++20','-O2','-fobjc-arc','-frounding-math','-ffp-contract=off',str(root/'engine/environment_asset_tests.mm'),str(root/'engine/environment_asset.mm'),str(root/'engine/environment_lighting.cpp'),'-framework','Foundation','-o',str(binary)],check=True)
subprocess.run([str(binary),str(root/'local/assets/native/ARA1'),str(fixture)],check=True)
