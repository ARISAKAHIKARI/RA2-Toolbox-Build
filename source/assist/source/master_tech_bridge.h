// Captured master technology semantics; all writes stay manager-owned.
// Each thunk skips the original pushes/CanBuild/test as one verified span.
static __declspec(naked) void masterBuildEntry(){__asm {
 mov ecx,[eax+esi*4]
 mov eax,[ecx+634h]
 cmp eax,-1
 push 44575Fh
 ret
}}
static __declspec(naked) void masterUnitEntry(){__asm {
 mov eax,[edx+esi*4]
 mov eax,[eax+634h]
 cmp eax,-1
 push 4457A4h
 ret
}}
static __declspec(naked) void masterInfantryEntry(){__asm {
 mov edx,[ecx+esi*4]
 mov eax,[edx+634h]
 cmp eax,-1
 push 4457E5h
 ret
}}
static __declspec(naked) void masterAirEntry(){__asm {
 mov ecx,[eax+esi*4]
 mov eax,[ecx+634h]
 cmp eax,-1
 push 445825h
 ret
}}
static __declspec(naked) void masterPruneEntry(){__asm {
 push 6AAA68h
 ret
}}
static RiskSite masterTechSites[]={
 {0x44574A,5,{0x6A,1,0x6A,0,0x8B},{}},
 {0x445795,5,{0x6A,1,0x6A,0,0x8B},{}},
 {0x4457D0,5,{0x6A,1,0x6A,0,0x8B},{}},
 {0x445810,5,{0x6A,1,0x6A,0,0x8B},{}},
 {0x6AA788,6,{0x0F,0x85,0xDA,2,0,0},{}}
};
static void prepareMasterTech(){
 const uintptr_t targets[]={uintptr_t(&masterBuildEntry),uintptr_t(&masterUnitEntry),uintptr_t(&masterInfantryEntry),uintptr_t(&masterAirEntry),uintptr_t(&masterPruneEntry)};
 for(unsigned i=0;i<5;++i){auto&s=masterTechSites[i];memset(s.after,0x90,s.size);s.after[0]=0xE9;uint32_t offset=uint32_t(targets[i])-s.address-5;memcpy(s.after+1,&offset,4);}
}
static bool restoreMasterTech(){bool ok=true;for(auto&s:masterTechSites)if(!restoreRiskSite(s))ok=false;return ok;}
