#!/usr/bin/env python3
"""Layout preview from the executed Win32 construction harness; not a screenshot.

Usage: python tools/render_ui_preview.py /path/to/CJK-font.otf
Font is used locally for rendering and is not redistributed in the release.
"""
from pathlib import Path
import json,sys
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parent.parent
data=json.loads((R/'ui-geometry.json').read_text());fontpath=sys.argv[1]
BG='#121822';SURFACE='#1a2331';FIELD='#202b3b';TEXT='#e7eef7';MUTED='#99aac1';ACCENT='#68c6ff'
fonts={s:ImageFont.truetype(fontpath,s) for s in (13,14,16,20,22)}
def render(scroll,name):
 im=Image.new('RGB',(1120,940),BG);d=ImageDraw.Draw(im)
 def text(x,y,s,size=14,fill=TEXT):d.text((x,y),s,font=fonts[size],fill=fill,anchor='lt')
 d.rectangle((0,0,1120,32),fill='#0c111a');text(16,8,'红警工具箱  ·  v1.0',13);text(1018,7,'—     □     ×',13,MUTED)
 text(22,54,'红警工具箱',20);text(24,91,'v1.0',14,MUTED)
 for i,label in enumerate(('高刷光标','单位统计','战术助手')):
  y=148+i*52
  d.rounded_rectangle((16,y,176,y+42),radius=9,fill='#24435b' if i==2 else SURFACE)
  text(30,y+12,label,14,ACCENT if i==2 else TEXT)
 text(24,350,'功能开关',14,MUTED)
 for i,label in enumerate(('高刷光标','单位统计','战术助手')):
  y=388+i*36;d.rounded_rectangle((24,y+4,56,y+22),radius=9,fill=ACCENT);d.ellipse((42,y+6,54,y+20),fill=BG);text(66,y+5,label)
 d.line((190,44,190,900),fill='#273346');text(210,908,'设置自动保存 · 切换界面不影响已开启的功能',13,MUTED)
 # 908-pixel-wide embedded pane; actual layout centers the 685-pixel document.
 px=196+(908-685)//2;py=44;height=850;bodytop=124;footer=height-84
 for c in data['geometry']:
  y=c['y']+(footer if c['page']==-2 else -scroll if c['page']>=0 else 0)
  if c['page']>=0 and (y<bodytop or y+c['h']>footer-10):continue
  x=px+c['x'];y+=py;w=c['w'];h=c['h'];label=c['text'];cls=c['class']
  if cls=='BUTTON':
   if c.get('toggle'):
    cy=y+(h-18)//2;on=c.get('checked',False);d.rounded_rectangle((x+2,cy,x+34,cy+18),radius=9,fill=ACCENT if on else '#435062');dx=x+(18 if on else 4);d.ellipse((dx,cy+2,dx+14,cy+16),fill=BG if on else TEXT);text(x+44,y+max(0,(h-14)//2),label)
   else:
    d.rounded_rectangle((x,y,x+w,y+h),radius=8,fill=SURFACE);tw=d.textlength(label,font=fonts[14]);text(x+(w-tw)/2,y+max(0,(h-14)//2),label)
  elif cls in ('EDIT','COMBOBOX','HOTKEY'):
   d.rounded_rectangle((x,y,x+w,y+h),radius=4,fill=FIELD,outline='#344258');value=label
   if cls=='HOTKEY' and not value:value='未绑定'
   if c['id']==990:value='搜索功能，例如：装车、铁幕、CS'
   text(x+8,y+max(0,(h-14)//2),value,14,MUTED if not label else TEXT)
   if cls=='COMBOBOX':d.polygon(((x+w-18,y+10),(x+w-10,y+10),(x+w-14,y+15)),fill=MUTED)
  elif cls=='LISTBOX':d.rounded_rectangle((x,y,x+w,y+h),radius=4,fill=FIELD)
  else:text(x,y+2,label,20 if c['id'] in (989,900,901,902) else 14,ACCENT if c['id'] in (900,901,902) else TEXT if c['id']==989 else MUTED)
 d.rounded_rectangle((1091,py+bodytop,1096,py+footer-10),radius=3,fill=SURFACE)
 track=footer-10-bodytop;thumb=max(30,int(track*track/(data['content_bottom']-bodytop)));sy=py+bodytop+int(scroll*track/data['content_bottom']);d.rounded_rectangle((1091,sy,1096,sy+thumb),radius=3,fill='#60738b')
 d.rectangle((0,928,1120,940),fill=BG)
 im.save(R/'docs'/name)
render(0,'UI-Preview.png')
start=next(c['y'] for c in data['geometry'] if c['id']==901)-124-12
render(start,'UI-Automation-Preview.png')
print('Rendered actual geometry with illustrative native styling; not Windows screenshots')
