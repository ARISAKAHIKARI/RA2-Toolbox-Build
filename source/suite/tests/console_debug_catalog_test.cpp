// Portable encoding budget check using the actual product catalogue and serializer.
// This does not run a game or claim native Win32 control execution.
#include "../source/console_model.h"
#include "../source/settings_categories.h"
#include "../../shared/source/debug_entry_catalog.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <cstring>

int main(int argc,char**argv){
    consolemodel::Snapshot snapshot;snapshot.module=2;snapshot.generation=1;snapshot.revision=1;snapshot.page="tactical/5";snapshot.title=u8"调试";
    std::set<std::uint32_t> ids;std::size_t checks=0;
    auto check=[&](bool okay,const char*what){++checks;if(!okay){std::cerr<<"FAIL "<<what<<'\n';std::exit(1);}};
    check(suite::categories::Debug==5&&suite::categories::Count==6,"six categories preserve debug index");
    const int oldExtra[]={123};check(suite::categories::row(5,oldExtra,1)==suite::categories::Automatic,"legacy native section 5 retains automatic classification");
    for(const char*label:{u8"入口调试：用途、影响特性与实时安装状态",u8"每项独立保存。关闭校验不启用对应功能；调用、补丁、断点各自控制。"}){
        consolemodel::Control c;c.id=std::uint32_t(snapshot.controls.size()+1);c.type="label";c.label=c.text=label;c.readonly=true;snapshot.controls.push_back(c);
    }
    for(std::uint32_t i=0;i<debugentries::Count;++i){
        const auto&e=debugentries::Entries[i];consolemodel::Control c;
        c.id=9000+i;c.type="toggle";c.label=c.text=e.name;c.group=e.group;c.checked=true;c.enabled=(e.flags&debugentries::Checkable)!=0;c.row=int(i*40+100);c.column=20;
        c.hint=std::string(e.purpose)+u8"\r\n影响特性："+e.features+u8"\r\n关闭后仅跳过此入口的参考签名；线程、内存、原目标结构与恢复记录继续检查。";
        auto&d=c.debug;d.present=true;d.siteId=e.id;d.address=e.address;d.kind=e.kind==debugentries::Kind::Call?"call":e.kind==debugentries::Kind::Patch?"patch":e.kind==debugentries::Kind::Breakpoint?"breakpoint":"system";
        d.purpose=e.purpose;d.features=e.features;d.state=(e.flags&debugentries::Disabled)?"unused":"unknown";d.hook=e.kind==debugentries::Kind::Call?"na":"unknown";d.detail=u8"尚未连接运行组件；设置已保存在本机";
        check(ids.insert(e.id).second,"catalogue entry identity unique");check(d.siteId!=c.id,"display identity is independent of control token");
        snapshot.controls.push_back(std::move(c));
    }
    const auto initial=consolemodel::encodeSnapshot(snapshot);
    check(initial.size()<consolemodel::MaxMessageBytes,"complete catalogue fits 262144 byte budget");
    if(argc==3&&std::strcmp(argv[1],"--snapshot")==0){std::ofstream output(argv[2],std::ios::binary);output<<initial;check(bool(output),"snapshot written");}
    for(auto&c:snapshot.controls)if(c.debug.present){c.debug.ageMs=0xffffffffu;c.debug.sampled=true;c.debug.state="blocked";c.debug.hook="conflict";c.debug.detail=u8"签名不匹配；入口存在外部修改，仍保留本工具的恢复记录；错误码 -2147483648；采样已过期，当前状态待确认；新设置等待运行组件应用；此项签名校验已关闭";}
    const auto worst=consolemodel::encodeSnapshot(snapshot);check(worst.size()<consolemodel::MaxMessageBytes,"all entries with long diagnostics fit budget");
    std::cout<<"PASS debug catalogue: "<<debugentries::Count<<" entries, "<<initial.size()<<" initial bytes, "<<worst.size()<<" long-diagnostic bytes / "<<consolemodel::MaxMessageBytes<<", "<<checks<<" checks\n";
}
