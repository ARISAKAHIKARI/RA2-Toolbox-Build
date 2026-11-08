"""Offline SHA preprocessing; no game access and no network requests."""
import argparse,json,struct,os,sys,traceback,time
from pathlib import Path
import numpy as np
import dis_render as engine
from preview_builder import valid_pack,atomic_text,checkpoint,pack_digest

def decode(path):
 b=Path(path).read_bytes()
 if len(b)<8 or len(b)>32*1024*1024:raise ValueError('Invalid SHA file size')
 zero,w,h,n=struct.unpack_from('<4H',b)
 if zero!=0 or w!=55 or h!=43 or n not in (450,517) or 8+n*24>len(b):raise ValueError('This build requires a RA2/YR-compatible 450/517-frame, 55x43 canvas SHA')
 frames=np.zeros((n,h,w),np.uint8)
 for i in range(n):
  p=8+24*i;x,y,sw,sh=struct.unpack_from('<4H',b,p);fmt=struct.unpack_from('<I',b,p+8)[0]&255;off=struct.unpack_from('<I',b,p+20)[0]
  if not off or not sw or not sh:continue
  if x+sw>w or y+sh>h:raise ValueError('Frame outside canvas')
  if fmt<=1:
   if off+sw*sh>len(b):raise ValueError('Truncated uncompressed frame')
   frames[i,y:y+sh,x:x+sw]=np.frombuffer(b,dtype=np.uint8,count=sw*sh,offset=off).reshape(sh,sw)
  elif fmt==2:
   q=off
   for row in range(sh):
    if q+2>len(b):raise ValueError('Truncated raw row')
    length=struct.unpack_from('<H',b,q)[0]
    if length<sw+2 or q+length>len(b):raise ValueError('Invalid raw row')
    frames[i,y+row,x:x+sw]=np.frombuffer(b,dtype=np.uint8,count=sw,offset=q+2)
    q+=length
  elif fmt==3:
   q=off
   for row in range(sh):
    if q+2>len(b):raise ValueError('Truncated RLE row')
    length=struct.unpack_from('<H',b,q)[0];end=q+length;q+=2;col=0
    if length<2 or end>len(b):raise ValueError('Invalid RLE row')
    while q<end:
     v=b[q];q+=1
     if v:
      if col>=sw:raise ValueError('RLE pixel outside frame')
      frames[i,y+row,x+col]=v;col+=1
     else:
      if q>=end:raise ValueError('Truncated RLE skip')
      skip=b[q];q+=1
      if not skip:raise ValueError('Invalid zero-length RLE skip')
      col+=skip
      if col>sw:raise ValueError('RLE skip outside frame')
    if col!=sw:raise ValueError('RLE row width differs')
  else:raise ValueError('Unsupported SHA compression')
 if not np.any(frames):raise ValueError('SHA is fully transparent; import the original visible resource')
 return frames

def palette(path):
 b=Path(path).read_bytes()
 if len(b)==768:
  rgb=np.frombuffer(b,np.uint8).reshape(256,3).copy()
  if rgb.max()<=63:rgb=np.minimum(rgb.astype(np.uint16)*4,255).astype(np.uint8)
  p=np.column_stack([rgb,np.full(256,255,np.uint8)]);p[0]=0;return p
 if len(b)!=1024:raise ValueError('Palette must be 1024-byte BGRA or 768-byte Westwood PAL')
 p=np.frombuffer(b,np.uint8).reshape(256,4)[:,[2,1,0,3]].copy();p[0]=0;return p

def bgra(im):
 a=im[:,:,3].astype(np.uint32);rgb=im[:,:,:3].astype(np.uint32)
 rgb=(rgb*a[:,:,None]+127)//255
 return ((a<<24)|(rgb[:,:,0]<<16)|(rgb[:,:,1]<<8)|rgb[:,:,2]).astype('<u4').reshape(-1)

def digest(a,b,w=55,h=43):
 return pack_digest(a,b,w,h)
def var(v):
 o=bytearray()
 while v>=128:o.append((v&127)|128);v>>=7
 o.append(v);return o
def write_pack(path,images,first,last):
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True);temp=path.with_suffix('.tmp');previous=None;count=0
 try:
  with open(temp,'wb') as f:
   f.write(struct.pack('<8sIIIQ',b'RA2DIS1\0',55,43,1001,digest(first,last)))
   for im in images:
    if count==0:
     if not np.array_equal(im,first):raise ValueError('First endpoint differs')
     f.write(im.tobytes())
    else:
     delta=im^previous;changed=np.flatnonzero(delta)
     if not len(changed):f.write(var(len(im)));f.write(b'\0')
     else:
      starts=np.r_[0,np.flatnonzero(np.diff(changed)>1)+1];ends=np.r_[starts[1:],len(changed)];pos=0
      for s,e in zip(starts,ends):
       begin=int(changed[s]);end=int(changed[e-1])+1;f.write(var(begin-pos));f.write(var(end-begin));f.write(delta[begin:end].tobytes());pos=end
      if pos<len(im):f.write(var(len(im)-pos));f.write(b'\0')
    previous=im;count+=1
   if count!=1001 or not np.array_equal(previous,last):raise ValueError('Last endpoint/sample count differs')
  os.replace(temp,path)
 finally:
  if temp.exists():temp.unlink()

def validate_groups(groups,frame_count=450):
 pairs=set()
 for g in groups:
  start=int(g['start']);count=int(g['count']);loop=bool(g.get('loop',False))
  if start<0 or count<1 or start+count>frame_count:raise ValueError('Group range outside source frames')
  pairs.update((i,i+1) for i in range(start,start+count-1))
  if loop and count>1:pairs.add((start+count-1,start))
 return sorted(pairs)
def request_pairs(path,canonical,frame_count):
 """A task selects work; the profile's canonical transition table stays whole."""
 path=Path(path)
 if not path.exists():return canonical,True
 raw=path.read_bytes()
 if len(raw)>65536:raise ValueError('Precompute request is too large')
 def unique(items):
  out={}
  for key,value in items:
   if key in out:raise ValueError('Duplicate request field')
   out[key]=value
  return out
 value=json.loads(raw.decode('utf-8-sig'),object_pairs_hook=unique)
 if type(value) is not dict or set(value)-{'schema','mode','pairs'}:raise ValueError('Invalid precompute request fields')
 if type(value.get('schema')) is not int or value['schema']!=1:raise ValueError('Unsupported precompute request schema')
 mode=value.get('mode')
 if type(mode) is not str or mode not in ('pairs','all','preview'):raise ValueError('Invalid precompute request mode')
 if mode!='pairs':
  if 'pairs' in value:raise ValueError('Pairs are only valid in pairs mode')
  return canonical,mode=='preview'
 pairs=value.get('pairs')
 if type(pairs) is not list or not 1<=len(pairs)<=512:raise ValueError('Request needs 1 to 512 pairs')
 out=[];seen=set()
 for pair in pairs:
  if type(pair) is not list or len(pair)!=2 or any(type(i) is not int for i in pair):raise ValueError('Pair indices must be integers')
  a,b=pair
  if not 0<=a<frame_count or not 0<=b<frame_count or a==b:raise ValueError('Pair outside source frames or identical indices')
  if (a,b) not in seen:seen.add((a,b));out.append((a,b))
 return out,False
def progress(path,done,total,state,message=''):
 text=f'[Progress]\nDone={done}\nTotal={total}\nState={state}\nMessage={message.replace(chr(10)," ")}\n'
 p=Path(path);p.parent.mkdir(parents=True,exist_ok=True);t=p.with_suffix('.tmp');t.write_text(text,encoding='utf-8');os.replace(t,p)
def run(args):
 profile=Path(args.profile);frames=decode(profile/'original.sha');pal=palette(profile/'palette.bgra');groups=json.loads((profile/'groups.json').read_text(encoding='utf-8-sig'));canonical=validate_groups(groups,len(frames))
 pairs,preview=request_pairs(Path(args.status).with_name('request.json'),canonical,len(frames));total=len(pairs);configured=False
 if args.cancel and Path(args.cancel).exists():progress(args.status,0,total,'cancelled');return
 atomic_text(profile/'pairs.txt',''.join(f'{a} {b}\n' for a,b in canonical),encoding='ascii')
 for j,(a,b) in enumerate(pairs):
  if args.cancel and Path(args.cancel).exists():progress(args.status,j,total,'cancelled');return
  progress(args.status,j,total,'running')
  first=bgra(pal[frames[a]]);last=bgra(pal[frames[b]]);target=profile/'timelines'/f'{a}-{b}.dis'
  if valid_pack(target,first,last,args.cancel):continue
  identical=np.array_equal(first,last);data=None
  if not identical:
   if not configured:engine.configure(frames,pal);configured=True
   data=engine.setup(a,b)
  def images():
   for k in range(1001):
    if k%100==0:checkpoint(args.cancel)
    yield first if identical else bgra(engine.render(data,k/1000))
  write_pack(target,images(),first,last)
 if preview:
  from preview_builder import build
  build(profile,frames,pal,groups,args.cancel)
 checkpoint(args.cancel)
 progress(args.status,total,total,'done')
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--profile',required=True);parser.add_argument('--status',required=True);parser.add_argument('--cancel');args=parser.parse_args()
 try:run(args)
 except InterruptedError:progress(args.status,0,0,'cancelled')
 except Exception as e:
  progress(args.status,0,0,'error',str(e));Path(args.status+'.error.txt').write_text(traceback.format_exc(),encoding='utf-8');sys.exit(1)
