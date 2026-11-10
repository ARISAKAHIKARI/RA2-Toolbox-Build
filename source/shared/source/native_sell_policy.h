#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace nativesell {
enum Status : int {
 Off=0,Ready=1,RestoreFailed=-2,Waiting=-3,NetworkUnavailable=-4,ReceiverUnavailable=-5,
 QueueUnavailable=-6,
 CursorConflict=-20,CursorContext=-21,CursorUnreadable=-22,CursorOriginal=-23,CursorInstall=-24,
 OwnerConflict=-30,OwnerContext=-31,OwnerUnreadable=-32,OwnerOriginal=-33,OwnerInstall=-34
};
inline bool requiresNativeQueue(bool singleplayer,bool humanOwner){return !singleplayer||humanOwner;}
// Only callers that actually enter the original OutList path require its
// capacity/append contract. SP-AI consumes the event before this instruction.
template<class Read> bool queueCompatible(Read read){
 static constexpr uint8_t original[]={0x81,0x3D,0xC8,0x02,0xA8,0x00,0x80,0,0,0};
 uint8_t actual[sizeof original]{};
 return read(0x4AC18C,actual,sizeof actual)&&!memcmp(actual,original,sizeof actual);
}
inline const wchar_t* failureText(int status){
 switch(status){
 case RestoreFailed:return L"原生入口恢复未完成";
 case Waiting:return L"等待可操作的对局";
 case NetworkUnavailable:return L"联机需要先启用事件所属方保留";
 case ReceiverUnavailable:return L"单机出售接收器未就绪";
 case QueueUnavailable:return L"联机原生事件队列尚未适配";
 case CursorConflict:return L"光标入口存在写入冲突";
 case CursorContext:return L"光标入口上下文不匹配";
 case CursorUnreadable:return L"光标入口不可读";
 case CursorOriginal:return L"光标入口已被替换，尚未适配";
 case CursorInstall:return L"光标入口安装失败";
 case OwnerConflict:return L"出售入口存在写入冲突";
 case OwnerContext:return L"出售入口上下文不匹配";
 case OwnerUnreadable:return L"出售入口不可读";
 case OwnerOriginal:return L"出售入口已被替换，尚未适配";
 case OwnerInstall:return L"出售入口安装失败";
 default:return nullptr;
 }
}
}
