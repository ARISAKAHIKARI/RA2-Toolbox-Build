// The platform handler is outside the GNU C++ scopes under test. It consumes
// only this fixture's real own-process NOACCESS-page write.
extern "C" unsigned long _exception_code();
extern "C" void FixtureRuntimeScopeBody(void*);
extern "C" unsigned FixtureRuntimeScopeOuter(void* context){
 unsigned caught=0;
 __try {FixtureRuntimeScopeBody(context);}
 __except(_exception_code()==0xc0000005u?1:0){caught=1;}
 return caught;
}
