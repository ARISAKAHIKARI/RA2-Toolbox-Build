#pragma once
#include "frame_signal_protocol.h"
namespace framesignal {
// One sampler owns this channel. Notifications coalesce; no old-frame queue.
class Client {
 HANDLE mapping_=nullptr,event_=nullptr;Channel*channel_=nullptr;DWORD pid_=0;
public:
 ~Client(){close();}
 void close(){if(channel_){InterlockedExchange(&channel_->enabled,0);UnmapViewOfFile(channel_);}channel_=nullptr;if(event_)CloseHandle(event_);if(mapping_)CloseHandle(mapping_);event_=mapping_=nullptr;pid_=0;}
 bool open(DWORD pid){
  if(pid_==pid&&channel_)return true;close();if(!pid)return false;
  wchar_t path[96];name(path,pid,GetCurrentProcessId());mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Channel),path);
  if(!mapping_)return false;const bool existed=GetLastError()==ERROR_ALREADY_EXISTS;
  channel_=(Channel*)MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(Channel));if(!channel_){close();return false;}
  if(existed&&(channel_->magic!=Magic||channel_->version!=Version||channel_->pid!=pid||channel_->owner!=GetCurrentProcessId())){close();return false;}
  if(!existed){ZeroMemory(channel_,sizeof(Channel));channel_->magic=Magic;channel_->version=Version;channel_->pid=pid;channel_->owner=GetCurrentProcessId();channel_->frame=-1;}
  name(path,pid,GetCurrentProcessId(),true);event_=CreateEventW(nullptr,FALSE,FALSE,path);if(!event_){close();return false;}pid_=pid;return true;
 }
 void heartbeat(){if(channel_){channel_->heartbeat=GetTickCount();InterlockedExchange(&channel_->enabled,1);}}
 bool wait(Stamp&stamp,DWORD milliseconds=8){heartbeat();if(event_)WaitForSingleObject(event_,milliseconds);return read(channel_,stamp)&&stamp.state==Native&&stamp.serial>0&&stamp.frequency>0;}
 bool current(Stamp&stamp){return read(channel_,stamp);}
};
}
