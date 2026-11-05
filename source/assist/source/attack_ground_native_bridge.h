// Explicit SP-only shot. SetTarget(self) canonicalizes to CellClass, so the
// ordinary event cannot retain an exact zero-distance target. Fire(self) does;
// both the infantry and unit overrides keep the engine's own firing checks.
static bool ownGroundNativeSignature(uint32_t address,const unsigned char*bytes,size_t size){
 unsigned char actual[16]{};return size<=sizeof actual&&read(address,actual,size)&&memcmp(actual,bytes,size)==0;
}
static int ownGroundShot(uint32_t p,int slot,bool& voiced){
 if(!gameentry::active(gameentry::Client::Assist)||!eventdelivery::isSingleplayer(get<int>(0xA8B238))||!simulationReady())return -43;
 const int k=kind(p);const uint32_t vt=get<uint32_t>(p);
 const uint32_t fireError=k==1?0x51C8B0:0x740FD0,fire=k==1?0x51DF60:0x741340;
 if((k!=1&&k!=2)||vt!=(k==1?0x7EB058u:0x7F5C70u)||slot<0||slot>=18||
    get<uint32_t>(vt+0x3C0)!=fireError||get<uint32_t>(vt+0x3CC)!=fire)return -43;
 static const unsigned char infantryError[]={0x83,0xEC,0x14,0x53,0x8B,0xD9};
 static const unsigned char unitError[]={0x83,0xEC,0x0C,0x53,0x55,0x56};
 static const unsigned char infantryFire[]={0x8B,0x44,0x24,0x08,0x56,0x8B,0xF1};
 static const unsigned char unitFire[]={0x83,0xEC,0x10,0x56,0x8B,0xF1};
 static const unsigned char commonFire[]={0x55,0x8B,0xEC,0x83,0xE4,0xF8};
 if(!ownGroundNativeSignature(fireError,k==1?infantryError:unitError,6)||
    !ownGroundNativeSignature(fire,k==1?infantryFire:unitFire,k==1?7:6)||
    !ownGroundNativeSignature(0x6FDD50,commonFire,sizeof commonFire))return -43;
 // true performs the original range check (verified in both supported PEs).
 // A busy/reloading unit is skipped now, never replayed after the key press.
 if(call<int>(fireError,p,p,slot,true)!=0)return 0;
 cancelNative(p,false);
 // Suicide/instant weapons may destroy their source inside Fire.
 if(!voiced){voiced=true;inputFeedback(p,0,false);}
 const uint32_t bullet=call<uint32_t>(fire,p,p,slot);
 if(bullet)return 1;
 // Some native particle weapons return null after starting their rearm timer.
 // Fire may destroy its source; use bounded reads instead of dereferencing it.
 int lastFire=-1,timer[3]{};
 return (read(p+0x120,&lastFire,sizeof lastFire)&&lastFire==get<int>(0xA8ED84))||
        (read(p+0x2EC,timer,sizeof timer)&&timer[0]==get<int>(0xA8ED84)&&timer[2]>0)?1:0;
}
