// Only the fixture's OUTER handler is built here. The inner guarded functions
// are linked from unmodified production guard.cpp with the same x86 SEH ABI.
extern "C" void* _exception_info();
extern "C" unsigned long _exception_code();
extern "C" unsigned fixtureCode;
extern "C" int fixtureResult;
extern "C" void GuardedAssistTick();
extern "C" int GuardedAssistFilter(void*);
extern "C" int GuardedRiskAction(unsigned,unsigned,unsigned);
extern "C" int GuardedSidebarViewAction(unsigned,unsigned);
extern "C" int GuardedTechUiParameter(void*,unsigned);
extern "C" int GuardedControlWorldEligible();
extern "C" void GuardedCursorResource();
extern "C" unsigned FixtureOuter(unsigned route){
 unsigned caught=0;fixtureResult=-1;
 __try {
  if(route==0){GuardedAssistTick();fixtureResult=0;}
  else if(route==1)fixtureResult=GuardedAssistFilter((void*)7);
  else if(route==2)fixtureResult=GuardedRiskAction(3,4,5);
  else if(route==3)fixtureResult=GuardedSidebarViewAction(6,7);
  else if(route==4)fixtureResult=GuardedTechUiParameter((void*)8,9);
  else if(route==6)fixtureResult=GuardedControlWorldEligible();
  else {GuardedCursorResource();fixtureResult=12;}
 }__except(_exception_code()==fixtureCode?1:0){caught=1;}
 return caught;
}

extern "C" void RuntimeScopedCall(void*,void(__cdecl*)(void*),void(__cdecl*)(void*));
extern "C" void FixtureScopeBody(void*);
extern "C" void FixtureScopeCleanup(void*);
extern "C" unsigned FixtureScopedOuter(void*context){
 unsigned caught=0;
 __try {RuntimeScopedCall(context,FixtureScopeBody,FixtureScopeCleanup);}
 __except(_exception_code()==fixtureCode?1:0){caught=1;}
 return caught;
}
