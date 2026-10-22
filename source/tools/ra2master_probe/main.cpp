#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include "diff_core.h"

using probe::Snapshot;
static const char* VERSION="0.1.0";
static std::wstring output;
static std::string utf8(const std::wstring& s) {
    if(s.empty())return {};
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);
    std::string r(n,' ');WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n,nullptr,nullptr);return r;
}
static std::string j(const std::string& s) {
    std::string r="\""; for(unsigned char c:s) {
        if(c=='"'||c=='\\'){r+='\\';r+=c;}
        else if(c<32){char b[7];snprintf(b,sizeof(b),"\\u%04x",c);r+=b;}
        else r+=c;
    } return r+'"';
}
static std::string hx(uint64_t n) {char b[32];snprintf(b,sizeof(b),"0x%llX",(unsigned long long)n);return b;}
static void say(const char* s){std::cout<<s<<std::endl;}
static std::string ask(const char* s) {
    say(s);std::string line;if(!std::getline(std::cin,line))throw std::runtime_error("输入已关闭。");
    if(line=="q"||line=="Q")throw std::runtime_error("用户结束采集；已完成的结果仍保留。");return line;
}
static std::string winerr(DWORD code=GetLastError()){return "Win32 error "+std::to_string(code);}
struct Handle {
    HANDLE h=nullptr;
    explicit Handle(HANDLE v=nullptr):h(v){}
    ~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
    Handle(const Handle&)=delete; Handle& operator=(const Handle&)=delete;
    operator HANDLE()const{return h;}
};
static void writeFile(const std::wstring& name,const void* data,size_t size) {
    Handle f(CreateFileW((output+L"\\"+name).c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(f.h==INVALID_HANDLE_VALUE)throw std::runtime_error("无法保存结果："+winerr());
    const char* p=(const char*)data;
    while(size){DWORD n=0,w=(DWORD)std::min(size,size_t(1<<20));
        if(!WriteFile(f,p,w,&n,nullptr)||n!=w)throw std::runtime_error("结果写入失败："+winerr());p+=n;size-=n;}
}
static void writeText(const std::wstring& name,const std::string& s){writeFile(name,s.data(),s.size());}
static std::string sha256(const std::wstring& path) {
    Handle f(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr));
    if(f.h==INVALID_HANDLE_VALUE)return "unavailable:"+winerr();
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return "unavailable:BCryptOpen";
    if(BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(alg,0);return "unavailable:BCryptCreate";}
    uint8_t buf[65536],digest[32];DWORD n=0;bool ok=true;
    while(true){if(!ReadFile(f,buf,sizeof(buf),&n,nullptr)){ok=false;break;}if(!n)break;
        if(BCryptHashData(hash,buf,n,0)<0){ok=false;break;}}
    if(ok)ok=BCryptFinishHash(hash,digest,32,0)>=0;
    BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);
    return ok?probe::hex(digest,32):"unavailable:hash_failed";
}
struct Proc {DWORD id;std::wstring name;};
static std::vector<Proc> processes() {
    Handle h(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));
    if(h.h==INVALID_HANDLE_VALUE)throw std::runtime_error("进程列表读取失败："+winerr());
    PROCESSENTRY32W e{};e.dwSize=sizeof(e);std::vector<Proc> r;
    if(Process32FirstW(h,&e))do{r.push_back({e.th32ProcessID,e.szExeFile});}while(Process32NextW(h,&e));return r;
}
static MODULEENTRY32W mainModule(DWORD pid) {
    HANDLE raw=INVALID_HANDLE_VALUE;
    for(int n=0;n<8;++n){raw=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
        if(raw!=INVALID_HANDLE_VALUE||GetLastError()!=ERROR_BAD_LENGTH)break;Sleep(30);}
    Handle h(raw);MODULEENTRY32W m{};m.dwSize=sizeof(m);
    if(h.h==INVALID_HANDLE_VALUE||!Module32FirstW(h,&m))throw std::runtime_error("主模块读取失败："+winerr());return m;
}
struct Section {std::string name;size_t begin,end;DWORD flags;};
struct Image {uintptr_t base;size_t size;std::wstring path;WORD machine;DWORD stamp;std::vector<Section> sections;};
static bool readable(DWORD p){return !(p&PAGE_GUARD)&&(p&0xff)!=PAGE_NOACCESS;}
static bool executable(DWORD p){p&=0xff;return p==PAGE_EXECUTE||p==PAGE_EXECUTE_READ||p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY;}
static Snapshot readImage(HANDLE h,uintptr_t base,size_t size) {
    Snapshot s;s.bytes.resize(size);s.valid.resize(size);s.stable.resize(size);
    for(size_t off=0;off<size;){const size_t n=std::min(size-off,size_t(4096-(base+off)%4096));
        MEMORY_BASIC_INFORMATION m{};SIZE_T got=0;
        if(VirtualQueryEx(h,(LPCVOID)(base+off),&m,sizeof(m))&&m.State==MEM_COMMIT&&readable(m.Protect)){
            ReadProcessMemory(h,(LPCVOID)(base+off),s.bytes.data()+off,n,&got);
            if(got<=n)std::fill(s.valid.begin()+off,s.valid.begin()+off+got,1);
        }off+=n;
    }return s;
}
static Image describe(HANDLE h,const MODULEENTRY32W& m) {
    Image x{(uintptr_t)m.modBaseAddr,m.modBaseSize,m.szExePath,0,0,{}};
    if(x.size<4096||x.size>128*1024*1024)throw std::runtime_error("主模块大小不受支持。");
    auto head=readImage(h,x.base,std::min(x.size,size_t(65536)));
    auto checked=[&](size_t off,size_t n){if(off>head.bytes.size()||n>head.bytes.size()-off)throw std::runtime_error("PE 头越界。");
        for(size_t i=off;i<off+n;++i)if(!head.valid[i])throw std::runtime_error("PE 头不可读。");};
    checked(0,sizeof(IMAGE_DOS_HEADER));IMAGE_DOS_HEADER d{};memcpy(&d,head.bytes.data(),sizeof(d));
    if(d.e_magic!=IMAGE_DOS_SIGNATURE||d.e_lfanew<0)throw std::runtime_error("非有效 PE。");
    size_t nt=(size_t)d.e_lfanew;checked(nt,4+sizeof(IMAGE_FILE_HEADER));DWORD sig=0;memcpy(&sig,head.bytes.data()+nt,4);
    if(sig!=IMAGE_NT_SIGNATURE)throw std::runtime_error("PE 签名无效。");
    IMAGE_FILE_HEADER f{};memcpy(&f,head.bytes.data()+nt+4,sizeof(f));x.machine=f.Machine;x.stamp=f.TimeDateStamp;
    if(f.Machine!=IMAGE_FILE_MACHINE_I386)throw std::runtime_error("目标不是 32 位游戏/修改大师，请重新选择。");
    size_t so=nt+4+sizeof(f)+f.SizeOfOptionalHeader;
    if(f.NumberOfSections>96)throw std::runtime_error("PE 节表异常。");
    checked(so,f.NumberOfSections*sizeof(IMAGE_SECTION_HEADER));
    for(unsigned i=0;i<f.NumberOfSections;++i){IMAGE_SECTION_HEADER sh{};memcpy(&sh,head.bytes.data()+so+i*sizeof(sh),sizeof(sh));
        char name[9]{};memcpy(name,sh.Name,8);size_t b=sh.VirtualAddress,n=std::max(sh.Misc.VirtualSize,sh.SizeOfRawData);
        if(b<x.size)x.sections.push_back({name,b,std::min(x.size,b+n),sh.Characteristics});}
    return x;
}
static std::string modules(DWORD pid) {
    Handle h(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid));
    std::ostringstream o;o<<"[";bool comma=false;MODULEENTRY32W m{};m.dwSize=sizeof(m);
    if(h.h!=INVALID_HANDLE_VALUE&&Module32FirstW(h,&m)){do{
        if(comma)o<<",";comma=true;o<<"{\"name\":"<<j(utf8(m.szModule))<<",\"base\":"<<j(hx((uintptr_t)m.modBaseAddr))<<",\"size\":"<<m.modBaseSize<<"}";
    }while(Module32NextW(h,&m));}o<<"]";return o.str();
}
static std::string imageMeta(const Image& im,DWORD pid) {
    std::ostringstream o;const auto slash=im.path.find_last_of(L"\\/");
    o<<"\"pid\":"<<pid<<",\"name\":"<<j(utf8(im.path.substr(slash==std::wstring::npos?0:slash+1)))
     <<",\"image_base\":"<<j(hx(im.base))<<",\"image_size\":"<<im.size<<",\"machine\":"<<im.machine
     <<",\"pe_timestamp\":"<<im.stamp<<",\"file_sha256\":"<<j(sha256(im.path))<<",\"sections\":[";
    bool comma=false;for(auto& s:im.sections){if(comma)o<<",";comma=true;o<<"{\"name\":"<<j(s.name)<<",\"rva\":"<<s.begin<<",\"size\":"<<s.end-s.begin<<",\"characteristics\":"<<s.flags<<"}";}o<<"]";return o.str();
}
static Snapshot capture(HANDLE h,const Image& im,DWORD pid,const std::wstring& label) {
    DWORD code=0;if(!GetExitCodeProcess(h,&code)||code!=STILL_ACTIVE)throw std::runtime_error("游戏已退出。请重新启动一轮采集。");
    auto module=mainModule(pid);if((uintptr_t)module.modBaseAddr!=im.base||module.modBaseSize!=im.size)throw std::runtime_error("目标主模块已变化。");
    say("正在采样…");auto a=readImage(h,im.base,im.size);Sleep(150);auto b=readImage(h,im.base,im.size);
    size_t valid=0,stable=0;for(size_t i=0;i<im.size;++i){valid+=a.valid[i]!=0;a.stable[i]=a.valid[i]&&b.valid[i]&&a.bytes[i]==b.bytes[i];stable+=a.stable[i]!=0;}
    if(!valid)throw std::runtime_error("主模块没有读到任何字节；检查权限。");
    std::ostringstream meta;meta<<"{\"stage\":"<<j(utf8(label))<<",\"tick_ms\":"<<GetTickCount64()<<",\"read_bytes\":"<<valid
        <<",\"stable_bytes\":"<<stable<<",\"image_bytes\":"<<im.size<<",\"modules\":"<<modules(pid)<<"}";
    writeText(label+L"-capture.json",meta.str());std::cout<<"已读 "<<valid<<" / "<<im.size<<" 字节。"<<std::endl;return a;
}
static size_t diff(const Image& im,const Snapshot& a,const Snapshot& b,const Snapshot* c,const std::wstring& name) {
    std::ostringstream o;o<<"{\"schema\":1,\"image_base\":"<<j(hx(im.base))<<",\"mode\":"<<j(c?"stable_off_on_off_candidates":"pairwise_observation")
        <<",\"warning\":\"Correlation only; not a traced writer or proof of multiplayer synchronization\",\"changes\":[";
    bool comma=false;size_t total=0,emitted=0,skipped=0;
    for(const auto& s:im.sections){
        const bool code=(s.flags&IMAGE_SCN_MEM_EXECUTE)!=0;
        if(!c&&!code)continue;
        auto r=probe::runs(s.begin,s.end,[&](size_t i){return c?probe::reversible(a,b,*c,i):probe::changed(a,b,i);});
        for(auto run:r){total+=run.length;if(emitted>=30000){++skipped;continue;}++emitted;
            if(comma)o<<",";comma=true;const size_t i=run.offset,n=run.length,lo=i>16?std::max(s.begin,i-16):s.begin,hi=std::min(s.end,i+n+16);
            o<<"{\"section\":"<<j(s.name)<<",\"executable\":"<<(code?"true":"false")<<",\"rva\":"<<j(hx(i))<<",\"va\":"<<j(hx(im.base+i))
                <<",\"length\":"<<n<<",\"before\":"<<j(probe::hex(a.bytes.data()+i,n))<<",\"enabled\":"<<j(probe::hex(b.bytes.data()+i,n));
            if(c)o<<",\"disabled_again\":"<<j(probe::hex(c->bytes.data()+i,n));
            o<<",\"context_rva\":"<<j(hx(lo))<<",\"context_before\":"<<j(probe::hex(a.bytes.data()+lo,hi-lo))
                <<",\"context_enabled\":"<<j(probe::hex(b.bytes.data()+lo,hi-lo))<<",\"context_before_valid\":"<<j(probe::hex(a.valid.data()+lo,hi-lo))
                <<",\"context_enabled_valid\":"<<j(probe::hex(b.valid.data()+lo,hi-lo))<<"}";
        }
    }
    o<<"],\"changed_bytes\":"<<total<<",\"emitted_runs\":"<<emitted<<",\"omitted_runs\":"<<skipped<<"}";
    writeText(name+L".json",o.str());return total;
}
// Supplementary injection evidence: only private executable pages in the selected game.
// No heaps, stacks, screenshots, or other processes are captured here.
static void privateCode(HANDLE h,const std::wstring& label) {
    std::ostringstream meta;meta<<"[";bool comma=false;size_t used=0;uintptr_t addr=0;MEMORY_BASIC_INFORMATION m{};unsigned k=0;
    while(addr<0x100000000ull&&VirtualQueryEx(h,(LPCVOID)addr,&m,sizeof(m))){
        uintptr_t base=(uintptr_t)m.BaseAddress,next=base+m.RegionSize;if(next<=addr)break;addr=next;
        if(m.State!=MEM_COMMIT||m.Type!=MEM_PRIVATE||!executable(m.Protect))continue;
        size_t n=std::min(size_t(m.RegionSize),size_t(1024*1024));n=std::min(n,size_t(8*1024*1024)-used);
        std::wstring file=label+L"-private-"+std::to_wstring(k++)+L".mem";size_t valid=0;
        if(n){auto s=readImage(h,base,n);for(auto v:s.valid)valid+=v!=0;
            writeFile(file,s.bytes.data(),n);writeFile(file+L".valid",s.valid.data(),n);used+=n;}
        if(comma)meta<<",";comma=true;meta<<"{\"base\":"<<j(hx(base))<<",\"allocation_base\":"<<j(hx((uintptr_t)m.AllocationBase))
            <<",\"region_size\":"<<m.RegionSize<<",\"protection\":"<<m.Protect<<",\"saved_bytes\":"<<n<<",\"valid_bytes\":"<<valid
            <<",\"truncated\":"<<(n<m.RegionSize?"true":"false")<<",\"file\":"<<(n?j(utf8(file)):"null")<<"}";
    }meta<<"]";writeText(label+L"-private-regions.json",meta.str());
}
static DWORD selectGame() {
    auto all=processes();std::vector<Proc> games;bool spawn=false;
    for(auto& p:all){if(!_wcsicmp(p.name.c_str(),L"gamemd.exe"))games.push_back(p);
        if(!_wcsicmp(p.name.c_str(),L"gamemd-spawn.exe"))spawn=true;}
    if(games.empty())throw std::runtime_error(spawn?"只找到 gamemd-spawn.exe。请启动修改大师能识别的 gamemd.exe；本工具不会自动把 spawn 当作它。":"未找到 gamemd.exe。请先进入单机对局，再运行本工具。");
    if(games.size()==1){std::cout<<"目标 gamemd.exe，PID "<<games[0].id<<std::endl;return games[0].id;}
    say("找到多个 gamemd.exe，请输入要采集的 PID：");for(auto& p:games)std::cout<<p.id<<std::endl;
    auto line=ask("PID（q 退出）：");DWORD pid=(DWORD)std::stoul(line);for(auto& p:games)if(p.id==pid)return pid;
    throw std::runtime_error("PID 不在 gamemd.exe 列表中。");
}
static void dumpTrainer() {
    auto line=ask("是否采集修改大师运行后的主模块？[回车=是，n=跳过]（用于查看解压后的代码）");
    if(line=="n"||line=="N")return;
    auto all=processes();std::vector<Proc> choices;
    for(auto p:all){std::wstring lower=p.name;for(auto& c:lower)c=towlower(c);
        if(lower.find(L"修改大师")!=std::wstring::npos||lower.find(L"修改大師")!=std::wstring::npos||lower.find(L"ra2master")!=std::wstring::npos){
            if(p.id!=GetCurrentProcessId())choices.push_back(p);}}
    say("候选修改大师进程：");for(auto& p:choices)std::cout<<p.id<<"  "<<utf8(p.name)<<std::endl;
    std::string id=ask("输入修改大师 PID（任务管理器“详细信息”可查；回车跳过）：");if(id.empty())return;
    DWORD pid=(DWORD)std::stoul(id);std::wstring pname;for(auto& p:all)if(p.id==pid)pname=p.name;
    if(pname.empty())throw std::runtime_error("PID 不存在。");
    if(!_wcsicmp(pname.c_str(),L"gamemd.exe")||pname.find(L"spawn")!=std::wstring::npos)throw std::runtime_error("此项应选择修改大师，不是游戏进程。");
    std::cout<<"将采集："<<utf8(pname)<<"，PID "<<pid<<std::endl;
    if(ask("确认它就是修改大师？输入 y 继续：")!="y")return;
    Handle h(OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid));
    if(!h.h)throw std::runtime_error("修改大师读取权限不足："+winerr());
    auto im=describe(h,mainModule(pid));auto s=readImage(h,im.base,im.size);size_t count=0;for(auto v:s.valid)count+=v!=0;
    if(!count)throw std::runtime_error("修改大师主模块完全不可读。");
    writeFile(L"trainer-main.mem",s.bytes.data(),s.bytes.size());writeFile(L"trainer-main.valid",s.valid.data(),s.valid.size());
    writeText(L"trainer-main.json","{\"layout\":\"memory_image_offset_equals_RVA\",\"unread_bytes_are_zero\":true,"+imageMeta(im,pid)+",\"read_bytes\":"+std::to_string(count)+",\"modules\":"+modules(pid)+"}");
    say("修改大师主模块已保存。它是内存映像，不能作为 EXE 运行，也不代表虚拟化代码已还原。");
}
static void setupOutput() {
    wchar_t exe[32768];DWORD n=GetModuleFileNameW(nullptr,exe,32768);if(!n||n>=32768)throw std::runtime_error("自身路径读取失败。");
    std::wstring root(exe,n);root=root.substr(0,root.find_last_of(L"\\/"));SYSTEMTIME st{};GetLocalTime(&st);wchar_t name[100];
    swprintf(name,100,L"\\RA2Master-Capture-%04u%02u%02u-%02u%02u%02u-%lu",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,GetCurrentProcessId());
    output=root+name;if(!CreateDirectoryW(output.c_str(),nullptr))throw std::runtime_error("结果目录创建失败；请先解压到可写文件夹："+winerr());
}
int main() {
    SetConsoleOutputCP(CP_UTF8);SetConsoleCP(CP_UTF8);
    try {
        setupOutput();std::cout<<"RA2Master 实机采集工具 v"<<VERSION<<std::endl;
        if(ask("模式：回车=游戏开关对比；输入 2=只采集修改大师主模块") == "2") {
            dumpTrainer();std::cout<<"结果目录："<<utf8(output)<<std::endl;ask("回车关闭窗口：");return 0;
        }
        say("请先进入 gamemd.exe 的单机对局，退出 RA2 Toolbox，暂时关闭修改大师。\n每一步只改变指定开关；建议暂停游戏。回到此窗口按回车采样，q 退出。\n本工具只请求查询/读取权限，不注入、不修改游戏，也不自动上传。\n结果中含代码片段及可选的修改大师内存映像，请直接回传分析，不要公开发布。\n");
        DWORD pid=selectGame();Handle h(OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid));
        if(!h.h)throw std::runtime_error("无法读取游戏："+winerr()+"。若游戏以管理员身份运行，请同样以管理员运行本工具。");
        Image im=describe(h,mainModule(pid));writeText(L"session.json","{\"tool_version\":"+j(VERSION)+",\"target_name_policy\":\"exact_gamemd.exe\","+imageMeta(im,pid)+"}");
        writeText(L"README.txt","Read-only diagnostic capture. Partial output is useful. *.json contains addresses, changed bytes and read coverage. *.mem is a raw memory-layout image, NOT an executable. *.valid has one byte per captured byte (1=read, 0=unread). No network tracing, API writer tracing or multiplayer proof. Do not publish private raw captures.\r\n");
        ask("[1/4] 修改大师尚未启动。回车采集游戏原始状态：");auto clean=capture(h,im,pid,L"00-clean");privateCode(h,L"00-clean");
        ask("[2/4] 打开修改大师，确认已识别游戏，“控制全部”关闭，其余功能保持关闭。回车：");auto off=capture(h,im,pid,L"01-off");
        size_t init=diff(im,clean,off,nullptr,L"01-trainer-start-code");
        ask("[3/4] 仅开启修改大师的“控制全部”（名称相近的控制所有单位功能）。回车：");auto on=capture(h,im,pid,L"02-control-on");
        size_t enable=diff(im,off,on,nullptr,L"02-control-on-code");privateCode(h,L"02-control-on");
        ask("[4/4] 关闭刚才的“控制全部”，保持当前对局。回车：");auto again=capture(h,im,pid,L"03-control-off");
        size_t disable=diff(im,on,again,nullptr,L"03-control-off-code");
        size_t rev=diff(im,off,on,&again,L"04-control-reversible");
        std::ostringstream summary;summary<<"Tool "<<VERSION<<"\nTrainer-start executable changed bytes: "<<init<<"\nControl-on executable changed bytes: "<<enable<<"\nControl-off executable changed bytes: "<<disable<<"\nStable off/on/off reversible bytes (all PE sections): "<<rev<<"\nThese are candidate correlations, not proof of the writer or network synchronization.\n";
        writeText(L"summary.txt",summary.str());say("控制全部的采集已保存。");
        if(ask("如果修改大师还有独立的“卖别人建筑”开关，要继续采集吗？[y=继续，回车=跳过]")=="y"){
            ask("保持控制全部关闭、卖别人建筑关闭，回车采集：");auto s0=capture(h,im,pid,L"10-sell-off");
            ask("只开启“卖别人建筑”，回车采集：");auto s1=capture(h,im,pid,L"11-sell-on");diff(im,s0,s1,nullptr,L"11-sell-on-code");privateCode(h,L"11-sell-on");
            ask("再关闭“卖别人建筑”，回车采集：");auto s2=capture(h,im,pid,L"12-sell-off");diff(im,s1,s2,nullptr,L"12-sell-off-code");diff(im,s0,s1,&s2,L"13-sell-reversible");
        }
        try{dumpTrainer();}catch(const std::exception& e){say(e.what());writeText(L"trainer-error.txt",e.what());}
        say("完成。把下面整个结果文件夹压缩为 ZIP 发回来即可：");std::cout<<utf8(output)<<std::endl;
        ask("回车关闭窗口：");return 0;
    }catch(const std::exception& e){say(e.what());if(!output.empty()){
        try{writeText(L"error.txt",e.what());}catch(...){}std::cout<<"已有结果："<<utf8(output)<<std::endl;}
        say("按回车退出。");std::string s;std::getline(std::cin,s);return 1;}
}
