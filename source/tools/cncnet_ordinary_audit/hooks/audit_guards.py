"""Read-only additional guard-context intersection audit, not multiplayer proof."""
from pathlib import Path
import argparse,collections,hashlib,json,os,re,shlex,subprocess
from ordinary_pe import PE,intersects,coverage_method,validate_upstream

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo',type=Path,required=True,help='Toolbox source repository root')
parser.add_argument('--ordinary-dll',type=Path,required=True,help='Official ordinary CnCNetYR v17 DLL')
parser.add_argument('--upstream-source',type=Path,required=True,help='Pinned public Spawner checkout root (contains src/)')
parser.add_argument('--output-dir',type=Path,required=True)
parser.add_argument('--dynamic-writes',type=Path,help='Previously parsed dynamic writes JSON; default OUTPUT_DIR/dynamic-write-intersections.json')
parser.add_argument('--cxx',default=os.environ.get('CXX','c++'),help='GNU-compatible C++17 compiler command')
args=parser.parse_args()
ROOT=args.repo.resolve()
OUT=args.output_dir.resolve();OUT.mkdir(parents=True,exist_ok=True)
HERE=Path(__file__).resolve().parent
source_pin=validate_upstream(args.upstream_source)
normal=PE(args.ordinary_dll.resolve()).report()
assert normal['sha256']=='58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d','Wrong ordinary DLL sample'
binary=OUT/('dump_guards.exe' if os.name=='nt' else 'dump_guards')
subprocess.run([*shlex.split(args.cxx),'-std=c++17','-O2','-I',str(ROOT),str(HERE/'dump_guards.cpp'),'-o',str(binary)],check=True)
dump=subprocess.check_output([str(binary)],text=True)
(OUT/'guard-declarations.tsv').write_text(dump)
rows=[]
for line in dump.splitlines():
 family,site,address,size=line.split('\t')
 rows.append({'family':family,'site':int(site),'address':int(address),'size':int(size),'enabled':'startup-required' if family=='startup' else 'feature-specific'})
manual=[
 ('scroll hash',0x4A9848,0x46,'legacy-disabled-SceneRefresh'),
 ('clamp hash',0x6D8645,0xB9,'feature-specific'),
 ('zoom hash',0x560BF0,0x58A,'legacy-disabled-ViewZoom'),
 ('Deselect hash',0x5F44A0,123,'feature-specific'),('FindIndex hash',0x40CF00,38,'feature-specific'),
 ('Current object getter hash',0x4AEB10,23,'feature-specific'),('Current object setter hash',0x4AEB30,24,'feature-specific'),
 ('nativeMega prologue',0x646E95,19,'feature-specific'),('nativeMega ctorcall',0x646EF0,5,'feature-specific'),
 ('nativeMega queuecopy',0x646F16,7,'feature-specific'),('nativeMega timer',0x646F20,6,'feature-specific'),('nativeMega return',0x646F55,3,'feature-specific'),
 ('nativeMega entry original',0x646E90,5,'feature-specific'),('nativeObject signature',0x6FFE00,5,'feature-specific'),
 ('orderlines',0x70D150,37,'feature-specific'),('GUIBuildSound',0x750920,10,'feature-specific'),('canSell',0x4494C0,8,'feature-specific'),
 ('Sticky ForceMission',0x5B2FD0,10,'feature-specific'),('Sticky NextMission',0x5B3570,10,'feature-specific'),
 ('render-head',0x4F4480,9,'legacy-disabled-SceneRefresh'),('setPosition',0x6D6000,9,'feature-specific'),
 ('edge scroll',0x692B60,9,'legacy-disabled-SceneRefresh'),('drag scroll',0x693440,9,'legacy-disabled-SceneRefresh'),
 ('FlagRedraw',0x4F42F0,46,'legacy-disabled-SceneRefresh'),('AllCheer ctor',0x4C66C0,8,'feature-specific'),
 ('Taunt Execute',0x536350,11,'feature-specific'),('Tactical coordinate',0x6D2790,9,'feature-specific'),
 ('advance mission',0x6385C0,10,'feature-specific'),('cursor Mix Offset prologue',0x5B4430,9,'feature-specific'),
 ('ControlHouse tail native/forwarded',0x50B6F5,4,'feature-specific'),
]
for label,address,size,enabled in manual:rows.append({'family':'manual-guard','label':label,'address':address,'size':size,'enabled':enabled})
text=(ROOT/'source/assist/source/network_sender_bridge.h').read_text()
probes=text[text.index('static bool networkSenderContext'):text.index('static bool restoreNetworkSender')]
for a,n in re.findall(r'\{(0x[0-9A-Fa-f]+),(\d+),\{',probes):
 rows.append({'family':'network-context','address':int(a,16),'size':int(n),'enabled':'feature-specific'})
# Actual data slots guarded by exact vtable identity. These are not whole-function fingerprints.
for label,base in [('aircraft',0x7E22A4),('infantry',0x7EB058),('unit',0x7F5C70)]:
 for offset in (0x354,0x358,0x368,0x370):rows.append({'family':'vtable-slot','label':label+' local voice','address':base+offset,'size':4,'enabled':'feature-specific'})
for label,base in [('aircraft',0x7E22A4),('unit',0x7F5C70)]:
 rows.append({'family':'vtable-slot','label':label+' Deselect','address':base+0x150,'size':4,'enabled':'feature-specific'})
rows.extend([
 {'family':'vtable-slot','label':'vector FindIndex','address':0x7E4F64+0x10,'size':4,'enabled':'feature-specific'},
 {'family':'vtable-slot','label':'building CanSell','address':0x7E3EBC+0x98,'size':4,'enabled':'feature-specific'},
])
conflicts=[]
for q in rows:
 q['address_hex']=f"0x{q['address']:08X}"
 assert q['size']>0
 for family in ('hooks','patches'):
  for p in normal[family]:
   if intersects(q,p):
    conflicts.append({'guard':q,'ordinary_family':family,'ordinary':p})
source_names=['assist/source/signatures.h','shared/source/patch_context.h','shared/source/direct_patch_policy.h','assist/source/network_sender_bridge.h','assist/source/aircraft_selection_bridge.h','assist/source/input_feedback.h','assist/source/bridge.cpp','assist/source/scroll_refresh_bridge.h','assist/source/view_zoom_bridge.h','assist/source/sticky_bridge.h','assist/source/placement_bridge.h','assist/source/social_bridge.h','assist/source/native_sell_cursor_bridge.h','cursor/source/hide.cpp','assist/source/native_hotkeys.h']
dynamic_path=(args.dynamic_writes or OUT/'dynamic-write-intersections.json').resolve()
dynamic=json.loads(dynamic_path.read_text())
assert dynamic['upstream_commit']=='fbe8d7d2620584d04ba14fa3580943bfa1448d40'
assert len(dynamic['calls'])==75 and dynamic['unresolved']==[]
upstream=args.upstream_source.resolve()
upstream_hashes={}
for path in sorted({record['path'] for record in dynamic['calls']}):
 source=(upstream/'src'/path).resolve()
 assert upstream in source.parents and source.is_file(),f'Missing/unsafe upstream source: {path}'
 upstream_hashes['src/'+path]=hashlib.sha256(source.read_bytes()).hexdigest()
dynamic_intersections=[]
for q in rows:
 for p in dynamic['calls']:
  if intersects(q,p):
   dynamic_intersections.append({'guard':q,'ordinary_dynamic':p})
result={
 'scope':'Additional static signature/context/body-hash and known vtable-slot guard ranges. Actual patch-site before-byte checks are covered separately by physical-site inventory.',
 'ordinary_sha256':normal['sha256'],'ordinary_hook_count':len(normal['hooks']),'ordinary_patch_count':len(normal['patches']),
 'descriptor_coverage_method':coverage_method(normal),'verified_upstream_source_pin':source_pin,
 'declared_guard_count':len(rows),'unique_address_size_ranges':len({(r['address'],r['size']) for r in rows}),
 'family_counts':dict(collections.Counter(r['family'] for r in rows)),
 'source_sha256':{name:hashlib.sha256((ROOT/'source'/name).read_bytes()).hexdigest() for name in source_names},
 'guards':rows,'descriptor_intersections':conflicts,
 'dynamic_public_write_count':len(dynamic['calls']),'dynamic_public_write_source':dynamic_path.name,
 'upstream_source_sha256':upstream_hashes,
 'dynamic_source_sha256':hashlib.sha256(dynamic_path.read_bytes()).hexdigest(),
 'dynamic_public_write_intersections':dynamic_intersections,
 'known_separate_install_conflict':{'site':'0x006D8640','name':'CameraClamp','note':'The body hash begins at site+5 and deliberately excludes the five-byte install site; zero additional-context intersections must not be presented as zero patch-install collisions.'},
 'limitations':[
  'Intersects on-disk ordinary DLL registered hook/patch ranges with explicit coverage_size overrides for the two zero-declared-size callbacks; their 25-byte source-skipped envelopes do not establish actual loader overwrite lengths or live activation.',
  'Dynamically selected CommandClass GetName six-byte leaf shape and cursor file-object vtable pointer validation cannot all be resolved statically without the live objects; failure skips that optional read/resource operation, not all Assist startup.',
  'The native MegaMission timer slot only requires executable committed callback memory; it does not require the original timer function pointer.',
  'No user base EXE memory, full initialization, live Windows execution, real-game rendering or multiplayer session was tested here.',
  'In particular the v17 CameraClamp install collision is independent of this zero-intersection additional-context result.',
 ]
}
(OUT/'guard-intersections.json').write_text(json.dumps(result,indent=2))
print(json.dumps({k:result[k] for k in ('ordinary_hook_count','ordinary_patch_count','declared_guard_count','unique_address_size_ranges','family_counts')}))
print('additional guard intersections:',len(conflicts))
print('dynamic public write intersections:',len(dynamic_intersections))
for c in conflicts:print(json.dumps(c))
for c in dynamic_intersections:print(json.dumps(c))
