#pragma once
// DLL-local allocation support: no game allocator or C++ unwinder. Failures
// reach the existing x86 SEH boundary. Task sizes are bounded by World/input.
#include <new>
#include <cstddef>
[[noreturn]] static void runtimeAllocationFailure(){RaiseException(0xE0524101u,EXCEPTION_NONCONTINUABLE,0,nullptr);__builtin_trap();}
void* operator new(std::size_t n){void*p=HeapAlloc(GetProcessHeap(),0,n?n:1);if(!p)runtimeAllocationFailure();return p;}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{if(p)HeapFree(GetProcessHeap(),0,p);}
void operator delete[](void*p)noexcept{::operator delete(p);}
void operator delete(void*p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void*p,std::size_t)noexcept{::operator delete(p);}
namespace std { inline namespace __1 {
size_t __next_prime(size_t n){if(n>4194304u)runtimeAllocationFailure();if(n<=2)return 2;if(!(n&1))++n;
 for(;;n+=2){bool prime=true;for(size_t d=3;d<=n/d;d+=2)if(n%d==0){prime=false;break;}if(prime)return n;}
}
void __libcpp_verbose_abort(const char*,...){runtimeAllocationFailure();}
} }
