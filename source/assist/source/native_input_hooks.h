// Availability of native event construction; there are no input detours.
// nativeHookStatus is interface readiness, not an installed code-hook state.
static void restoreNativeHooks(){}
static void nativeHooksTick(uint32_t flags){snap.nativeHookStatus=(flags&(NativeOrders|CustomCS))?(loadInterfaces()?1:-1):0;}
