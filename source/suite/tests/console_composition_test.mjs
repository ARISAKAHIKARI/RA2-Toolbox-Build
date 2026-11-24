import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import {fileURLToPath} from 'node:url';

// Controlled DOM event fixture: execute the production listeners and store.
// It does not emulate Windows IME, layout, WebView2, or trusted browser events.
const here=path.dirname(fileURLToPath(import.meta.url));
const at=process.argv.indexOf('--app');
const app=at<0?path.join(here,'../frontend/app.js'):path.resolve(process.argv[at+1]);
const source=fs.readFileSync(app,'utf8');
const seam='  store.ready(); schedule();';
assert.equal(source.split(seam).length,2,'fixture seam must identify the production startup once');

function setup(){
 let document;
 class Element {
  constructor(tag='div'){
   this.tagName=tag.toUpperCase();this.children=[];this.parentNode=null;this.listeners=new Map();
   this.attributes=new Map();this.dataset={};this.style={};this.value='';this.textContent='';
   this.classList={add(){},remove(){},toggle(){return false}};
  }
  append(...nodes){for(const node of nodes){if(typeof node!=='object')continue;node.remove();node.parentNode=this;this.children.push(node)}}
  prepend(...nodes){for(const node of [...nodes].reverse()){node.remove();node.parentNode=this;this.children.unshift(node)}}
  replaceChildren(...nodes){for(const child of this.children)child.parentNode=null;this.children=[];this.append(...nodes)}
  get firstChild(){return this.children[0]||null}
  get previousSibling(){return this.parentNode?.children[this.parentNode.children.indexOf(this)-1]||null}
  get nextSibling(){return this.parentNode?.children[this.parentNode.children.indexOf(this)+1]||null}
  insertBefore(node,reference){node.remove();const index=reference?this.children.indexOf(reference):this.children.length;assert.ok(index>=0);this.children.splice(index,0,node);node.parentNode=this}
  replaceWith(node){this.parentNode?.insertBefore(node,this);this.remove()}
  remove(){if(this.parentNode){const siblings=this.parentNode.children;siblings.splice(siblings.indexOf(this),1);this.parentNode=null}}
  setAttribute(name,value){this.attributes.set(name,String(value))}
  removeAttribute(name){this.attributes.delete(name)}
  toggleAttribute(name,value){if(value)this.setAttribute(name,'');else this.removeAttribute(name)}
  querySelector(){return this.children[0]||new Element('span')}
  addEventListener(type,listener,capture=false){if(!this.listeners.has(type))this.listeners.set(type,[]);this.listeners.get(type).push({listener,capture})}
  contains(node){return node===this||this.children.some(child=>child.contains(node))}
  focus(){if(document.activeElement&&document.activeElement!==this)document.activeElement.blur();document.activeElement=this;dispatch(this,'focus');dispatch(this,'focusin')}
  blur(){if(document.activeElement!==this)return;document.activeElement=null;dispatch(this,'blur')}
  select(){}
 }
 const elements=new Map(),sent=[];
 document=new Element('document');document.activeElement=null;
 document.getElementById=id=>{if(!elements.has(id))elements.set(id,new Element());return elements.get(id)};
 document.createElement=tag=>new Element(tag);
 document.createElementNS=(_,tag)=>new Element(tag);
 document.createTextNode=text=>{const node=new Element('#text');node.textContent=text;return node};
 const globalListeners=new Map(),bridgeListeners=[];
 const context=vm.createContext({console,Date,Map,Set,Object,Number,String,Boolean,RangeError,document,
  requestAnimationFrame(){},setTimeout(){return 1},clearTimeout(){},setInterval(){return 1},
  addEventListener(type,listener){globalListeners.set(type,listener)},
  chrome:{webview:{postMessage(message){sent.push(message)},addEventListener(type,listener){if(type==='message')bridgeListeners.push(listener)}}}
 });
 function dispatch(target,type,properties={}){
  const event={type,target,defaultPrevented:false,stopped:false,key:'',keyCode:0,isComposing:false,
   preventDefault(){this.defaultPrevented=true},stopPropagation(){this.stopped=true},...properties};
  for(const record of document.listeners.get(type)||[])if(record.capture)record.listener(event);
  if(!event.stopped)for(const record of target.listeners.get(type)||[])record.listener(event);
  if(!event.stopped&&target!==document)for(const record of document.listeners.get(type)||[])if(!record.capture)record.listener(event);
  return event;
 }
 // Expose private production factories only. The listener bodies stay intact.
 vm.runInContext(source.replace(seam,'  global.__consoleDomFixture={makeRow,patchRow,render};\n'+seam),context,{filename:app});
 const store=context.RA2Console.store;
 store.receive({kind:'state',protocol:1,module:0,generation:1,revision:1,page:'cursor',controls:[
  {id:1,type:'text',label:'模糊关键词',text:'old'},
  {id:2,type:'choice',label:'自定义值',text:'old',options:['old'],selected:0,readonly:false,presentation:'editable'},
  {id:3,type:'hotkey',label:'快捷键',selected:0,options:['press','release']}
 ]});
 function row(id){const result=context.__consoleDomFixture.makeRow(store.getControl(id));context.__consoleDomFixture.patchRow(result);return result}
 function edit(id,value){const result=row(id),input=result.textInput||result.input;input.focus();input.value=value;dispatch(input,'input');return input}
 const actions=()=>sent.filter(message=>message.kind==='action');
 const render=context.__consoleDomFixture.render;
 const receive=message=>bridgeListeners.forEach(listener=>listener({data:message}));
 const find=(className,root=document.getElementById('settings'))=>[
  ...(root.className===className?[root]:[]),...root.children.flatMap(child=>find(className,child))
 ];
 return {context,document,store,elements,sent,dispatch,row,edit,actions,render,receive,find};
}
const navButton=(root,index)=>root.children.find(child=>child.dataset.pageIndex===String(index));
const cases=[];
const test=(name,run)=>cases.push({name,run});

test('top-level display pages route, restore and show only their controls',()=>{
 const f=setup(),node=id=>f.document.getElementById(id);
 const nav=Array.from({length:8},(_,i)=>navButton(node('navigation'),i));
 assert.deepEqual(node('navigation').children.filter(child=>child.className==='nav-group-label').map(child=>child.textContent),['对局工具','显示','高级']);
 assert.deepEqual(nav.map(b=>b.attributes.get('aria-label')),['光标','渲染','监控视图','控制','自动功能','游戏规则','实验功能','调试']);
 const state=(module,revision,controls)=>({kind:'state',protocol:1,module,generation:1,revision,controls,page:module?'monitor':'cursor'});
 f.receive({kind:'shell',protocol:1,servicesReady:true,selectedCategory:0,videoSection:1});
 f.receive(state(1,1,[{id:11,type:'toggle',label:'副相机',group:'副相机'}]));
 f.render();
 assert.equal(node('pageTitle').textContent,'监控视图');
 assert.equal(nav[2].attributes.get('aria-current'),'page');
 assert.equal(f.find('setting-row').length,1);
 f.dispatch(nav[0],'click');
 assert.equal(f.sent.at(-1).module,0);
 f.receive(state(0,2,[{id:12,type:'toggle',label:'光标补帧',group:'光标设置'},{id:13,type:'toggle',label:'画质选项',group:'渲染'}]));
 f.render();
 assert.equal(node('pageTitle').textContent,'光标');
 assert.deepEqual(f.find('setting-label').map(label=>label.textContent),['光标补帧']);
 const sent=f.sent.length;
 f.dispatch(nav[1],'click');f.render();
 assert.equal(f.sent.length,sent+1);
 assert.equal(f.sent.at(-1).pageIndex,1);
 assert.equal(node('pageTitle').textContent,'渲染');
 assert.deepEqual(f.find('setting-label').map(label=>label.textContent),['画质选项']);
 const early=setup();
 early.dispatch(navButton(early.document.getElementById('navigation'),1),'click');
 assert.equal(early.sent.at(-1).kind,'navigate');
 assert.equal(early.sent.at(-1).module,0);
 const restored=setup();
 restored.receive({kind:'shell',protocol:1,servicesReady:true,selectedCategory:0,videoSection:0,pageIndex:1});
 restored.render();
 assert.equal(restored.document.getElementById('pageTitle').textContent,'渲染');
 assert.equal(navButton(restored.document.getElementById('navigation'),1).attributes.get('aria-current'),'page');
});

test('automatic features have one level of expansion and no empty fold',()=>{
 const f=setup(),node=id=>f.document.getElementById(id),select=navButton(f.document.getElementById('navigation'),4);
 f.dispatch(select,'click');
 assert.equal(f.sent.at(-1).category,2);
 const controls=[
  {id:11,type:'heading',label:'自动功能'},
  {id:12,type:'toggle',label:'过滤低伤害自动目标',groupHeader:true},
  {id:13,type:'label',label:'过滤运行中'},
  {id:14,type:'toggle',label:'自动维修',group:'维修',groupHeader:true},
  {id:15,type:'number',label:'血量阈值',group:'维修',text:'40'},
  {id:16,type:'label',label:'维修范围暂不可用',group:'维修',runtimeError:true},
 ];
 const state=revision=>({kind:'state',protocol:1,module:2,generation:1,revision,page:'tactical/2',controls});
 f.receive(state(1));f.render();
 assert.equal(f.find('group-summary').length,2);
 assert.equal(f.find('group-summary')[0].tagName,'DIV');
 assert.equal(f.find('group-summary')[0].hidden,false);
 assert.equal(f.find('group-summary')[1].hidden,false);
 const buttons=f.find('feature-config-toggle');
 assert.equal(buttons.length,2);
 assert.equal(buttons[1].hidden,true);
 assert.equal(buttons[0].attributes.get('aria-expanded'),'false');
 assert.equal(buttons[0].textContent,'展开','inline child settings use the same expand action everywhere');
 assert.ok(f.find('group-status')[0].textContent.includes('维修范围暂不可用'));
 f.dispatch(buttons[0],'click');
 assert.equal(buttons[0].attributes.get('aria-expanded'),'true');
 assert.equal(buttons[0].textContent,'收起');
 assert.ok(f.find('setting-label').some(label=>label.textContent==='血量阈值'));
 f.dispatch(buttons[0],'click');
 assert.equal(buttons[0].textContent,'展开');
 const search=node('search');search.value='血量阈值';f.dispatch(search,'input');f.render();
 assert.equal(f.find('feature-config-toggle')[0].attributes.get('aria-expanded'),'true');
 f.dispatch(node('foldAll'),'click');f.receive(state(2));f.render();
 assert.equal(f.find('feature-config-toggle')[0].attributes.get('aria-expanded'),'false');
 search.value='';f.dispatch(search,'input');f.render();
 assert.equal(f.find('feature-config-toggle')[0].attributes.get('aria-expanded'),'false');
});

test('production settings have a single page and keep their tactical owner',()=>{
 const f=setup(),node=id=>f.document.getElementById(id);
 f.receive({kind:'shell',protocol:1,servicesReady:true,selectedCategory:2,pageIndex:8});
 f.receive({kind:'state',protocol:1,module:2,generation:1,revision:1,page:'tactical/2',controls:[
  {id:71,nativeId:799,type:'toggle',label:'生产预定',group:'生产预定',groupHeader:true},
  {id:72,nativeId:817,type:'number',label:'预定面板缩放',group:'生产预定',text:'100'},
  {id:73,nativeId:100,type:'toggle',label:'自动维修',group:'自动维修',groupHeader:true}
 ]});
 f.render();
 assert.equal(node('pageTitle').textContent,'生产');
 assert.deepEqual(f.find('setting-label').map(label=>label.textContent),['生产预定']);
 assert.equal(f.store.act(71,'toggle',true),true);
 assert.equal(f.actions().at(-1).module,2);
 assert.equal(f.actions().at(-1).id,71);
 f.dispatch(navButton(node('navigation'),4),'click');f.render();
 assert.deepEqual(f.find('setting-label').map(label=>label.textContent),['自动维修']);
 f.dispatch(navButton(node('navigation'),8),'click');
 assert.equal(f.sent.at(-1).pageIndex,8);
 assert.equal(f.sent.at(-1).category,2);
 f.dispatch(node('productionNotices'),'click');
 assert.equal(f.sent.at(-1).module,1);
 assert.equal(f.sent.at(-1).pageIndex,9);
 f.receive({kind:'state',protocol:1,module:1,generation:1,revision:1,page:'monitor',controls:[
  {id:74,type:'toggle',label:'显示建造时间',group:'生产提醒'},
  {id:75,type:'toggle',label:'副相机',group:'副相机'}
 ]});
 f.render();
 assert.deepEqual(f.find('setting-label').map(label=>label.textContent),['显示建造时间']);
 assert.equal(navButton(node('navigation'),8).attributes.get('aria-current'),'page');
 assert.equal(f.store.act(74,'toggle',true),true);
 assert.equal(f.actions().at(-1).module,1);
 f.dispatch(navButton(node('navigation'),2),'click');f.render();
 assert.deepEqual(f.find('setting-label').map(label=>label.textContent),['副相机']);
});

test('native shared rows and status notes have no redundant configuration title',()=>{
 const f=setup(),nav=f.document.getElementById('navigation');
 f.dispatch(navButton(nav,4),'click');
 f.receive({kind:'state',protocol:1,module:2,generation:1,revision:1,page:'tactical/2',controls:[
  {id:1,type:'toggle',group:'自动功能',label:'磁电全局无限吸',row:80,column:20},
  {id:2,type:'hotkey',group:'自动功能',label:'磁电全局无限吸',row:80,column:345},
  {id:3,type:'toggle',group:'防御',label:'防御状态',groupHeader:true},
  {id:4,type:'label',group:'防御',label:'等待对局'},
  {id:5,type:'label',group:'防御',label:'防御选项',groupHeader:true}
 ]});
 f.render();
 assert.ok(f.find('feature-subheading').every(title=>title.hidden));
 assert.ok(f.find('feature-config-toggle').every(button=>button.hidden));
 assert.ok(!f.find('setting-label').some(label=>label.textContent==='等待对局'),'no explanatory row is mounted');
 assert.equal(f.find('label-help').length,1);
 const [featureHelp]=f.find('label-help',f.find('feature-header-actions')[0]);
 assert.ok(!featureHelp.open);
 assert.equal(featureHelp.children[0].attributes.get('aria-label'),'查看防御状态的说明');
 assert.equal(featureHelp.children[1].textContent,'等待对局');
 const groupLabel=f.find('setting-label').find(label=>label.textContent==='防御选项');
 assert.ok(groupLabel&&!groupLabel.hidden,'semantic group labels remain visible; only explanatory notes are tucked away');
});
test('legacy self-named master settings infer one fold, broad groups stay separate',()=>{
 const core=setup().context.RA2ConsoleCore;
 const canonical=controls=>controls.map(control=>core.canonical(control));
 const master=canonical([
  {id:11,type:'toggle',label:'自动装车',group:'自动装车',row:10,column:10},
  {id:12,type:'hotkey',label:'自动装车',group:'自动装车',row:10,column:200},
  {id:13,type:'number',label:'装车距离',group:'自动装车',row:60,column:20},
  {id:14,type:'choice',label:'装车策略',group:'自动装车',row:110,column:20}
 ]);
 const blocks=core.featureBlocks(master);
 assert.equal(blocks.length,1);assert.equal(blocks[0].title,'自动装车');
 assert.deepEqual([...blocks[0].headers.map(c=>c.id)],[11,12]);
 assert.deepEqual([...blocks[0].details.map(c=>c.id)],[13,14]);
 assert.equal(core.hasFeatureConfig(blocks[0]),true);
 const unrelated=core.featureBlocks(canonical([
  {id:21,type:'toggle',label:'装车',group:'自动功能',row:10,column:10},
  {id:22,type:'toggle',label:'维修',group:'自动功能',row:55,column:10},
  {id:23,type:'number',label:'修理阈值',group:'自动功能',row:100,column:10}
 ]));
 assert.ok(unrelated.every(block=>!block.title&&!block.headers.length),'a broad legacy section must not become a titled fold');
});
test('one feature-row grammar on every tactical page including independent rows',()=>{
 const sections={3:'移动、攻击与操作手势',4:'维修与自动救援',5:'科技与建造',6:'命令与兼容'};
 for(const navIndex of [3,4,5,6]){
  const f=setup(),nav=f.document.getElementById('navigation');
  f.dispatch(navButton(nav,navIndex),'click');
  const section=sections[navIndex],controls=[
   {id:4001,nativeId:7001,type:'toggle',groupHeader:true,featureId:7001,group:'功能甲',section,label:'功能甲',checked:true},
   {id:4002,nativeId:7002,type:'number',featureId:7001,group:'功能甲',section,label:'阈值',text:'50'},
   {id:4003,nativeId:7003,type:'hotkey',featureId:7001,group:'功能甲',section,label:'快捷键',options:['hold']},
   {id:4011,nativeId:7011,type:'toggle',group:'独立功能',label:'独立功能',checked:true},
   {id:4012,nativeId:7012,type:'button',group:'立即操作',label:'执行操作'}
  ];
  f.receive({kind:'state',protocol:1,module:2,generation:1,revision:1,page:'tactical/'+(navIndex-2),controls});
  f.render();
  assert.equal(f.find('feature-card').length,1,'one feature owner on page '+navIndex);
  assert.equal(f.find('feature-common').length,2,'plain commands use same row on page '+navIndex);
  const [button]=f.find('feature-config-toggle');
  assert.equal(button.hidden,false);
  assert.equal(button.textContent,'展开');
  assert.equal(f.find('setting-row').length,3,'no invisible child controls mounted');
  f.dispatch(button,'click');
  assert.equal(f.find('setting-row').length,5);
  assert.ok(f.find('feature-subheading').every(node=>node.hidden),'no redundant parameter/shortcut headings');
  assert.deepEqual(f.find('settings-group').map(node=>node.children[0].children[0].textContent).filter(Boolean).length>0,true);
 }
});
test('all page types share flat neutral feature surfaces and consistent nested controls',()=>{
 const css=fs.readFileSync(path.join(here,'../frontend/app.css'),'utf8');
 const start=css.indexOf('/* Universal feature hierarchy:');
 const end=css.indexOf('.group-status {',start);
 assert.ok(start>=0&&end>start);
 const featureCss=css.slice(start,end);
 assert.match(featureCss,/\.feature-card, \.feature-common\s*\{/);
 assert.match(featureCss,/border-bottom:1px solid var\(--divider\)/);
 assert.match(featureCss,/\.feature-card \.feature-details\s*\{/);
 assert.match(featureCss,/border-left:1px solid var\(--divider\)/);
 assert.doesNotMatch(featureCss,/var\(--blush/,'no colored second-level cards');
 assert.match(css,/\.settings-line:not\(\.compound-row\) > \.setting-row:not\(\[data-type="toggle"\]\)/);
 assert.match(css,/\.setting-row\[data-type="toggle"\] > \.setting-editor\s*\{/);
});
test('compact shortcuts retain trigger, double-click, special keys and clear',()=>{
 const f=setup(),row=f.row(3);
 assert.equal(row.input.textContent,'未绑定');
 assert.equal(row.mode.hidden,true);assert.equal(row.doubleLabel.hidden,true);assert.equal(row.clear.hidden,true);
 assert.equal(row.special.disabled,false);assert.ok(!row.hotkeyDetails.open);
 f.dispatch(row.hotkeySummary,'click');
 assert.ok(row.hotkeyDetails.open);
 f.dispatch(row.special,'keydown',{key:'Escape'});
 assert.equal(row.hotkeyDetails.open,false);
 f.receive({kind:'state',protocol:1,module:0,generation:1,revision:2,page:'cursor',controls:[
  {id:3,type:'hotkey',label:'快捷键',selected:0x124b,options:['press','release']}
 ]});
 row.control=f.store.getControl(3);f.context.__consoleDomFixture.patchRow(row);
 assert.equal(row.input.textContent,'Ctrl + K');
 assert.equal(row.mode.hidden,false);assert.equal(row.doubleLabel.hidden,false);assert.equal(row.clear.hidden,false);
 f.dispatch(row.hotkeySummary,'click');
 row.mode.value='8192';f.dispatch(row.mode,'change');
 assert.equal(f.actions().at(-1).value,0x224b);
 row.double.checked=true;f.dispatch(row.double,'change');
 assert.equal(f.actions().at(-1).value,0xa24b);
 row.special.value='27';f.dispatch(row.special,'change');
 assert.equal(f.actions().at(-1).value,0xa21b);
 assert.equal(row.hotkeyDetails.open,false);
 f.dispatch(row.hotkeySummary,'click');f.dispatch(row.clear,'click');
 assert.equal(f.actions().at(-1).value,0);
 assert.equal(row.hotkeyDetails.open,false);
});

test('named type filters fold independently and edit the original control token',()=>{
 const f=setup();
 f.dispatch(navButton(f.document.getElementById('navigation'),4),'click');
 const field=(id,type,label,extra)=>({id:id+60000,nativeId:id,type,label,visible:true,...extra});
 const scope={group:'自动装车',section:'适用单位清单',featureId:9208};
 const controls=[
  field(9208,'toggle','限定乘员类型',{...scope,checked:false}),
  field(9209,'text','乘员类型 ID',{...scope,enabled:false,text:'E1'}),
  field(9210,'toggle','限定载具类型',{...scope,checked:true}),
  field(9211,'text','载具类型 ID',{...scope,text:'HTK'}),
  field(9216,'toggle','限定卸载载具',{...scope,group:'智能卸载',featureId:9216})
 ];
 const state=(revision,controls)=>({kind:'state',protocol:1,module:2,generation:1,revision,page:'tactical/2',controls});
 f.receive(state(1,controls));f.render();
 const titles=f.find('feature-title').map(title=>title.children[0].textContent);
 assert.deepEqual(titles,['自动装车','智能卸载']);
 const folds=f.find('feature-config-toggle');assert.equal(folds.length,2);
 assert.equal(folds[0].attributes.get('aria-expanded'),'false');
 assert.equal(f.find('field').length,0);
 f.dispatch(folds[0],'click');
 assert.equal(folds[1].attributes.get('aria-expanded'),'false');
 const [passenger,transport]=f.find('field');
 assert.equal(passenger.disabled,true);assert.equal(transport.value,'HTK');
 const toggle=f.find('toggle')[0].children[0];toggle.checked=true;f.dispatch(toggle,'change');
 const action=f.actions().at(-1);
 assert.equal(action.id,69208);assert.equal(action.value,true);
 f.receive({kind:'ack',protocol:1,module:2,generation:1,id:action.id,seq:action.seq,ok:true,revision:2});
 f.receive(state(2,controls.map(c=>c.nativeId===9208?{...c,checked:true}:c.nativeId===9209?{...c,enabled:true}:c)));f.render();
 assert.equal(passenger.disabled,false);
 passenger.focus();passenger.value='E1,E2';f.dispatch(passenger,'input');f.dispatch(passenger,'keydown',{key:'Enter'});
 const edit=f.actions().at(-1);
 assert.equal(edit.module,2);assert.equal(edit.id,69209);assert.equal(edit.action,'text');assert.equal(edit.value,'E1,E2');
 f.dispatch(folds[0],'click');
 assert.equal(folds[0].attributes.get('aria-expanded'),'false');
});

test('feature settings and shortcuts stay attached to their master switch',()=>{
 const core=setup().context.RA2ConsoleCore;
 const controls=[
  {id:11,type:'toggle',label:'自动维修',group:'自动功能',groupHeader:true},
  {id:12,type:'number',label:'血量阈值',group:'自动功能'},
  {id:13,type:'hotkey',label:'快捷键',group:'自动功能'},
  {id:14,type:'toggle',label:'自动装车',group:'自动功能',groupHeader:true},
  {id:15,type:'number',label:'装载范围',group:'自动功能'}
 ].map(control=>core.canonical(control));
 const blocks=core.featureBlocks(controls);
 assert.equal(blocks.length,2);
 assert.equal(blocks[0].headers[0].id,11);
 assert.deepEqual(JSON.parse(JSON.stringify(blocks[0].details.map(c=>c.id))),[12,13]);
 assert.equal(blocks[1].headers[0].id,14);
 assert.deepEqual(JSON.parse(JSON.stringify(blocks[1].details.map(c=>c.id))),[15]);
 const filtered=core.groupControls(controls,'血量阈值').get('自动功能');
 assert.deepEqual(JSON.parse(JSON.stringify(filtered.map(c=>c.id))),[11,12]);
});

for(const id of [1,2])for(const key of ['Enter','Escape'])for(const composition of [{isComposing:true},{keyCode:229}]){
 test(`${id===1?'text':'editable choice'} keeps composition ${key} (${Object.keys(composition)[0]})`,()=>{
  const fixture=setup(),input=fixture.edit(id,'中文候选');
  const event=fixture.dispatch(input,'keydown',{key,code:key,...composition});
  assert.equal(fixture.actions().length,0,'IME candidate selection/cancellation must not submit a setting');
  assert.equal(fixture.document.activeElement,input,'IME owns focus until composition finishes');
  assert.equal(fixture.store.drafts.get(`0:${id}`).value,'中文候选');
  assert.equal(event.defaultPrevented,false,'the native composition event remains available to the IME');
 });
}
for(const composition of [{isComposing:true},{keyCode:229}]){
 test(`search keeps composition Escape (${Object.keys(composition)[0]})`,()=>{
  const fixture=setup(),input=fixture.document.getElementById('search');input.focus();input.value='中文候选';fixture.dispatch(input,'input');
  const event=fixture.dispatch(input,'keydown',{key:'Escape',code:'Escape',...composition});
  assert.equal(input.value,'中文候选');assert.equal(event.defaultPrevented,false);
 });
 test(`hotkey capture ignores IME event and remains armed (${Object.keys(composition)[0]})`,()=>{
  const fixture=setup(),button=fixture.row(3).input;fixture.dispatch(button,'click');
  const event=fixture.dispatch(button,'keydown',{key:'Enter',code:'Enter',...composition});
  assert.equal(fixture.actions().length,0);assert.equal(event.defaultPrevented,false);
  fixture.dispatch(button,'keydown',{key:'k',code:'KeyK',keyCode:75});
  assert.equal(fixture.actions().length,1);assert.equal(fixture.actions()[0].value&255,75);
 });
 test(`hotkey encoder does not infer a key from composition code (${Object.keys(composition)[0]})`,()=>{
  const fixture=setup();assert.equal(fixture.context.RA2ConsoleCore.encodeHotkey({key:'Process',code:'KeyQ',...composition}),null);
 });
}
test('ordinary Enter still commits a finished Unicode edit exactly once',()=>{
 const fixture=setup(),input=fixture.edit(1,'已确认的中文');
 fixture.dispatch(input,'keydown',{key:'Enter',code:'Enter',keyCode:13});
 assert.equal(fixture.actions().length,1);assert.equal(fixture.actions()[0].value,'已确认的中文');assert.equal(fixture.document.activeElement,null);
});
test('ordinary Escape still cancels an editor and clears search',()=>{
 const fixture=setup(),input=fixture.edit(1,'取消的修改');fixture.dispatch(input,'keydown',{key:'Escape',code:'Escape',keyCode:27});
 assert.equal(fixture.actions().length,0);assert.equal(fixture.store.drafts.size,0);
 const search=fixture.document.getElementById('search');search.value='搜索';fixture.dispatch(search,'keydown',{key:'Escape',keyCode:27});assert.equal(search.value,'');
});
test('ordinary settings never expose native numeric fallback or duplicate status rows',()=>{
 const f=setup(),core=f.context.RA2ConsoleCore;
 const field=(id,type,label,extra={})=>core.canonical({id,type,label,...extra});
 assert.equal(core.displaySettingLabel(field(1,'hotkey','设置 441')),'快捷键');
 assert.equal(core.displaySettingLabel(field(2,'choice','设置 410')),'选项');
 assert.equal(core.displaySettingLabel(field(3,'number','搜索半径')),'搜索半径');
 const controls=[
  field(4,'toggle','保持操作权限',{group:'实验功能'}),
  field(5,'label','保持操作权限：关闭',{nativeId:9537,group:'实验功能'}),
  field(6,'label','操控私有队列：原实现',{nativeId:9751,group:'实验功能'}),
  field(7,'label','保持操作权限：未能解除输入锁',{nativeId:9537,group:'实验功能',runtimeError:true}),
  field(8,'label','当前：60 Hz',{nativeId:9161,group:'实验功能'}),
  field(9,'label','小图相机：入口冲突或还原未完成',{nativeId:9464,group:'实验功能'})
 ];
 const ids=[...core.layoutGroups(controls,'',6).values()].flat().map(c=>c.id);
 assert.ok(!ids.includes(5)&&!ids.includes(6));
 assert.ok(ids.includes(4)&&ids.includes(7)&&ids.includes(8)&&ids.includes(9));
 assert.ok([...core.layoutGroups(controls,'',7).values()].flat().some(c=>c.id===5));
});
test('participant filters share their master feature and never create a duplicate card',()=>{
 const core=setup().context.RA2ConsoleCore;
 const controls=[];
 for(const [master,key,policy,scope,title] of [[885,9507,895,9236,'自动躲弹'],[880,9508,9040,9230,'防空火控'],[705,0,9195,9200,'飞机分配攻击']]){
  const row=(id,type,label,y,x,extra={})=>core.canonical({id:id+60000,nativeId:id,type,label,row:y,column:x,group:title,featureId:master,...extra});
  controls.push(row(master,'toggle',title,master,20,{groupHeader:true}));
  if(key)controls.push(row(key,'hotkey',title,master,345));
  controls.push(row(policy,'choice','运行方式',master+1,290));
  controls.push(row(scope,'toggle','限定参与单位',master+2,380));
  controls.push(row(scope+1,'text','单位类型 ID',master+3,20,{text:'E1'}));
 }
 const groups=core.layoutGroups(controls,'',4);
 const blocks=core.featureBlocks([...groups.values()][0]);
 assert.equal(blocks.length,3);
 for(const block of blocks){
  assert.equal(block.headers.filter(c=>c.type==='toggle').length,1);
  assert.ok(block.details.some(c=>c.type==='text'));
  assert.ok(block.details.some(c=>c.type==='choice'));
 }
 const searched=core.featureBlocks([...core.layoutGroups(controls,'单位类型 ID',4).values()][0]);
 assert.equal(searched.length,3);
 assert.ok(searched.every(b=>b.headers.some(c=>c.type==='toggle')));
});
test('all manual command type restrictions share one collapsed control entry',()=>{
 const core=setup().context.RA2ConsoleCore;
 const title='命令类型手动指定';
 const fields=[];
 for(let f=0;f<21;f++){
  if(f===17)continue;
  fields.push(core.canonical({id:9200+2*f,type:'toggle',label:'功能 '+f+' · 单位',group:title,section:title,featureId:9610,checked:false,row:100+f*2,column:20}));
  fields.push(core.canonical({id:9201+2*f,type:'text',label:'功能 '+f+' · 类型 ID',group:title,section:title,featureId:9610,enabled:false,row:101+f*2,column:20}));
 }
 for(const [id,type,label] of [[781,'toggle','矿车'],[783,'text','矿车类型 ID'],[782,'toggle','矿场'],[784,'text','矿场类型 ID']])
  fields.push(core.canonical({id,type,label,group:title,section:title,featureId:9610,row:200+id,column:20}));
 const master=core.canonical({id:705,type:'toggle',label:'飞机分配攻击',group:'飞机分配攻击',featureId:705,groupHeader:true});
 const blocks=core.featureBlocks([...core.layoutGroups([master,...fields],'',3).values()][0]);
 assert.equal(blocks.length,2);
 const manual=blocks.find(b=>b.title===title);
 assert.equal(manual.headers.length,0);
 assert.equal(manual.details.length,44);
 assert.equal(core.hasFeatureConfig(manual),true);
 assert.equal(blocks.find(b=>b.title==='飞机分配攻击').details.length,0);
 const searched=core.featureBlocks([...core.layoutGroups([master,...fields],'矿车类型 ID',3).values()][0]);
 assert.equal(searched.length,1);
 assert.equal(searched[0].title,title);
});
let failures=0;
for(const {name,run} of cases){try{run();console.log(`PASS ${name}`)}catch(error){failures++;console.error(`FAIL ${name}\n${error.stack}`)}}
console.log(JSON.stringify({suite:'console_composition',checks:cases.length,failures,level:'production listeners with controlled DOM event fixture'}));
process.exitCode=failures?1:0;
