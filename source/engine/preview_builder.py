import json,struct,base64,os
from pathlib import Path
import numpy as np
import cv2

def uintvar(f):
 v=0
 for i in range(5):
  b=f.read(1)
  if not b:raise ValueError('Truncated delta')
  x=b[0]
  if i==4 and x&0xf0:raise ValueError('Invalid delta integer')
  v|=(x&127)<<(i*7)
  if x<128:return v
 raise ValueError('Invalid delta')
def checkpoint(cancel):
 if cancel and Path(cancel).exists():raise InterruptedError('Cancelled')
def exact(f,n):
 b=f.read(n)
 if len(b)!=n:raise ValueError('Truncated timeline')
 return b
def pack_digest(a,b,w=55,h=43):
 value=1469598103934665603
 for byte in struct.pack('<II',w,h)+a.tobytes()+b.tobytes():value=((value^byte)*1099511628211)&0xffffffffffffffff
 return value
def pixels(path,first=None,last=None,cancel=None):
 """Parse every sample. The yielded working array is reused by the decoder."""
 checkpoint(cancel)
 with open(path,'rb') as f:
  magic,w,h,count,digest=struct.unpack('<8sIIIQ',exact(f,28))
  if magic!=b'RA2DIS1\0' or (w,h,count)!=(55,43,1001):raise ValueError('Unsupported timeline')
  if first is not None and last is not None and digest!=pack_digest(first,last,w,h):raise ValueError('Timeline source digest differs')
  prev=np.frombuffer(exact(f,w*h*4),'<u4').copy();start=prev.copy()
  if first is not None and not np.array_equal(start,first):raise ValueError('First endpoint differs')
  for k in range(count):
   if k%100==0:checkpoint(cancel)
   if k:
    pos=0
    while pos<len(prev):
     skip=uintvar(f)
     if skip>len(prev)-pos:raise ValueError('Invalid delta skip')
     pos+=skip;n=uintvar(f)
     if n>len(prev)-pos or (not n and pos<len(prev)):raise ValueError('Invalid delta bounds')
     if n:prev[pos:pos+n]^=np.frombuffer(exact(f,n*4),'<u4');pos+=n
   if k==count-1:
    if f.read(1):raise ValueError('Trailing timeline bytes')
    if last is not None and not np.array_equal(prev,last):raise ValueError('Last endpoint differs')
    if digest!=pack_digest(start,prev,w,h):raise ValueError('Timeline endpoint digest differs')
   yield prev
def valid_pack(path,first,last,cancel=None):
 try:
  for _ in pixels(path,first,last,cancel):pass
  return True
 except InterruptedError:raise
 except (OSError,ValueError,struct.error):return False
def samples(path,cancel=None):
 for k,prev in enumerate(pixels(path,cancel=cancel)):
   if k%50==0:
    a=(prev>>24).astype(np.float32);im=np.zeros((len(prev),4),np.uint8);im[:,3]=a
    for j,sh in enumerate([16,8,0]):im[:,j]=np.clip(np.rint(((prev>>sh)&255)*255/np.maximum(a,1)),0,255)
    yield im.reshape(43,55,4)
def atomic_text(path,text,cancel=None,encoding='utf-8'):
 path=Path(path);temp=path.with_name(path.name+'.tmp')
 try:
  temp.write_text(text,encoding=encoding);checkpoint(cancel);os.replace(temp,path)
 finally:
  if temp.exists():temp.unlink()
def png(im):
 ok,b=cv2.imencode('.png',im[:,:,[2,1,0,3]])
 if not ok:raise ValueError('PNG encode failed')
 return base64.b64encode(b).decode()
def build(profile,frames,palette,groups,cancel=None):
 profile=Path(profile)
 data=[]
 for item in groups:
  checkpoint(cancel)
  g=dict(item);s=g['start'];n=g['count'];g['end']=s+n-1;g['id']=f'{s:03}-{s+n-1:03}'
  g.setdefault('name',g['id']);g.setdefault('kind','循环动画' if g.get('loop') else ('静态' if n==1 else '状态驱动序列'))
  g['native']=[]
  for i in range(s,s+n):checkpoint(cancel);g['native'].append(png(palette[frames[i]]))
  g['pairs']=[[i,i+1] for i in range(s,s+n-1)]
  if g.get('loop') and n>1:g['pairs'].append([s+n-1,s])
  if n>1:
   ims=[]
   for a,b in g['pairs']:
    checkpoint(cancel);ims.extend(list(samples(profile/'timelines'/f'{a}-{b}.dis',cancel))[:-1])
   if not g.get('loop'):ims.append(palette[frames[s+n-1]])
   cols=20;atlas=np.zeros((((len(ims)+cols-1)//cols)*43,cols*55,4),np.uint8)
   for i,im in enumerate(ims):atlas[(i//cols)*43:(i//cols+1)*43,(i%cols)*55:(i%cols+1)*55]=im
   g['atlas']=png(atlas);g['sample_count']=len(ims)
  data.append(g)
 template=Path(__file__).with_name('player-template.html').read_text(encoding='utf-8')
 atomic_text(profile/'preview.html',template.replace('__DATA__',json.dumps(data,ensure_ascii=False,separators=(',',':'))),cancel)
