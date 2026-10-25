"""Replay original terrain origin/height slices and compare the C++ helper.
Only coordinate arithmetic runs; TMP drawing, Win32 and game updates do not.
The private capture is never copied into the sanitized result.
"""
from pathlib import Path
import argparse,hashlib,json,math,struct,subprocess,tempfile
from native_bib_capture import Capture,SUPPORTED_SHA
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_ESP

def verify(path,stage,portable):
 c=Capture(path,stage);u=c.emulator();base=0x60000000;u.mem_map(base,0x10000);sp=base+0x8000
 def put(p,v):u.mem_write(p,struct.pack('<I',v&0xffffffff))
 def signed(p):return struct.unpack('<i',u.mem_read(p,4))[0]
 cases=[(x,y,level,cx,cy) for x,y in ((66,66),(68,73),(131,131),(211,211),(150,144)) for level in (-128,-1,0,1,2,6,14,127) for cx,cy in ((0,0),(-120,80),(30,-15))]
 payload=''.join(f'131 146 3 5 124 135 {x} {y} {level}\n' for x,y,level,_,_ in cases)
 rows=[tuple(map(int,l.split())) for l in subprocess.check_output([str(portable),'--origins'],input=payload,text=True).splitlines()]
 assert len(rows)==len(cases)
 for (x,y,level,cx,cy),(ok,helper_x,helper_y) in zip(cases,rows):
  assert ok
  coords,point,tactical,cell,typeptr=base+0x100,base+0x200,base+0x300,base+0x1000,base+0x2000
  u.mem_write(coords,struct.pack('<iii',x*256,y*256,0));put(sp+4,point);put(sp+8,coords);u.reg_write(UC_X86_REG_ESP,sp)
  # Tactical::CoordsToScreen integer XY portion, before the Z/FPU path.
  u.emu_start(0x6D1F10,0x6D1F9B,count=100)
  put(point,u.reg_read(UC_X86_REG_EDI));put(point+4,u.reg_read(UC_X86_REG_ESI))
  put(tactical+0xB0,cx);put(tactical+0xB4,cy);u.reg_write(UC_X86_REG_EAX,point);u.reg_write(UC_X86_REG_EBP,tactical);u.reg_write(UC_X86_REG_ESP,sp)
  # The actual terrain caller subtracts the camera and 30 pixels in X.
  u.emu_start(0x6D51F6,0x6D5226,count=100)
  put(point,signed(sp+0x2C));put(point+4,signed(sp+0x30));put(sp+0x20,point);u.mem_write(cell+0x11B,struct.pack('<b',level));put(typeptr,0)
  u.reg_write(UC_X86_REG_ESI,cell);u.reg_write(UC_X86_REG_EDI,typeptr);u.reg_write(UC_X86_REG_ESP,sp)
  # Native DrawIsoTile MOVSX level and level*15 subtraction. Stop before
  # the GetImage virtual call; no art, renderer or simulation is executed.
  u.emu_start(0x480409,0x480430,count=100)
  native_x=signed(sp+0x14);native_y=struct.unpack('<i',struct.pack('<I',u.reg_read(UC_X86_REG_EBX)))[0]
  assert (native_x,native_y)==(helper_x-cx,helper_y-cy), ((x,y,level,cx,cy),(native_x,native_y),(helper_x-cx,helper_y-cy))
 offsets=struct.unpack('<30I',c.read(0x7EC450,120));widths=[offsets[n+1]-offsets[n] for n in range(29)]
 assert offsets[0]==0 and offsets[-1]==900 and widths==[4*min(y+1,29-y) for y in range(29)]
 ranges=((0x6D1F10,0x6D1F9B),(0x6D51F6,0x6D5226),(0x480409,0x480430))
 return {'stage':stage,'native_origin_cases':len(cases),'zero_mismatches':True,'signed_height_values':[-128,-1,0,1,2,6,14,127],
  'native_tmp_plane':{'nonempty_rows':len(widths),'pixels':sum(widths),'row_widths_match_production_formula':True},
  'code_ranges':[{'start':hex(a),'end':hex(b),'sha256':hashlib.sha256(c.read(a,b-a)).hexdigest()} for a,b in ranges]}

def south_inventory(path):
 c=Capture(path,'01-off');m=0x87F7E8;width,height=struct.unpack('<ii',c.read(m+0xF4,8));factor=struct.unpack('<f',c.read(m+0x1488,4))[0]
 ox,oy,rw,rh=[c.signed(m+off) for off in (0x1490,0x1498,0x14A4,0x14A8)];span_x=rw*30./factor;span_y=rh*15./factor;edge=[None]*math.ceil(span_x)
 raw_w,raw_h=c.signed(m+0x1240),c.signed(m+0x1244);items=c.word(m+0x13C);accepted=0;nominal_crossing=0;fallback_crossing=0
 for y in range(512):
  for x in range(512):
   if not(x>0 and y>0 and abs(x-y)<width and x+y>width and x+y<=width+2*height):continue
   try:
    cell=c.word(items+(y*512+x)*4)
    if struct.unpack('<hh',c.read(cell+0x24,4))!=(x,y):continue
    level=struct.unpack('<b',c.read(cell+0x11B,1))[0]
   except AssertionError:continue
   accepted+=1;center_x=(x-y+ox)*30;center_y=(x+y+1-level-oy)*15
   flat_x,flat_y=x-y+ox,x+y-oy;tile_top=(flat_y-level)*15;tile_left=flat_x*30-30
   if level==2 and tile_top<raw_h*15<tile_top+30 and tile_left<raw_w*30 and tile_left+60>0:
    # A candidate has nominal TMP pixels on both sides of the bottom edge,
    # yet the original flat 2x1 Radar fill gate excludes its source cell.
    assert flat_y>=raw_h and not(flat_x+2>0 and flat_x<raw_w and 0<=flat_y<raw_h)
    nominal_crossing+=1;fallback_crossing+=c.signed(cell+0x38)==65535
   for j in range(60):
    column=center_x-30+j
    if 0<=column<len(edge):
     bottom=center_y+min(j,59-j)//2
     if edge[column] is None or bottom>edge[column]:edge[column]=bottom
 known=[v for v in edge if v is not None]
 return {'stage':'01-off','source':'valid captured cell coordinates and signed levels; nominal 900-pixel TMP footprint only','legal_cells_read':accepted,'exact_radar_span_y':span_y,
  'raw_radar_dimensions':[raw_w,raw_h], 'level2_nominal_bottom_crossings_with_horizontal_overlap':nominal_crossing,
  'crossings_rejected_by_flat_rect':nominal_crossing,'fallback_tile_crossings':fallback_crossing,
  'south_bottom_min':min(known),'south_bottom_max':max(known),'unsupported_columns':sum(v is None for v in edge),
  'columns_whose_nominal_edge_is_above_viewport_bottom':sum(v is not None and v<span_y for v in edge),
  'limitations':'Excludes TMP extra images, source-index transparency, objects and the actual rasterizer; this envelope is diagnostic, never a global crop/mask.'}

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('capture');parser.add_argument('--output',type=Path);args=parser.parse_args()
 source=Path(__file__).with_name('radar_boundary.cpp')
 with tempfile.TemporaryDirectory() as tmp:
  portable=Path(tmp)/'radar-boundary';subprocess.run(['g++','-std=c++17','-O2',str(source),'-o',str(portable)],check=True)
  stages=[verify(args.capture,s,portable) for s in ('01-off','02-on','03-off')]
 result={'supported_executable_sha256':SUPPORTED_SHA,'scope':'original coordinate arithmetic and TMP row-offset table; not full native scene rendering','stages':stages,
  'captured_nominal_south_coverage':south_inventory(args.capture),
  'conclusion':'Existing half-cell center plus (-30,-15) TMP image origin matches the native terrain caller. Level is signed. Physical map legality remains separate from camera/usable extents.'}
 if args.output:args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(result))
