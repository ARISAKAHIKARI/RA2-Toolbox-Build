#!/usr/bin/env python3
"""Windows visible-game capture and passive ACK observation; never sends input.
GDI capture cost/observed frame changes are NOT game FPS or input-to-photon latency.
"""
import argparse,ctypes as C,json,math,os,struct,time,zlib
from pathlib import Path

def assist_prefix(first,second,pid):
 # This observer opens only Local\\RA2Assist_v042_; require its matching ABI.
 if len(first)!=76 or first!=second:return None
 values=struct.unpack('<19I',first)
 return values if values[0]==0x41533130 and values[1]==42 and values[2]==pid else None

def intersection(a,b):
 l,t=max(a[0],b[0]),max(a[1],b[1]);r,d=min(a[2],b[2]),min(a[3],b[3])
 return (l,t,r,d) if r>l and d>t else None

def quantiles(values):
 if not values:return {'samples':0}
 v=sorted(values)
 return {'samples':len(v),'p50_ms':v[math.ceil(len(v)*.50)-1],'p95_ms':v[math.ceil(len(v)*.95)-1],'p99_ms':v[math.ceil(len(v)*.99)-1],'max_ms':v[-1]}

def png(width,height,bgra):
 if width<1 or height<1 or width*height>16777216 or len(bgra)!=width*height*4:raise ValueError('invalid image extent')
 # Lossless RGB conversion; no resizing or invented interpolation.
 raw=bytearray(height*(width*3+1))
 for y in range(height):
  row=y*(width*3+1)+1;src=y*width*4
  raw[row:row+width*3:3]=bgra[src+2:src+width*4:4]
  raw[row+1:row+width*3:3]=bgra[src+1:src+width*4:4]
  raw[row+2:row+width*3:3]=bgra[src:src+width*4:4]
 def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw,3))+chunk(b'IEND',b'')

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--interactive',action='store_true');ap.add_argument('--pid',type=int);ap.add_argument('--self-test',action='store_true');ap.add_argument('--seconds',type=float,default=15);ap.add_argument('--hz',type=int,default=20);ap.add_argument('--expect',default='');ap.add_argument('--include-cursor',action='store_true');ap.add_argument('--out',type=Path,default=Path('capture-results'));args=ap.parse_args()
 if args.interactive:
  choice=input('1 = capture self-test; 2 = visible game capture [1]: ').strip() or '1'
  if choice=='1':args.self_test=True
  elif choice=='2':
   try:args.pid=int(input('Game process PID (Task Manager Details): ').strip())
   except ValueError:ap.error('PID must be an integer')
   args.expect=input('Enabled overlays to check together (assist,stats,cursor,hud; blank to observe): ').strip()
   args.include_cursor=True
   print('Keep the game and selected overlays visible. Switch to the game within 5 seconds. Capture lasts 15 seconds.');time.sleep(5)
  else:ap.error('Choose 1 or 2')
 args.out.mkdir(parents=True,exist_ok=True)
 report={'scope':'visible game-client GDI CAPTUREBLT; optional OS-cursor composition; passive generic-command ACK observation','status':'BLOCKED','checks':[],'frames':[],'limits':['No input automation or game memory writes','Black/unchanged images are inconclusive, not proof the game froze','ACK observation is polling-delayed; excludes commands missed between polls and does not prove execution','Window visibility does not prove overlay pixels are correct','Exclusive fullscreen/GPU composition can be unsupported by GDI']}
 if os.name!='nt':
  report['reason']='Windows desktop/session unavailable';(args.out/'capture-report.json').write_text(json.dumps(report,indent=2));print('BLOCKED: requires Windows desktop and a visible game window');return 3
 if not 1<=args.hz<=120 or not 1<=args.seconds<=600:ap.error('hz 1..120; seconds 1..600')
 from ctypes import wintypes as W
 u=C.WinDLL('user32',use_last_error=True);g=C.WinDLL('gdi32',use_last_error=True);k=C.WinDLL('kernel32',use_last_error=True)
 def bind(lib,name,restype,*args):f=getattr(lib,name);f.restype=restype;f.argtypes=list(args);return f
 ptr=C.c_void_p;B=W.BOOL;D=W.DWORD;I=C.c_int;H=W.HANDLE
 bind(u,'GetDC',H,W.HWND);bind(u,'ReleaseDC',I,W.HWND,H);bind(u,'GetForegroundWindow',W.HWND);bind(u,'IsWindowVisible',B,W.HWND);bind(u,'IsIconic',B,W.HWND)
 bind(u,'GetWindowThreadProcessId',D,W.HWND,C.POINTER(D));bind(u,'GetClientRect',B,W.HWND,C.POINTER(W.RECT));bind(u,'GetWindowRect',B,W.HWND,C.POINTER(W.RECT));bind(u,'ClientToScreen',B,W.HWND,C.POINTER(W.POINT));bind(u,'GetSystemMetrics',I,I)
 bind(u,'GetClassNameW',I,W.HWND,W.LPWSTR,I);bind(u,'GetWindowLongW',C.c_long,W.HWND,I)
 bind(g,'CreateCompatibleDC',H,H);bind(g,'DeleteDC',B,H);bind(g,'DeleteObject',B,H);bind(g,'SelectObject',H,H,H);bind(g,'BitBlt',B,H,I,I,I,I,H,I,I,D)
 bind(k,'GetTickCount',D);bind(k,'OpenFileMappingW',H,D,B,W.LPCWSTR);bind(k,'MapViewOfFile',ptr,H,D,D,D,C.c_size_t);bind(k,'UnmapViewOfFile',B,ptr);bind(k,'CloseHandle',B,H)
 try:bind(u,'SetProcessDpiAwarenessContext',B,ptr)(ptr(-4))
 except AttributeError:bind(u,'SetProcessDPIAware',B)()
 class BI(C.Structure):_fields_=[('size',D),('width',C.c_long),('height',C.c_long),('planes',W.WORD),('bits',W.WORD),('compression',D),('imageSize',D),('xppm',C.c_long),('yppm',C.c_long),('used',D),('important',D)]
 bind(g,'CreateDIBSection',H,H,C.POINTER(BI),W.UINT,C.POINTER(ptr),H,D)
 class CI(C.Structure):_fields_=[('cbSize',D),('flags',D),('cursor',H),('pos',W.POINT)]
 class II(C.Structure):_fields_=[('icon',B),('hotX',D),('hotY',D),('mask',H),('color',H)]
 bind(u,'GetCursorInfo',B,C.POINTER(CI));bind(u,'GetIconInfo',B,H,C.POINTER(II));bind(u,'DrawIconEx',B,H,I,I,H,I,I,W.UINT,H,W.UINT)
 callback=C.WINFUNCTYPE(B,W.HWND,W.LPARAM);bind(u,'EnumWindows',B,callback,W.LPARAM)
 classes={'assist':'RA2AssistOverlay010','stats':'RA2UnitTransparentOverlay','cursor':'RA2CursorOverlay3','hud':'RA2CursorHUD9'}
 expected=[x for x in args.expect.split(',') if x]
 if any(x not in classes for x in expected):ap.error('expect uses assist,stats,cursor,hud')
 def pid(hwnd):p=D();u.GetWindowThreadProcessId(hwnd,C.byref(p));return p.value
 def rect(hwnd,client=False):
  r=W.RECT();ok=u.GetClientRect(hwnd,C.byref(r)) if client else u.GetWindowRect(hwnd,C.byref(r))
  if not ok:return None
  if client:
   point=W.POINT();
   if not u.ClientToScreen(hwnd,C.byref(point)):return None
   return (point.x,point.y,point.x+r.right,point.y+r.bottom)
  return (r.left,r.top,r.right,r.bottom)
 def inventory():
  found=[]
  @callback
  def collect(hwnd,_):
   name=C.create_unicode_buffer(256);u.GetClassNameW(hwnd,name,256)
   found.append({'hwnd':int(hwnd),'pid':pid(hwnd),'class':name.value,'visible':bool(u.IsWindowVisible(hwnd)),'minimized':bool(u.IsIconic(hwnd)),'rect':rect(hwnd),'exstyle':u.GetWindowLongW(hwnd,-20)&0xffffffff});return True
  if not u.EnumWindows(collect,0):raise C.WinError(C.get_last_error())
  return found
 class Surface:
  def __init__(self,w,h):
   if w<1 or h<1 or w*h>16777216:raise ValueError('capture exceeds 16M pixels')
   self.w=w;self.h=h;self.screen=self.dc=self.bitmap=self.old=None;self.bits=ptr()
   try:
    self.screen=u.GetDC(None);self.dc=g.CreateCompatibleDC(self.screen)
    if not self.screen or not self.dc:raise C.WinError(C.get_last_error())
    bi=BI();bi.size=C.sizeof(bi);bi.width=w;bi.height=-h;bi.planes=1;bi.bits=32
    self.bitmap=g.CreateDIBSection(self.screen,C.byref(bi),0,C.byref(self.bits),None,0)
    if not self.bitmap or not self.bits:raise C.WinError(C.get_last_error())
    self.old=g.SelectObject(self.dc,self.bitmap)
    if not self.old or self.old==ptr(-1).value:raise C.WinError(C.get_last_error())
   except Exception:self.close();raise
  def close(self):
   if self.old and self.old!=ptr(-1).value:g.SelectObject(self.dc,self.old)
   if self.bitmap:g.DeleteObject(self.bitmap)
   if self.dc:g.DeleteDC(self.dc)
   if self.screen:u.ReleaseDC(None,self.screen)
   self.old=self.bitmap=self.dc=self.screen=None
  def capture(self,x,y,cursor=False):
   start=time.perf_counter_ns()
   if not g.BitBlt(self.dc,0,0,self.w,self.h,self.screen,x,y,0x40cc0020):raise C.WinError(C.get_last_error())
   cursor_drawn=False
   if cursor:
    ci=CI();ci.cbSize=C.sizeof(ci)
    if u.GetCursorInfo(C.byref(ci)) and ci.flags&1:
     ii=II()
     if u.GetIconInfo(ci.cursor,C.byref(ii)):
      try:cursor_drawn=bool(u.DrawIconEx(self.dc,ci.pos.x-x-int(ii.hotX),ci.pos.y-y-int(ii.hotY),ci.cursor,0,0,0,None,3))
      finally:
       if ii.mask:g.DeleteObject(ii.mask)
       if ii.color:g.DeleteObject(ii.color)
   data=C.string_at(self.bits,self.w*self.h*4);return data,(time.perf_counter_ns()-start)/1e6,cursor_drawn
 surfaces=[];windows=[];mapping=None;view=None
 try:
  if args.self_test:
   bind(u,'CreateWindowExW',W.HWND,D,W.LPCWSTR,W.LPCWSTR,D,I,I,I,I,W.HWND,H,H,ptr);bind(u,'DestroyWindow',B,W.HWND);bind(u,'SetLayeredWindowAttributes',B,W.HWND,D,W.BYTE,D);bind(u,'UpdateWindow',B,W.HWND)
   # Three OS-owned STATIC windows exercise layered-window inclusion at known
   # coordinates, without any game executable or changes to user settings.
   vx=u.GetSystemMetrics(76);vy=u.GetSystemMetrics(77);x=vx+80;y=vy+80
   for dx,dy,w,h,style in [(0,0,240,160,5),(20,20,60,60,6),(110,20,60,60,4)]:
    hwnd=u.CreateWindowExW(0x080800a8,'STATIC','RA2 capture self-test',0x90000000|style,x+dx,y+dy,w,h,None,None,None,None)
    if not hwnd:raise C.WinError(C.get_last_error())
    windows.append(hwnd);u.SetLayeredWindowAttributes(hwnd,0,255,2);u.UpdateWindow(hwnd)
   time.sleep(.2);surface=Surface(240,160);surfaces.append(surface);data,ms,_=surface.capture(x,y)
   def pixel(px,py):i=(py*240+px)*4;return tuple(data[i:i+3])
   white=pixel(40,40);black=pixel(130,40);ok=min(white)>240 and max(black)<15
   report['checks']=[{'case':'two_layered_windows_in_same_capture','status':'PASS' if ok else 'INCONCLUSIVE','white_bgr':white,'black_bgr':black}]
   (args.out/'self-test.png').write_bytes(png(240,160,data));report['status']='PASS' if ok else 'INCONCLUSIVE';report['capture_ms']=ms
  else:
   if not args.pid:ap.error('provide game --pid, or use --self-test')
   candidates=[x for x in inventory() if x['pid']==args.pid and x['visible'] and not x['minimized']]
   candidates=[x for x in candidates if rect(x['hwnd'],True)]
   if not candidates:raise RuntimeError('No visible window for requested PID')
   target=max(candidates,key=lambda x:(x['rect'][2]-x['rect'][0])*(x['rect'][3]-x['rect'][1]));hwnd=target['hwnd']
   report['target']={key:target[key] for key in ('hwnd','pid','class')};report['expected_overlays']=expected
   mapping=k.OpenFileMappingW(4,False,'Local\\RA2Assist_v042_'+str(args.pid))
   if mapping:view=k.MapViewOfFile(mapping,4,0,0,76)
   report['passive_ack_mapping']=bool(view);last_ack=None;ack_ages=[];costs=[];intervals=[];last_capture=None;last_crc=None;changes=0;captured=0;simultaneous=0;visible_counts={key:0 for key in classes};skipped=0;last_image=None;start=time.perf_counter();due=start;surface=None
   while time.perf_counter()-start<args.seconds:
    now=time.perf_counter();wait=due-now
    if wait>0:time.sleep(wait)
    due=time.perf_counter()+1/args.hz;row={'elapsed_ms':(time.perf_counter()-start)*1000,'foreground_pid':pid(u.GetForegroundWindow()),'minimized':bool(u.IsIconic(hwnd))}
    all_windows=inventory();overlays={key:[x for x in all_windows if x['class']==value and x['visible']] for key,value in classes.items()};row['overlays']=overlays
    roi=rect(hwnd,True);virtual=(u.GetSystemMetrics(76),u.GetSystemMetrics(77),u.GetSystemMetrics(76)+u.GetSystemMetrics(78),u.GetSystemMetrics(77)+u.GetSystemMetrics(79));roi=intersection(roi,virtual) if roi else None
    if row['minimized'] or row['foreground_pid']!=args.pid or not roi:
     row['capture']='SKIPPED_not_foreground_or_minimized';skipped+=1
    else:
     w,h=roi[2]-roi[0],roi[3]-roi[1]
     if not surface or (surface.w,surface.h)!=(w,h):
      if surface:surface.close();surfaces.remove(surface)
      surface=Surface(w,h);surfaces.append(surface)
     data,ms,cursor_drawn=surface.capture(roi[0],roi[1],args.include_cursor);stamp=time.perf_counter();crc=zlib.crc32(data);costs.append(ms)
     if last_capture is not None:intervals.append((stamp-last_capture)*1000)
     if last_crc is not None and crc!=last_crc:changes+=1
     last_capture=stamp;last_crc=crc;captured+=1;row.update(capture='OK',rect=roi,capture_ms=ms,crc32=crc,os_cursor_composited=cursor_drawn)
     visible_now={key:any(x['rect'] and intersection(x['rect'],roi) for x in overlays[key]) for key in classes}
     for key in classes:
      if visible_now[key]:visible_counts[key]+=1
     if expected and all(visible_now[key] for key in expected):simultaneous+=1
     if captured==1:(args.out/'first.png').write_bytes(png(w,h,data))
     last_image=(w,h,data)
    if view:
     first=C.string_at(view,76);second=C.string_at(view,76)
     if first==second:
      values=assist_prefix(first,second,args.pid)
      if values is not None:
       req,ack,created=values[9],values[10],values[14]
       if last_ack is not None and ack!=last_ack and ack==req:
        age=(k.GetTickCount()-created)&0xffffffff
        if age<60000:ack_ages.append(age);row['observed_command_ack_age_ms']=age
       last_ack=ack
      else:row['ack_probe']='unsupported_protocol_or_identity'
    report['frames'].append(row)
   if last_image:(args.out/'last.png').write_bytes(png(*last_image))
   report.update(captured=captured,skipped=skipped,expected_overlays_simultaneous_frames=simultaneous,visible_frame_counts=visible_counts,changed_capture_pairs=changes,capture_cost=quantiles(costs),capture_intervals=quantiles(intervals),observed_generic_command_ack_age=quantiles(ack_ages))
   for name in expected:report['checks'].append({'case':name+'_window_visible_during_capture','status':'PASS' if visible_counts[name] else 'INCONCLUSIVE'})
   if len(expected)>1:report['checks'].append({'case':'expected_overlay_windows_visible_in_same_capture','status':'PASS' if simultaneous else 'INCONCLUSIVE'})
   report['status']='CAPTURED_REVIEW_REQUIRED' if captured else 'INCONCLUSIVE'
 except Exception as e:report['status']='ERROR';report['reason']=str(e)
 finally:
  for surface in surfaces:surface.close()
  for hwnd in reversed(windows):u.DestroyWindow(hwnd)
  if view:k.UnmapViewOfFile(view)
  if mapping:k.CloseHandle(mapping)
  (args.out/'capture-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 print(report['status'],args.out/'capture-report.json');return 0 if report['status'] in ('PASS','CAPTURED_REVIEW_REQUIRED') else 2
if __name__=='__main__':raise SystemExit(main())
