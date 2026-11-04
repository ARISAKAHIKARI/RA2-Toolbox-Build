#include "../../shared/source/hook_diagnostic_gate.h"
#include <cassert>
#include <cstdio>
int main(){using namespace hookdiagnostic;Channel c;c.pid=21;c.owner=42;c.heartbeat=100;c.camera=0;assert(!permitsCamera(c,21,42,101));c.camera=1;assert(permitsCamera(c,21,42,101));assert(!permitsCamera(c,22,42,101));assert(!permitsCamera(c,21,43,101));assert(!permitsCamera(c,21,42,2100));assert(!permitsCamera(c,21,42,99));c.version=2;assert(!permitsCamera(c,21,42,101));c.version=1;c.heartbeat=0xfffffff0;assert(permitsCamera(c,21,42,0x10));c.camera=2;assert(!permitsCamera(c,21,42,0x10));puts("PASS: diagnostic camera defaults off; owner/game/version/heartbeat verified; tick wrap supported");}
