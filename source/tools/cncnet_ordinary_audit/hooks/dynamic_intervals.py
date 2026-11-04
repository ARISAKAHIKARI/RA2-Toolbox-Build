#!/usr/bin/env python3
"""Extract explicit Patch::Apply targets from the pinned public C++ sources.

This inventories possible writes without claiming each runtime branch executes.
No upstream code or binary is executed. Actual compiled descriptor coverage is
handled separately by the PE table scanner.
"""
import argparse,ast,json,re
from pathlib import Path
from ordinary_pe import validate_upstream,PE,RELEASE_RUNTIME_SHA256,intersects

TOKEN=re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/',re.S)
def clean(text):
 return TOKEN.sub(lambda m: ''.join('\n' if c=='\n' else ' ' for c in m[0]) if m[0].startswith(('/',)) else m[0],text)
def split_args(text):
 result=[];start=0;depth=0;quote=None;escape=False
 for i,c in enumerate(text):
  if quote:
   if escape:escape=False
   elif c=='\\':escape=True
   elif c==quote:quote=None
  elif c in ('"',"'"):quote=c
  elif c in '([{':depth+=1
  elif c in ')]}':depth-=1
  elif c==',' and depth==0:result.append(text[start:i].strip());start=i+1
 result.append(text[start:].strip());return result
def end_of_call(text,start):
 depth=1;quote=None;escape=False
 for i in range(start,len(text)):
  c=text[i]
  if quote:
   if escape:escape=False
   elif c=='\\':escape=True
   elif c==quote:quote=None
  elif c in ('"',"'"):quote=c
  elif c=='(':depth+=1
  elif c==')':
   depth-=1
   if not depth:return i
 raise ValueError('Unclosed call')
def const(expr):
 node=ast.parse(re.sub(r'(?<=\w)[uUlL]+\b','',expr),mode='eval').body
 def calc(n):
  if isinstance(n,ast.Constant) and type(n.value)==int:return n.value
  if isinstance(n,ast.BinOp) and isinstance(n.op,(ast.Add,ast.Sub,ast.Mult)):
   a,b=calc(n.left),calc(n.right)
   return a+b if isinstance(n.op,ast.Add) else a-b if isinstance(n.op,ast.Sub) else a*b
  raise ValueError('Nonconstant address '+expr)
 return calc(node)
def parse(root):
 entries=[];unresolved=[]
 rx=re.compile(r'Patch::Apply_(RAW|LJMP|CALL6|CALL|OFFSET|VTABLE|TYPED\s*<\s*(\w+)\s*>)\s*\(')
 for p in sorted(root.rglob('*')):
  if p.suffix not in {'.cpp','.h'} or p.parent.name=='Utilities':continue
  source=p.read_text();text=clean(source)
  for m in rx.finditer(text):
   api=m[1];args=split_args(text[m.end():end_of_call(text,m.end())])
   info={'path':str(p.relative_to(root)),'line':text.count('\n',0,m.start())+1,'api':api,'address_expression':args[0]}
   try:
    address=const(args[0])
    if api in {'LJMP','CALL'}:size=5
    elif api=='CALL6':size=6
    elif api in {'OFFSET','VTABLE'}:size=4
    elif args[1].startswith('{'):
     values=split_args(args[1][1:-1]);count=len([v for v in values if v]);size=count*({'WORD':2,'DWORD':4,'int':4,'byte':1,'char':1}.get(m[2],1))
    elif api=='RAW':
     value=ast.literal_eval(args[1]);assert isinstance(value,str);size=len(value.encode('ascii'))+1
    else:raise ValueError('Unknown payload sizing')
    info.update(address=address,address_hex=hex(address),size=size);entries.append(info)
   except Exception as e:info['reason']=str(e);unresolved.append(info)
 return entries,unresolved
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--repo',type=Path);ap.add_argument('--ordinary-dll',type=Path);ap.add_argument('--upstream-source',required=True,type=Path);ap.add_argument('--output-dir',required=True,type=Path);a=ap.parse_args();a.output_dir.mkdir(parents=True,exist_ok=True)
 source_pin=validate_upstream(a.upstream_source)
 if a.ordinary_dll is not None:PE(a.ordinary_dll)
 entries,unresolved=parse(a.upstream_source/'src')
 allowed=json.loads((a.output_dir/'all-write-intersections.json').read_text())['toolbox_allowlist'];overlaps=[]
 for e in entries:
  for s in allowed:
   if intersects(e,s):overlaps.append({'ordinary':e,'toolbox':s})
 out={'upstream_commit':'fbe8d7d2620584d04ba14fa3580943bfa1448d40','ordinary_sha256':'58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d','toolbox_code_commit':'b372ace50101e0099a5e9533c50470b4cabb1536','toolbox_release_commit':'5508c80f7b81abad4fc51847eb7f3530acddc9af','scope':'All literal-address Patch::Apply calls outside Utilities; includes conditional RA2-mode and configuration branches, not proof of execution','calls':entries,'unresolved':unresolved,'toolbox_write_intersections':overlaps}
 out['verified_upstream_source_pin']=source_pin;out['toolbox_release_runtime_sha256']=RELEASE_RUNTIME_SHA256
 (a.output_dir/'dynamic-write-intersections.json').write_text(json.dumps(out,indent=2));print(json.dumps({'calls':len(entries),'unresolved':unresolved,'intersections':overlaps,'verified_source_blobs':source_pin['verified_blobs']},indent=2))
if __name__=='__main__':main()
