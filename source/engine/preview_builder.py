import json,struct,base64,io
from pathlib import Path
import numpy as np
import cv2

def uintvar(f):
 v=0
 for i in range(5):
  b=f.read(1)
  if not b:raise ValueError('Truncated delta')
  x=b[0];v|=(x&127)<<(i*7)
  if x<128:return v
 raise ValueError('Invalid delta')
def samples(path):
 with open(path,'rb') as f:
  magic,w,h,count,digest=struct.unpack('<8sIIIQ',f.read(28))
  if magic!=b'RA2DIS1\0' or (w,h,count)!=(55,43,1001):raise ValueError('Unsupported timeline')
  prev=np.frombuffer(f.read(w*h*4),'<u4').copy()
  for k in range(1001):
   if k:
    pos=0
    while pos<len(prev):
     skip=uintvar(f);n=uintvar(f);pos+=skip
     if pos+n>len(prev) or (not n and pos<len(prev)):raise ValueError('Invalid delta bounds')
     if n:prev[pos:pos+n]^=np.frombuffer(f.read(n*4),'<u4');pos+=n
   if k%50==0:
    a=(prev>>24).astype(np.float32);im=np.zeros((len(prev),4),np.uint8);im[:,3]=a
    for j,sh in enumerate([16,8,0]):im[:,j]=np.clip(np.rint(((prev>>sh)&255)*255/np.maximum(a,1)),0,255)
    yield im.reshape(h,w,4)
def png(im):
 ok,b=cv2.imencode('.png',im[:,:,[2,1,0,3]])
 if not ok:raise ValueError('PNG encode failed')
 return base64.b64encode(b).decode()
def build(profile,frames,palette,groups,cancel=None):
 data=[]
 for item in groups:
  if cancel and Path(cancel).exists():raise InterruptedError('Cancelled')
  g=dict(item);s=g['start'];n=g['count'];g['end']=s+n-1;g['id']=f'{s:03}-{s+n-1:03}'
  g.setdefault('name',g['id']);g.setdefault('kind','循环动画' if g.get('loop') else ('静态' if n==1 else '状态驱动序列'))
  g['native']=[png(palette[frames[i]]) for i in range(s,s+n)]
  g['pairs']=[[i,i+1] for i in range(s,s+n-1)]
  if g.get('loop') and n>1:g['pairs'].append([s+n-1,s])
  if n>1:
   ims=[]
   for a,b in g['pairs']:ims.extend(list(samples(profile/'timelines'/f'{a}-{b}.dis'))[:-1])
   if not g.get('loop'):ims.append(palette[frames[s+n-1]])
   cols=20;atlas=np.zeros((((len(ims)+cols-1)//cols)*43,cols*55,4),np.uint8)
   for i,im in enumerate(ims):atlas[(i//cols)*43:(i//cols+1)*43,(i%cols)*55:(i%cols+1)*55]=im
   g['atlas']=png(atlas);g['sample_count']=len(ims)
  data.append(g)
 template=Path(__file__).with_name('player-template.html').read_text(encoding='utf-8')
 (profile/'preview.html').write_text(template.replace('__DATA__',json.dumps(data,ensure_ascii=False,separators=(',',':'))),encoding='utf-8')
