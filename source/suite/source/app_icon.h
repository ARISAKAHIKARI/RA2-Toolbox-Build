#pragma once
#include <windows.h>

namespace suite {
inline HICON applicationIcon(HINSTANCE instance,bool small){
 const int width=GetSystemMetrics(small?SM_CXSMICON:SM_CXICON);
 const int height=GetSystemMetrics(small?SM_CYSMICON:SM_CYICON);
 auto icon=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,width,height,LR_SHARED));
 return icon?icon:LoadIconW(nullptr,IDI_APPLICATION);
}
}
