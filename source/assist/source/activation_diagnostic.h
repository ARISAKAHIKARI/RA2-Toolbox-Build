#pragma once
#include <cstdint>
namespace assist { namespace activation {
// Readiness remains exactly 1. Extra values explain a denied entry without
// granting work, retiring ownership or changing the shared-memory layout.
enum Gate:int { Idle=0, Ready=1, Signature=3, WorldUnknown=5, CleanupPending=6,
 AuthorityUnknown=7, ViewOnly=8, Paused=9, DisplayHidden=10, HouseChanged=11 };
inline const wchar_t* name(int gate){switch(gate){
 case Idle:return L"initial-or-no-world";case Ready:return L"ready";
 case Signature:return L"signature-mismatch";case WorldUnknown:return L"world-unreadable-or-transitional";
 case CleanupPending:return L"owned-feature-restoration-pending";case AuthorityUnknown:return L"house-roster-unreadable";
 case ViewOnly:return L"observer-defeated-or-no-command-house";case Paused:return L"game-not-running";
 case DisplayHidden:return L"tactical-hidden-or-dialog";case HouseChanged:return L"player-context-changed";
 default:return L"legacy-or-unknown";
}}
inline const wchar_t* message(int gate){switch(gate){
 case Signature:return L"游戏入口签名暂不兼容，尚未启用功能；每秒重新核对，请查看 logs/Runtime.log。";
 case WorldUnknown:return L"游戏场景资料暂时不可读，保留设置并等待恢复。";
 case CleanupPending:return L"旧功能仍有待还原的状态，新开关暂未启用；请查看 logs/Runtime.log。";
 case AuthorityUnknown:return L"当前玩家或玩家列表暂时不可读，恢复后会继续应用设置。";
 case ViewOnly:return L"当前是观战、已结束的玩家或无可操作玩家，操作功能暂不启用。";
 case Paused:return L"游戏当前处于暂停或过场状态，恢复后会继续应用设置。";
 case DisplayHidden:return L"游戏战场未显示或菜单尚未关闭，恢复后会继续应用设置。";
 case HouseChanged:return L"操控方刚刚改变，正在重新核对当前玩家。";
 default:return nullptr;
}}
struct LogCadence {
 uint64_t key=0;uint32_t at=0;bool seen=false;
 bool due(uint64_t next,uint32_t now,bool blocked){
  if(seen&&uint32_t(now-at)<250)return false;
  if(seen&&next==key&&(!blocked||uint32_t(now-at)<5000))return false;
  seen=true;key=next;at=now;return true;
 }
};
}} // assist::activation
