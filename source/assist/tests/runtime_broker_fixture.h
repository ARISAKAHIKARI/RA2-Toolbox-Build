#pragma once
#include <windows.h>
#include <stdint.h>
namespace brokerfixture {
constexpr UINT Message=WM_APP+42;
constexpr UINT SentMessage=WM_APP+43;
struct Probe {
 uint32_t owner=0,pid=0,thread=0,window=0;
 volatile LONG messageCalls=0,postCalls=0,ack=0;
 uint32_t childPID=0,childTargetPID=0,childThread=0,childWindow=0,childBrokerPID=0;
 volatile LONG childReady=0;
};
inline void name(wchar_t*out,DWORD owner){wsprintfW(out,L"Local\\RA2BrokerFixture_%lu",(unsigned long)owner);}
}
