#include "../../shared/source/runtime_fault.h"
extern "C" void* _exception_info();
extern "C" unsigned long _exception_code();
struct ExceptionRecord32 {unsigned code,flags;void* nested;void* address;unsigned count,info[15];};
struct ExceptionPointers32 {ExceptionRecord32* record;void* context;};
static_assert(sizeof(void*)==4&&sizeof(ExceptionRecord32)==80,"Windows x86 SEH ABI");
extern "C" int AssistExceptionFilter(void* raw,unsigned boundary){
 const auto* pointers=(const ExceptionPointers32*)raw;const auto* record=pointers?pointers->record:nullptr;
 if(record){
  const bool access=(record->code==0xc0000005u||record->code==0xc0000006u)&&record->count>=2;
  AssistReportException(boundary,record->code,(unsigned)record->address,access?record->info[0]:~0u,access?record->info[1]:0);
 }
 return 1;
}
// A failed read-only native legality query rejects this batch before submission.
// Other exceptions still reach the outer Runtime fault boundary.
extern "C" int GuardedEnterAction(unsigned address,unsigned source,unsigned target,unsigned* action){
 __try {*action=((unsigned(__thiscall*)(void*,void*,bool))address)((void*)source,(void*)target,true);return 1;}
 __except(_exception_code()==0xc0000005u?AssistExceptionFilter(_exception_info(),9):0){return 0;}
}
extern "C" unsigned BeginAssistCapture();
extern "C" void EndAssistCapture(unsigned);
// Compile this tiny boundary with the MSVC x86 ABI. MinGW x86's DWARF
// exception mode cannot emit Windows SEH funclets reliably.
extern "C" void AssistTick();
extern "C" void AssistFault();
extern "C" void GuardedAssistTick() {
    __try { AssistTick(); }
    __except(AssistExceptionFilter(_exception_info(),1)) { AssistFault(); }
}
