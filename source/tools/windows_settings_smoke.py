"""Run the shipped Windows UI and real INI APIs; no game process is required."""
import ctypes as C
from ctypes import wintypes as W
import json, os, pathlib, re, shutil, subprocess, sys, tempfile, time

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

class MonitorInfo(C.Structure):
    _fields_=[('cbSize',W.DWORD),('rcMonitor',W.RECT),('rcWork',W.RECT),
              ('dwFlags',W.DWORD),('szDevice',W.WCHAR*32)]
class DevMode(C.Structure):
    # DEVMODEW's display/printer union is 16 bytes with DWORD alignment.
    _fields_=[('dmDeviceName',W.WCHAR*32),('dmSpecVersion',W.WORD),
              ('dmDriverVersion',W.WORD),('dmSize',W.WORD),('dmDriverExtra',W.WORD),
              ('dmFields',W.DWORD),('displayUnion',W.DWORD*4),
              ('dmColor',C.c_short),('dmDuplex',C.c_short),('dmYResolution',C.c_short),
              ('dmTTOption',C.c_short),('dmCollate',C.c_short),('dmFormName',W.WCHAR*32),
              ('dmLogPixels',W.WORD),('dmBitsPerPel',W.DWORD),('dmPelsWidth',W.DWORD),
              ('dmPelsHeight',W.DWORD),('dmDisplayFlags',W.DWORD),('dmDisplayFrequency',W.DWORD),
              ('dmICMMethod',W.DWORD),('dmICMIntent',W.DWORD),('dmMediaType',W.DWORD),
              ('dmDitherType',W.DWORD),('dmReserved1',W.DWORD),('dmReserved2',W.DWORD),
              ('dmPanningWidth',W.DWORD),('dmPanningHeight',W.DWORD)]
u.MonitorFromWindow.argtypes=[W.HWND,W.DWORD];u.MonitorFromWindow.restype=W.HANDLE
u.GetMonitorInfoW.argtypes=[W.HANDLE,C.POINTER(MonitorInfo)];u.GetMonitorInfoW.restype=W.BOOL
u.EnumDisplaySettingsExW.argtypes=[W.LPCWSTR,W.DWORD,C.POINTER(DevMode),W.DWORD]
u.EnumDisplaySettingsExW.restype=W.BOOL

def native_monitor_hz(root):
    assert C.sizeof(DevMode)==220,C.sizeof(DevMode)
    monitor=u.MonitorFromWindow(root,2)
    info=MonitorInfo();info.cbSize=C.sizeof(info)
    mode=DevMode();mode.dmSize=C.sizeof(mode)
    if monitor and u.GetMonitorInfoW(monitor,C.byref(info)) and u.EnumDisplaySettingsExW(info.szDevice,0xFFFFFFFF,C.byref(mode),0):
        if mode.dmFields&0x400000 and mode.dmDisplayFrequency>=5:return min(500,mode.dmDisplayFrequency)
    return 60

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
def information_rate(root):
    h=child(root,614,'RA2UnitMonitorUI')
    m=re.fullmatch(r'当前：(\d+) Hz',control_text(h)) if h else None
    return int(m.group(1)) if m else 0
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
    until(lambda:child(root,613,'RA2UnitMonitorUI') and information_rate(root)>0,'information refresh controls ready')
    time.sleep(.3)
    return p,root,frozen,danger
def close(p,root):
    u.PostMessageW(root,0x10,0,0)
    p.wait(timeout=20)
    assert p.returncode==0,p.returncode
def click(root,h,expected,*,owner_id=None,persisted=None,label='checkbox'):
    assert h and u.IsWindowEnabled(h),('control not ready',label,h)
    assert check(h)!=expected,('control already at requested value',label,expected)
    owner=child(root,owner_id,'RA2Assist010') if owner_id is not None else None
    if owner_id is not None:assert owner,('missing setting owner',label,owner_id)
    send(h,0xF5) # BM_CLICK: actual styled button notification path
    until(lambda:check(h)==expected,label+' checkbox value')
    # Shared display edits are posted to another UI thread, then saved by a
    # debounced WM_TIMER. A local checkmark is not an owner/disk acknowledgement.
    # Send exactly one click: a lost edit must fail, never be retried into passing.
    if owner is not None:
        until(lambda:check(owner)==expected,label+' owner accepted edit')
    if persisted is not None:
        until(lambda:persisted()==expected,label+' persisted value')
    assert check(h)==expected,('value bounced',label,u.GetDlgCtrlID(h))
    if owner is not None:assert check(owner)==expected,('owner value bounced',label)

def failure_diagnostics(folder,root):
    def attempt(fn):
        try:return fn()
        except Exception as error:return {'error':repr(error)}
    def control(id,parentclass):
        h=child(root,id,parentclass)
        return {'hwnd':h,'checked':check(h),'enabled':bool(u.IsWindowEnabled(h))} if h else None
    def read_file(path):
        raw=path.read_bytes()
        encoding='utf-16' if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig'
        return raw.decode(encoding,errors='replace')[-4000:]
    details={'EnemyMarkers':{
        'checkbox601':attempt(lambda:control(601,'RA2UnitMonitorUI')),
        'owner785':attempt(lambda:control(785,'RA2Assist010')),
        'ini':attempt(lambda:ini(folder,'Display','EnemyMarkers'))},
        'files':{path.name:attempt(lambda path=path:read_file(path))
                 for pattern in ('*.log','*.ini') for path in folder.glob(pattern)}}
    # Windows runners may use cp1252 stdout. ASCII JSON preserves every code
    # point without allowing diagnostics to replace the original assertion.
    print('UI failure diagnostics:',json.dumps(details,ensure_ascii=True),flush=True)
def scenario(folder):
    p,root,f,d=launch(folder)
    try:
        checked=[(u.GetDlgCtrlID(h),text(h)) for h in windows(root)
                 if text(h,True).lower()=='button' and u.GetPropW(h,'RA2.Modern.Checked')]
        assert not checked,('fresh defaults must have all switches off',checked)
        for id,expected in {200:1618,209:593,210:599,451:580,9051:32786,9057:67,9058:66}.items():
            h=child(root,id,'RA2Assist010')
            assert h and send(h,0x402)==expected,(id,send(h,0x402) if h else None,expected)
        production=child(root,799,'RA2Assist010');dodge=child(root,885,'RA2Assist010')
        assert production and dodge
        for navigation,title,production_shown,dodge_shown in [
                (202,'战术控制',True,False),(203,'战场信息',False,False),
                (204,'实验功能',False,True),(202,'战术控制',True,False)]:
            send(child(root,navigation,'RA2Toolbox020'),0xF5)
            until(lambda:title==text(child(root,989,'RA2Assist010')) and
                  bool(u.IsWindowVisible(production))==production_shown and
                  bool(u.IsWindowVisible(dodge))==dodge_shown,
                  'production/experimental controls in '+title)
        # Both checkboxes and the remaining shared display checkboxes use the same owner.
        keys={601:'EnemyMarkers',602:'Iron',603:'Capacity',604:'AllTransports',
              605:'Frozen',606:'ParasiteMarker',607:'CrateBuffMarkers',
              611:'PassengerBubbles',612:'DangerPassengers'}
        owners={601:785,602:301,603:300,604:303,605:723,606:892,607:893,611:897,612:898}
        for id,key in keys.items():
            h=child(root,id,'RA2UnitMonitorUI')
            click(root,h,1,owner_id=owners[id],
                  persisted=lambda key=key:ini(folder,'Display',key),label=key)
            assert ini(folder,'Display',key)==1,(key,'not saved on')
        secondary=[(9162,'Display','MinimapSecondary'),(9163,'Watch','SecondaryEnabled')]
        for id,section,key in secondary:
            h=child(root,id,'RA2Assist010');assert h and check(h)==0
            click(root,h,1,persisted=lambda section=section,key=key:ini(folder,section,key),label=key)
            assert ini(folder,section,key)==1
        secondary_stats=child(root,120,'RA2UnitMonitorUI');assert secondary_stats and check(secondary_stats)==0
        click(root,secondary_stats,1,persisted=lambda:k.GetPrivateProfileIntW('overlay','secondaryEnabled',999,str(folder/'settings.ini')),label='secondary statistics')
        assert k.GetPrivateProfileIntW('overlay','secondaryEnabled',999,str(folder/'settings.ini'))==1
        assist_layout=[(9164,'Watch','SecondaryX',10000,5000),(9165,'Watch','SecondaryY',0,3000),
                       (9166,'Display','MinimapSecondaryX',10000,2000),(9167,'Display','MinimapSecondaryY',10000,8000),
                       (9168,'Display','MinimapSecondaryWidth',320,960)]
        stats_layout=[(121,'secondaryX',0,10000),(122,'secondaryY',0,10000)]
        for id,section,key,initial,adjusted in assist_layout:
            h=child(root,id,'RA2Assist010');assert h and control_text(h)==str(initial),(id,'secondary layout default')
            set_control_text(h,str(adjusted))
            until(lambda section=section,key=key,adjusted=adjusted:ini(folder,section,key)==adjusted,key+' saved')
        for id,key,initial,adjusted in stats_layout:
            h=child(root,id,'RA2UnitMonitorUI');assert h and control_text(h)==str(initial),(id,'secondary statistics layout default')
            set_control_text(h,str(adjusted))
            until(lambda key=key,adjusted=adjusted:k.GetPrivateProfileIntW('overlay',key,999,str(folder/'settings.ini'))==adjusted,key+' saved')
        assert ini(folder,'Display','MinimapZoom')==0 and ini(folder,'Watch','Enabled')==0,'secondary enabled primary overlay'
        manual=child(root,609,'RA2UnitMonitorUI');adaptive=child(root,613,'RA2UnitMonitorUI')
        assert manual and adaptive and u.IsWindowEnabled(manual) and check(adaptive)==0
        set_control_text(manual,'144')
        until(lambda:ini(folder,'Display','FPS')==144 and information_rate(root)==144,'manual information rate saved and active')
        expected_monitor_hz=native_monitor_hz(root)
        click(root,adaptive,1,owner_id=9160,persisted=lambda:ini(folder,'Display','AdaptiveFPS'),label='adaptive information rate')
        until(lambda:ini(folder,'Display','AdaptiveFPS')==1 and not u.IsWindowEnabled(manual) and information_rate(root)==expected_monitor_hz,'adaptive information rate matches native monitor')
        assert ini(folder,'Display','FPS')==144 and control_text(manual)=='144','adaptive mode overwrote manual target'
        # Real minimap mode selection persists through the same owner notification.
        mode=child(root,9065,'RA2Assist010');assert mode and send(mode,0x147)==0
        send(mode,0x14E,1);send(u.GetParent(mode),0x111,9065|(1<<16),mode)
        until(lambda:ini(folder,'Display','MinimapMode')==1,'real minimap mode saved')
        for id,key,value in [(9100,'MinimapAlignX',2),(9101,'MinimapAlignY',1)]:
            h=child(root,id,'RA2Assist010');assert h and send(h,0x147)==0
            send(h,0x14E,value);send(u.GetParent(h),0x111,id|(1<<16),h)
            until(lambda:ini(folder,'Display',key)==value,key+' saved')
        scale=child(root,831,'RA2Assist010');assert scale
        set_control_text(scale,'10')
        until(lambda:ini(folder,'Display','MinimapScale')==10,'10 percent minimap saved')
        # Change an unrelated tactical setting: it must not reset these values.
        h=child(root,702,'RA2Assist010');click(root,h,1,persisted=lambda:ini(folder,'Cooperative','AdvancedEnter'),label='advanced enter')
        assert ini(folder,'Display','Frozen')==ini(folder,'Display','DangerPassengers')==1
        assert ini(folder,'Display','AdaptiveFPS')==1 and ini(folder,'Display','FPS')==144
        close(p,root)
        p,root,f,d=launch(folder)
        for id,key in keys.items():
            h=child(root,id)
            assert check(h)==1,(key,'restart lost saved on')
        manual=child(root,609,'RA2UnitMonitorUI');adaptive=child(root,613,'RA2UnitMonitorUI')
        for id,section,key in secondary:
            assert check(child(root,id,'RA2Assist010'))==1 and ini(folder,section,key)==1
        assert check(child(root,120,'RA2UnitMonitorUI'))==1 and k.GetPrivateProfileIntW('overlay','secondaryEnabled',999,str(folder/'settings.ini'))==1
        for id,section,key,_,adjusted in assist_layout:
            assert control_text(child(root,id,'RA2Assist010'))==str(adjusted) and ini(folder,section,key)==adjusted,(key,'secondary layout restart')
        for id,key,_,adjusted in stats_layout:
            assert control_text(child(root,id,'RA2UnitMonitorUI'))==str(adjusted) and k.GetPrivateProfileIntW('overlay',key,999,str(folder/'settings.ini'))==adjusted,(key,'secondary statistics layout restart')
        assert check(adaptive)==1 and not u.IsWindowEnabled(manual) and control_text(manual)=='144','adaptive state lost on restart'
        until(lambda:information_rate(root)==native_monitor_hz(root),'adaptive monitor rate restored after restart')
        click(root,adaptive,0,owner_id=9160,persisted=lambda:ini(folder,'Display','AdaptiveFPS'),label='adaptive information rate')
        until(lambda:ini(folder,'Display','AdaptiveFPS')==0 and u.IsWindowEnabled(manual) and information_rate(root)==144,'manual target restored after disabling adaptive')
        assert send(child(root,9065,'RA2Assist010'),0x147)==1,'real minimap mode lost on restart'
        assert ini(folder,'Display','MinimapScale')==10,'10 percent minimap lost on restart'
        assert control_text(child(root,831,'RA2Assist010'))=='10','10 percent minimap control lost on restart'
        assert send(child(root,9100,'RA2Assist010'),0x147)==2 and send(child(root,9101,'RA2Assist010'),0x147)==1,'minimap alignment lost on restart'
        mode=child(root,9065,'RA2Assist010');send(mode,0x14E,0);send(u.GetParent(mode),0x111,9065|(1<<16),mode)
        # Switch off and exit immediately, exercising pending autosave at shutdown.
        set_control_text(manual,'240')
        for id,section,key,initial,_ in assist_layout:set_control_text(child(root,id,'RA2Assist010'),str(initial))
        for id,key,initial,_ in stats_layout:set_control_text(child(root,id,'RA2UnitMonitorUI'),str(initial))
        for id in [604]+[id for id in keys if id!=604]:send(child(root,id),0xF5)
        for id,_,_ in secondary:send(child(root,id,'RA2Assist010'),0xF5)
        send(child(root,120,'RA2UnitMonitorUI'),0xF5)
        close(p,root)
        p,root,f,d=launch(folder)
        for id,key in keys.items():
            assert check(child(root,id))==0 and ini(folder,'Display',key)==0,(key,'restart lost saved off')
        manual=child(root,609,'RA2UnitMonitorUI');adaptive=child(root,613,'RA2UnitMonitorUI')
        for id,section,key in secondary:
            assert check(child(root,id,'RA2Assist010'))==0 and ini(folder,section,key)==0
        assert check(child(root,120,'RA2UnitMonitorUI'))==0 and k.GetPrivateProfileIntW('overlay','secondaryEnabled',999,str(folder/'settings.ini'))==0
        for id,section,key,initial,_ in assist_layout:
            assert control_text(child(root,id,'RA2Assist010'))==str(initial) and ini(folder,section,key)==initial,(key,'secondary layout reset')
        for id,key,initial,_ in stats_layout:
            assert control_text(child(root,id,'RA2UnitMonitorUI'))==str(initial) and k.GetPrivateProfileIntW('overlay',key,999,str(folder/'settings.ini'))==initial,(key,'secondary statistics layout reset')
        assert check(adaptive)==0 and u.IsWindowEnabled(manual) and control_text(manual)=='240'
        assert ini(folder,'Display','AdaptiveFPS')==0 and ini(folder,'Display','FPS')==240
        until(lambda:information_rate(root)==240,'manual information rate restored after restart')
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
        try:failure_diagnostics(folder,root)
        except Exception as error:print('UI diagnostics failed:',ascii(error),flush=True)
        raise
    finally:
        if p.poll() is None:p.kill();p.wait()

if __name__=='__main__':
    source=pathlib.Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='ra2-settings-') as t:
        folder=pathlib.Path(t)
        for name in ('RA2Toolbox.exe','GameRuntime.dll'):shutil.copy2(source/name,folder/name)
        scenario(folder)
    result={'status':'PASS','version':'1.0.38-pre.11','runs':3,'display_switches':13,
            'checks':['all fresh UI switches off','user hotkeys','cross-pane edits',
                      'real Win32 INI writes','restart on','restart off','independent secondary map/statistics/watch toggles and restart',
                      'real pane navigation for production and experimental controls',
                      'independent secondary placement and map width persistence across restarts',
                      'information adaptive rate matches native monitor','adaptive mode retains manual Hz and disables its editor',
                      'adaptive on/off persistence and manual target restoration',
                      'minimap diagram/scene mode and alignment restart','10 percent minimap save/restart and control readback',
                      'asynchronous statistics demo surface and shutdown','immediate exit','existing settings preserved'],
            'game_tested':False,'multiplayer_tested':False}
    print(json.dumps(result,ensure_ascii=False))
    (source/'WINDOWS_SETTINGS_SMOKE.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')

