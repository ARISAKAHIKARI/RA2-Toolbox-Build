#include "../../assist/source/exception_policy.h"
extern "C" void* _exception_info();
extern "C" void CaptureCursorResource();
extern "C" void CursorResourceFault();
extern "C" void GuardedCursorResource(){
 __try {CaptureCursorResource();}
 __except(assistguard::noReportAccess(_exception_info())){CursorResourceFault();}
}
