extern "C" void GuardedFrameDispatch();
static unsigned char* frameCode;
static void __declspec(naked) assistFrameObserver(){
 __asm {
  pushfd
  pushad
  mov ebx, esp
  sub esp, 528
  and esp, -16
  fxsave [esp]
  cld
  call GuardedFrameDispatch
  fxrstor [esp]
  mov esp, ebx
  popad
  popfd
  jmp dword ptr [frameCode]
 }
}

static unsigned int frameCallTarget=0x55D360;
static void __declspec(naked) assistFrameCallObserver(){
 __asm {
  pushfd
  pushad
  mov ebx, esp
  sub esp, 528
  and esp, -16
  fxsave [esp]
  cld
  call GuardedFrameDispatch
  fxrstor [esp]
  mov esp, ebx
  popad
  popfd
  jmp dword ptr [frameCallTarget]
 }
}
