#include "../source/choice_descriptions.h"
#include "../source/console_model.h"
#include <cassert>
#include <iostream>
#include <set>
int main(int argc,char**argv){
 std::set<std::vector<std::string>>seen;consolemodel::Snapshot snapshot;snapshot.module=0;snapshot.generation=1;snapshot.revision=1;snapshot.page="cursor";
 unsigned count=0;
 for(const auto&g:choicedescriptions::groups()){
  assert(g.options.size()>1&&g.options.size()==g.descriptions.size());assert(seen.insert(g.options).second);
  assert(choicedescriptions::describe(g.options)==g.descriptions);
  for(const auto&d:g.descriptions)assert(d.size()>40);
  consolemodel::Control c;c.id=++count;c.type="choice";c.readonly=true;c.label=g.options.front()=="文字标记"?"特殊单位显示方式":"实现方式";
  c.group=g.options.front();c.row=count*100;c.selected=0;c.options=g.options;c.optionDescriptions=choicedescriptions::describe(g.options);snapshot.controls.push_back(c);
 }
 assert(choicedescriptions::describe({"关闭","无关选项"}).empty());
 assert(choicedescriptions::describe({"文字标记","真实模型叠加","新选项"}).empty());
 assert(choicedescriptions::describe({"文字标记","真实模型叠加"})[1].find("不擦除")!=std::string::npos);
 auto high=choicedescriptions::describe({"仅光标","光标＋拖屏（实验性）","全部（实验性）","强制高刷（实验性 · 联机待验证）"});assert(high.size()==4&&high[1].find("暂不可用")!=std::string::npos);
 consolemodel::Control disabled;disabled.id=++count;disabled.type="choice";disabled.readonly=true;disabled.enabled=false;disabled.label="高刷范围";disabled.group="光标范围";disabled.row=count*100;disabled.selected=1;
 disabled.options={"仅光标","光标＋拖屏（实验性）","全部（实验性）","强制高刷（实验性 · 联机待验证）"};disabled.optionEnabled={true,false,false,false};disabled.optionDescriptions=high;snapshot.controls.push_back(disabled);
 auto json=consolemodel::encodeSnapshot(snapshot);assert(json.find("\"optionDescriptions\":[")!=std::string::npos);
 if(argc>1)std::cout<<json;else std::cout<<"PASS "<<count<<" mode groups, exact option matching, disabled help, typed JSON\n";
}
