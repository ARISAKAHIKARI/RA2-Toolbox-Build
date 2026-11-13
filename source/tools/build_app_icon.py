"""Encode the transparent medicine-box PNG into all Windows icon sizes.

The checked-in PNG is the canonical artwork; release builds consume app.ico.
This maintenance command requires Pillow, not an image-generation service.
"""
from pathlib import Path
import argparse,hashlib,json
from PIL import Image

p=argparse.ArgumentParser();p.add_argument('--preview',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1];source=root/'suite/source/app-icon.png';ico=source.with_name('app.ico')
with Image.open(source) as rendered:im=rendered.convert('RGBA')
assert im.width==im.height and im.width>=128,'Square source of at least 128 pixels required'
if im.width<256: im=im.resize((256,256),Image.Resampling.LANCZOS)
assert im.getextrema()[3]==(0,255),'Source must preserve transparent background and opaque artwork'
sizes=[(v,v) for v in [16,20,24,32,40,48,64,128,256]]
im.save(ico,format='ICO',sizes=sizes)
with Image.open(ico) as saved:
 assert saved.ico.sizes()==set(sizes)
 for size in sizes:
  frame=saved.ico.getimage(size).convert('RGBA');assert frame.getextrema()[3]==(0,255)
if a.preview:
 a.preview.parent.mkdir(parents=True,exist_ok=True);im.resize((256,256),Image.Resampling.LANCZOS).save(a.preview)
print(json.dumps({'icon':str(ico),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'sizes':sizes,'bytes':ico.stat().st_size,'transparent':True}))
