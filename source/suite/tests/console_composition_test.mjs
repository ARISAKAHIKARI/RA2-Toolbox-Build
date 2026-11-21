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
  {id:4,type:'label',group:'防御',label:'等待对局'}
 ]});
 f.render();
 assert.ok(f.find('feature-subheading').every(title=>title.hidden));
 assert.ok(f.find('feature-config-toggle').every(button=>button.hidden));
 assert.equal(f.find('setting-label').some(label=>label.textContent==='等待对局'),false,'standalone explanatory labels are not printed into the settings list');
 assert.equal(f.find('label-help').length,1);
 assert.equal(f.find('label-help')[0].open,false);
 assert.equal(f.find('label-help')[0].children[0].textContent,'ⓘ');
 assert.ok(f.find('label-help')[0].children[1].children[0].textContent.includes('等待对局'));
});
test('unbound shortcuts show the capture and special-key picker only',()=>{
 const f=setup(),row=f.row(3);
 assert.equal(row.input.textContent,'未绑定');
 assert.equal(row.mode.hidden,true);assert.equal(row.doubleLabel.hidden,true);assert.equal(row.clear.hidden,true);
 assert.equal(row.special.disabled,false);
 f.receive({kind:'state',protocol:1,module:0,generation:1,revision:2,page:'cursor',controls:[
  {id:3,type:'hotkey',label:'快捷键',selected:0x124b,options:['press','release']}
 ]});
 row.control=f.store.getControl(3);f.context.__consoleDomFixture.patchRow(row);
 assert.equal(row.mode.hidden,false);assert.equal(row.doubleLabel.hidden,false);assert.equal(row.clear.hidden,false);
 assert.equal(row.input.textContent,'Ctrl + K');
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
let failures=0;
for(const {name,run} of cases){try{run();console.log(`PASS ${name}`)}catch(error){failures++;console.error(`FAIL ${name}\n${error.stack}`)}}
console.log(JSON.stringify({suite:'console_composition',checks:cases.length,failures,level:'production listeners with controlled DOM event fixture'}));
process.exitCode=failures?1:0;
