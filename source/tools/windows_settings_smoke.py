"""Run the shipped Windows UI and real INI APIs; no game process is required."""
import ctypes as C
from ctypes import wintypes as W
import json, os, pathlib, shutil, subprocess, sys, tempfile, time

u=C.WinDLL('user32',use_last_error=True)
CB=C.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
u.EnumWindows.argtypes=[CB,W.LPARAM]
u.EnumChildWindows.argtypes=[W.HWND,CB,W.LPARAM]
u.GetWindowThreadProcessId.argtypes=[W.HWND,C.POINTER(W.DWORD)]
u.GetClassNameW.argtypes=[W.HWND,W.LPWSTR,C.c_int]
u.GetWindowTextW.argtypes=[W.HWND,W.LPWSTR,C.c_int]
u.IsWindowVisible.argtypes=[W.HWND];u.IsWindowVisible.restype=W.BOOL
u.GetWindowRect.argtypes=[W.HWND,C.POINTER(W.RECT)];u.GetWindowRect.restype=W.BOOL
u.GetDlgCtrlID.argtypes=[W.HWND]
u.GetParent.argtypes=[W.HWND];u.GetParent.restype=W.HWND
u.IsWindowEnabled.argtypes=[W.HWND];u.IsWindowEnabled.restype=W.BOOL
u.GetPropW.argtypes=[W.HWND,W.LPCWSTR];u.GetPropW.restype=W.HANDLE
u.SendMessageTimeoutW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM,W.UINT,W.UINT,C.POINTER(C.c_size_t)]
u.SendMessageTimeoutW.restype=W.LPARAM
u.PostMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM]
k=C.WinDLL('kernel32',use_last_error=True)
k.GetPrivateProfileIntW.argtypes=[W.LPCWSTR,W.LPCWSTR,C.c_int,W.LPCWSTR]
k.GetPrivateProfileIntW.restype=W.UINT

def text(h,cls=False):
    b=C.create_unicode_buffer(500)
    (u.GetClassNameW if cls else u.GetWindowTextW)(h,b,len(b))
    return b.value
def send(h,m,w=0,l=0):
    result=C.c_size_t()
    if not u.SendMessageTimeoutW(h,m,w,l,2,5000,C.byref(result)):
        raise RuntimeError(f'message timeout {text(h)} {m:x}: {C.get_last_error()}')
    return result.value
def set_control_text(h,value):
    # SetWindowText only changes controls in the calling process. System text
    # messages marshal the buffer to the actual edit control in the host.
    b=C.create_unicode_buffer(value)
    assert send(h,0xC,0,C.addressof(b)),('WM_SETTEXT failed',u.GetDlgCtrlID(h))
    readback=C.create_unicode_buffer(len(value)+20)
    send(h,0xD,len(readback),C.addressof(readback))
    assert readback.value==value,('control text mismatch',u.GetDlgCtrlID(h),readback.value,value)
def control_text(h):
    b=C.create_unicode_buffer(100)
    send(h,0xD,len(b),C.addressof(b))
    return b.value
def windows(parent=None):
    result=[]
    @CB
    def visit(h,_):
        result.append(h);return True
    if parent is None:u.EnumWindows(visit,0)
    else:u.EnumChildWindows(parent,visit,0)
    return result
def until(fn,label,seconds=15):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        result=fn()
        if result:return result
        time.sleep(.05)
    raise AssertionError('timeout: '+label)
def root_for(pid):
    for h in windows():
        n=W.DWORD();u.GetWindowThreadProcessId(h,C.byref(n))
        if n.value==pid and text(h,True)=='RA2Toolbox020':return h
def stats_surface(pid):
    for h in windows():
        n=W.DWORD();u.GetWindowThreadProcessId(h,C.byref(n))
        if n.value!=pid or text(h,True)!='RA2UnitTransparentOverlay' or not u.IsWindowVisible(h):continue
        r=W.RECT()
        if u.GetWindowRect(h,C.byref(r)) and r.right-r.left>100 and r.bottom-r.top>100:return h
def child(root,id,parentclass=None):
    return next((h for h in windows(root) if u.GetDlgCtrlID(h)==id and
        (parentclass is None or text(u.GetParent(h),True)==parentclass)),None)
def check(h):return send(h,0xF0)
def ini(folder,section,key):return k.GetPrivateProfileIntW(section,key,999,str(folder/'Assist.ini'))
def launch(folder):
    p=subprocess.Popen([str(folder/'RA2Toolbox.exe')],cwd=folder)
    root=until(lambda:root_for(p.pid),'main window')
    frozen=until(lambda:child(root,605),'visible frozen control')
    danger=until(lambda:child(root,612),'visible dangerous cargo control')
    until(lambda:u.IsWindowEnabled(frozen) and u.IsWindowEnabled(danger),'display owner ready')
    time.sleep(.3)
    return p,root,frozen,danger
def close(p,root):
    u.PostMessageW(root,0x10,0,0)
    p.wait(timeout=20)
    assert p.returncode==0,p.returncode
def click(root,h,expected):
    send(h,0xF5) # BM_CLICK: actual styled button notification path
    until(lambda:check(h)==expected,'checkbox value')
    time.sleep(.6) # cover autosave debounce and asynchronous state publication
    assert check(h)==expected,('value bounced',u.GetDlgCtrlID(h))
def scenario(folder):
    p,root,f,d=launch(folder)
    try:
        checked=[(u.GetDlgCtrlID(h),text(h)) for h in windows(root)
                 if text(h,True).lower()=='button' and u.GetPropW(h,'RA2.Modern.Checked')]
        assert not checked,('fresh defaults must have all switches off',checked)
        for id,expected in {200:1618,209:593,210:599,451:580,9051:32786,9057:67,9058:66}.items():
            h=child(root,id,'RA2Assist010')
            assert h and send(h,0x402)==expected,(id,send(h,0x402) if h else None,expected)
        # Both checkboxes and the remaining shared display checkboxes use the same owner.
        keys={601:'EnemyMarkers',602:'Iron',603:'Capacity',604:'AllTransports',
              605:'Frozen',606:'ParasiteMarker',607:'CrateBuffMarkers',
              611:'PassengerBubbles',612:'DangerPassengers'}
        for id,key in keys.items():
            h=child(root,id)
            click(root,h,1)
            assert ini(folder,'Display',key)==1,(key,'not saved on')
        # Real minimap mode selection persists through the same owner notification.
        mode=child(root,9065,'RA2Assist010');assert mode and send(mode,0x147)==0
        send(mode,0x14E,1);send(u.GetParent(mode),0x111,9065|(1<<16),mode)
        until(lambda:ini(folder,'Display','MinimapMode')==1,'real minimap mode saved')
        for id,key,value in [(9100,'MinimapAlignX',2),(9101,'MinimapAlignY',1)]:
            h=child(root,id,'RA2Assist010');assert h and send(h,0x147)==0
            send(h,0x14E,value);send(u.GetParent(h),0x111,id|(1<<16),h)
            until(lambda:ini(folder,'Display',key)==value,key+' saved')
        scale=child(root,831,'RA2Assist010');assert scale
        set_control_text(scale,'1000')
        until(lambda:ini(folder,'Display','MinimapScale')==1000,'1000 percent minimap saved')
        # Change an unrelated tactical setting: it must not reset these values.
        h=child(root,702,'RA2Assist010');click(root,h,1)
        assert ini(folder,'Display','Frozen')==ini(folder,'Display','DangerPassengers')==1
        close(p,root)
        p,root,f,d=launch(folder)
        for id,key in keys.items():
            h=child(root,id)
            assert check(h)==1,(key,'restart lost saved on')
        assert send(child(root,9065,'RA2Assist010'),0x147)==1,'real minimap mode lost on restart'
        assert ini(folder,'Display','MinimapScale')==1000,'1000 percent minimap lost on restart'
        assert control_text(child(root,831,'RA2Assist010'))=='1000','1000 percent minimap control lost on restart'
        assert send(child(root,9100,'RA2Assist010'),0x147)==2 and send(child(root,9101,'RA2Assist010'),0x147)==1,'minimap alignment lost on restart'
        mode=child(root,9065,'RA2Assist010');send(mode,0x14E,0);send(u.GetParent(mode),0x111,9065|(1<<16),mode)
        # Switch off and exit immediately, exercising pending autosave at shutdown.
        for id in [604]+[id for id in keys if id!=604]:send(child(root,id),0xF5)
        close(p,root)
        p,root,f,d=launch(folder)
        for id,key in keys.items():
            assert check(child(root,id))==0 and ini(folder,'Display',key)==0,(key,'restart lost saved off')
        assert send(child(root,9065,'RA2Assist010'),0x147)==0 and ini(folder,'Display','MinimapMode')==0
        assert ini(folder,'Cooperative','AdvancedEnter')==1,'first-use defaults overwrote existing settings'
        # The shipped statistics renderer must finish on its worker and publish a
        # real layered surface. Keep it active through shutdown to cover ownership.
        demo=child(root,101,'RA2UnitMonitorUI');assert demo
        send(demo,0xF5)
        until(lambda:stats_surface(p.pid),'asynchronous statistics demo surface')
        for _ in range(10):
            send(root,0);send(u.GetParent(demo),0)
            time.sleep(.05)
        assert p.poll() is None,'renderer terminated the host'
        close(p,root)
    except Exception:
        print('UI failure diagnostics:',{f.name:f.read_text(errors='replace')[-4000:] for f in folder.glob('*.log')})
        raise
    finally:
        if p.poll() is None:p.kill();p.wait()

if __name__=='__main__':
    source=pathlib.Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='ra2-settings-') as t:
        folder=pathlib.Path(t)
        for name in ('RA2Toolbox.exe','GameRuntime.dll'):shutil.copy2(source/name,folder/name)
        scenario(folder)
    result={'status':'PASS','version':'1.0.38-pre.3','runs':3,'display_switches':9,
            'checks':['all fresh UI switches off','user hotkeys','cross-pane edits',
                      'real Win32 INI writes','restart on','restart off',
                      'minimap diagram/scene mode and alignment restart','1000 percent minimap save/restart and control readback',
                      'asynchronous statistics demo surface and shutdown','immediate exit','existing settings preserved'],
            'game_tested':False,'multiplayer_tested':False}
    print(json.dumps(result,ensure_ascii=False))
    (source/'WINDOWS_SETTINGS_SMOKE.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
