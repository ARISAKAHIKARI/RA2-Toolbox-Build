// Availability of normal native event construction; there are no input detours.
static void restoreNativeHooks(){}
static void nativeHooksTick(uint32_t flags){snap.nativeHookStatus=(flags&(NativeOrders|CustomCS))?(loadInterfaces()?1:-1):0;}
