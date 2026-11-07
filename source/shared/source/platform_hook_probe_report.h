#pragma once
// Portable, bounded report model and serializers for the read-only hook probe.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace hookprobe {
struct Options {
 uint32_t pid=0;std::wstring baselineExe,outputDirectory;
};
struct Progress {
 std::atomic<bool> cancel{false};
 std::atomic<uint32_t> modulesDone{0},modulesTotal{0},findings{0};
};
struct Result {
 bool success=false,cancelled=false,partial=false;
 std::wstring summary,jsonPath,htmlPath;uint32_t modulesScanned=0,findings=0;
};
struct FileIdentity {
 std::wstring path;std::string sha256,identity;
 uint64_t bytes=0,writeTime=0,readStartedMs=0,readFinishedMs=0,readRequestedBytes=0;
 bool unchangedAfterRead=false,unchangedAtFinish=false;
};
struct ChainStep {
 uint64_t address=0,target=0,pointerAddress=0,exportDisplacement=0;
 std::string kind,status,targetModule,nearestExport;
 bool indirect=false,call=false;
};
struct Difference {
 uint32_t module=0,rva=0,length=0;uint64_t address=0,candidateAddress=0;
 std::string section,type="code-bytes",stability="stable-between-two-reads",expected,actual;
 std::vector<std::string> referenceIntersections;
 std::vector<uint8_t> candidateBytes;
 std::vector<ChainStep> chain;std::string chainStatus="no-recognized-transfer-candidate";
};
struct DeclarationRecord {uint32_t site=0,bytes=0;std::string name;};
struct SectionCoverage {
 std::string name,status="unscanned";uint32_t rva=0,size=0;
 uint64_t comparedBytes=0,ignoredBytes=0,unreadableBytes=0,unscannedBytes=0,changedBytes=0,dynamicBytes=0;
};
struct ModuleRecord {
 std::wstring path,baselinePath;uint64_t base=0;uint32_t mappedSize=0;
 bool main=false,system=false,pe64=false,structuralMatch=false,stableIdentity=false,metadataPartial=false;
 std::string name,status="unscanned",baselineSource="loaded-module-disk-path",identityCheck="not-checked",error;
 uint32_t machine=0,timestamp=0,imageSize=0,entryRva=0,relocations=0;
 uint64_t preferredBase=0;
 FileIdentity file;
 std::vector<SectionCoverage> sections;std::vector<std::string> notes;
 uint64_t comparedBytes=0,ignoredBytes=0,unreadableBytes=0,unscannedBytes=0,changedBytes=0,dynamicBytes=0;
 std::string declarationStatus="not-present",declarationSource="normalized-disk-baseline";
 uint64_t declarationBytes=0,declarationReadableBytes=0;
 bool declarationMemoryDiffers=false;
 std::vector<DeclarationRecord> declarations;
};
struct ReferenceRecord {
 uint32_t id=0,canonicalAddress=0,size=0;
 std::string kind,name,purpose,features,rangeSource,status="reference-only-layout-not-verified";
 uint32_t intersections=0;
};
struct ProbeReport {
 std::string format="ra2-readonly-hook-probe-v1",startedUtc,finishedUtc,processCreationTime;
 uint32_t pid=0;uint64_t elapsedMs=0;
 bool partial=false,cancelled=false,processAliveAtEnd=false,moduleListStable=false,enumerationComplete=false;
 std::string outcome="unknown",error;
 std::vector<ModuleRecord> modules;std::vector<Difference> differences;std::vector<ReferenceRecord> references;
 uint32_t referenceRangeCount=0;
 std::vector<std::string> notes;
};
inline std::string utf8(const std::wstring&w){
 std::string s;for(size_t i=0;i<w.size();++i){uint32_t c=uint32_t(w[i]);
  if(sizeof(wchar_t)==2&&c>=0xd800&&c<=0xdbff){if(i+1<w.size()&&uint32_t(w[i+1])>=0xdc00&&uint32_t(w[i+1])<=0xdfff)c=0x10000+((c-0xd800)<<10)+(uint32_t(w[++i])-0xdc00);else c=0xfffd;}
  else if(c>=0xd800&&c<=0xdfff)c=0xfffd;
  if(c>0x10ffff)c=0xfffd;
  if(c<0x80)s+=char(c);else if(c<0x800){s+=char(0xc0|(c>>6));s+=char(0x80|(c&63));}
  else if(c<0x10000){s+=char(0xe0|(c>>12));s+=char(0x80|((c>>6)&63));s+=char(0x80|(c&63));}
  else{s+=char(0xf0|(c>>18));s+=char(0x80|((c>>12)&63));s+=char(0x80|((c>>6)&63));s+=char(0x80|(c&63));}
 }return s;
}
inline std::string validUtf8(const std::string&s){
 std::string o;for(size_t i=0;i<s.size();){const uint8_t c=uint8_t(s[i]);if(c<128){o+=s[i++];continue;}
  size_t n=c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;bool ok=n&&i+n<=s.size();
  for(size_t j=1;ok&&j<n;++j)ok=(uint8_t(s[i+j])&0xc0)==0x80;
  if(ok&&n==3)ok=!(c==0xe0&&uint8_t(s[i+1])<0xa0)&&!(c==0xed&&uint8_t(s[i+1])>=0xa0);
  if(ok&&n==4)ok=!(c==0xf0&&uint8_t(s[i+1])<0x90)&&!(c==0xf4&&uint8_t(s[i+1])>=0x90);
  if(ok){o.append(s,i,n);i+=n;}else{o+="\xef\xbf\xbd";++i;}
 }return o;
}
inline std::string hexAddress(uint64_t n){std::ostringstream s;s<<"0x"<<std::hex<<std::uppercase<<n;return s.str();}
inline std::string hexBytes(const uint8_t*p,size_t n,size_t maximum=32){
 static const char h[]="0123456789ABCDEF";std::string s;for(size_t i=0;i<(std::min)(n,maximum);++i){if(i)s+=' ';s+=h[p[i]>>4];s+=h[p[i]&15];}return s;
}
inline std::string jsonString(const std::string&v){
 const std::string s=validUtf8(v.size()>2048?v.substr(0,2048)+" [text truncated]":v);std::string o="\"";static const char h[]="0123456789abcdef";
 for(unsigned char c:s){if(c=='"')o+="\\\"";else if(c=='\\')o+="\\\\";else if(c<32||c==127){o+="\\u00";o+=h[c>>4];o+=h[c&15];}else o+=char(c);}return o+'"';
}
inline std::string htmlText(const std::string&v){
 std::string o;for(unsigned char c:validUtf8(v.size()>2048?v.substr(0,2048)+" [text truncated]":v)){switch(c){case '&':o+="&amp;";break;case '<':o+="&lt;";break;case '>':o+="&gt;";break;case '"':o+="&quot;";break;case '\'':o+="&#39;";break;default:if(c<32&&c!='\n'&&c!='\r'&&c!='\t')o+="&#xfffd;";else o+=char(c);}}return o;
}
inline const char*jsonBool(bool b){return b?"true":"false";}
inline void jsonStrings(std::ostream&o,const std::vector<std::string>&v){o<<'[';for(size_t i=0;i<v.size();++i){if(i)o<<',';o<<jsonString(v[i]);}o<<']';}
inline void jsonCoverage(std::ostream&o,uint64_t compared,uint64_t ignored,uint64_t unread,uint64_t unscanned,uint64_t changed,uint64_t dynamic){
 o<<"\"compared_bytes\":"<<compared<<",\"loader_ignored_bytes\":"<<ignored<<",\"unreadable_bytes\":"<<unread<<",\"unscanned_bytes\":"<<unscanned<<",\"changed_bytes\":"<<changed<<",\"dynamic_between_reads_bytes\":"<<dynamic;
}
inline std::string reportJson(const ProbeReport&r){
 std::ostringstream o;o<<"{\n\"format\":"<<jsonString(r.format)<<",\"pid\":"<<r.pid<<",\"process_creation_time\":"<<jsonString(r.processCreationTime)
 <<",\"started_utc\":"<<jsonString(r.startedUtc)<<",\"finished_utc\":"<<jsonString(r.finishedUtc)<<",\"elapsed_ms\":"<<r.elapsedMs
 <<",\"partial\":"<<jsonBool(r.partial)<<",\"cancelled\":"<<jsonBool(r.cancelled)<<",\"process_alive_at_end\":"<<jsonBool(r.processAliveAtEnd)
 <<",\"module_list_stable\":"<<jsonBool(r.moduleListStable)<<",\"enumeration_complete\":"<<jsonBool(r.enumerationComplete)<<",\"outcome\":"<<jsonString(r.outcome)<<",\"error\":"<<jsonString(r.error)
 <<",\"writer_attribution\":\"unknown; transfer destination is not writer identity\",\"instruction_boundaries_verified\":false,\"notes\":";jsonStrings(o,r.notes);
 o<<",\n\"modules\":[";
 for(size_t i=0;i<r.modules.size();++i){if(i)o<<',';const auto&m=r.modules[i];o<<"\n{\"index\":"<<i<<",\"name\":"<<jsonString(m.name)<<",\"path\":"<<jsonString(utf8(m.path))<<",\"base\":"<<jsonString(hexAddress(m.base))<<",\"mapped_size\":"<<m.mappedSize<<",\"main\":"<<jsonBool(m.main)<<",\"system\":"<<jsonBool(m.system)<<",\"status\":"<<jsonString(m.status)<<",\"error\":"<<jsonString(m.error)
 <<",\"baseline_source\":"<<jsonString(m.baselineSource)<<",\"baseline_path\":"<<jsonString(utf8(m.baselinePath))<<",\"structural_match\":"<<jsonBool(m.structuralMatch)<<",\"identity_check\":"<<jsonString(m.identityCheck)<<",\"metadata_partial\":"<<jsonBool(m.metadataPartial)<<",\"stable_memory_identity\":"<<jsonBool(m.stableIdentity)
 <<",\"pe\":{\"machine\":"<<m.machine<<",\"pe64\":"<<jsonBool(m.pe64)<<",\"timestamp\":"<<m.timestamp<<",\"image_size\":"<<m.imageSize<<",\"entry_rva\":"<<jsonString(hexAddress(m.entryRva))<<",\"preferred_base\":"<<jsonString(hexAddress(m.preferredBase))<<",\"relocations_applied\":"<<m.relocations<<"},\"baseline_file\":{\"sha256\":"<<jsonString(m.file.sha256)<<",\"file_identity\":"<<jsonString(m.file.identity)<<",\"bytes\":"<<m.file.bytes<<",\"write_time\":"<<jsonString(hexAddress(m.file.writeTime))<<",\"read_requested_bytes\":"<<m.file.readRequestedBytes<<",\"read_started_ms\":"<<m.file.readStartedMs<<",\"read_finished_ms\":"<<m.file.readFinishedMs<<",\"unchanged_after_read\":"<<jsonBool(m.file.unchangedAfterRead)<<",\"unchanged_at_finish\":"<<jsonBool(m.file.unchangedAtFinish)<<"},\"coverage\":{";
 jsonCoverage(o,m.comparedBytes,m.ignoredBytes,m.unreadableBytes,m.unscannedBytes,m.changedBytes,m.dynamicBytes);o<<"},\"sections\":[";
 for(size_t j=0;j<m.sections.size();++j){if(j)o<<',';const auto&s=m.sections[j];o<<"{\"name\":"<<jsonString(s.name)<<",\"rva\":"<<jsonString(hexAddress(s.rva))<<",\"size\":"<<s.size<<",\"status\":"<<jsonString(s.status)<<',';jsonCoverage(o,s.comparedBytes,s.ignoredBytes,s.unreadableBytes,s.unscannedBytes,s.changedBytes,s.dynamicBytes);o<<'}';}
 o<<"],\"hook_declarations\":{\"status\":"<<jsonString(m.declarationStatus)<<",\"source\":"<<jsonString(m.declarationSource)<<",\"bytes\":"<<m.declarationBytes<<",\"live_readable_bytes\":"<<m.declarationReadableBytes<<",\"live_differs_from_baseline\":"<<jsonBool(m.declarationMemoryDiffers)<<",\"installed_is_not_proven\":true,\"records\":[";
 for(size_t j=0;j<m.declarations.size();++j){if(j)o<<',';const auto&d=m.declarations[j];o<<"{\"canonical_site\":"<<jsonString(hexAddress(d.site))<<",\"size\":"<<d.bytes<<",\"name\":"<<jsonString(d.name)<<'}';}o<<"]},\"notes\":";jsonStrings(o,m.notes);o<<'}';}
 o<<"],\n\"differences\":[";
 for(size_t i=0;i<r.differences.size();++i){if(i)o<<',';const auto&d=r.differences[i];o<<"\n{\"module\":"<<d.module<<",\"section\":"<<jsonString(d.section)<<",\"rva\":"<<jsonString(hexAddress(d.rva))<<",\"address\":"<<jsonString(hexAddress(d.address))<<",\"length\":"<<d.length<<",\"type\":"<<jsonString(d.type)<<",\"stability\":"<<jsonString(d.stability)<<",\"expected_sample_hex\":"<<jsonString(d.expected)<<",\"actual_sample_hex\":"<<jsonString(d.actual)<<",\"reference_intersections\":";jsonStrings(o,d.referenceIntersections);o<<",\"candidate_sample_hex\":"<<jsonString(hexBytes(d.candidateBytes.data(),d.candidateBytes.size()))<<",\"candidate_address\":"<<jsonString(hexAddress(d.candidateAddress))<<",\"chain_status\":"<<jsonString(d.chainStatus)<<",\"chain\":[";
 for(size_t j=0;j<d.chain.size();++j){if(j)o<<',';const auto&h=d.chain[j];o<<"{\"address\":"<<jsonString(hexAddress(h.address))<<",\"kind\":"<<jsonString(h.kind)<<",\"target\":"<<jsonString(hexAddress(h.target))<<",\"indirect\":"<<jsonBool(h.indirect)<<",\"call\":"<<jsonBool(h.call)<<",\"pointer_address\":"<<jsonString(hexAddress(h.pointerAddress))<<",\"target_module\":"<<jsonString(h.targetModule)<<",\"nearest_named_export\":"<<jsonString(h.nearestExport)<<",\"export_displacement\":"<<jsonString(hexAddress(h.exportDisplacement))<<",\"status\":"<<jsonString(h.status)<<'}';}o<<"]}";}
 o<<"],\n\"reference_catalog\":{\"layout_verified\":false,\"canonical_base\":\"0x400000\",\"entry_count\":"<<r.references.size()<<",\"modeled_range_count\":"<<r.referenceRangeCount<<",\"entries\":[";
 for(size_t i=0;i<r.references.size();++i){if(i)o<<',';const auto&e=r.references[i];o<<"{\"id\":"<<e.id<<",\"address\":"<<jsonString(hexAddress(e.canonicalAddress))<<",\"kind\":"<<jsonString(e.kind)<<",\"name\":"<<jsonString(e.name)<<",\"purpose\":"<<jsonString(e.purpose)<<",\"features\":"<<jsonString(e.features)<<",\"range_source\":"<<jsonString(e.rangeSource)<<",\"range_size\":"<<e.size<<",\"status\":"<<jsonString(e.status)<<",\"intersections\":"<<e.intersections<<'}';}
 o<<"]}\n}\n";return o.str();
}
inline std::string reportHtml(const ProbeReport&r){
 std::ostringstream o;o<<R"HTML(<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; base-uri 'none'; form-action 'none'"><title>平台 Hook 只读探测报告</title><style>body{font:15px/1.65 system-ui,sans-serif;background:#101923;color:#e5ebf4;margin:0}main{max-width:1500px;margin:auto;padding:28px}h1{margin:0 0 8px}h2{margin:30px 0 12px}p{margin:8px 0}.muted{color:#b3c1d4}.banner{border-left:4px solid #e5b456;background:#263243;padding:14px 18px}input,select{font:inherit;color:#eef5ff;background:#17283b;border:1px solid #5c7390;border-radius:6px;padding:8px;margin:4px}table{border-collapse:collapse;width:100%;margin:14px 0}th,td{border:1px solid #3a4a60;vertical-align:top;text-align:left;padding:9px;overflow-wrap:anywhere}th{background:#21334a}code,pre{font:12px/1.6 ui-monospace,monospace;white-space:pre-wrap;overflow-wrap:anywhere}details{background:#192637;border:1px solid #3a4a60;padding:10px;margin:10px 0}summary{cursor:pointer;font-weight:600}small{color:#c5d2e4}.scroll{overflow:auto}.tag{color:#8ed6ed}li{margin:5px 0}button{font:inherit;padding:6px 12px}a{color:#9ad6ff}</style></head><body><main><h1>平台 Hook 只读探测报告</h1>)HTML";
 o<<"<p class=muted>PID "<<r.pid<<" · 创建时间 "<<htmlText(r.processCreationTime)<<" · "<<htmlText(r.startedUtc)<<" · "<<r.elapsedMs<<" ms</p><div class=banner><strong>"<<htmlText(r.outcome)<<"</strong><p>"<<(r.partial?"本次覆盖不完整；未知、未扫描和不可读区域不能当作无改写。":"已完成本次限定范围比较；这是分时读取，不能证明进程从未被改写。")<<"</p><p>跳转目标所属模块不等于改写者。字节模式候选不证明指令边界、函数用途、恶意性或兼容性。</p></div>";
 if(!r.error.empty())o<<"<p>错误："<<htmlText(r.error)<<"</p>";
 o<<"<h2>范围与限制</h2><ul>";for(const auto&n:r.notes)o<<"<li>"<<htmlText(n)<<"</li>";o<<"</ul><h2>模块与扫描覆盖（"<<r.modules.size()<<"）</h2>";
 for(size_t i=0;i<r.modules.size();++i){const auto&m=r.modules[i];o<<"<details><summary>["<<i<<"] "<<htmlText(m.name)<<" · "<<htmlText(m.status)<<" · "<<hexAddress(m.base)<<"</summary><p>加载路径："<<htmlText(utf8(m.path))<<"</p><p>基准："<<htmlText(m.baselineSource)<<" · "<<htmlText(utf8(m.baselinePath))<<"</p><p>SHA-256：<code>"<<htmlText(m.file.sha256)<<"</code></p><p>文件身份："<<htmlText(m.file.identity)<<" · 读取后稳定="<<(m.file.unchangedAfterRead?"是":"否/未知")<<" · 扫描结束稳定="<<(m.file.unchangedAtFinish?"是":"否/未知")<<"</p><p>PE 身份："<<htmlText(m.identityCheck)<<"；它只证明本次结构相符，不证明磁盘文件是原版。模块身份复验="<<(m.stableIdentity?"相符":"未知/变化")<<"</p><p>已比较 "<<m.comparedBytes<<" B · 加载器正常写入已排除 "<<m.ignoredBytes<<" B · 不可读 "<<m.unreadableBytes<<" B · 未扫描 "<<m.unscannedBytes<<" B · 差异 "<<m.changedBytes<<" B · 两次读取间变化 "<<m.dynamicBytes<<" B</p>";
 if(!m.error.empty())o<<"<p>"<<htmlText(m.error)<<"</p>";
 o<<"<table><tr><th>可执行节</th><th>RVA / 大小</th><th>状态</th><th>已比较 / 不可读 / 未扫描</th></tr>";for(const auto&s:m.sections)o<<"<tr><td>"<<htmlText(s.name)<<"</td><td>"<<hexAddress(s.rva)<<" / "<<s.size<<"</td><td>"<<htmlText(s.status)<<"</td><td>"<<s.comparedBytes<<" / "<<s.unreadableBytes<<" / "<<s.unscannedBytes<<"</td></tr>";o<<"</table><p>.syhks00："<<htmlText(m.declarationStatus)<<" · 声明字节 "<<m.declarationBytes<<" · 运行时可读 "<<m.declarationReadableBytes<<" · 运行时内容不同="<<(m.declarationMemoryDiffers?"是":"否/未知")<<"。下面是磁盘基准声明，不能证明已经安装。</p>";
 if(!m.declarations.empty()){o<<"<details><summary>声明（"<<m.declarations.size()<<"）</summary><table><tr><th>原始站点</th><th>字节数</th><th>名称</th></tr>";for(const auto&d:m.declarations)o<<"<tr><td>"<<hexAddress(d.site)<<"</td><td>"<<d.bytes<<"</td><td>"<<htmlText(d.name)<<"</td></tr>";o<<"</table></details>";}for(const auto&n:m.notes)o<<"<p class=muted>"<<htmlText(n)<<"</p>";o<<"</details>";}
 o<<"<h2>差异与跳转候选（"<<r.differences.size()<<"）</h2><div><input id=search placeholder=\"搜索地址、入口、模块、特性\" aria-label=\"搜索差异\"><select id=module aria-label=\"模块筛选\"><option value=\"\">全部模块</option>";
 for(size_t i=0;i<r.modules.size();++i)o<<"<option value=\""<<i<<"\">"<<htmlText(r.modules[i].name)<<"</option>";
 o<<"</select><select id=kind aria-label=\"差异类型筛选\"><option value=\"\">全部类型</option><option>code-bytes</option><option>transfer-candidate</option><option>dynamic-sample</option></select><span id=count></span></div><div class=scroll><table id=diffs><thead><tr><th>模块 / 地址</th><th>类型与样本</th><th>有界跳转链</th><th>工具箱参考交集</th></tr></thead><tbody>";
 for(const auto&d:r.differences){o<<"<tr data-module=\""<<d.module<<"\" data-kind=\""<<htmlText(d.type)<<"\"><td>"<<(d.module<r.modules.size()?htmlText(r.modules[d.module].name):"?")<<"<br><code>"<<hexAddress(d.address)<<"</code><br>RVA "<<hexAddress(d.rva)<<" · "<<d.length<<" B<br>"<<htmlText(d.section)<<"</td><td><span class=tag>"<<htmlText(d.type)<<"</span><br>"<<htmlText(d.stability)<<"<pre>基准 "<<htmlText(d.expected)<<"\n内存 "<<htmlText(d.actual)<<"</pre></td><td>"<<htmlText(d.chainStatus);
 for(const auto&h:d.chain){o<<"<p><code>"<<hexAddress(h.address)<<" → "<<hexAddress(h.target)<<"</code><br>"<<htmlText(h.kind)<<" · "<<htmlText(h.status)<<"<br>目标 "<<htmlText(h.targetModule);if(!h.nearestExport.empty())o<<"<br>最近命名导出 "<<htmlText(h.nearestExport)<<" + "<<hexAddress(h.exportDisplacement)<<"（仅地址标签）";o<<"</p>";}o<<"</td><td>";for(const auto&s:d.referenceIntersections)o<<htmlText(s)<<"<br>";o<<"</td></tr>";}
 o<<"</tbody></table></div><h2>工具箱入口参考表（"<<r.references.size()<<"）</h2><p class=muted>以下来自工具箱已建模的固定地址、签名与依赖窗口。当前 EXE 布局尚未据此认证；交集只代表地址范围相交，不能直接得出必须关掉某项功能。</p><details><summary>展开入口作用、受影响特性与交集计数</summary><table><tr><th>入口 / 类型</th><th>参考范围</th><th>作用 / 特性</th><th>覆盖 / 交集</th></tr>";
 for(const auto&e:r.references)o<<"<tr><td>"<<htmlText(e.name)<<"<br>"<<htmlText(e.kind)<<"</td><td>"<<hexAddress(e.canonicalAddress)<<" + "<<e.size<<" B<br>"<<htmlText(e.rangeSource)<<"</td><td>"<<htmlText(e.purpose)<<"<br>"<<htmlText(e.features)<<"</td><td>"<<htmlText(e.status)<<"<br>"<<e.intersections<<"</td></tr>";
 o<<R"HTML(</table></details><p class=muted>JSON 包含机器可读元数据。报告仅保存短字节样本，不含完整 EXE/DLL 或内存转储。</p></main><script>'use strict';const s=document.getElementById('search'),m=document.getElementById('module'),k=document.getElementById('kind'),rows=Array.from(document.querySelectorAll('#diffs tbody tr'));function filter(){const q=s.value.toLocaleLowerCase();let n=0;for(const r of rows){const show=(!m.value||r.dataset.module===m.value)&&(!k.value||r.dataset.kind===k.value)&&(!q||r.textContent.toLocaleLowerCase().includes(q));r.hidden=!show;if(show)n++;}document.getElementById('count').textContent=n+' / '+rows.length;}for(const e of [s,m,k])e.addEventListener('input',filter);filter();</script></body></html>)HTML";return o.str();
}
} // namespace hookprobe
