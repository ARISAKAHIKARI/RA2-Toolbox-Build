import io,json,zipfile,re,shutil,sys
from pathlib import Path
import numpy as np
import cairosvg
from PIL import Image,ImageDraw
R=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(R/'engine'))
from precompute import decode,palette
F=decode(R/'assets/original.sha');P=palette(R/'assets/palette.bgra');used=set(np.unique(F[2:]));free=[i for i in range(1,256) if i not in used]

def encode(frames):
 import struct
 n,h,w=frames.shape;head=bytearray(struct.pack('<4H',0,w,h,n));data=bytearray();offset=8+24*n
 for frame in frames:
  ys,xs=np.where(frame)
  if not len(xs):head.extend(struct.pack('<4H4I',0,0,0,0,1,0,0,0));continue
  x0,x1,y0,y1=int(xs.min()),int(xs.max()+1),int(ys.min()),int(ys.max()+1);chunk=frame[y0:y1,x0:x1].tobytes()
  head.extend(struct.pack('<4H4I',x0,y0,x1-x0,y1-y0,1,0,0,offset+len(data)));data.extend(chunk)
 return head+data

entries=[('bibata-ice','Bibata Modern Ice · 普通指针','bibata','Bibata_Cursor-main/svg/groups/modern/left_ptr.svg',{'#00FF00':'#F5F5F5','#0000FF':'#222222','#FF0000':'#F5F5F5'}),('bibata-amber','Bibata Modern Amber · 普通指针','bibata','Bibata_Cursor-main/svg/groups/modern/left_ptr.svg',{'#00FF00':'#FFB74D','#0000FF':'#222222','#FF0000':'#FFB74D'}),('capitaine','Capitaine Dark · 普通指针','capitaine','capitaine-cursors-master/src/svg/dark/default.svg',{})]
board=Image.new('RGB',(660,150),(35,42,50));draw=ImageDraw.Draw(board)
for j,(id,name,repo,src,replace) in enumerate(entries):
 z=zipfile.ZipFile(R/'third-party'/f'{repo}-upstream-source.zip');svg=z.read(src).decode()
 raw=svg
 for a,b in replace.items():svg=svg.replace(a,b)
 im=Image.open(io.BytesIO(cairosvg.svg2png(bytestring=svg.encode(),output_width=38,output_height=38))).convert('RGBA');box=im.getbbox();im=im.crop(box)
 # Source SHA has binary transparency; keep crisp silhouette and quantize at most 24 theme colors.
 a=np.array(im);opaque=a[:,:,3]>=128;rgb=Image.fromarray(a[:,:,:3]).quantize(colors=min(24,len(free)),method=Image.Quantize.MEDIANCUT);idx=np.array(rgb);cp=np.array(rgb.getpalette(),np.uint8).reshape(-1,3)
 frame=F.copy();pal=P.copy();mapping={}
 for k in np.unique(idx[opaque]):
  pi=free[len(mapping)];mapping[int(k)]=pi;pal[pi,:3]=cp[k];pal[pi,3]=255
 custom=np.zeros(idx.shape,np.uint8)
 for k,v in mapping.items():custom[(idx==k)&opaque]=v
 # Both normal and minimap arrow keep their top-left click hotspot.
 for number,scale in [(0,1),(1,.5)]:
  pic=Image.fromarray(custom)
  if scale!=1:pic=pic.resize((max(1,int(pic.width*scale)),max(1,int(pic.height*scale))),Image.Resampling.NEAREST)
  arr=np.array(pic);frame[number]=0;frame[number,:arr.shape[0],:arr.shape[1]]=arr
 folder=R/'profiles'/id;folder.mkdir(parents=True,exist_ok=True)
 (folder/'original.sha').write_bytes(encode(frame));(folder/'palette.bgra').write_bytes(pal[:,[2,1,0,3]].tobytes())
 for file in ['groups.json','pairs.txt']:shutil.copyfile(R/'assets'/file,folder/file)
 (folder/'profile.ini').write_text('[Profile]\nName='+name+'\n',encoding='utf-16')
 (folder/'SOURCE.svg').write_text(raw);(folder/'rendered-source.svg').write_text(svg)
 licensefile='Bibata_Cursor-main/LICENSE' if repo=='bibata' else 'capitaine-cursors-master/COPYING'
 (folder/'LICENSE-source.txt').write_bytes(z.read(licensefile))
 (folder/'SOURCE-NOTICE.txt').write_text(f'{name}\nSource: https://github.com/'+('ful1e5/Bibata_Cursor' if repo=='bibata' else 'keeferrourke/capitaine-cursors')+'\nOnly the ordinary pointer frames 0 and 1 are replaced. The other RA2 game action frames come from the user supplied original.sha and are NOT claimed to be open-source artwork.\nConversion: render vector at 38px, crop transparent margins, alpha threshold 128, quantize <=24 colors into unused palette entries; mini pointer nearest-neighbor 50%.\nComplete original sources and build script are included under third-party and source.\n')
 # Reuse verified unchanged game sequences; create distinct stock preview title and normal frames.
 page=(R/'assets/preview.html').read_text();data=page.split('const groups=',1)[1].split(';const $',1)[0];preview=json.loads(data)
 import base64
 for g in preview:
  if g['start'] in (0,1) and g['count']==1:
   b=io.BytesIO();Image.fromarray(pal[frame[g['start']]]).save(b,format='PNG');g['native']=[base64.b64encode(b.getvalue()).decode()]
 (folder/'preview.html').write_text((R/'engine/player-template.html').read_text().replace('__DATA__',json.dumps(preview,ensure_ascii=False,separators=(',',':'))))
 q=Image.fromarray(pal[frame[0]]).resize((165,129),Image.Resampling.NEAREST);board.paste(q,(j*220+25,15),q);draw.text((j*220+12,132),id,fill='white')
board.save(R/'theme-preview.png')
