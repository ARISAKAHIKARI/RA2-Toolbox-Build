// YRpp InfantryTypeClass: Ivan +EAE, Infiltrate +EBE, C4 +EC2, Engineer +EC3.
// Only raw capability reads here; ranking remains in the controller planner.
static bool antiTheftThreat(uint32_t p,uint32_t house){
 if(!p||kind(p)!=1||!nativeEnemy(p,house)||get<int>(p+0x6C)<=0||!get<bool>(p+0x74))return false;
 uint32_t t=typeOf(p,1);return t&&(get<bool>(t+0xEAE)||get<bool>(t+0xEBE)||get<bool>(t+0xEC2)||get<bool>(t+0xEC3));
}
static bool antiTheftBuilding(uint32_t p){
 if(!p||kind(p)!=3||!refreshFeatureTypes())return false;uint32_t t=typeOf(p,3);if(!t)return false;
 char id[24]{};if(!read(t+0x24,id,23))return false;
 if(featureConfig.filters[featuretypes::AntiTheftDefense].custom)return featuretypes::allowed(featureConfig,featuretypes::AntiTheftDefense,id);
 return oreconfig::equal(id,"NALASR")||oreconfig::equal(id,"GAPILL")||oreconfig::equal(id,"YAGGUN");
}
