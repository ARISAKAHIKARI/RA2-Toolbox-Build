// Focused production-store and optional local Chromium diagnostics page fixture.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import os from 'node:os';
import {fileURLToPath} from 'node:url';
import {createRequire} from 'node:module';
import {execFileSync} from 'node:child_process';

const here=path.dirname(fileURLToPath(import.meta.url));
const context=vm.createContext({console,Date,Map,Set,Object,Number,String,Boolean,RangeError});
vm.runInContext(fs.readFileSync(path.join(here,'../frontend/app.js'),'utf8'),context);
const {ConsoleStore,canonical,canonicalDebug,debugAddress,debugStatus,debugKindLabel,debugToggleLabel,filterDebugControls}=context.RA2ConsoleCore;
const tests=[];
function test(name,run){tests.push({name,run});}
function entry(id,patch={},debug={}){return {id,type:'toggle',label:'CanBuild',group:'原生调用',checked:true,enabled:true,visible:true,...patch,debug:{present:true,siteId:0,address:0x4f7870,kind:'call',purpose:'查询建造权限',features:'科技；建造栏',state:'ready',detail:'Ares 入口跳转可调用',hook:'na',ageMs:25,sampled:true,...debug}};}
function setup(controls=[entry(9001)]){
  const messages=[],store=new ConsoleStore(message=>messages.push(message),()=>{},{initialSeq:0});
  const state=(patch={})=>({kind:'state',protocol:1,module:2,generation:3,revision:1,page:'tactical/5',controls,...patch});
  store.navigate(2,5);store.receive(state());return {store,messages,state};
}
const normalize=(...items)=>items.map(canonical);

test('debug navigation is category 5 and preserves independent category caches',()=>{
  const {store,messages}=setup();assert.equal(messages[0].module,2);assert.equal(messages[0].category,5);
  assert.equal(store.getControl(9001).debug.address,0x4f7870);store.navigate(2,4);assert.equal(store.state,null);
  store.navigate(2,5);assert.equal(store.getControl(9001).label,'CanBuild');assert.equal(store.navigate(2,6),false);
});
test('typed optional metadata is immutable and ordinary controls remain ordinary',()=>{
  const c=canonical(entry(9001));assert.ok(Object.isFrozen(c.debug));assert.equal(c.debug.siteId,0);assert.equal(c.debug.sampled,true);
  assert.equal(canonical({id:10,type:'toggle'}).debug,null);assert.equal(canonicalDebug({present:false}),null);assert.equal(debugAddress(0x4f7870),'0x004F7870');
  assert.equal(debugKindLabel('system'),'Windows 消息入口');assert.equal(debugToggleLabel(canonical(entry(90,{}, {kind:'system'}))),'无需签名校验');
  assert.equal(debugToggleLabel(canonical(entry(91,{}, {state:'unused'}))),'本版本未启用');
});
test('unconnected data is unknown even when a retained sample says ready or installed',()=>{
  const c=canonical(entry(9001,{}, {sampled:false,state:'ready',kind:'breakpoint',hook:'installed'}));
  const s=debugStatus(c,1000,1000);assert.equal(s.state,'unknown');assert.equal(s.label,'尚未观测');assert.equal(s.hook,'unknown');assert.equal(s.tone,'neutral');
});
test('call readiness is independent from hook installation',()=>{
  const call=debugStatus(canonical(entry(9001)),1000,1000);assert.equal(call.label,'可调用');assert.equal(call.hookLabel,'无需 Hook');
  const dr=debugStatus(canonical(entry(9002,{}, {kind:'breakpoint',hook:'inactive'})),1000,1000);assert.equal(dr.label,'已就绪');assert.equal(dr.hook,'inactive');assert.equal(dr.state,'ready');
});
test('actual blocked sites and hook conflicts retain separate evidence',()=>{
  const s=debugStatus(canonical(entry(9001,{}, {kind:'patch',hook:'conflict',state:'blocked'})),1000,1000);
  assert.equal(s.state,'blocked');assert.equal(s.hook,'conflict');assert.equal(s.label,'受阻');
});
test('relative age expires quiet observations without relying on host clocks',()=>{
  const c=canonical(entry(9001,{}, {ageMs:250}));assert.equal(debugStatus(c,10000,14750).state,'ready');
  const expired=debugStatus(c,10000,14751);assert.equal(expired.state,'unknown');assert.equal(expired.label,'观测已过期');assert.equal(expired.ageMs,5001);
  assert.equal(debugStatus(c,10000,9999).ageMs,250);assert.equal(debugStatus(c,99999999,99999999).state,'ready');
});
test('switching validation off does not manufacture a ready observation',()=>{
  const c=canonical(entry(9001,{checked:false},{state:'blocked'}));assert.equal(debugStatus(c,1000,1000).state,'blocked');
  const unobserved=canonical(entry(9001,{checked:false},{sampled:false}));assert.equal(debugStatus(unobserved,1000,1000).state,'unknown');
});
test('malformed metadata cannot display unverified ready or installed state',()=>{
  for(const debug of [{address:-1},{ageMs:4294967296},{siteId:'4'},{sampled:'true'}]){
    const c=canonical(entry(9001,{},debug));assert.equal(debugStatus(c,1000,1000).state,'unknown');
  }
  const c=canonical(entry(9001,{}, {state:'anything',hook:'anything'}));assert.equal(c.debug.state,'unknown');assert.equal(c.debug.hook,'unknown');assert.equal(debugStatus(c,1000,1000).label,'状态待确认');
});
test('search includes full addresses, entry labels, purposes and affected features',()=>{
  const controls=normalize(entry(9001),entry(9002,{label:'Other',group:'镜头'},{address:0x40a000,purpose:'移动镜头',features:'镜头定位'}));
  for(const search of ['0x004f7870','4f7870','CanBuild','建造权限','科技'])assert.deepEqual(Array.from(filterDebugControls(controls,{search,receivedAt:1000,now:1000}),c=>c.id),[9001]);
  assert.deepEqual(Array.from(filterDebugControls(controls,{group:'镜头',receivedAt:1000,now:1000}),c=>c.id),[9002]);
});
test('status filters distinguish blocked, unobserved, unused and unchecked entries',()=>{
  const controls=normalize(entry(1),entry(2,{}, {state:'blocked'}),entry(3,{}, {sampled:false}),entry(4,{}, {state:'unused',sampled:false}),entry(5,{checked:false}),entry(6,{visible:false}));
  for(const [state,expected] of [['ready',[1,5]],['blocked',[2]],['unknown',[3]],['unused',[4]],['unchecked',[5]],['attention',[2]]])assert.deepEqual(Array.from(filterDebugControls(controls,{state,receivedAt:1000,now:1000}),c=>c.id),expected);
});
test('individual toggle uses opaque control token and waits for ACK snapshot revision',()=>{
  const {store,messages,state}=setup();assert.equal(store.act(9001,'toggle',false),true);const sent=messages.at(-1);
  assert.equal(sent.id,9001);assert.notEqual(sent.id,0x4f7870);assert.equal(sent.action,'toggle');assert.equal(sent.value,false);assert.equal(sent.generation,3);
  assert.equal(store.getControl(9001).checked,true);assert.equal(store.getControl(9001).debug.state,'ready');
  store.receive({kind:'ack',protocol:1,module:2,generation:3,id:9001,seq:1,ok:true,revision:3});assert.equal(store.isPending(9001),true);
  store.receive(state({revision:2,controls:[entry(9001,{checked:false})]}));assert.equal(store.isPending(9001),true);
  store.receive(state({revision:3,controls:[entry(9001,{checked:false})]}));assert.equal(store.isPending(9001),false);assert.equal(store.getControl(9001).checked,false);
});
test('toggle rejection restores observed setting and displays an explicit error',()=>{
  const {store}=setup();store.act(9001,'toggle',false);store.receive({kind:'ack',protocol:1,module:2,generation:3,id:9001,seq:1,ok:false,error:'当前入口不可修改'});
  assert.equal(store.value(store.getControl(9001),'toggle'),true);assert.equal(store.error(9001),'当前入口不可修改');assert.equal(store.getControl(9001).debug.state,'ready');
});
test('cached previous-generation diagnostics cannot submit or claim current readiness',()=>{
  const {store,state}=setup();store.navigate(2,4);store.receive(state({generation:4,page:'tactical/4',controls:[]}));store.navigate(2,5);
  assert.equal(store.act(9001,'toggle',false),false);assert.equal(debugStatus(store.getControl(9001),store.state.receivedAt,Date.now(),false).label,'等待当前页状态');
});
test('new snapshot owns its local receipt time and refreshes an expired observation',()=>{
  const {store,state}=setup();const old=store.state.receivedAt;assert.equal(debugStatus(store.getControl(9001),old,old+6000).state,'unknown');
  store.receive(state({revision:2,receivedAt:1,controls:[entry(9001,{}, {ageMs:0})]}));assert.notEqual(store.state.receivedAt,1);assert.equal(debugStatus(store.getControl(9001),store.state.receivedAt,store.state.receivedAt).state,'ready');
});
test('same address call and breakpoint retain independent controls and settings',()=>{
  const {store,messages,state}=setup([entry(9001,{}, {siteId:0x014f7870}),entry(9002,{label:'CanBuild DR'}, {siteId:0x034f7870,kind:'breakpoint',hook:'inactive'})]);
  assert.equal(store.act(9002,'toggle',false),true);assert.equal(messages.at(-1).id,9002);
  assert.equal(store.value(store.getControl(9001),'toggle'),true);assert.equal(store.value(store.getControl(9002),'toggle'),false);
  store.receive(state({revision:2,controls:[entry(9001,{}, {siteId:0x014f7870}),entry(9002,{checked:false,label:'CanBuild DR'}, {siteId:0x034f7870,kind:'breakpoint',hook:'inactive'})]}));
  assert.equal(store.getControl(9001).checked,true);assert.equal(store.getControl(9002).checked,false);
});
const catalogIndex=process.argv.indexOf('--catalog');
if(catalogIndex>=0)test('entire production-encoded catalogue is accepted by the real store',()=>{
  const snapshot=JSON.parse(fs.readFileSync(process.argv[catalogIndex+1],'utf8')),store=new ConsoleStore(()=>{},()=>{},{initialSeq:0});
  assert.equal(store.navigate(2,5),true);assert.equal(store.receive(snapshot),true);
  const rows=store.state.controls.filter(c=>c.debug?.present);assert.ok(rows.length>=144);assert.equal(rows.length,snapshot.controls.filter(c=>c.debug?.present).length);
  assert.equal(filterDebugControls(store.state.controls,{receivedAt:store.state.receivedAt,now:store.state.receivedAt}).length,rows.length);
  assert.equal(new Set(rows.map(c=>c.id)).size,rows.length);assert.equal(new Set(rows.map(c=>c.debug.siteId)).size,rows.length);
});
let failed=0;
for(const {name,run}of tests){try{run();console.log(`PASS ${name}`);}catch(error){failed++;console.error(`FAIL ${name}\n${error.stack}`);}}
if(failed)process.exitCode=1;else console.log(`${tests.length} debug console core scenarios passed`);

if(!failed && process.argv.includes('--browser')){
  const require=createRequire(import.meta.url),{chromium}=require(require.resolve('playwright',{paths:[process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES || process.cwd()]}));
  const out=process.env.CONSOLE_DEBUG_OUT || fs.mkdtempSync(path.join(os.tmpdir(),'ra2-debug-console-'));fs.mkdirSync(out,{recursive:true});
  const html=path.join(out,'console.html');execFileSync('python3',[path.join(here,'../../tools/bundle_console.py'),'--output',html]);
  const browser=await chromium.launch({headless:true,args:['--no-sandbox'],...(process.env.CONSOLE_BROWSER_EXECUTABLE?{executablePath:process.env.CONSOLE_BROWSER_EXECUTABLE}:{})});
  const errors=[],page=await browser.newPage({viewport:{width:1280,height:900},deviceScaleFactor:1});
  try{
    page.on('pageerror',error=>errors.push(error.message));
    await page.addInitScript(()=>{const listeners=[];window.__sent=[];window.chrome={webview:{postMessage(m){window.__sent.push(m);},addEventListener(type,fn){if(type==='message')listeners.push(fn);}}};window.__receive=m=>listeners.forEach(fn=>fn({data:m}));});
    await page.goto('file://'+html);await page.waitForFunction(()=>Boolean(window.RA2Console));
    const controls=[entry(9001),entry(9002,{label:'Sidebar.Draw',group:'侧栏'},{siteId:1,address:0x6a6300,kind:'patch',state:'blocked',hook:'inactive',purpose:'请求重绘侧栏',features:'科技建造栏刷新',detail:'入口字节与预期不一致'}),entry(9003,{label:'Tech.Full',group:'硬件断点'},{siteId:2,address:0x4f7870,kind:'breakpoint',hook:'inactive',purpose:'进入 CanBuild 前调整许可参数',detail:'已就绪；功能当前未启用'}),entry(9004,{label:'Control.Site',group:'硬件断点'},{siteId:3,address:0x4fc123,kind:'breakpoint',hook:'installed',purpose:'处理控制功能断点',features:'单位控制',detail:'本组已安装'}),entry(9005,{label:'Camera.Move',group:'镜头'},{siteId:4,address:0x68a001,sampled:false,state:'unknown',purpose:'移动镜头',features:'镜头定位',detail:'未连接游戏，尚无观测'}),entry(9006,{label:'Legacy.Entry',group:'历史入口'},{siteId:5,address:0x6aa600,state:'unused',sampled:false,purpose:'历史侧栏入口',features:'无直接消费者',detail:'当前功能未调用此入口'}),entry(9007,{label:'Rules.Patch',group:'规则',checked:false},{siteId:6,address:0x61a000,kind:'patch',state:'blocked',hook:'conflict',purpose:'应用规则参数',features:'单项游戏规则',detail:'已有其他修改；当前签名校验已关闭'}),entry(9008,{label:'Literal <entry>',group:'原生调用'},{siteId:7,address:0x44a000,purpose:'<img src=x onerror="window.__unsafe=true">',features:'用于验证元数据按纯文本显示'})];
    let revision=1;const snapshot=()=>({kind:'state',protocol:1,module:2,generation:7,revision:revision++,page:'tactical/5',title:'调试',status:'入口观测已刷新',controls});
    const receive=m=>page.evaluate(m=>window.__receive(m),m);
    await receive({kind:'shell',protocol:1,servicesReady:true,selectedCategory:5,version:'debug fixture',modules:[{module:2,ready:true,enabled:true}]});await receive(snapshot());
    await page.waitForFunction(()=>document.querySelectorAll('.debug-row').length===8);
    assert.equal(await page.locator('#pageTitle').textContent(),'调试');assert.equal(await page.locator('.nav-button').count(),6);
    assert.equal(await page.locator('.debug-row[data-control-id="9001"] .debug-badge').first().textContent(),'可调用');
    assert.equal(await page.locator('.debug-row[data-control-id="9003"] .debug-badge').first().textContent(),'已就绪');
    assert.equal(await page.locator('.debug-row[data-control-id="9005"] .debug-badge').first().textContent(),'尚未观测');
    assert.equal(await page.locator('#debugPanel img').count(),0);assert.equal(await page.evaluate(()=>Boolean(window.__unsafe)),false);
    await page.locator('#search').fill('4F7870');await page.waitForFunction(()=>document.querySelectorAll('.debug-row').length===2);
    await page.locator('#search').fill('');await page.locator('#debugGroup').selectOption('硬件断点');await page.waitForFunction(()=>document.querySelectorAll('.debug-row').length===2);
    await page.locator('#debugGroup').selectOption('');await page.locator('#debugFilter').selectOption('unknown');await page.waitForFunction(()=>document.querySelectorAll('.debug-row').length===1);
    await page.locator('#debugFilter').selectOption('');await page.waitForFunction(()=>document.querySelectorAll('.debug-row').length===8);
    const toggle=page.locator('.debug-row[data-control-id="9001"] input');await toggle.focus();await toggle.uncheck();
    const action=await page.evaluate(()=>window.__sent.filter(m=>m.kind==='action').at(-1));assert.equal(action.id,9001);assert.equal(action.value,false);assert.equal(action.generation,7);
    assert.equal(await page.locator('.debug-row[data-control-id="9001"] .debug-toggle-state').textContent(),'等待应用');
    await receive({kind:'ack',protocol:1,module:2,generation:7,id:9001,seq:action.seq,ok:true,revision});controls[0].checked=false;await receive(snapshot());
    await page.waitForFunction(()=>document.querySelector('.debug-row[data-control-id="9001"] .debug-toggle-state').textContent==='校验关闭');
    assert.equal(await page.evaluate(()=>document.activeElement===document.querySelector('.debug-row[data-control-id="9001"] input')),true,'live snapshots preserve focused input');
    for(const viewport of [{width:1280,height:900},{width:1024,height:768},{width:760,height:560}]){
      await page.setViewportSize(viewport);await receive(snapshot());await page.locator('#debugPanel').waitFor();
      const overflow=await page.evaluate(()=>({document:document.documentElement.scrollWidth>innerWidth,content:document.getElementById('contentScroll').scrollWidth>document.getElementById('contentScroll').clientWidth}));
      assert.equal(overflow.document,false);assert.equal(overflow.content,false);await page.screenshot({path:path.join(out,`debug-${viewport.width}.png`)});
    }
    controls[0].debug.ageMs=6000;await receive(snapshot());await page.waitForFunction(()=>document.querySelector('.debug-row[data-control-id="9001"] .debug-badge').textContent==='观测已过期');
    controls[0].debug.ageMs=0;await receive(snapshot());await page.waitForFunction(()=>document.querySelector('.debug-row[data-control-id="9001"] .debug-badge').textContent==='可调用');
    await page.getByRole('button',{name:'控制',exact:true}).click();assert.equal(await page.locator('#debugPanel').isHidden(),true);
    await page.getByRole('button',{name:'调试',exact:true}).click();await receive(snapshot());await page.waitForFunction(()=>document.querySelectorAll('.debug-row').length===8);
    assert.deepEqual(errors,[]);console.log('PASS debug browser: live metadata, independent status, filtering, opaque toggle ACK, focus, expiry, navigation and 3 responsive sizes');
  }finally{await browser.close();}
}
