import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import {fileURLToPath} from 'node:url';
const here=path.dirname(fileURLToPath(import.meta.url)),at=process.argv.indexOf('--app');
const app=at<0?path.join(here,'../frontend/app.js'):path.resolve(process.argv[at+1]);
const context=vm.createContext({});vm.runInContext(fs.readFileSync(app,'utf8'),context,{filename:app});
const {encodeHotkey,keyName}=context.RA2ConsoleCore;
const cases=[];
const test=(name,run)=>cases.push({name,run});
for(const [code,vk,label]of [
 ['NumpadMultiply',106,'Num *'],['NumpadAdd',107,'Num +'],['NumpadComma',108,'Num 分隔'],
 ['NumpadSubtract',109,'Num -'],['NumpadDecimal',110,'Num .'],['NumpadDivide',111,'Num /']
]){
 test(`${code} displays its captured key instead of a letter`,()=>{
  const value=encodeHotkey({code,keyCode:vk});assert.equal(value,vk);assert.equal(keyName(value),label);
  assert.equal(keyName(encodeHotkey({code,keyCode:vk,ctrlKey:true,shiftKey:true,altKey:true},0xa000)),`Ctrl + Shift + Alt + ${label}`);
 });
}
test('an unmapped accepted virtual key has a visible unambiguous fallback',()=>{
 for(const vk of [12,41,47,91,160,173,175,255])assert.equal(keyName(encodeHotkey({keyCode:vk})),`VK ${vk}`);
});
test('ordinary letters digits function keys and numpad digits retain their names',()=>{
 for(const [vk,label]of [[48,'0'],[57,'9'],[65,'A'],[90,'Z'],[96,'Num 0'],[105,'Num 9'],[112,'F1'],[135,'F24'],[13,'Enter'],[27,'Esc'],[32,'Space']])assert.equal(keyName(vk),label);
 assert.equal(keyName(0),'未绑定');
});
let failures=0;
for(const {name,run}of cases){try{run();console.log(`PASS ${name}`)}catch(error){failures++;console.error(`FAIL ${name}\n${error.stack}`)}}
console.log(JSON.stringify({suite:'console_hotkey_presentation',checks:cases.length,failures,level:'actual production encodeHotkey and keyName'}));
process.exitCode=failures?1:0;
