import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import {fileURLToPath} from 'node:url';
const here=path.dirname(fileURLToPath(import.meta.url)),app=path.join(here,'../frontend/app.js');
const source=fs.readFileSync(app,'utf8'),seam='  store.ready(); schedule();';
// Reuse the established controlled DOM fixture. Production event listeners,
// state reconciliation and help rendering are executed without substitution.
const fixture=fs.readFileSync(path.join(here,'console_composition_test.mjs'),'utf8');
const setup=new Function('vm','source','seam','app',fixture.slice(fixture.indexOf('function setup(){'),fixture.indexOf('\nconst cases=[];'))+'\nreturn setup;')(vm,source,seam,app);
const f=setup();
const choice={id:4,type:'choice',readonly:true,label:'特殊单位',options:['文字标记','不透明叠加'],optionDescriptions:['只显示文字','叠加真实外观，保留原画面'],optionEnabled:[true,true],selected:0};
const state=(control,revision)=>({kind:'state',protocol:1,module:0,generation:1,revision,page:'cursor',controls:[control]});
f.store.receive(state(choice,2));const row=f.row(4);
assert.equal(row.choiceHelp.hidden,false);assert.match(row.choiceCurrent.textContent,/文字标记：只显示文字/);assert.equal(row.choiceEntries.children.length,2);
assert.match(row.choiceEntries.children[1].children[1].textContent,/保留原画面/);
row.input.value='1';f.dispatch(row.input,'change');assert.equal(f.actions().length,1);assert.match(row.choiceCurrent.textContent,/不透明叠加：叠加真实外观/);
const action=f.actions()[0];f.store.receive({kind:'ack',protocol:1,module:0,generation:1,id:4,seq:action.seq,ok:false,error:'已停用'});
row.control=f.store.getControl(4);f.context.__consoleDomFixture.patchRow(row);assert.match(row.choiceCurrent.textContent,/文字标记：只显示文字/);
f.store.receive(state({...choice,selected:1,enabled:false,optionEnabled:[true,false]},3));row.control=f.store.getControl(4);f.context.__consoleDomFixture.patchRow(row);
assert.equal(row.input.disabled,true);assert.match(row.choiceCurrent.textContent,/当前不可用/);assert.equal(row.choiceHelp.hidden,false);
row.input.value='1';f.dispatch(row.input,'change');assert.equal(f.actions().length,1,'Reading help cannot enable a disabled mode');
f.store.receive(state({...choice,options:['旧一','旧二'],optionDescriptions:undefined},4));row.control=f.store.getControl(4);f.context.__consoleDomFixture.patchRow(row);assert.equal(row.choiceHelp.hidden,true,'Legacy snapshots do not retain unrelated help');
f.store.receive(state({...choice,optionDescriptions:['<img src=x onerror=bad()>','第二项']},5));row.control=f.store.getControl(4);f.context.__consoleDomFixture.patchRow(row);assert.match(row.choiceCurrent.textContent,/<img src=x/);assert.equal(row.choiceCurrent.children.length,0,'Help is plain text');
console.log('PASS current/all-mode help, pending selection, rejection rollback, disabled mode, legacy snapshots, text-only rendering');
