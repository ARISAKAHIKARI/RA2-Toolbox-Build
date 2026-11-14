#define NOMINMAX
#include <windows.h>
#include <stdexcept>
#include <thread>
#include <cstring>
#include "../../shared/source/host_crash.h"
int main(int argc,char**argv){
 if(argc!=3)return 2;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 wchar_t path[4096]{};if(!MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,4096))return 3;
 hostcrash::install(path);
 if(!std::strcmp(argv[2],"thread")){std::thread worker([]{throw std::runtime_error("controlled unhandled worker exception");});worker.join();}
 else if(!std::strcmp(argv[2],"av")){
  auto*p=static_cast<volatile unsigned*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS));if(!p)return 4;*p=123;
 }else return 5;
 return 6;
}
