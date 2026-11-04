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
const { ConsoleStore, groupControls, settingLines, encodeHotkey, keyName, canonical, validText, placeAfter, groupPresentation } = context.RA2ConsoleCore;
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
  assert.equal(validText('中'.repeat(21845)),true);assert.equal(validText('中'.repeat(21846)),false);
  assert.equal(validText('😀'.repeat(16384)),true);assert.equal(validText('😀'.repeat(16385)),false);
  assert.equal(validText('x'.repeat(65536)),true);assert.equal(validText('x'.repeat(65537)),false);
  assert.equal(validText('\ud800'),false);assert.equal(validText('\udc00'),false);assert.equal(validText('a\u0000b'),false);
  const {store,messages}=setup([textControl]);store.beginEdit(1);store.draft(1,'中'.repeat(21846));
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
  assert.equal(validText('\u0001'.repeat(65536)),true);
  assert.equal(store.act(1,'text','\u0001'.repeat(65536)),false);
  assert.equal(messages.length,0);assert.equal(store.pending.size,0);
  assert.equal(store.act(1,'range',2147483648),false);
});
test('keyed DOM placement attaches a detached first group and row, then preserves and reorders existing nodes',()=>{
  class Node {
    constructor(name){this.name=name;this.parentNode=null;this.children=[];this.insertions=0;}
    get firstChild(){return this.children[0]||null;}
    get previousSibling(){if(!this.parentNode)return null;const at=this.parentNode.children.indexOf(this);return this.parentNode.children[at-1]||null;}
    get nextSibling(){if(!this.parentNode)return null;const at=this.parentNode.children.indexOf(this);return this.parentNode.children[at+1]||null;}
    insertBefore(node,anchor){
      if(node===anchor)return;
      if(node.parentNode){const siblings=node.parentNode.children;siblings.splice(siblings.indexOf(node),1);}
      const index=anchor?this.children.indexOf(anchor):this.children.length;
      assert.ok(index>=0,'DOM anchor must belong to its container');this.children.splice(index,0,node);node.parentNode=this;this.insertions++;
    }
  }
  const groups=new Node('settings'),first=new Node('first group'),second=new Node('second group');
  assert.equal(first.previousSibling,null);placeAfter(groups,first,null);
  assert.equal(groups.firstChild,first,'detached first node needs insertion even with previousSibling=null');
  placeAfter(groups,second,first);assert.deepEqual(groups.children.map(node=>node.name),['first group','second group']);
  const before=groups.insertions;placeAfter(groups,first,null);placeAfter(groups,second,first);assert.equal(groups.insertions,before,'unchanged order must not remount nodes');
  placeAfter(groups,second,null);placeAfter(groups,first,second);assert.deepEqual(groups.children.map(node=>node.name),['second group','first group']);
  const body=new Node('group body'),row=new Node('first row');placeAfter(body,row,null);assert.equal(row.parentNode,body);
  placeAfter(first,row,null);assert.equal(row.parentNode,first);assert.equal(body.children.length,0,'a node from another parent must be moved');
});
test('native close-check reports unsubmitted work without changing drafts, pending actions, or close state',()=>{
  const {store,messages,changes}=setup([textControl,{id:2,type:'toggle',checked:false}]);
  store.beginEdit(1);store.draft(1,'未提交的编辑');store.act(2,'toggle',true);
  const draft=store.drafts.get('0:1'),pending=store.pending.get('0:2'),sequence=store.seq,state=store.state,changed=changes.length;
  assert.equal(store.receive({kind:'close-check',protocol:1,seq:12345}),true);
  assert.deepEqual({...messages.at(-1)},{kind:'close',seq:12345,unsubmitted:2,protocol:1});
  assert.equal(store.drafts.get('0:1'),draft);assert.equal(store.pending.get('0:2'),pending);
  assert.equal(store.value(store.getControl(1),'text'),'未提交的编辑');assert.equal(store.pending.size,1);
  assert.equal(store.closed,false);assert.equal(store.seq,sequence);assert.equal(store.state,state);assert.equal(changes.length,changed);
  assert.equal(messages.filter(message=>message.kind==='action').length,1,'close check must not submit or commit settings');
  store.cancelEdit(1);store.receive({kind:'ack',protocol:1,module:0,generation:1,id:2,seq:pending.seq,ok:true});
  store.receive({kind:'state',protocol:1,module:0,generation:1,revision:2,page:'cursor',controls:[textControl,{id:2,type:'toggle',checked:true}]});
  assert.equal(store.receive({kind:'close-check',protocol:1,seq:12346}),true);
  assert.equal(messages.at(-1).unsubmitted,0);assert.equal(store.closed,false);
});
test('close-check accepts only positive safe nonce values and bounds the count',()=>{
  const {store,messages}=setup([textControl]);const count=messages.length;
  for(const seq of [undefined,0,-1,1.25,NaN,Infinity,Number.MAX_SAFE_INTEGER+1,'123',true]) assert.equal(store.receive({kind:'close-check',protocol:1,seq}),false);
  assert.equal(store.receive({kind:'close-check',protocol:2,seq:1}),false);assert.equal(messages.length,count);
  for(let index=0;index<10001;index++)store.drafts.set(`0:${index}`,{value:'草稿',page:'0'});
  assert.equal(store.receive({kind:'close-check',protocol:1,seq:Number.MAX_SAFE_INTEGER}),true);
  assert.equal(messages.at(-1).seq,Number.MAX_SAFE_INTEGER);assert.equal(messages.at(-1).unsubmitted,10000);
  assert.equal(store.drafts.size,10001);assert.equal(store.closed,false);
});

test('all fifteen groups preserve pinned master controls and recover the saved collapse preference',()=>{
  for(let group=9601;group<=9615;++group){
    const controls=[canonical({id:1,type:'toggle',label:'开关',groupHeader:true,collapsible:true,collapsedDefault:true}),canonical({id:2,type:'label',label:'正常状态',collapsible:true})];
    let view=groupPresentation(controls,true);assert.equal(view.collapsed,true);assert.equal(view.headers.length,1);assert.equal(view.details.length,1);
    controls[1]=canonical({...controls[1],runtimeError:true});view=groupPresentation(controls,true);assert.equal(view.collapsed,false);assert.equal(view.forced,true);
    controls[1]=canonical({...controls[1],runtimeError:false});assert.equal(groupPresentation(controls,true).collapsed,true);
    assert.equal(groupPresentation(controls,false).collapsed,false);assert.equal(groupPresentation(controls,true,true).collapsed,false);
    assert.equal(groupPresentation(controls,true,false,new Set([2])).forced,true);
  }
});
test('metadata and immediate actions retain the original click time and reject duplicate pending execution',()=>{
  const {store,messages}=setup([{id:1,type:'button',label:'立即自毁',immediate:true,groupHeader:true,collapsible:true,collapsedDefault:true,runtimeError:true}]);
  const c=store.getControl(1);for(const key of ['immediate','groupHeader','collapsible','collapsedDefault','runtimeError'])assert.equal(c[key],true);
  const before=Date.now();assert.equal(store.act(1,'press'),true);const sent=messages.at(-1);assert.ok(sent.inputTime>=before&&sent.inputTime<=Date.now());
  assert.equal(Object.hasOwn(sent,'receivedAt'),false);assert.equal(store.act(1,'press'),false);assert.equal(messages.length,1);
  store.receive({kind:'ack',protocol:1,module:0,generation:1,id:1,seq:sent.seq,ok:false,error:'已过期'});
  assert.equal(store.act(1,'press'),true);assert.ok(messages.at(-1).seq>sent.seq);
});

test('searching one editor retains its feature and never merges missing row coordinates',()=>{
  const feature=[
    {id:1,type:'toggle',label:'高级进驻',checked:true,column:20},
    {id:2,type:'choice',label:'高级进驻',options:['仅鼠标','Alt','Ctrl','Shift','自定义'],column:240},
    {id:3,type:'hotkey',label:'高级进驻',selected:18,options:['hold'],column:350},
    {id:4,type:'choice',label:'高级进驻',options:['停用','中键','侧键 1','侧键 2','右键','左键'],column:510}
  ].map(control=>canonical({...control,row:120,group:'键鼠组合操作'}));
  const other=canonical({id:5,type:'toggle',label:'自动维修',row:158,column:20,group:'键鼠组合操作'});
  assert.deepEqual(Array.from(groupControls([...feature,other],'左键').values()).flat().map(c=>c.id),[1,2,3,4]);
  assert.equal(settingLines([...feature,other]).length,2);
  assert.equal(settingLines([canonical({id:6,type:'text'}),canonical({id:7,type:'text'})]).length,2);
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
  execFileSync(process.env.CONSOLE_PYTHON || 'python3',[path.join(here,'../../tools/bundle_console.py'),'--output',html]);
  const browser = await chromium.launch({headless:true,...(process.env.CONSOLE_BROWSER_CHANNEL?{channel:process.env.CONSOLE_BROWSER_CHANNEL}:{}),args:['--no-sandbox']});
  let page;const errors=[],consoleErrors=[];
  const screenshotPath=process.env.CONSOLE_SCREENSHOT || path.join(temporary,'console-desktop.png');
  const diagnosticsPrefix=screenshotPath.replace(/\.png$/i,'');
  try {
    page = await browser.newPage({viewport:{width:1240,height:880},deviceScaleFactor:1});
    page.on('pageerror',error=>{errors.push(error.message);console.error('BROWSER_PAGE_ERROR',error.stack || error.message);});
    page.on('console',message=>{if(['error','warning'].includes(message.type())){consoleErrors.push(message.text());console.error(`BROWSER_${message.type().toUpperCase()}`,message.text());}});
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
    await receive({kind:'shell',protocol:1,version:'1.0.41-pre.4',selectedCategory:0,videoSection:0,servicesReady:true,modules:[{module:0,ready:true,enabled:true},{module:1,ready:true,enabled:false}],config:{pending:0,busy:false,failed:false}});
    await receive(snapshot()); await page.getByLabel('渲染升级',{exact:true}).waitFor();
    assert.equal(await page.getByLabel('绘制方式',{exact:true}).evaluate(element=>element.closest('.setting-row').dataset.settingLevel),'secondary','ordinary parameter rows must read below feature switches');
    await page.screenshot({path:screenshotPath,fullPage:false});
    const assertFits = async () => {
      assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,'the shell must fit the viewport');
      assert.deepEqual(await page.locator('.setting-editor').evaluateAll(editors=>editors.filter(element=>element.getClientRects().length).flatMap(editor=>[...editor.children]).filter(element=>element.getClientRects().length).filter(element=>{const box=element.getBoundingClientRect(),bounds=document.getElementById('contentScroll').getBoundingClientRect();return box.left<bounds.left-1||box.right>bounds.right+1;}).map(element=>element.getAttribute('aria-label')||element.className)),[],'every visible editor must fit inside the settings pane');
    };
    await assertFits();
    assert.equal(await page.locator('#saveState').getAttribute('data-state'),'saved');
    const tabs=page.getByRole('tab');await tabs.nth(0).focus();await page.keyboard.press('ArrowRight');
    assert.equal(await page.evaluate(()=>window.__sent.at(-1).module),1);
    await page.waitForFunction(()=>document.querySelector('[data-module="1"]').tabIndex===0);
    await page.keyboard.press('Home');
    await page.waitForFunction(()=>document.querySelector('[data-module="0"]').getAttribute('aria-selected')==='true');
    await page.keyboard.press('Control+f');await page.keyboard.type('渲染升级');
    await page.waitForFunction(()=>document.getElementById('resultCount').textContent==='找到 1 项设置');
    assert.equal(await page.locator('#foldAll').isVisible(),false,'search results remain expanded');
    await page.keyboard.press('Escape');
    await page.waitForFunction(()=>document.getElementById('search').value==='');
    await page.getByLabel('绘制方式',{exact:true}).waitFor();
    for(const viewport of [{width:720,height:780},{width:540,height:720},{width:390,height:720}]){
      await page.setViewportSize(viewport);await page.getByRole('button',{name:'队形预览快捷键',exact:true}).scrollIntoViewIfNeeded();await assertFits();
      await page.locator('#contentScroll').evaluate(element=>{element.scrollTop=0;});
      await page.screenshot({path:diagnosticsPrefix+'-'+viewport.width+'.png',fullPage:false});
    }
    await page.setViewportSize({width:1240,height:880});
    await page.locator('#contentScroll').evaluate(element=>{element.scrollTop=0;});
    assert.equal(await page.getByRole('heading',{name:'视频',exact:true}).count(),1);
    assert.equal(await page.getByLabel('未连接时的功能',{exact:true}).isDisabled(),true);
    const switchBox=await page.getByLabel('渲染升级',{exact:true}).boundingBox();assert.ok(switchBox.width>=24 && switchBox.height>=24,'accessible switch owns a full pointer target');
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
    assert.equal(await page.locator('#saveState').getAttribute('data-state'),'error');
    await page.screenshot({path:diagnosticsPrefix+'-save-error.png',fullPage:false});
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
    // Exercise real DOM folds, pinned switches, hover-only explanations and
    // error recovery, including all groups exported by the native metadata.
    const folded=[];for(let n=9601;n<=9615;++n){const group=`参数组 ${n}`;folded.push({id:n*10,type:'toggle',label:`主开关 ${n}`,group,groupHeader:true,collapsible:true,collapsedDefault:true,hint:'只在悬停时显示说明'}, {id:n*10+1,type:'number',label:`子参数 ${n}`,group,text:'3',collapsible:true,collapsedDefault:true,hint:'子参数说明'}, {id:n*10+2,type:'label',label:`正常状态 ${n}`,group,collapsible:true,collapsedDefault:true});}
    const foldedState=(controls)=>({kind:'state',protocol:1,module:0,generation:4,revision:revision++,page:'cursor',title:'收纳回归',controls,status:''});
    await receive(foldedState(folded));await page.locator('#contentScroll').evaluate(element=>{element.scrollTop=0;});
    await page.getByLabel('主开关 9601',{exact:true}).waitFor();assert.equal(await page.locator('.settings-group.collapsed').count(),15);
    assert.equal(await page.locator('#foldAll').textContent(),'全部展开','default collapsed groups must expose the correct bulk action');
    await page.locator('#foldAll').click();await page.waitForFunction(()=>document.querySelectorAll('.settings-group.collapsed').length===0);
    assert.equal(await page.locator('#foldAll').textContent(),'全部折叠');
    await page.locator('#foldAll').click();await page.waitForFunction(()=>document.querySelectorAll('.settings-group.collapsed').length===15);
    await page.setViewportSize({width:1240,height:880});await page.screenshot({path:diagnosticsPrefix+'-folded.png',fullPage:false});
    assert.equal(await page.getByLabel('子参数 9601',{exact:true}).isVisible(),false,'folding details must preserve the master switch');
    const first=page.locator('.settings-group').filter({has:page.getByText('参数组 9601',{exact:true})});
    assert.equal(await first.locator('.group-header-controls .setting-row').getAttribute('title'),'只在悬停时显示说明');assert.equal(await first.locator('.setting-description:visible').count(),0,'ordinary help must not occupy a row');
    const failing=new Set([9607,9612,9613,9614]);await receive(foldedState(folded.map(c=>failing.has(Math.floor(c.id/10))&&c.type==='label'?{...c,label:`错误待还原 ${Math.floor(c.id/10)}`,runtimeError:true}:c)));
    for(const n of failing){const section=page.locator('.settings-group').filter({has:page.getByText(`参数组 ${n}`,{exact:true})});await section.scrollIntoViewIfNeeded();await page.getByText(`错误待还原 ${n}`,{exact:true}).waitFor();assert.equal(await section.locator('.group-summary').getAttribute('aria-expanded'),'true');await section.locator('.group-summary').click();assert.equal(await section.locator('.group-summary').getAttribute('aria-expanded'),'true','a real error cannot be collapsed');}
    await receive(foldedState(folded));await page.waitForFunction(()=>document.querySelectorAll('.settings-group.collapsed').length===15);assert.equal(await page.locator('.settings-group.collapsed').count(),15,'recovery restores the prior closed preference');
    const danger=[{id:9185,type:'button',label:'立即自毁',immediate:true}];await receive({kind:'state',protocol:1,module:0,generation:5,revision:1,page:'cursor',controls:danger,status:''});await page.locator('#contentScroll').evaluate(element=>{element.scrollTop=0;});
    const button=page.getByRole('button',{name:'立即自毁',exact:true});await button.waitFor();const sentBefore=await page.evaluate(()=>window.__sent.length);await button.dblclick();const submitted=await page.evaluate(()=>window.__sent.slice(-1)[0]);assert.equal(await page.evaluate(()=>window.__sent.length),sentBefore+1);assert.equal(submitted.action,'press');assert.ok(submitted.inputTime>0);assert.equal(Object.hasOwn(submitted,'receivedAt'),false);
    // Reproduce the shipped native row from tactical_controls.h: one feature,
    // four independent control tokens, one name and compact inline editors.
    const compound=[
      {id:702,type:'toggle',label:'高级进驻',checked:true,column:20},
      {id:703,type:'choice',label:'高级进驻',selected:1,options:['仅鼠标','Alt','Ctrl','Shift','自定义'],readonly:true,column:240},
      {id:704,type:'hotkey',label:'高级进驻',selected:0x2012,options:['hold'],column:350},
      {id:740,type:'choice',label:'高级进驻',selected:5,options:['停用','中键','侧键 1','侧键 2','右键','左键'],readonly:true,column:510}
    ].map(control=>({...control,row:120,group:'键鼠组合操作'}));
    const compactState=(patch={})=>({kind:'state',protocol:1,module:0,generation:6,revision:revision++,page:'cursor',controls:compound,status:'',...patch});
    await receive(compactState());await page.setViewportSize({width:1240,height:880});await page.locator('#contentScroll').evaluate(element=>{element.scrollTop=0;});
    const line=page.locator('.compound-row');await line.waitFor();
    assert.equal(await line.count(),1);assert.equal(await line.locator('.setting-row').count(),4);
    assert.equal(await line.locator('.setting-label').filter({hasText:/^高级进驻$/}).count(),1);
    assert.equal(await line.locator('[data-setting-level="primary"]').count(),1);
    assert.equal(await line.locator('[data-setting-level="secondary"]').count(),3);
    const hierarchy=await line.evaluate(element=>{const labels=[...element.querySelectorAll('.setting-label')],main=getComputedStyle(labels[0]),sub=getComputedStyle(labels[1]),name=labels[0].getBoundingClientRect(),toggle=element.querySelector('.toggle').getBoundingClientRect();return{mainSize:parseFloat(main.fontSize),subSize:parseFloat(sub.fontSize),mainWeight:Number(main.fontWeight),subWeight:Number(sub.fontWeight),mainColor:main.color,subColor:sub.color,switchGap:toggle.left-name.right};});
    assert.ok(hierarchy.mainSize>hierarchy.subSize && hierarchy.mainWeight>hierarchy.subWeight,'feature names must read above their smaller subsettings');
    assert.notEqual(hierarchy.mainColor,hierarchy.subColor,'secondary captions must use their quieter color');
    assert.ok(hierarchy.switchGap>=0 && hierarchy.switchGap<=16,'the feature switch must stay beside its name');
    assert.ok((await line.boundingBox()).height<=96,'the complete feature must fit in at most two compact lines on desktop');
    await page.getByLabel('高级进驻 · 鼠标',{exact:true}).selectOption('1');
    action=await page.evaluate(()=>window.__sent.at(-1));assert.equal(action.id,740);assert.equal(action.action,'select');assert.equal(action.value,1);
    const compactKey=page.getByRole('button',{name:'高级进驻 · 自定义键',exact:true});await compactKey.click();
    await compactKey.evaluate(element=>{window.__compactKey=element;});await receive(compactState({controls:compound.map(c=>c.id===702?{...c,checked:false}:c)}));
    await page.waitForFunction(()=>!document.getElementById('control-0-702').checked);
    assert.equal(await compactKey.evaluate(element=>element===window.__compactKey && document.activeElement===element),true,'background revisions preserve the focused editor node');
    await page.keyboard.press('Escape');
    for(const viewport of [{width:1794,height:900},{width:1240,height:880},{width:720,height:780},{width:390,height:720}]) {
      await page.setViewportSize(viewport);await assertFits();
      await page.screenshot({path:diagnosticsPrefix+'-compact-'+viewport.width+'.png',fullPage:false});
    }
    await page.setViewportSize({width:1240,height:880});await page.getByLabel('高级进驻',{exact:true}).click();
    action=await page.evaluate(()=>window.__sent.at(-1));assert.equal(action.id,702);assert.equal(action.action,'toggle');
    await page.locator('#search').fill('左键');await page.waitForFunction(()=>document.getElementById('resultCount').textContent==='找到 4 项设置');
    assert.equal(await line.locator('.setting-row').count(),4,'search retains sibling editors and the feature switch');await page.locator('#search').fill('');
    const pinned=[{id:880,type:'toggle',label:'科技设置',group:'科技设置',groupHeader:true,collapsible:true,collapsedDefault:true},{id:881,type:'number',label:'科技等级',text:'1',group:'科技设置',collapsible:true,collapsedDefault:true}];
    await receive(compactState({generation:7,controls:pinned}));await page.getByLabel('科技设置',{exact:true}).waitFor();
    assert.equal(await page.locator('.group-header-controls .setting-label:visible').count(),0,'a pinned switch shares the group title');
    const pinnedGap=await page.locator('.settings-group').evaluate(element=>element.querySelector('.toggle').getBoundingClientRect().left-element.querySelector('.group-title').getBoundingClientRect().right);
    assert.ok(pinnedGap>=0 && pinnedGap<=16,'the pinned master switch must sit beside the shared title');
    await page.locator('.group-summary').click();await page.getByLabel('科技等级',{exact:true}).waitFor();
    assert.equal(await page.locator('.group-body .setting-row').getAttribute('data-setting-level'),'secondary','details inherit the main switch hierarchy');
    await assertFits();
    await page.getByLabel('科技设置',{exact:true}).click();action=await page.evaluate(()=>window.__sent.at(-1));assert.equal(action.id,880);
    await receive({kind:'ack',protocol:1,module:0,generation:7,id:880,seq:action.seq,ok:true});
    await receive(compactState({generation:7,controls:pinned.map(control=>control.id===880?{...control,checked:action.value}:control)}));
    // A clean, correctly routed acceptance view: ordinary saved settings,
    // two complete feature rows, and an expanded master/detail group.
    await page.getByRole('button',{name:'控制',exact:true}).click();
    assert.equal(await page.evaluate(()=>window.__sent.at(-1).module),2);assert.equal(await page.evaluate(()=>window.__sent.at(-1).category),1);
    const hierarchyControls=[
      ...compound.map(control=>control.type==='hotkey'?{...control,selected:0x3012}:control),
      {id:477,type:'toggle',label:'磁电轮换攻击',checked:true,row:164,column:20,group:'键鼠组合操作'},
      {id:479,type:'hotkey',label:'磁电轮换攻击',selected:0x3412,options:['hold'],row:164,column:240,group:'键鼠组合操作'},
      {id:478,type:'choice',label:'磁电轮换攻击',selected:1,options:['停用','中键','侧键 1','侧键 2','右键','左键'],readonly:true,row:164,column:510,group:'键鼠组合操作'},
      {id:705,type:'toggle',label:'飞机分配攻击',checked:false,group:'飞机分配攻击',groupHeader:true,collapsible:true},
      {id:750,type:'number',label:'每个目标分配上限',text:'2',hint:'限制同时分配给同一目标的飞机数量。',group:'飞机分配攻击',collapsible:true},
      {id:751,type:'choice',label:'目标优先级',selected:0,options:['当前目标','剩余生命','距离最近'],readonly:true,group:'飞机分配攻击',collapsible:true},
      {id:752,type:'label',label:'设置已保存 · 进入对局后生效',group:'飞机分配攻击',collapsible:true}
    ];
    const controlPage=(revision,controls=hierarchyControls)=>({kind:'state',protocol:1,module:2,generation:2,revision,page:'tactical/1',title:'控制',controls,status:'设置已保存 · 可在进入游戏前调整'});
    await receive(controlPage(1));await page.getByLabel('飞机分配攻击',{exact:true}).waitFor();
    await page.getByLabel('飞机分配攻击',{exact:true}).click();action=await page.evaluate(()=>window.__sent.at(-1));
    assert.equal(action.module,2);assert.equal(action.id,705);assert.equal(action.value,true);
    await receive({kind:'ack',protocol:1,module:2,generation:2,id:705,seq:action.seq,ok:true,revision:2});
    await receive(controlPage(2,hierarchyControls.map(control=>control.id===705?{...control,checked:true}:control)));
    await page.waitForFunction(()=>!window.RA2Console.store.pending.size && !window.RA2Console.store.drafts.size && document.getElementById('saveState').dataset.state==='saved' && document.getElementById('globalNotice').hidden);
    await page.locator('#toast').waitFor({state:'hidden',timeout:10000});
    assert.equal(await page.getByRole('button',{name:'控制',exact:true}).getAttribute('aria-current'),'page');
    await page.getByLabel('每个目标分配上限',{exact:true}).waitFor();await page.mouse.move(0,0);
    for(const viewport of [{width:1240,height:880},{width:720,height:780},{width:390,height:720}]) {
      await page.setViewportSize(viewport);await page.locator('#contentScroll').evaluate(element=>{element.scrollTop=0;});await assertFits();
      await page.screenshot({path:diagnosticsPrefix+'-hierarchy-'+viewport.width+'.png',fullPage:false});
    }
    await page.setViewportSize({width:1240,height:880});
    assert.deepEqual(errors,[],'browser must not emit uncaught JS errors');
    console.log('PASS browser fixture: all control types, drafts, gestures, module navigation, error/persistence status, 3000-row virtualization, responsive layout');
  } finally {
    if(page) {
      try {
        fs.mkdirSync(path.dirname(screenshotPath),{recursive:true});
        await page.screenshot({path:diagnosticsPrefix+'-final.png',fullPage:false});
        fs.writeFileSync(diagnosticsPrefix+'-dom.html',await page.content());
        const state=await page.evaluate(()=>{
          const store=window.RA2Console?.store;
          const rect=element=>{if(!element)return null;const box=element.getBoundingClientRect(),style=getComputedStyle(element);return{left:box.left,top:box.top,width:box.width,height:box.height,display:style.display,visibility:style.visibility,overflowY:style.overflowY,hidden:element.hidden};};
          return {url:location.href,current:store?.current,generation:store?[...store.generation]:[],pages:store?[...store.pages].map(([key,value])=>({key,module:value.module,generation:value.generation,revision:value.revision,controls:value.controls.length,first:value.controls.slice(0,4)})):[],shell:store?.shell,notice:store?.notice,sent:window.__sent,body:rect(document.body),workspace:rect(document.querySelector('.workspace')),scroll:rect(document.getElementById('contentScroll')),settings:rect(document.getElementById('settings')),groups:[...document.querySelectorAll('.settings-group')].map(element=>({name:element.querySelector('.group-title')?.textContent,section:rect(element),body:rect(element.querySelector('.group-body')),controls:element._group?.controls.length,intersecting:element._group?.intersecting})),mountedRows:document.querySelectorAll('.setting-row').length};
        });
        fs.writeFileSync(diagnosticsPrefix+'-state.json',JSON.stringify({errors,consoleErrors,state},null,2));
        console.log('BROWSER_DIAGNOSTICS',JSON.stringify({errors,consoleErrors,state}));
      }catch(error){console.error('BROWSER_DIAGNOSTICS_ERROR',error.stack || error.message);}
    }
    await browser.close();
  }
}
