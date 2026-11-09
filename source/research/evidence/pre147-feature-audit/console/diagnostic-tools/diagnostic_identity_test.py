#!/usr/bin/env python3
"""Actual diagnostic identity gate with controlled export-table bytes.

This tests no PE parsing, linker output, Windows UI, loader or game execution.
The source-owned gate is imported directly; metadata is evaluated from the
actual builder expression without running a build or fabricating binaries.
"""
import ast
import copy
import hashlib
import importlib
import json
from pathlib import Path
import re
import struct
import sys

ROOT = Path('/workspace/scratch/33b3ee07b26a/ra2-toolbox')
OUT = Path(__file__).parent
sys.path.insert(0, str(ROOT/'source/tools'))
product = importlib.import_module('verify_windows_outputs')
builder = importlib.import_module('build_hook_diagnostics')
diagnostic = importlib.import_module('verify_hook_diagnostics')

checks=[]
def check(name, predicate):
    try:
        passed=bool(predicate())
        reason=''
    except Exception as ex:
        passed=False
        reason=type(ex).__name__+': '+str(ex)
    checks.append(dict(name=name,passed=passed,reason=reason))
    print(('PASS ' if passed else 'FAIL ')+name+(' '+reason if reason else ''))

def target_expression(source):
    candidates=[node for node in ast.walk(ast.parse(source))
                if isinstance(node,ast.Call) and isinstance(node.func,ast.Name)
                and node.func.id=='dict'
                and any(item.arg=='assist_protocol' for item in node.keywords)]
    if len(candidates)!=1:
        raise ValueError('Expected exactly one builder target metadata expression')
    return candidates[0]

class Exports:
    def __init__(self,values):
        self.symbols={name:0x1000+index*0x100 for index,name in enumerate(values)}
        self.values={self.symbols[name]:value for name,value in values.items()}
    def exports(self):return self.symbols
    def at_rva(self,rva,size):return self.values[rva][:size]

expected_assist=struct.pack('<II',product.ASSIST_PROTOCOL,product.ASSIST_SHARED_BYTES)
expected_mapping=(product.ASSIST_MAPPING+'\0').encode('utf-16le')
host={'AssistHostProtocolInfo':expected_assist,'AssistHostMappingPrefix':expected_mapping}
runtime={'AssistProtocolInfo':expected_assist,'AssistMappingPrefix':expected_mapping,
         'GameEntryManagerInfo':struct.pack('<II',product.ENTRY_PROTOCOL,product.ENTRY_REQUEST_BYTES),
         'GameRuntimeInfo':struct.pack('<II',product.RUNTIME_PROTOCOL,product.RUNTIME_CONTROL_BYTES)}

def accepted(h=host,r=runtime):
    return diagnostic.verify_protocol_identity(Exports(h),Exports(r))

def rejects(h,r):
    try:accepted(h,r)
    except (ValueError,KeyError,struct.error):return True
    return False

names=('ASSIST_PROTOCOL','ASSIST_SHARED_BYTES','ASSIST_MAPPING','ENTRY_PROTOCOL',
       'ENTRY_REQUEST_BYTES','RUNTIME_PROTOCOL','RUNTIME_CONTROL_BYTES')
check('both diagnostic tools bind every identity field to the current product verifier',
      lambda:all(getattr(builder,n)==getattr(product,n)==getattr(diagnostic,n) for n in names))
for relative,constant in [('source/assist/source/protocol.h','ASSIST_PROTOCOL'),
                          ('source/shared/source/game_entry_api.h','ENTRY_PROTOCOL'),
                          ('source/shared/source/runtime_protocol.h','RUNTIME_PROTOCOL')]:
    check('current C++ protocol matches '+constant,
          lambda relative=relative,constant=constant:
          int(re.search(r'\bVersion\s*=\s*(\d+)',(ROOT/relative).read_text()).group(1))==getattr(product,constant))

source=(ROOT/'source/tools/build_hook_diagnostics.py').read_text()
expression=target_expression(source)
metadata=eval(compile(ast.Expression(expression),'actual-builder-target','eval'),builder.__dict__)
fields={'assist_protocol':'ASSIST_PROTOCOL','assist_shared_bytes':'ASSIST_SHARED_BYTES',
        'assist_mapping':'ASSIST_MAPPING','entry_protocol':'ENTRY_PROTOCOL',
        'entry_request_bytes':'ENTRY_REQUEST_BYTES','runtime_protocol':'RUNTIME_PROTOCOL',
        'runtime_control_bytes':'RUNTIME_CONTROL_BYTES','cursor_protocol':'CURSOR_PROTOCOL'}
check('builder target metadata uses imported names and current values',lambda:
      all(isinstance(next(k.value for k in expression.keywords if k.arg==key),ast.Name)
          and next(k.value for k in expression.keywords if k.arg==key).id==name
          and metadata[key]==getattr(product,name) for key,name in fields.items()))
check('builder does not claim game or multiplayer execution',lambda:
      metadata['game_tested'] is False and metadata['multiplayer_tested'] is False)
check('exact current host and Runtime identity is accepted and reported',lambda:
      all(accepted()[key]==metadata[key] for key in fields if key!='cursor_protocol'))

def mutate(side,symbol,value):
    h,r=copy.deepcopy(host),copy.deepcopy(runtime)
    target=h if side=='host' else r
    if value is None:target.pop(symbol)
    else:target[symbol]=value
    return h,r

variants=[
 ('old host protocol','host','AssistHostProtocolInfo',struct.pack('<II',product.ASSIST_PROTOCOL-1,product.ASSIST_SHARED_BYTES)),
 ('old Runtime Assist protocol','runtime','AssistProtocolInfo',struct.pack('<II',product.ASSIST_PROTOCOL-1,product.ASSIST_SHARED_BYTES)),
 ('host Shared size mismatch','host','AssistHostProtocolInfo',struct.pack('<II',product.ASSIST_PROTOCOL,product.ASSIST_SHARED_BYTES-4)),
 ('Runtime Shared size mismatch','runtime','AssistProtocolInfo',struct.pack('<II',product.ASSIST_PROTOCOL,product.ASSIST_SHARED_BYTES+4)),
 ('old host mapping','host','AssistHostMappingPrefix','Local\\RA2Assist_v100_\0'.encode('utf-16le')),
 ('old Runtime mapping','runtime','AssistMappingPrefix','Local\\RA2Assist_v100_\0'.encode('utf-16le')),
 ('host mapping missing NUL','host','AssistHostMappingPrefix',expected_mapping[:-2]),
 ('Runtime mapping missing NUL','runtime','AssistMappingPrefix',expected_mapping[:-2]),
 ('host protocol export missing','host','AssistHostProtocolInfo',None),
 ('Runtime protocol export missing','runtime','AssistProtocolInfo',None),
 ('host mapping export missing','host','AssistHostMappingPrefix',None),
 ('Runtime mapping export missing','runtime','AssistMappingPrefix',None),
 ('entry API version mismatch','runtime','GameEntryManagerInfo',struct.pack('<II',product.ENTRY_PROTOCOL+1,product.ENTRY_REQUEST_BYTES)),
 ('entry request size mismatch','runtime','GameEntryManagerInfo',struct.pack('<II',product.ENTRY_PROTOCOL,product.ENTRY_REQUEST_BYTES-4)),
 ('Runtime transport version mismatch','runtime','GameRuntimeInfo',struct.pack('<II',product.RUNTIME_PROTOCOL-1,product.RUNTIME_CONTROL_BYTES)),
 ('Runtime transport size mismatch','runtime','GameRuntimeInfo',struct.pack('<II',product.RUNTIME_PROTOCOL,product.RUNTIME_CONTROL_BYTES-4)),
 ('entry identity export missing','runtime','GameEntryManagerInfo',None),
 ('Runtime transport export missing','runtime','GameRuntimeInfo',None),
 ('short host identity export','host','AssistHostProtocolInfo',expected_assist[:4]),
 ('short Runtime identity export','runtime','AssistProtocolInfo',expected_assist[:4]),
]
for name,side,symbol,value in variants:
    check('reject '+name,lambda side=side,symbol=symbol,value=value:rejects(*mutate(side,symbol,value)))
oldh,oldr=copy.deepcopy(host),copy.deepcopy(runtime)
oldh['AssistHostProtocolInfo']=oldr['AssistProtocolInfo']=struct.pack('<II',100,23162400)
oldh['AssistHostMappingPrefix']=oldr['AssistMappingPrefix']='Local\\RA2Assist_v100_\0'.encode('utf-16le')
check('reject mutually consistent obsolete host and Runtime pair',lambda:rejects(oldh,oldr))
h,r=copy.deepcopy(host),copy.deepcopy(runtime)
h['AssistHostProtocolInfo']=r['AssistProtocolInfo']=struct.pack('<II',product.ASSIST_PROTOCOL,23162400)
check('reject mutually consistent current version with obsolete Shared size',lambda:rejects(h,r))

# Characterize the captured pre-fix code without running a PE loader. Only its
# original export identity block is executed, exactly as it appeared in main.
old_source=(OUT/'before/verify_hook_diagnostics.py').read_text()
old_block=old_source[old_source.index(' def exported(pe,name,n):'):old_source.index(' report=dict(')]
old_block='\n'.join(line[1:] for line in old_block.splitlines())
def old_accepts(h,r):
    try:exec(old_block,{'host':Exports(h),'runtime':Exports(r),'struct':struct})
    except (AssertionError,KeyError,struct.error):return False
    return True
before=dict(valid_current_rejected=not old_accepts(host,runtime),obsolete_pair_accepted=old_accepts(oldh,oldr))
old_expr=target_expression((OUT/'before/build_hook_diagnostics.py').read_text())
old_metadata=eval(compile(ast.Expression(old_expr),'captured-old-target','eval'),{})
before['build_target_assist_protocol']=old_metadata['assist_protocol']

inputs=['source/tools/build_hook_diagnostics.py','source/tools/verify_hook_diagnostics.py',
        'source/tools/verify_windows_outputs.py','source/assist/source/protocol.h',
        'source/shared/source/game_entry_api.h','source/shared/source/runtime_protocol.h']
result=dict(checks=len(checks),failures=sum(not c['passed'] for c in checks),passed=all(c['passed'] for c in checks))
receipt=dict(result=result,checks=checks,before=before,
             command=[sys.executable,str(Path(__file__))],
             source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in inputs},
             fixture_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
             scope=__doc__,windows_tested=False,game_tested=False,pe_binaries_tested=False)
(OUT/'identity-test-receipt.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(result,separators=(',',':')))
raise SystemExit(0 if result['passed'] else 1)
