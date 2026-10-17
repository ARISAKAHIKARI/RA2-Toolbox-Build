extern "C" void CaptureCursorResource();
extern "C" void CursorResourceFault();
extern "C" void GuardedCursorResource(){
 __try {CaptureCursorResource();}
 __except(1){CursorResourceFault();}
}
