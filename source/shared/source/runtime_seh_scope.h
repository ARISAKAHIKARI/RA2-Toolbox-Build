#pragma once
#include <type_traits>

// The x86 Runtime uses GNU C++ without C++ exception unwinding. Put cleanup
// above the callback in a Windows SEH frame instead of relying on a destructor
// when a platform handler unwinds through our callback.
#if defined(_WIN32)
#define RA2_RUNTIME_SCOPE_CDECL __cdecl
#else
#define RA2_RUNTIME_SCOPE_CDECL
#endif
extern "C" void RuntimeScopedCall(void* context,
 void(RA2_RUNTIME_SCOPE_CDECL* body)(void*),
 void(RA2_RUNTIME_SCOPE_CDECL* cleanup)(void*));

namespace runtimeunwind {
template<class Body,class Cleanup>struct Invocation {
 Body& body;Cleanup& cleanup;
 static void RA2_RUNTIME_SCOPE_CDECL invoke(void* raw){static_cast<Invocation*>(raw)->body();}
 static void RA2_RUNTIME_SCOPE_CDECL finish(void* raw){static_cast<Invocation*>(raw)->cleanup();}
};
template<class Body,class Cleanup>void run(Body&& body,Cleanup&& cleanup){
 Invocation<typename std::remove_reference<Body>::type,typename std::remove_reference<Cleanup>::type> context{body,cleanup};
 RuntimeScopedCall(&context,&decltype(context)::invoke,&decltype(context)::finish);
}
}
#undef RA2_RUNTIME_SCOPE_CDECL
