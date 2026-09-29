#!/usr/bin/env python3
"""Produce reviewable C++ arithmetic edits using Clang's resolved float types.

Only expressions in the supplied source's ssx namespaces are changed. Integer
indexing, double precision and explicit scalar helpers are left intact.
Rounding-scope migration is deliberately a separate, reviewed change.
"""
import argparse,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
source=a.source.resolve();text=source.read_bytes().decode('latin1') # Clang ranges count bytes, not Unicode characters.
ast=subprocess.run(['clang++','-std=c++20','-Xclang','-ast-dump=json','-Xclang','-ast-dump-filter=ssx','-fsyntax-only',str(source)],check=True,capture_output=True,text=True).stdout
roots=[];decoder=json.JSONDecoder();offset=0
while offset<len(ast):
 while offset<len(ast) and ast[offset].isspace():offset+=1
 if offset==len(ast):break
 node,offset=decoder.raw_decode(ast,offset)
 if node.get('loc',{}).get('file')==str(source):roots.append(node)
assert roots,'No source namespaces found'
edits={}
def extent(node):
 r=node['range'];b,e=r['begin'],r['end']
 assert 'offset' in b and 'offset' in e,'Macro-expanded arithmetic requires manual migration'
 return b['offset'],e['offset']+e['tokLen']
def visit(node):
 kind=node.get('kind');children=node.get('inner',[])
 if node.get('type',{}).get('desugaredQualType',node.get('type',{}).get('qualType'))=='float':
  if kind=='BinaryOperator' and node.get('opcode') in '+-*/':
   assert len(children)==2
   edits[extent(node)]=({'+' :'add','-':'sub','*':'mul','/':'div'}[node['opcode']],[extent(c) for c in children])
  elif kind=='CallExpr' and text[slice(*extent(node))].startswith('std::sqrt('):
   assert len(children)==2;edits[extent(node)]=('sqrt',[extent(children[1])])
  elif kind=='CompoundAssignOperator':raise RuntimeError('Compound float assignment requires manual migration')
 for child in children:visit(child)
for root in roots:visit(root)
def render(start,end):
 cursor=start;parts=[]
 for (b,e),(op,args) in sorted(edits.items(),key=lambda item:(item[0][0],-item[0][1])):
  if b<cursor or e>end:continue
  parts.append(text[cursor:b]);parts.append('terrain_original::'+op+'('+','.join(render(*x) for x in args)+')');cursor=e
 parts.append(text[cursor:end]);return ''.join(parts)
a.output.write_bytes(render(0,len(text)).encode('latin1'))
subprocess.run(['clang++','-std=c++20','-fsyntax-only','-I'+str(source.parent),'-include',str(source.parent/'terrain_contact_math.hpp'),str(a.output.resolve())],check=True)
a.report.write_text(json.dumps([{'offset':b,'original':text[b:e].encode('latin1').decode('utf8'),'operation':op} for (b,e),(op,args) in sorted(edits.items())],indent=2)+'\n')
print(f'{len(edits)} float expressions migrated; output {a.output}')
