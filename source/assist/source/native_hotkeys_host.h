#include "native_hotkeys.h"
namespace nativekeyhost {
static nativekeys::Bindings bindings;static uint32_t lastPID=0,lastMatch=0;static DWORD sampled=0;
static const nativekeys::Bindings& current(){DWORD now=GetTickCount();
 if(lastPID!=pid||lastMatch!=commandMatch()||!sampled||DWORD(now-sampled)>=250){lastPID=pid;lastMatch=commandMatch();sampled=now;bindings=nativekeys::load([](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);});}
 return bindings;
}
static void down(uint8_t(&keys)[256]){for(int n=0;n<256;++n)keys[n]=(GetAsyncKeyState(n)&0x8000)!=0;}
}
