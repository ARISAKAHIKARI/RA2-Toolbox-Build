"""Rasterize the editable SVG into the Windows icon (librsvg + Cairo + Pillow).

This is an asset maintenance command, not a dependency of release builds.
"""
from pathlib import Path
import argparse,ctypes as C,ctypes.util,json,tempfile
from PIL import Image

p=argparse.ArgumentParser();p.add_argument('--preview',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1];svg=root/'suite/source/app-icon.svg';ico=svg.with_name('app.ico')
def function(lib,name,args,result):
 f=getattr(lib,name);f.argtypes=args;f.restype=result;return f
rsvg=C.CDLL(ctypes.util.find_library('rsvg-2'));cairo=C.CDLL(ctypes.util.find_library('cairo'));gobject=C.CDLL(ctypes.util.find_library('gobject-2.0'))
ptr=C.c_void_p
create=function(rsvg,'rsvg_handle_new_from_data',[C.c_char_p,C.c_size_t,C.POINTER(ptr)],ptr)
surface=function(cairo,'cairo_image_surface_create',[C.c_int,C.c_int,C.c_int],ptr)(0,1024,1024)
context=function(cairo,'cairo_create',[ptr],ptr)(surface)
handle=None
try:
 data=svg.read_bytes();error=ptr();handle=create(data,len(data),C.byref(error));assert handle,'Invalid SVG'
 function(cairo,'cairo_scale',[ptr,C.c_double,C.c_double],None)(context,4,4)
 viewport=(C.c_double*4)(0,0,256,256)
 render=function(rsvg,'rsvg_handle_render_document',[ptr,ptr,C.POINTER(C.c_double),C.POINTER(ptr)],C.c_int)
 assert render(handle,context,viewport,C.byref(error)),'SVG rendering failed'
 with tempfile.TemporaryDirectory() as temporary:
  png=Path(temporary)/'icon.png';assert function(cairo,'cairo_surface_write_to_png',[ptr,C.c_char_p],C.c_int)(surface,str(png).encode())==0
  with Image.open(png) as rendered:im=rendered.convert('RGBA')
finally:
 if handle:function(gobject,'g_object_unref',[ptr],None)(handle)
 function(cairo,'cairo_destroy',[ptr],None)(context);function(cairo,'cairo_surface_destroy',[ptr],None)(surface)
sizes=[(v,v) for v in [16,20,24,32,40,48,64,128,256]]
im.save(ico,format='ICO',sizes=sizes)
with Image.open(ico) as saved:
 assert saved.ico.sizes()==set(sizes)
 for size in sizes:
  frame=saved.ico.getimage(size).convert('RGBA');assert frame.getextrema()[3]==(0,255)
if a.preview:
 a.preview.parent.mkdir(parents=True,exist_ok=True);im.resize((256,256),Image.Resampling.LANCZOS).save(a.preview)
print(json.dumps({'icon':str(ico),'sizes':sizes,'bytes':ico.stat().st_size,'transparent':True}))
