#!/usr/bin/env python3
"""Verify the standalone test host's PE/ABI/imports and asInvoker manifest.

The normal product host requires elevation and PerMonitorV2; this diagnostic
host deliberately inherits the caller token and uses Windows DPI scaling.
Both policies are checked explicitly without weakening the production check.
"""
from pathlib import Path
import argparse, hashlib, json, struct, sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from pe32_image import Image
from verify_windows_outputs import (SYSTEM, HOOKS, CURSOR_MAPPING, verify_cursor_mapping,
 verify_host_manifest, verify_worker_manifest, RELEASE_FILE_VERSION,
 ASSIST_PROTOCOL, ASSIST_SHARED_BYTES, ASSIST_MAPPING, ENTRY_PROTOCOL,
 ENTRY_REQUEST_BYTES, RUNTIME_PROTOCOL, RUNTIME_CONTROL_BYTES)

def verify_protocol_identity(host,runtime):
 # Equal host/Runtime exports are not sufficient: both must match the exact
 # current product ABI. Keep the expected values shared with its PE verifier.
 def exported(pe,symbol,size):
  exports=pe.exports()
  if symbol not in exports:raise ValueError('Missing identity export: '+symbol)
  return pe.at_rva(exports[symbol],size)
 expected=struct.pack('<II',ASSIST_PROTOCOL,ASSIST_SHARED_BYTES)
 prefix=(ASSIST_MAPPING+'\0').encode('utf-16le')
 for pe,info,mapping in ((host,'AssistHostProtocolInfo','AssistHostMappingPrefix'),
                         (runtime,'AssistProtocolInfo','AssistMappingPrefix')):
  if exported(pe,info,8)!=expected:raise ValueError(info+': current Assist protocol/Shared size mismatch')
  if exported(pe,mapping,len(prefix))!=prefix:raise ValueError(mapping+': current mapping namespace mismatch')
 if exported(runtime,'GameEntryManagerInfo',8)!=struct.pack('<II',ENTRY_PROTOCOL,ENTRY_REQUEST_BYTES):
  raise ValueError('entry manager API mismatch')
 if exported(runtime,'GameRuntimeInfo',8)!=struct.pack('<II',RUNTIME_PROTOCOL,RUNTIME_CONTROL_BYTES):
  raise ValueError('Runtime diagnostic transport ABI mismatch')
 return dict(assist_protocol=ASSIST_PROTOCOL,assist_shared_bytes=ASSIST_SHARED_BYTES,
             assist_mapping=ASSIST_MAPPING,entry_protocol=ENTRY_PROTOCOL,
             entry_request_bytes=ENTRY_REQUEST_BYTES,runtime_protocol=RUNTIME_PROTOCOL,
             runtime_control_bytes=RUNTIME_CONTROL_BYTES)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);a=p.parse_args();root=a.root.resolve()
 records={};images={}
 for name in ('RA2HookTest.exe','GameRuntime.dll','GameRuntimeHost32.exe'):
  raw=(root/name).read_bytes();pe=Image(raw,allow_x64=True);images[name]=pe
  assert pe.machine==(0x8664 if name=='RA2HookTest.exe' else 0x14c),(name,'architecture')
  assert bool(pe.characteristics&0x2000)==name.endswith('.dll'),(name,'image type')
  imports=pe.imports();assert all(n.lower() in SYSTEM or n.lower().startswith(('api-ms-win-','ext-ms-win-')) for n in imports),(name,'external dependency')
  assert (RELEASE_FILE_VERSION+'\0').encode('utf-16le') in pe.resource_bytes(),(name,'file version')
  exports=pe.exports()
  if name=='GameRuntime.dll':
   assert {n.split('@')[0] for n in exports}==set(HOOKS[name])
   for symbol in ('GameRuntimeHook','GameRuntimePostHook'):
    rva=exports[symbol];assert exports.get(symbol+'@12',rva)==rva
    start=pe.optional+pe.u16(pe.nt+20)
    sections=[i for i,(_,base,size,_,virtual) in enumerate(pe.sections) if base<=rva<base+min(size,virtual)]
    assert len(sections)==1 and pe.u32(start+40*sections[0]+36)&0x20000000
  elif name=='GameRuntimeHost32.exe':assert not exports
  records[name]=dict(bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest(),machine=pe.architecture,imports=imports,exports=list(exports))
 host=images['RA2HookTest.exe'];runtime=images['GameRuntime.dll']
 # The Assist-only diagnostic host has no Cursor client/mapping. Its paired
 # Runtime still contains the Cursor service and must be the current ABI.
 cursor_protocol=verify_cursor_mapping(runtime,'GameRuntime.dll')
 records['GameRuntime.dll']['cursor_protocol']=cursor_protocol
 records['GameRuntime.dll']['cursor_mapping']=CURSOR_MAPPING
 records['RA2HookTest.exe']['common_controls_activation']=verify_host_manifest(host,'x64')
 records['RA2HookTest.exe']['process_manifest']=verify_worker_manifest(host,'amd64','diagnostic host')
 records['GameRuntimeHost32.exe']['process_manifest']=verify_worker_manifest(images['GameRuntimeHost32.exe'],'x86','broker')
 identity=verify_protocol_identity(host,runtime)
 report=dict(verified=records,host_architecture='x64',runtime_architecture='x86',**identity,cursor_protocol=cursor_protocol,game_tested=False,multiplayer_tested=False)
 (root/'HOOK_TEST_VERIFIED.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
 print(f'PASS standalone x64 test host + x86 Runtime/broker; PE/import/manifest/Assist{ASSIST_PROTOCOL}/Runtime{RUNTIME_PROTOCOL}/Cursor{cursor_protocol} verified')
if __name__=='__main__':main()
