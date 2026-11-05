#pragma once
// Fixed-width, x86 SEH diagnostic ABI; no Windows SDK dependency in the MSVC
// exception boundary. This is IPC reporting, never an execution entry point.
namespace runtimefault {
enum Phase : unsigned { Idle=0, LoadCollect=1, LoadLegality=2, LoadAssign=3, LoadSubmit=4, LoadRetry=5 };
struct Context {unsigned phase=0,source=0,target=0;};
}
extern "C" void RuntimeReportException(unsigned boundary,unsigned code,unsigned address,
 unsigned operation,unsigned access,unsigned command,unsigned phase,unsigned source,unsigned target);
extern "C" void AssistReportException(unsigned boundary,unsigned code,unsigned address,unsigned operation,unsigned access);
