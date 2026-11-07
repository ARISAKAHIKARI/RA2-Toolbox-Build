// The input dispatcher has already checked the live game/session context.
// Send the same owner command as a UI click; do not maintain a second set of
// feature mutations or bypass disabled controls and availability decisions.
static bool automaticHotkey(WPARAM action){
 const auto*entry=autofeatures::byAction(int(action));if(!entry)return false;
 HWND control=GetDlgItem(ui,entry->control);if(!ready||!control)return true;
 if(!IsWindowEnabled(control)){notify(std::wstring(entry->name)+L"当前不可切换，请查看自动功能页的提示。");return true;}
 const bool wasChecked=SendMessageW(control,BM_GETCHECK,0,0)==BST_CHECKED;
 {ControlUpdate guard;SendMessageW(control,BM_SETCHECK,wasChecked?BST_UNCHECKED:BST_CHECKED,0);}
 // Call the owner directly: forwarding through the modern control subclass
 // would apply its automatic check transition a second time.
 proc(ui,WM_COMMAND,MAKEWPARAM(entry->control,BN_CLICKED),reinterpret_cast<LPARAM>(control));
 return true;
}
