// Local sidebar presentation only. Never write Factory/House/type fields.
// Native StripClass click sets CameoDataStruct +0x10 to 1 (0x6AB80D).
// TabDataStruct::Draw scans these states at 0x6A95C8..0x6A95E9 to shade
// other building cameos. A raw Produce event does not run that frontend step.
struct ReservedSidebar {uint32_t match=0,house=0,origin=0,seen=0,sent=0,factory=0;int index=-1,tab=-1;};
static ReservedSidebar reservedSidebar[2];
static void watchReservedSidebar(uint32_t house,int index,int tab,uint32_t origin,DWORD now){
 if(tab>=0&&tab<2)reservedSidebar[tab]={match,house,origin,0,now,0,index,tab};
}
static void reservedSidebarTick(uint32_t house,DWORD now){
 for(auto&w:reservedSidebar){if(w.tab<0)continue;
  if(w.match!=match||w.house!=house){w={};continue;}
  uint32_t factory=get<uint32_t>(house+(w.tab?0x53CC:0x53BC));
  bool validFactory=factory>=0x10000&&get<uint32_t>(factory+0x6C)==house;
  uint32_t product=validFactory?get<uint32_t>(factory+0x58):0;
  uint32_t type=product&&kind(product)==3?get<uint32_t>(product+0x520):0;
  int index=type>=0x10000?get<int>(type+0xDF8):-1;
  uint32_t identity=product?get<uint32_t>(product+0x10):0;
  bool same=index==w.index&&identity!=w.origin;
  if(same&&!w.seen){w.seen=identity;w.factory=factory;}
  bool active=same&&identity==w.seen;
  if(!w.seen){if(DWORD(now-w.sent)>5000)w={};continue;}
  const uint32_t strip=0x87F7E8+0x1544+w.tab*3988;int count=get<int>(strip+84);bool changed=false;
  if(count<0||count>75){w={};continue;}
  for(int n=0;n<count;++n){uint32_t cameo=strip+88+n*52;
   if(get<int>(cameo)!=w.index||get<int>(cameo+4)!=7)continue;
   int state=get<int>(cameo+0x10);uint32_t linked=get<uint32_t>(cameo+0xC);
   if(active){
    // Preserve native pause (2), completed/placement and other frontend states.
    if(linked!=factory){put<uint32_t>(cameo+0xC,factory);changed=true;}
    if(!state){put<int>(cameo+0x10,1);changed=true;}
   }else if(!same&&linked==w.factory&&(state==1||state==2)){
    put<int>(cameo+0x10,0);put<uint32_t>(cameo+0xC,0);changed=true;
   }
   break;
  }
  if(changed){put<uint8_t>(strip+0x3C,1);put<uint8_t>(0x87F7E8+0x53A6,1);}
  if(!active)w={};
 }
}
