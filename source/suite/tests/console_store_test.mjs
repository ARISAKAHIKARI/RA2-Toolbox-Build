import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';
import os from 'node:os';
import { createRequire } from 'node:module';
import { execFileSync } from 'node:child_process';

const here = path.dirname(fileURLToPath(import.meta.url));
const script = fs.readFileSync(path.join(here, '../frontend/app.js'), 'utf8');
const context = vm.createContext({ console, Date, Map, Set, Object, Number, String, Boolean, RangeError });
vm.runInContext(script, context);
const { ConsoleStore, groupControls, encodeHotkey, keyName, canonical, validText } = context.RA2ConsoleCore;
const tests = [];
function test(name, run) { tests.push({ name, run }); }
function setup(controls = []) {
  const messages = [], changes = [];
  const store = new ConsoleStore(message => messages.push(message), reason => changes.push(reason), { initialSeq: 0 });
  const state = (patch = {}) => ({kind:'state',protocol:1,module:0,generation:1,revision:1,page:'cursor',title:'光标与渲染',status:'已连接',controls,...patch});
  store.receive(state());
  return {store,messages,changes,state};
}
const textControl = {id:1,type:'text',label:'自定义类型',text:'HARV',enabled:true,visible:true};

test('ready does not overwrite saved navigation before services initialize', () => {
  const messages = [], store = new ConsoleStore(message => messages.push(message), () => {}, {initialSeq:0});
  store.ready(); assert.equal(messages.length,1); assert.equal(messages[0].kind,'ready');
  assert.equal(messages[0].protocol,1); assert.equal(store.navigate(2,3),true); assert.equal(messages[1].category,3);
});
test('reject malformed state without changing generation, drafts, or current state', () => {
  const {store,state} = setup([textControl]); store.beginEdit(1); store.draft(1,'CMIN');
  assert.equal(store.receive(state({generation:2,revision:2,controls:[textControl,textControl]})),false);
  assert.equal(store.state.generation,1); assert.equal(store.value(store.getControl(1),'text'),'CMIN');
  assert.equal(store.receive(state({generation:2,controls:[{id:0,type:'text'}]})),false);
  assert.equal(store.receive(state({protocol:8})),false);
  assert.equal(store.receive(state({module:3})),false);
  assert.equal(store.receive(state({generation:NaN})),false);
});
test('new model cannot overwrite the focused editing draft', () => {
  const {store,state,messages} = setup([textControl]);
  store.beginEdit(1); store.draft(1,'HARV,CMIN');
  store.receive(state({revision:2,controls:[{...textControl,text:'OTHER'}]}));
  assert.equal(store.value(store.getControl(1),'text'),'HARV,CMIN');
  assert.equal(store.commit(1),true); assert.equal(messages.at(-1).value,'HARV,CMIN');
  assert.equal(store.value(store.getControl(1),'text'),'HARV,CMIN');
});
test('only the newest acknowledgement can resolve a pending edit', () => {
  const {store,state} = setup([textControl]);
  assert.equal(store.act(1,'text','A'),true); assert.equal(store.act(1,'text','B'),true);
  assert.equal(store.receive({kind:'ack',protocol:1,module:0,generation:1,id:1,seq:1,ok:false,error:'old'}),false);
  assert.equal(store.value(store.getControl(1),'text'),'B');
  store.receive(state({revision:2,controls:[{...textControl,text:'A'}]}));
  assert.equal(store.isPending(1),true);
  assert.equal(store.receive({kind:'ack',protocol:1,module:0,generation:1,id:1,seq:2,ok:true}),true);
  assert.equal(store.isPending(1),false,'successful ack resolves a later authoritative observation');
  store.receive(state({revision:3,controls:[{...textControl,text:'B'}]}));
  assert.equal(store.isPending(1),false);
});
test('backend rejection shows an error and returns to observed state', () => {
  const {store} = setup([textControl]); store.act(1,'text','invalid');
  assert.equal(store.receive({kind:'ack',protocol:1,module:0,generation:1,id:1,seq:1,ok:'yes'}),false);
  assert.equal(store.receive({kind:'ack',protocol:1,module:0,generation:1,id:1,seq:1,ok:false,error:'配置无效'}),true);
  assert.equal(store.error(1),'配置无效'); assert.equal(store.value(store.getControl(1),'text'),'HARV');
});
test('generation rollover rejects stale state and clears stale actions', () => {
  const {store,state} = setup([textControl]); store.act(1,'text','A'); store.beginEdit(1); store.draft(1,'draft');
  assert.equal(store.receive(state({generation:2,revision:1,controls:[{...textControl,text:'new'}]})),true);
  assert.equal(store.pending.size,0); assert.equal(store.drafts.size,1,'stable text-token drafts survive unrelated structural changes');
  assert.equal(store.value(store.getControl(1),'text'),'draft'); assert.ok(store.notice);
  assert.equal(store.receive(state({generation:1,revision:999})),false);
  assert.equal(store.receive(state({generation:2,revision:1})),false);
  assert.equal(store.getControl(1).text,'new');
});
test('each tactical category has an independent cached state', () => {
  const {store,state} = setup([]); store.navigate(2,1);
  store.receive(state({module:2,page:'tactical/1',controls:[{...textControl,label:'控制'}]}));
  store.navigate(2,4); assert.equal(store.state,null);
  store.receive(state({module:2,page:'tactical/4',revision:2,controls:[{...textControl,label:'实验性功能'}]}));
  store.navigate(2,1); assert.equal(store.getControl(1).label,'控制');
  assert.equal(store.receive(state({module:2,page:'tactical/9',revision:3})),false);
});
test('all canonical setting types retain their action semantics', () => {
  const controls = [
    {id:1,type:'toggle',label:'开关',checked:false},
    {id:2,type:'choice',label:'模式',selected:0,options:['一','二'],readonly:true},
    {id:3,type:'choice',label:'自定义缩放',selected:0,options:['100%','125%'],text:'100%',presentation:'editable',readonly:false},
    {id:4,type:'choice',label:'可生产单位',selected:0,options:['步兵','坦克'],presentation:'list',readonly:true},
    {id:5,type:'number',label:'采样频率',text:'240'},
    {id:6,type:'range',label:'透明度',selected:1,text:'0',min:0,max:100},
    {id:7,type:'button',label:'重新连接'},
    {id:8,type:'label',label:'状态'},
    {id:9,type:'heading',label:'分组'},
    {id:10,type:'hotkey',label:'快捷键',selected:0x8341,options:['press','release']}
  ];
  const {store,messages} = setup(controls);
  assert.equal(store.act(1,'toggle',true),true);
  assert.equal(store.act(2,'select',1),true);
  assert.equal(store.act(2,'text','invalid'),false,'readonly choice still permits selection, not arbitrary text');
  assert.equal(store.act(3,'text','130%'),true);
  assert.equal(store.act(4,'activate',1),true);
  assert.equal(store.act(5,'text','500'),true);
  assert.equal(store.value(store.getControl(6),'range'),0,'zero must not fall back to selected index');
  assert.equal(store.act(6,'range',50),true);
  assert.equal(store.act(7,'press'),true);
  assert.equal(Object.hasOwn(messages.at(-1),'value'),false);
  assert.equal(store.act(8,'press'),false);
  assert.equal(store.act(10,'hotkey',0xb34b),true);
  assert.equal(store.act(2,'select',50),false); assert.equal(store.act(10,'hotkey',65536),false);
  assert.equal(store.act(1,'toggle',1),false); assert.equal(store.act(5,'text',123),false);
  assert.equal(messages.length,8);
});
test('disabled, invisible and closing controls cannot send actions', () => {
  const {store,messages} = setup([{...textControl,enabled:false},{...textControl,id:2,visible:false}]);
  assert.equal(store.act(1,'text','A'),false); assert.equal(store.act(2,'text','A'),false);
  assert.equal(store.enable(2,true),false); assert.equal(store.enable(0,true),true);
  store.receive({kind:'shell',protocol:1,closing:true,modules:[]});
  assert.equal(store.act(1,'text','A'),false); assert.equal(messages.length,1);
});
test('buttons resolve on ack without requiring an artificial state change', () => {
  const {store,state} = setup([{id:1,type:'button',label:'执行'}]); store.act(1,'press');
  store.receive(state({revision:2}));
  assert.equal(store.isPending(1),true,'unrelated state updates cannot acknowledge a button side effect'); store.receive({kind:'ack',protocol:1,module:0,generation:1,id:1,seq:1,ok:true});
  assert.equal(store.isPending(1),false);
});
test('unsupported controls remain visible as readonly explanatory entries', () => {
  const result = canonical({id:1,type:'native-unknown',label:'保留设置'});
  assert.equal(result.type,'label'); assert.equal(result.readonly,true); assert.ok(result.hint);
});
test('group filtering includes labels, hints, options and respects native visibility', () => {
  const controls = [{id:1,type:'heading',label:'画面'},canonical({id:2,type:'toggle',label:'渲染',hint:'GPU',checked:true}),canonical({id:3,type:'choice',group:'光标',label:'样式',options:['琥珀','冰蓝']}),canonical({id:4,type:'text',label:'隐藏',visible:false})];
  assert.equal(groupControls(controls).size,2);
  assert.equal(groupControls(controls,'gpu').get('画面')[0].id,2);
  assert.equal(groupControls(controls,'冰蓝').get('光标')[0].id,3);
  assert.equal(groupControls(controls,'隐藏').size,0);
});
test('gesture capture preserves release/hold/double and exact native modifier bits', () => {
  const event = {code:'KeyK',key:'k',ctrlKey:true,shiftKey:true,altKey:false,metaKey:false};
  assert.equal(encodeHotkey(event,0xb04a),0xb34b);
  assert.equal(encodeHotkey({...event,shiftKey:false},0x2041),0x224b);
  assert.equal(encodeHotkey({code:'ControlLeft',key:'Control',ctrlKey:true,metaKey:false},0x8000),0x8011);
  assert.equal(encodeHotkey({...event,metaKey:true},0),null);
  assert.equal(encodeHotkey({code:'KeyQ',key:'a',keyCode:65},0),65,'logical Windows VK follows the active layout');
  assert.equal(keyName(0xb34b),'Ctrl + Shift + K'); assert.equal(keyName(0),'未绑定');
});
test('default action sequence survives WebView reload time scale and never sends unsafe integers', () => {
  const store = new ConsoleStore(); assert.ok(store.seq > 1000000000000); assert.ok(Number.isSafeInteger(store.seq));
  assert.throws(() => new ConsoleStore(()=>{},()=>{},{initialSeq:Number.MAX_SAFE_INTEGER}),RangeError);
});
test('large immutable state revisions keep drafts and resolve only the changed setting', () => {
  const controls = Array.from({length:3000},(_,index) => ({id:index+1,type:'text',label:`设置 ${index+1}`,text:String(index),group:`分组 ${Math.floor(index/100)}`}));
  const {store,state} = setup(controls); store.beginEdit(1500); store.draft(1500,'尚未提交'); store.act(2800,'text','new');
  store.receive(state({revision:2,controls:controls.map(control => control.id===2800 ? {...control,text:'new'} : control)}));
  assert.equal(store.pending.size,0); assert.equal(store.value(store.getControl(1500),'text'),'尚未提交'); assert.equal(groupControls(store.state.controls).size,30);
});
test('UTF8 send limits match native decoded bytes and reject invalid string boundaries',()=>{
  assert.equal(validText('中'.repeat(5461)),true);assert.equal(validText('中'.repeat(5462)),false);
  assert.equal(validText('😀'.repeat(4096)),true);assert.equal(validText('😀'.repeat(4097)),false);
  assert.equal(validText('x'.repeat(16384)),true);assert.equal(validText('x'.repeat(16385)),false);
  assert.equal(validText('\ud800'),false);assert.equal(validText('\udc00'),false);assert.equal(validText('a\u0000b'),false);
  const {store,messages}=setup([textControl]);store.beginEdit(1);store.draft(1,'中'.repeat(5462));
  assert.equal(store.commit(1),false);assert.equal(messages.length,0);assert.equal(store.drafts.size,1);assert.ok(store.error(1));
  store.cancelEdit(1);assert.equal(store.drafts.size,0);assert.equal(store.error(1),'');
});
test('old-generation acknowledgements cannot settle a new-generation action',()=>{
  const {store,state}=setup([textControl]);store.act(1,'text','A');
  store.receive(state({generation:2,revision:1}));store.seq=0;store.act(1,'text','B');
  assert.equal(store.receive({kind:'ack',protocol:1,module:0,generation:1,id:1,seq:1,ok:true}),false);
  assert.equal(store.isPending(1),true);
  store.receive({kind:'ack',protocol:1,module:0,generation:2,id:1,seq:1,ok:true});
  store.receive(state({generation:2,revision:2,controls:[{...textControl,text:'B'}]}));assert.equal(store.isPending(1),false);
});
test('native normalization converges with snapshots before or after a successful ack',()=>{
  for(const before of [false,true]) {
    const {store,state}=setup([{...textControl,type:'number',text:'240'}]);store.act(1,'text','999');
    if(before)store.receive(state({revision:2,controls:[{...textControl,type:'number',text:'500'}]}));
    store.receive({kind:'ack',protocol:1,module:0,generation:1,id:1,seq:1,ok:true});
    if(!before)store.receive(state({revision:2,controls:[{...textControl,type:'number',text:'500'}]}));
    assert.equal(store.isPending(1),false);assert.equal(store.value(store.getControl(1),'text'),'500');
  }
});
test('an accepted rapid round-trip edit resolves even when native state deduplicates to the original',()=>{
  const {store}=setup([{id:1,type:'toggle',checked:true}]);store.act(1,'toggle',false);store.act(1,'toggle',true);
  store.receive({kind:'ack',protocol:1,module:0,generation:1,id:1,seq:2,ok:true});
  assert.equal(store.pending.size,0);assert.equal(store.value(store.getControl(1),'toggle'),true);
});
test('removed choice tokens clear only their records and preserve unrelated stable drafts',()=>{
  const choice={id:2,type:'choice',selected:0,options:['A','B'],readonly:true};
  const {store,state}=setup([textControl,choice]);store.beginEdit(1);store.draft(1,'本地草稿');store.act(2,'select',1);
  store.receive(state({revision:2,controls:[textControl,{...choice,id:3,options:['C','D']}]}));
  assert.equal(store.pending.size,0);assert.equal(store.value(store.getControl(1),'text'),'本地草稿');assert.ok(store.notice);
  assert.equal(store.act(2,'select',0),false);
});
test('cached old-generation pages remain readable but cannot submit stale actions',()=>{
  const {store,state}=setup([]);store.navigate(2,1);store.receive(state({module:2,page:'tactical/1',controls:[textControl]}));
  store.beginEdit(1);store.draft(1,'页一草稿');store.navigate(2,2);
  store.receive(state({module:2,generation:2,page:'tactical/2',controls:[{...textControl,id:2}]}));
  store.navigate(2,1);assert.equal(store.act(1,'text','stale'),false);assert.equal(store.value(store.getControl(1),'text'),'页一草稿');
  store.receive(state({module:2,generation:3,page:'tactical/1',controls:[textControl]}));assert.equal(store.commit(1),true);
});
test('ACK revision threshold excludes delayed snapshots and converges on native normalized value',()=>{
  const {store,state}=setup([{...textControl,type:'number',text:'240'}]);store.act(1,'text','999');
  store.receive({kind:'ack',protocol:1,module:0,generation:1,id:1,seq:1,ok:true,revision:3});
  store.receive(state({revision:2,controls:[{...textControl,type:'number',text:'300'}]}));
  assert.equal(store.isPending(1),true);assert.equal(store.value(store.getControl(1),'text'),'999');
  store.receive(state({revision:3,controls:[{...textControl,type:'number',text:'500'}]}));
  assert.equal(store.isPending(1),false);assert.equal(store.value(store.getControl(1),'text'),'500');
});
test('authoritative normalized state before its ACK revision settles without another snapshot',()=>{
  const {store,state}=setup([{...textControl,type:'number',text:'240'}]);store.act(1,'text','999');
  store.receive(state({revision:3,controls:[{...textControl,type:'number',text:'500'}]}));
  assert.equal(store.isPending(1),true);
  store.receive({kind:'ack',protocol:1,module:0,generation:1,id:1,seq:1,ok:true,revision:3});
  assert.equal(store.isPending(1),false);assert.equal(store.value(store.getControl(1),'text'),'500');
});
test('serialized JSON size is bounded separately from decoded text size',()=>{
  const {store,messages}=setup([textControl]);
  assert.equal(validText('\u0001'.repeat(16384)),true);
  assert.equal(store.act(1,'text','\u0001'.repeat(16384)),false);
  assert.equal(messages.length,0);assert.equal(store.pending.size,0);
  assert.equal(store.act(1,'range',2147483648),false);
});

let failed=0;
for (const {name,run} of tests) { try { run(); console.log(`PASS ${name}`); } catch(error) { failed++; console.error(`FAIL ${name}\n${error.stack}`); } }
if (failed) process.exitCode=1; else console.log(`${tests.length} console model tests passed`);

if (!failed && process.argv.includes('--browser')) {
  const require = createRequire(import.meta.url);
  const modulePath = require.resolve('playwright',{paths:[process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES || process.cwd()]});
  const {chromium} = require(modulePath);
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(),'ra2-console-'));
  const html = path.join(temporary,'console.html');
  execFileSync('python3',[path.join(here,'../../tools/bundle_console.py'),'--output',html]);
  const browser = await chromium.launch({headless:true,args:['--no-sandbox']});
  try {
    const page = await browser.newPage({viewport:{width:1240,height:880},deviceScaleFactor:1});
    const errors=[]; page.on('pageerror',error=>errors.push(error.message));
    await page.addInitScript(() => {
      const listeners=[];window.__sent=[];
      window.chrome={webview:{postMessage(message){window.__sent.push(message);},addEventListener(type,listener){if(type==='message')listeners.push(listener);}}};
      window.__receive=message=>listeners.forEach(listener=>listener({data:message}));
    });
    await page.goto('file://'+html); await page.waitForFunction(()=>Boolean(window.RA2Console));
    assert.equal(await page.evaluate(()=>window.__sent.length),1,'initial ready must not navigate before saved preferences arrive');
    const controls=[
      {id:100,type:'heading',label:'画面与性能'},
      {id:1,type:'toggle',label:'渲染升级',hint:'使用独立画面输出，保留原有游戏节奏。',checked:true},
      {id:2,type:'choice',label:'绘制方式',hint:'按设备与画面负载选择输出方式。',selected:0,options:['GPU 优先','CPU 绘制'],readonly:true},
      {id:3,type:'choice',label:'目标刷新率',hint:'选择预设，或输入自定义刷新率。',text:'170',selected:2,options:['60','120','170','240','500'],readonly:false,presentation:'editable'},
      {id:4,type:'number',label:'内存预算（MB）',hint:'限制渲染缓存的最大内存使用量。',text:'8192'},
      {id:5,type:'range',label:'覆盖层透明度',text:'80',selected:80,min:0,max:100,step:1},
      {id:6,type:'button',label:'显示设备',hint:'重新读取可用设备与刷新率。',text:'重新检测'},
      {id:101,type:'heading',label:'操作与资源'},
      {id:7,type:'text',label:'游戏目录',text:'C:\\Games\\Red Alert 2',hint:'素材与地图将从此目录读取。'},
      {id:8,type:'choice',label:'常驻单位类型',selected:0,options:['步兵','坦克','矿车','基地车'],presentation:'list',readonly:true,hint:'双击列表项可执行原有对应操作。'},
      {id:9,type:'hotkey',label:'队形预览快捷键',selected:0xb34a,options:['hold'],hint:'支持 Ctrl、Shift、Alt 与双击按住。'},
      {id:10,type:'label',label:'设置会自动保存；每项功能独立启用。'},
      {id:11,type:'toggle',label:'未连接时的功能',checked:false,enabled:false,hint:'连接游戏后可用。'}
    ];
    let revision=1;
    const snapshot=(patch={})=>({kind:'state',protocol:1,module:0,generation:1,revision:revision++,page:'cursor',title:'光标与渲染',status:'已连接 · 170 Hz',controls,...patch});
    const receive=async message=>page.evaluate(message=>window.__receive(message),message);
    await receive({kind:'shell',protocol:1,version:'1.0.41-pre.1',selectedCategory:0,videoSection:0,servicesReady:true,modules:[{module:0,ready:true,enabled:true},{module:1,ready:true,enabled:false}],config:{pending:0,busy:false,failed:false}});
    await receive(snapshot()); await page.getByLabel('渲染升级',{exact:true}).waitFor();
    await page.screenshot({path:process.env.CONSOLE_SCREENSHOT || path.join(temporary,'console-desktop.png'),fullPage:false});
    assert.equal(await page.getByRole('heading',{name:'视频',exact:true}).count(),1);
    assert.equal(await page.getByLabel('未连接时的功能',{exact:true}).isDisabled(),true);
    await page.getByLabel('渲染升级',{exact:true}).click();
    assert.equal(await page.evaluate(()=>window.__sent.at(-1).action),'toggle');
    assert.equal(await page.evaluate(()=>window.__sent.at(-1).value),false);
    let action=await page.evaluate(()=>window.__sent.at(-1));
    await receive({kind:'ack',protocol:1,module:0,generation:1,id:1,seq:action.seq,ok:true});
    await receive(snapshot({controls:controls.map(control=>control.id===1?{...control,checked:false}:control)}));
    await page.getByLabel('绘制方式',{exact:true}).selectOption('1');
    assert.equal(await page.evaluate(()=>window.__sent.at(-1).action),'select');
    await page.getByLabel('目标刷新率（自定义值）',{exact:true}).fill('333');
    await page.getByLabel('目标刷新率（自定义值）',{exact:true}).press('Enter');
    assert.equal(await page.evaluate(()=>window.__sent.at(-1).value),'333');
    await page.getByLabel('内存预算（MB）',{exact:true}).fill('4096');
    await page.getByLabel('内存预算（MB）',{exact:true}).press('Enter');
    assert.equal(await page.evaluate(()=>window.__sent.at(-1).value),'4096');
    await page.getByLabel('覆盖层透明度',{exact:true}).evaluate(input=>{input.value='33';input.dispatchEvent(new Event('input',{bubbles:true}));input.dispatchEvent(new Event('change',{bubbles:true}));});
    assert.equal(await page.evaluate(()=>window.__sent.at(-1).value),33);
    await page.getByRole('button',{name:'显示设备',exact:true}).click();
    assert.equal(await page.evaluate(()=>window.__sent.at(-1).action),'press');
    await page.locator('#contentScroll').evaluate(element=>{element.scrollTop=700;});
    const directory=page.getByLabel('游戏目录',{exact:true}); await directory.waitFor();
    await directory.fill('C:\\RA2');
    await receive(snapshot({controls:controls.map(control=>control.id===7?{...control,text:'EXTERNAL'}:control)}));
    assert.equal(await directory.inputValue(),'C:\\RA2','native revision must not reset focused draft');
    await directory.press('Enter'); assert.equal(await page.evaluate(()=>window.__sent.at(-1).value),'C:\\RA2');
    const list=page.getByLabel('常驻单位类型',{exact:true}); await list.selectOption('2'); await list.press('Enter');
    assert.equal(await page.evaluate(()=>window.__sent.at(-1).action),'activate'); assert.equal(await page.evaluate(()=>window.__sent.at(-1).value),2);
    const hotkey=page.getByRole('button',{name:'队形预览快捷键',exact:true}); await hotkey.click(); await page.keyboard.down('Control'); await page.keyboard.press('k'); await page.keyboard.up('Control');
    assert.equal(await page.evaluate(()=>window.__sent.at(-1).action),'hotkey'); assert.equal(await page.evaluate(()=>window.__sent.at(-1).value),0xb24b,'hold and double flags must survive chord capture');
    await receive({kind:'shell',protocol:1,config:{pending:1,busy:true,failed:false},error:''});
    await page.waitForFunction(()=>document.getElementById('saveState').textContent!=='已保存');
    await receive({kind:'shell',protocol:1,error:'目录不可写，请选择其他位置。',config:{pending:1,busy:false,failed:true,error:'配置保存失败'}});
    await page.getByRole('alert').filter({hasText:'目录不可写'}).waitFor();
    await receive({kind:'shell',protocol:1,error:'',config:{pending:0,busy:false,failed:false}});
    await page.getByRole('button',{name:'自动功能',exact:true}).click();
    assert.equal(await page.evaluate(()=>window.__sent.at(-1).module),2); assert.equal(await page.evaluate(()=>window.__sent.at(-1).category),2);
    await receive({kind:'state',protocol:1,module:2,generation:1,revision:1,page:'tactical/2',title:'自动功能',controls:[{id:1,type:'toggle',label:'自动维修',checked:true}],status:''});
    await page.getByLabel('自动维修',{exact:true}).waitFor();
    await page.getByRole('button',{name:'视频',exact:true}).click();
    const large=Array.from({length:3000},(_,index)=>({id:index+1,type:'text',label:`设置 ${index+1}`,text:String(index),group:'全部设置'}));
    await receive({kind:'state',protocol:1,module:0,generation:2,revision:1,page:'cursor',title:'光标与渲染',controls:large,status:'受控虚拟化检查'});
    await page.locator('#contentScroll').evaluate(element=>{element.scrollTop=0;});
    await page.getByLabel('设置 1',{exact:true}).waitFor();
    assert.ok(await page.locator('.setting-row').count()<170,'3000-setting state must not mount all controls');
    await page.locator('#contentScroll').evaluate(element=>{element.scrollTop=element.scrollHeight;});
    await page.getByLabel('设置 3000',{exact:true}).waitFor();
    assert.ok(await page.locator('.setting-row').count()<170);
    await page.setViewportSize({width:720,height:780});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,'narrow layout must not overflow');
    await receive({kind:'state',protocol:1,module:0,generation:3,revision:1,page:'cursor',controls:[{id:1,type:'label',label:'<img src=x onerror=window.__bad=1>',group:'文字显示'}],status:''});
    await page.locator('#contentScroll').evaluate(element=>{element.scrollTop=0;});
    await page.getByText('<img src=x onerror=window.__bad=1>',{exact:true}).waitFor();
    assert.equal(await page.locator('#settings img').count(),0);
    assert.equal(await page.evaluate(()=>window.__bad),undefined);
    assert.deepEqual(errors,[],'browser must not emit uncaught JS errors');
    console.log('PASS browser fixture: all control types, drafts, gestures, module navigation, error/persistence status, 3000-row virtualization, responsive layout');
  } finally { await browser.close(); }
}
