#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
namespace ddrawconfig {
enum class Kind {Unknown,Cnc,TS};
inline std::string lower(std::string s){for(auto&c:s)if(c>='A'&&c<='Z')c+=32;return s;}
inline std::string trim(const std::string&s){auto a=s.find_first_not_of(" \t\r");if(a==s.npos)return {};return s.substr(a,s.find_last_not_of(" \t\r")-a+1);}
// Read the loaded PE export names without invoking DLL code. Bounded RVAs
// prevent malformed or replaced modules from escaping their mapped image.
template<class Read> Kind identify(Read read,uint32_t base,uint32_t size){
 if(size<4096||size>64*1024*1024||uint64_t(base)+size>0x100000000ull)return Kind::Unknown;
 auto bytes=[&](uint32_t r,void*p,size_t n){return uint64_t(r)+n<=size&&read(base+r,p,n);};
 auto word=[&](uint32_t r){uint32_t v=0;bytes(r,&v,4);return v;};
 uint16_t magic=0;bytes(0,&magic,2);uint32_t pe=word(0x3C);if(magic!=0x5A4D||pe>size-128||word(pe)!=0x4550)return Kind::Unknown;
 bytes(pe+24,&magic,2);if(magic!=0x10B)return Kind::Unknown;
 uint32_t directory=word(pe+120),length=word(pe+124);if(directory<128||length<40||uint64_t(directory)+length>size)return Kind::Unknown;
 uint32_t n=word(directory+24),names=word(directory+32);if(!n||n>4096||uint64_t(names)+n*4>size)return Kind::Unknown;
 bool zoom=false,proc=false,bits=false,ts=false,target=false;
 for(uint32_t i=0;i<n;++i){char s[96]{};uint32_t at=word(names+4*i);if(!at||!bytes(at,s,sizeof s)||!memchr(s,0,sizeof s))continue;
  zoom|=!strcmp(s,"DDEnableZoom");proc|=!strcmp(s,"DDGetProcAddress");bits|=!strcmp(s,"pvBmpBits");ts|=!strcmp(s,"TSDDRAW");target|=!strcmp(s,"TargetFPS");
 }
 return zoom&&proc&&bits?Kind::Cnc:ts&&target?Kind::TS:Kind::Unknown;
}
struct Edit {std::string result,error;int sections=0;};
// Byte-preserving INI edit: keep unrelated lines, comments, encoding bytes and
// per-game compatibility keys. Reject UTF-16 rather than silently corrupt it.
inline Edit prepare(const std::string&input,Kind kind,std::string exe,unsigned hz){
 Edit out;if(kind==Kind::Unknown){out.error="renderer";return out;}
 if(input.empty()||input.size()>1024*1024||input.find('\0')!=input.npos){out.error="encoding";return out;}
 if(hz&&(hz<30||hz>500)){out.error="rate";return out;}
 exe=lower(exe);if(exe.empty()||exe.find_first_of("[]\r\n/\\")!=exe.npos){out.error="executable";return out;}
 struct Line {std::string text,end;};std::vector<Line> lines;
 for(size_t at=0;at<input.size();){size_t end=input.find('\n',at);if(end==input.npos){lines.push_back({input.substr(at),{}});break;}bool cr=end>at&&input[end-1]=='\r';lines.push_back({input.substr(at,end-at-(cr?1:0)),cr?"\r\n":"\n"});at=end+1;}
 std::string newline=input.find("\r\n")!=input.npos?"\r\n":"\n";
 bool selected=false,found=false,first=false,second=false;std::string section;
 auto emit=[&](const std::string&s){if(!out.result.empty()&&out.result.back()!='\n')out.result+=newline;out.result+=s+newline;};
 std::string key1=kind==Kind::Cnc?"maxfps":"targetfps",value=std::to_string(hz?int(hz):kind==Kind::Cnc?-1:0);
 auto finish=[&](){if(!selected)return;if(!first)emit(key1+"="+value);if(kind==Kind::Cnc&&!second)emit("minfps=-2");};
 for(size_t n=0;n<lines.size();++n){auto line=lines[n];std::string parsed=trim(line.text);if(n==0&&parsed.compare(0,3,"\xef\xbb\xbf")==0)parsed=parsed.substr(3);
  if(!parsed.empty()&&parsed[0]=='['){size_t end=parsed.find(']');if(end==parsed.npos){out.error="section";return out;}finish();section=lower(trim(parsed.substr(1,end-1)));
   selected=kind==Kind::TS?section=="ddraw":section==exe||section.compare(0,exe.size()+1,exe+"/")==0;
   first=second=false;if(selected){found=true;++out.sections;}
  }else if(selected&&!parsed.empty()&&parsed[0]!=';'&&parsed[0]!='#'){
   size_t eq=line.text.find('=');if(eq!=line.text.npos){std::string key=lower(trim(line.text.substr(0,eq)));bool a=key==key1,b=kind==Kind::Cnc&&key=="minfps";
    if(a||b){std::string old=line.text.substr(eq+1);size_t comment=old.find_first_of(";#");std::string tail=comment==old.npos?"":" "+old.substr(comment);line.text=line.text.substr(0,eq+1)+(a?value:"-2")+tail;first|=a;second|=b;}
   }
  }
  out.result+=line.text+line.end;
 }
 finish();if(!found){emit("");emit("["+(kind==Kind::TS?std::string("ddraw"):exe)+"]");emit(key1+"="+value);if(kind==Kind::Cnc)emit("minfps=-2");++out.sections;}
 return out;
}
}
