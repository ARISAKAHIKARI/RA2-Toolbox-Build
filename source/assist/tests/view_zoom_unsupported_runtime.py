"""Existing zoom.1 compatibility gate rejects the unsupported LBW fingerprint.
Actual compiled Runtime with mocked renderer; no resize or game simulation.
"""
from pathlib import Path
PROBE_SCOPE=__doc__
exec(compile(Path(__file__).with_name('view_zoom_runtime.py').read_text().split('\nfor mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('view_zoom_runtime.py')),'exec'))
assert hashlib.sha256(Path(a.game).read_bytes()).hexdigest()=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
rows=[]
for mode in (0,3):
 m=setup(mode);s=setzoom(m,50)
 assert s.viewZoomStatus==-2 and s.viewZoomApplied==100 and not m.resizes and not m.codeWrites
 assert m.r(0x8a00a4)==1280 and m.r(0x8a00a8)==960 and not s.viewZoomBusy
 rows.append({'case':f'lbw_unsupported_zoom_{mode}_no_resize','status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
print('PASS: unsupported LBW zoom stays at 100%, no resize or code writes')
