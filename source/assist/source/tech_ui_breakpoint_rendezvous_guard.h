// Compiled in guard.cpp with the MSVC x86 SEH target. GNU Runtime code calls
// this narrow frame so an unhandled *private* rendezvous trap fails locally.
// The caller's filter must reject every foreign/native exception unchanged.
extern "C" void* _exception_info();
extern "C" int __cdecl RuntimeBreakpointCall(void*context,
 void(__cdecl*body)(void*),int(__cdecl*filter)(void*,void*)){
 __try {body(context);return 1;}
 __except(filter(context,_exception_info())){return 0;}
}
