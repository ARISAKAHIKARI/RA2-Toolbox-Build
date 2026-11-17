static bool oreTypeAllowed(uint32_t type,bool miner){char id[24]{};if(!read(type+0x24,id,23))return false;
 return oreconfig::allowed(oreConfig,miner,id);
}
static bool oreMiner(uint32_t type){return type&&oreTypeAllowed(type,true)&&
 (oreConfig.customMiners||(get<bool>(type+0xE0E)||get<bool>(type+0xE0F)))&&get<int>(type+0x800)>0;}
static bool rawOreCompatible(uint32_t unit,uint32_t refinery){
 if(kind(unit)!=2||kind(refinery)!=3)return false;uint32_t t=typeOf(unit,2),r=typeOf(refinery,3);
 if(!refreshOreConfig()||!t||!r||!oreMiner(t)||!oreTypeAllowed(r,false)||get<bool>(refinery+0x1C8))return false;
 Vec docks=vec(t+0x3E8,256);for(int i=0;i<docks.count&&i<8;++i)if(item(docks,i)==r)return true;return false;
}
static bool rawDockBusy(uint32_t refinery,uint32_t unit){
 int n=get<int>(refinery+0xE8);uint32_t links=get<uint32_t>(refinery+0xE4);if(n<0||n>256||(n&&links<0x10000))return true;
 for(int i=0;i<n;++i){uint32_t p=get<uint32_t>(links+i*4);if(p&&p!=unit)return true;}return false;
}
static bool rawDockLinked(uint32_t refinery,uint32_t unit){
 int n=get<int>(refinery+0xE8);uint32_t links=get<uint32_t>(refinery+0xE4);if(n<0||n>256||(n&&links<0x10000))return true;
 for(int i=0;i<n;++i)if(get<uint32_t>(links+i*4)==unit)return true;return false;
}
