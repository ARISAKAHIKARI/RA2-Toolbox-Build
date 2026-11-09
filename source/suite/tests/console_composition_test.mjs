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
 const globalListeners=new Map();
 const context=vm.createContext({console,Date,Map,Set,Object,Number,String,Boolean,RangeError,document,
  requestAnimationFrame(){},setTimeout(){return 1},clearTimeout(){},setInterval(){return 1},
  addEventListener(type,listener){globalListeners.set(type,listener)},
  chrome:{webview:{postMessage(message){sent.push(message)},addEventListener(){}}}
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
 vm.runInContext(source.replace(seam,'  global.__consoleDomFixture={makeRow,patchRow};\n'+seam),context,{filename:app});
 const store=context.RA2Console.store;
 store.receive({kind:'state',protocol:1,module:0,generation:1,revision:1,page:'cursor',controls:[
  {id:1,type:'text',label:'模糊关键词',text:'old'},
  {id:2,type:'choice',label:'自定义值',text:'old',options:['old'],selected:0,readonly:false,presentation:'editable'},
  {id:3,type:'hotkey',label:'快捷键',selected:0,options:['press','release']}
 ]});
 function row(id){const result=context.__consoleDomFixture.makeRow(store.getControl(id));context.__consoleDomFixture.patchRow(result);return result}
 function edit(id,value){const result=row(id),input=result.textInput||result.input;input.focus();input.value=value;dispatch(input,'input');return input}
 const actions=()=>sent.filter(message=>message.kind==='action');
 return {context,document,store,elements,sent,dispatch,row,edit,actions};
}
const cases=[];
const test=(name,run)=>cases.push({name,run});

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
