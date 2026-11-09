#!/usr/bin/env python3
"""Build the production console with an isolated, visible review bridge.

This fixture exercises HTML/CSS/JS only. It never accesses a game process and
does not claim native Windows or real-game validation. Video names, sections,
types and header flags are read from the current production metadata.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    args.out.mkdir(parents=True, exist_ok=True)
    spec = importlib.util.spec_from_file_location("bundle_console", source / "tools/bundle_console.py")
    bundler = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(bundler)
    html = bundler.bundle(source / "suite/frontend")

    display = (source / "suite/source/display_settings.h").read_text(encoding="utf-8-sig")
    video = (source / "suite/source/video_settings_ui.h").read_text(encoding="utf-8-sig")
    names = re.search(r"enum DisplayOption \{([^}]+)\}", display)[1].split(",")[:-1]
    specs = re.findall(r"\{(\d+),(\d+),(\d+),DisplayControl::(\w+)\}", display)
    assert len(names) == len(specs), "production video metadata changed"
    by_name = {name: (index, *values) for index, (name, values) in enumerate(zip(names, specs))}
    sections = dict(re.findall(r'case VideoSection::(\w+):return L"([^"]+)"', video))
    sections["Render"] = "渲染与效果"
    video_controls = {"0": [], "1": []}
    entries = re.findall(r'\{(\w+),VideoGroup::(\w+),L"([^"]*)",L"([^"]*)",VideoSection::(\w+)(,true)?\}', video)
    assert len(entries) >= 40, "review requires the production video entries"
    for name, owner, label, options, section, header in entries:
        index, native_id, minimum, maximum, kind = by_name[name]
        control = dict(id=12000 + index, type={"Check": "toggle", "Number": "number", "Choice": "choice", "Hotkey": "hotkey"}[kind],
                       label=label, group=sections[section], groupHeader=bool(header), collapsible=True,
                       collapsedDefault=True, enabled=name != "RenderUpgrade", visible=True)
        if kind == "Check":
            control["checked"] = name not in {"WatchSecondary", "MapSecondary", "RenderUpgrade"}
        elif kind == "Number":
            value = {"MapScale": 100, "ToastDuration": 3000, "MapSecondaryWidth": 360, "TextMargin": 16}.get(name, int(minimum))
            control.update(text=str(value), min=int(minimum), max=int(maximum))
        elif kind == "Choice":
            control.update(options=options.split("|"), selected=0, readonly=True)
        else:
            control.update(selected=0, options=["press"])
        video_controls["0" if owner == "Render" else "1"].append(control)

    bridge = r"""
(() => {
  const video = __VIDEO_CONTROLS__;
  const control = (id,type,label,group,extra={}) => ({id,type,label,group,visible:true,enabled:true,...extra});
  const feature = (id,label,group=label,extra={}) => control(id,'toggle',label,group,{checked:true,groupHeader:true,collapsible:true,collapsedDefault:true,...extra});
  const detail = (id,type,label,group,extra={}) => control(id,type,label,group,{collapsible:true,collapsedDefault:true,...extra});
  const pages = {
    '0': [...video['0'],
      feature(10,'光标效果','光标效果'), detail(11,'choice','光标样式','光标效果',{options:['原版','琥珀','冰蓝'],selected:0,readonly:true}),
      detail(12,'number','光标大小 / %','光标效果',{text:'100'}),
      feature(13,'SHA / SHP 光标替换（已停用）','资源替换',{checked:false,enabled:false}),
      feature(14,'显示全图 SHA / SHP 替换（已停用）','资源替换',{checked:false,enabled:false}),
      detail(15,'label','从游戏中读取并缓存所需资源。','资源替换'),
      detail(16,'number','高级缩放 / %','高级光标设置',{text:'100'}),
      detail(17,'choice','触发方式','高级光标设置',{options:['按下','持续'],selected:0,readonly:true})],
    '1': video['1'],
    '2/1': [
      control(702,'toggle','高级进驻','键鼠组合操作',{checked:true,row:120,column:20}),
      control(703,'choice','高级进驻','键鼠组合操作',{options:['仅鼠标','Alt','Ctrl','Shift','自定义'],selected:1,readonly:true,row:120,column:240}),
      control(704,'hotkey','高级进驻','键鼠组合操作',{selected:0x2012,options:['hold'],row:120,column:350}),
      control(740,'choice','高级进驻','键鼠组合操作',{options:['停用','中键','侧键 1','侧键 2','右键','左键'],selected:2,readonly:true,row:120,column:510}),
      feature(705,'飞机分配攻击'),detail(750,'number','每个目标分配上限','飞机分配攻击',{text:'2'}),
      detail(751,'choice','目标优先级','飞机分配攻击',{options:['当前目标','剩余生命','距离最近'],selected:0,readonly:true}),
      detail(752,'label','设置已保存 · 进入对局后生效','飞机分配攻击'),
      feature(760,'队形排列与快速占位'),detail(761,'hotkey','队形快捷键','队形排列与快速占位',{selected:0x024b,options:['press','hold']})],
    '2/2': [
      feature(1001,'自动维修'),detail(1002,'number','维修阈值 / %','自动维修',{text:'75'}),
      feature(1011,'矿车自动卸矿'),detail(1012,'number','等待时间 / 毫秒','矿车自动卸矿',{text:'800'}),
      feature(1021,'生产预定'),detail(1022,'number','队列缩放 / %','生产预定',{text:'100'}),
      detail(1023,'text','自定义单位类型','生产预定',{text:'HARV,CMIN'}),
      detail(1024,'label','预定边界固定，队列向上增长。','生产预定'),
      feature(1031,'防空火控'),detail(1032,'choice','目标策略','防空火控',{options:['距离优先','威胁优先'],selected:1,readonly:true})],
    '2/3': [
      feature(2001,'科技'),detail(2002,'number','科技等级','科技',{text:'10'}),
      detail(2003,'choice','阵营限制','科技',{options:['保留原版','当前阵营'],selected:0,readonly:true}),
      feature(2011,'超级武器目标选择'),detail(2012,'hotkey','选择目标快捷键','超级武器目标选择',{selected:0,options:['press']}),
      detail(2013,'label','每项规则独立启用。','超级武器目标选择')],
    '2/4': [
      feature(3001,'计算模式'),detail(3002,'choice','采样方式','计算模式',{options:['自动','只读采样'],selected:0,readonly:true}),
      detail(3003,'number','更新频率 / Hz','计算模式',{text:'30'}),
      feature(3011,'CRC 镜像',{...{}}),detail(3012,'label','等待进入对局后显示运行状态。','CRC 镜像'),
      control(3021,'toggle','开箱结果（已停用）','已停用功能',{checked:false,enabled:false})],
    '2/5': [
      control(4001,'text','进程搜索词','游戏进程连接',{text:'gamemd.exe',row:20,column:20}),
      control(4002,'choice','匹配的游戏进程','游戏进程连接',{options:['gamemd.exe · PID 3210','ra2.exe · PID 1240'],selected:0,readonly:true,presentation:'list',row:50,column:20}),
      control(4003,'button','连接并加入检测名单','游戏进程连接',{actionRole:'primary',row:80,column:20}),
      control(4004,'button','刷新列表','游戏进程连接',{actionRole:'secondary',row:80,column:250}),
      control(4005,'button','移除所选记录','游戏进程连接',{actionRole:'danger',row:80,column:380}),
      control(4101,'text','进程 PID（留空自动识别）','平台 Hook 探测',{text:'',row:110,column:20}),
      control(4102,'button','开始只读扫描','平台 Hook 探测',{actionRole:'primary',row:140,column:20}),
      control(4103,'button','取消扫描','平台 Hook 探测',{actionRole:'secondary',enabled:false,row:140,column:200}),
      control(4104,'button','打开 HTML 报告','平台 Hook 探测',{actionRole:'secondary',enabled:false,row:140,column:330}),
      control(4105,'label','尚未扫描。','平台 Hook 探测',{row:170,column:20}),
      control(4201,'toggle','CanBuild','原生调用',{checked:true,debug:{present:true,siteId:1,address:0x4f7870,kind:'call',purpose:'查询建造权限',features:'科技；建造栏',state:'ready',detail:'受控入口观测',hook:'na',ageMs:0,sampled:true}}),
      control(4202,'toggle','Sidebar.Draw','侧栏',{checked:true,debug:{present:true,siteId:2,address:0x6a6300,kind:'patch',purpose:'请求重绘侧栏',features:'建造栏刷新',state:'blocked',detail:'受控错误示例',hook:'inactive',ageMs:0,sampled:true}})]
  };
  const listeners=[], sent=[];let module=0,category=0,revision=0,withError=false,saveError=false;
  const key=()=>module===2?`2/${category}`:String(module);
  const deliver=m=>listeners.forEach(fn=>fn({data:m}));
  const shell=()=>deliver({kind:'shell',protocol:1,servicesReady:true,selectedCategory:category,videoSection:module===2?0:module,version:'1.0.46 · UI 验证',modules:[0,1,2].map(module=>({module,ready:true,enabled:true})),config:saveError?{failed:true,pending:0,busy:false,canRetry:true,error:'受控保存失败：文件被占用'}:{failed:false,pending:0,busy:false}});
  const snapshot=()=>{
    let controls=pages[key()] || [];
    if(withError) {const name=controls.find(c=>!c.debug)?.group || '运行状态';controls=[...controls,control(49999,'label','运行错误：参数暂不可用，请检查当前连接。',name,{runtimeError:true,collapsible:true,collapsedDefault:true})];}
    deliver({kind:'state',protocol:1,module,generation:1,revision:++revision,page:module===2?`tactical/${category}`:module===0?'cursor':'monitor',title:'受控前端验证',status:'UI 受控验证 · 未连接游戏进程',controls});
  };
  const receive=m=>{
    sent.push(m);
    if(m.kind==='ready'){shell();snapshot();}
    else if(m.kind==='navigate'){module=m.module;category=m.category || 0;snapshot();}
    else if(m.kind==='config-retry'){saveError=false;shell();}
    else if(m.kind==='action'){
      const c=(pages[key()] || []).find(c=>c.id===m.id);if(!c)return;
      if(m.action==='toggle')c.checked=m.value;
      else if(m.action==='text')c.text=m.value;
      else if(['select','hotkey','range','activate'].includes(m.action))c.selected=m.value;
      deliver({kind:'ack',protocol:1,module,generation:1,id:m.id,seq:m.seq,ok:true,revision:revision+1});snapshot();
    }
  };
  window.chrome={webview:{addEventListener(type,fn){if(type==='message')listeners.push(fn);},postMessage(m){queueMicrotask(()=>receive(m));}}};
  window.addEventListener('message',event=>{
    if(event.source!==parent)return;
    if(event.data==='review:runtime-error'){withError=!withError;snapshot();}
    if(event.data==='review:save-error'){saveError=!saveError;shell();}
    if(event.data==='review:refresh')snapshot();
  });
  // Inspectable only in developer-run tests; this does not become a product UI.
  window.__review={sent,pages};
})();
"""
    bridge = bridge.replace("__VIDEO_CONTROLS__", json.dumps(video_controls, ensure_ascii=False))
    # Correct group string stays ordinary metadata, not an object.
    bridge = bridge.replace("feature(3011,'CRC 镜像',{...{}})", "feature(3011,'CRC 镜像')")
    html = html.replace('<script nonce="ra2-console">', '<script nonce="ra2-console">' + bridge + '\n</script>\n<script nonce="ra2-console">', 1)
    (args.out / "console-fixture.html").write_text(html, encoding="utf-8")
    review = """<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>RA2 Toolbox · Production UI review</title>
<style>body{margin:0;background:#090d13;color:#dbe3f0;font:13px system-ui}header{position:sticky;top:0;z-index:2;display:flex;gap:12px;align-items:center;padding:10px 16px;background:#18202d;flex-wrap:wrap}select,button{background:#243145;color:#e5ecf7;border:1px solid #4a5a70;border-radius:5px;padding:6px 10px}span{color:#a8b5c9}main{padding:16px;overflow:auto}iframe{display:block;border:1px solid #334155;width:1240px;height:880px;margin:0 auto}</style>
<header><strong>生产控制台 UI 受控检查</strong><label>文档视口 <select id="viewport"><option value="1240,880">1240 × 880</option><option value="720,780">720 × 780</option><option value="390,720">390 × 720</option></select></label><button id="runtimeError">切换运行错误</button><button id="saveError">切换保存错误</button><button id="refresh">模拟后台刷新</button><span>真实 app.js / app.css；桥接数据仅用于界面检查，未连接游戏。</span></header>
<main><iframe id="console" title="真实生产控制台" src="console-fixture.html"></iframe></main><script>
const frame=document.getElementById('console');document.getElementById('viewport').onchange=e=>{const [w,h]=e.target.value.split(',');frame.style.width=w+'px';frame.style.height=h+'px';};
for(const [id,message] of [['runtimeError','review:runtime-error'],['saveError','review:save-error'],['refresh','review:refresh']])document.getElementById(id).onclick=()=>frame.contentWindow.postMessage(message,'*');
</script></html>"""
    (args.out / "console-review.html").write_text(review, encoding="utf-8")
    files = ["suite/frontend/index.html", "suite/frontend/app.js", "suite/frontend/app.css", "suite/source/video_settings_ui.h", "suite/source/display_settings.h", "suite/tests/console_review_fixture.py"]
    metadata = {"schema": "ra2.console-review-fixture.v1", "game_process_accessed": False, "native_windows_validation": False,
                "video_entries_from_production": len(entries), "source_sha256": {name: hashlib.sha256((source / name).read_bytes()).hexdigest() for name in files},
                "html_sha256": hashlib.sha256(html.encode()).hexdigest()}
    (args.out / "fixture-provenance.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"review": str((args.out / 'console-review.html').resolve()), "video_entries": len(entries)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
