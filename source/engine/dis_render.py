# OpenCV DIS motion estimation + palette-constrained signed-distance sprite morph.
import numpy as np
import cv2
from scipy.spatial import cKDTree
from scipy.ndimage import distance_transform_edt

def configure(frames,palette):
 global F,PAL,N,H,W,RGB,colors,tree,yy,xx
 F=frames;PAL=palette;N,H,W=F.shape;cv2.setNumThreads(1)
 RGB=PAL[:,:3].astype(np.float32);colors=np.unique(RGB[1:],axis=0);tree=cKDTree(colors)
 yy,xx=np.mgrid[:H,:W].astype(np.float32)
def rgba(i):return PAL[F[i]].copy()
def flow(a,b):
 def gray(i):
  im=rgba(i);rgb=np.where(im[:,:,3:]>0,im[:,:,:3],96).astype(np.uint8)
  return cv2.resize(cv2.cvtColor(rgb,cv2.COLOR_RGB2GRAY),(W*4,H*4),interpolation=cv2.INTER_NEAREST)
 dis=cv2.DISOpticalFlow_create(cv2.DISOPTICAL_FLOW_PRESET_MEDIUM);dis.setFinestScale(0);dis.setPatchSize(8);dis.setPatchStride(3)
 f=dis.calc(gray(a),gray(b),None)
 return cv2.resize(f,(W,H),interpolation=cv2.INTER_AREA)/4

def remap(a,x,y):return cv2.remap(a,x.astype(np.float32),y.astype(np.float32),cv2.INTER_LINEAR,borderMode=cv2.BORDER_CONSTANT)
def inverse(f,t):
 x=xx-t*f[:,:,0];y=yy-t*f[:,:,1]
 for _ in range(2):
  ff=remap(f,x,y);x=xx-t*ff[:,:,0];y=yy-t*ff[:,:,1]
 return x,y
def setup(a,b):
 f=flow(a,b);back=flow(b,a)
 aa=rgba(a).astype(np.float32);bb=rgba(b).astype(np.float32)
 aa[:,:,:3]*=aa[:,:,3:]/255;bb[:,:,:3]*=bb[:,:,3:]/255
 def sdf(mask):
  return ((distance_transform_edt(mask)-distance_transform_edt(~mask))-np.where(mask,0.5,-0.5)).astype(np.float32)
 return aa,bb,f,back,sdf(F[a]>0),sdf(F[b]>0),rgba(a),rgba(b)
def render(data,t):
 aa,bb,f,back,sa,sb,original_a,original_b=data
 if t<=0:return original_a.copy()
 if t>=1:return original_b.copy()
 ax,ay=inverse(f,t);bx,by=inverse(back,1-t)
 aw=remap(aa,ax,ay);bw=remap(bb,bx,by)
 # Smooth residual blend, no midpoint endpoint switch; premultiplied alpha avoids black fringes.
 u=t*t*(3-2*t);mix=aw*(1-u)+bw*u
 da=remap(sa,ax,ay);db=remap(sb,bx,by)
 alpha=np.clip(np.rint(((1-u)*da+u*db+0.5)*255),0,255).astype(np.uint8)
 alpha[mix[:,:,3]<0.5]=0
 rgb=mix[:,:,:3]*255/np.maximum(mix[:,:,3:],1)
 _,k=tree.query(rgb.reshape(-1,3),workers=1)
 out=np.empty((H,W,4),np.uint8);out[:,:,:3]=colors[k].reshape(H,W,3);out[:,:,3]=alpha;out[alpha==0]=0
 return out

