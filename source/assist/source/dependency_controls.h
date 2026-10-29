#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "settings_dependencies.h"

namespace assist { namespace settingsdeps {
// Real HWND adapter shared by the console and its controlled Windows tests.
// A rejected synthetic notification restores the last accepted control value;
// EnableWindow alone is not an authorization check for WM_COMMAND.
class ControlBinding {
 enum class Kind {None,Check,Selection,Text};
 HWND window_=nullptr;Kind kind_=Kind::None;bool captured_=false;
 LRESULT number_=0;std::wstring text_;
 void capture(){
  if(!window_)return;
  if(kind_==Kind::Check)number_=SendMessageW(window_,BM_GETCHECK,0,0);
  else if(kind_==Kind::Selection)number_=SendMessageW(window_,CB_GETCURSEL,0,0);
  else if(kind_==Kind::Text){int n=GetWindowTextLengthW(window_);std::vector<wchar_t> text(size_t(n)+1);GetWindowTextW(window_,text.data(),n+1);text_=text.data();}
  captured_=true;
 }
public:
 ControlBinding()=default;
 explicit ControlBinding(HWND window):window_(window){
  wchar_t name[32]{};GetClassNameW(window_,name,32);
  if(!_wcsicmp(name,L"BUTTON"))kind_=Kind::Check;
  else if(!_wcsicmp(name,L"COMBOBOX"))kind_=Kind::Selection;
  else if(!_wcsicmp(name,L"EDIT"))kind_=Kind::Text;
  capture();
 }
 HWND window()const{return window_;}
 void refresh(Decision availability){
  if(!window_)return;
  // Capture before disabling, preserving even a partially edited field. Values
  // saved in the settings model remain authoritative for actual execution.
  if(!captured_||IsWindowEnabled(window_))capture();
  if(bool(IsWindowEnabled(window_))!=availability.enabled())EnableWindow(window_,availability.enabled());
 }
 bool rejectChange(Decision availability){
  if(availability.enabled())return false;
  if(window_&&captured_){
   if(kind_==Kind::Check)SendMessageW(window_,BM_SETCHECK,number_,0);
   else if(kind_==Kind::Selection)SendMessageW(window_,CB_SETCURSEL,number_,0);
   else if(kind_==Kind::Text)SetWindowTextW(window_,text_.c_str());
  }
  return true;
 }
};
}}
