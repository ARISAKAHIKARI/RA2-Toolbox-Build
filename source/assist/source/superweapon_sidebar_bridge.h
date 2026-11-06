#pragma once
// Included by bridge.cpp after get/put/call are defined. Only local targeting
// UI is cancelled. No superweapon instance, charge state or event is modified.
static int cancelSuperSidebarSelection(){
 if(get<int>(0x8809A0)<0)return 0; // Do not clear an unrelated building cursor.
 if(!nativeDependencyReady(0x4A8BF0))return -12;
 put<int>(0x8809A0,-1);
 call<void>(0x4A8BF0,0x87F7E8,(void*)nullptr);
 return 1;
}
