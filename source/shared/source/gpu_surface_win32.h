#pragma once
#include "gpu_surface.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
namespace gpupresent {
template<class T>class Com {
 T* p_=nullptr;
public:
 Com()=default;~Com(){reset();}Com(const Com&)=delete;Com&operator=(const Com&)=delete;
 Com(Com&&o)noexcept:p_(std::exchange(o.p_,nullptr)){}Com&operator=(Com&&o)noexcept{if(this!=&o){reset();p_=std::exchange(o.p_,nullptr);}return *this;}
 void reset(){if(p_)p_->Release();p_=nullptr;}T*get()const{return p_;}T**put(){reset();return &p_;}T*operator->()const{return p_;}explicit operator bool()const{return p_!=nullptr;}
};
inline HMODULE systemModule(const wchar_t*name){wchar_t path[MAX_PATH]{};auto n=GetSystemDirectoryW(path,MAX_PATH);if(!n||n>=MAX_PATH-64)return nullptr;return LoadLibraryW((std::wstring(path,n)+L"\\"+name).c_str());}
struct Device {
 // All windows on this adapter share one immediate context. Never let the
 // UI's watch windows race the independent main-overlay presenter.
 std::mutex presentationMutex;
 HMODULE d3dModule=nullptr,dxgiModule=nullptr;Com<ID3D11Device> device;Com<IDXGIFactory2> factory;uint64_t adapter=0;
 ~Device(){factory.reset();device.reset();if(d3dModule)FreeLibrary(d3dModule);if(dxgiModule)FreeLibrary(dxgiModule);}
 static std::shared_ptr<Device> create(uint64_t luid){
  auto out=std::make_shared<Device>();out->d3dModule=systemModule(L"d3d11.dll");out->dxgiModule=systemModule(L"dxgi.dll");
  if(!out->d3dModule||!out->dxgiModule)return {};
  using Factory=HRESULT(WINAPI*)(REFIID,void**);auto createFactory=reinterpret_cast<Factory>(GetProcAddress(out->dxgiModule,"CreateDXGIFactory1"));
  auto createDevice=reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(GetProcAddress(out->d3dModule,"D3D11CreateDevice"));
  if(!createFactory||!createDevice||FAILED(createFactory(__uuidof(IDXGIFactory2),reinterpret_cast<void**>(out->factory.put()))))return {};
  Com<IDXGIAdapter1> selected;
  for(UINT n=0;n<128;++n){Com<IDXGIAdapter1> adapter;if(out->factory->EnumAdapters1(n,adapter.put())==DXGI_ERROR_NOT_FOUND)break;if(!adapter)continue;DXGI_ADAPTER_DESC1 desc{};if(FAILED(adapter->GetDesc1(&desc)))continue;
   uint64_t id=0;std::memcpy(&id,&desc.AdapterLuid,8);if(id==luid){selected=std::move(adapter);break;}}
  if(!selected)return {};D3D_FEATURE_LEVEL requested=D3D_FEATURE_LEVEL_11_0,actual{};
  if(FAILED(createDevice(selected.get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,&requested,1,D3D11_SDK_VERSION,out->device.put(),&actual,nullptr)))return {};
  out->adapter=luid;return out;
 }
};
inline std::shared_ptr<Device> device(uint64_t luid){static std::mutex mutex;static std::map<uint64_t,std::weak_ptr<Device>> devices;std::lock_guard<std::mutex>lock(mutex);
 auto prior=devices[luid].lock();if(prior)return prior;auto next=Device::create(luid);if(next)devices[luid]=next;return next;}
struct SharedImage final:Resource {
 std::shared_ptr<Device> owner;Com<ID3D11Texture2D> texture;Com<ID3D11ShaderResourceView> view;Com<IDXGIKeyedMutex> keyed;HANDLE handle=nullptr;
 mutable std::atomic<bool> consumed{false};bool produced=false;
 ~SharedImage(){if(handle)CloseHandle(handle);}
 // Also reclaims frames dropped by the latest-only publication queue. A frame
 // never presented still owns key 1; a consumed frame returns key 0.
 bool writable(){const UINT64 key=produced&&!consumed.load(std::memory_order_acquire)?1:0;
  if(keyed->AcquireSync(key,0)!=S_OK)return false;if(FAILED(keyed->ReleaseSync(0)))return false;consumed=false;return true;}
 static std::shared_ptr<SharedImage> create(std::shared_ptr<Device> owner,int w,int h){
  if(!owner||w<1||h<1||w>8192||h>8192||uint64_t(w)*h>16*1024*1024)return {};
  auto out=std::make_shared<SharedImage>();out->owner=std::move(owner);out->width=w;out->height=h;out->adapter=out->owner->adapter;
  D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
  desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED_NTHANDLE|D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  if(FAILED(out->owner->device->CreateTexture2D(&desc,nullptr,out->texture.put()))||FAILED(out->texture->QueryInterface(__uuidof(IDXGIKeyedMutex),reinterpret_cast<void**>(out->keyed.put()))))return {};
  if(FAILED(out->owner->device->CreateShaderResourceView(out->texture.get(),nullptr,out->view.put())))return {};
  Com<IDXGIResource1> resource;if(FAILED(out->texture->QueryInterface(__uuidof(IDXGIResource1),reinterpret_cast<void**>(resource.put())))||FAILED(resource->CreateSharedHandle(nullptr,DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,nullptr,&out->handle)))return {};
  static std::atomic<uint64_t> sequence{0};out->sequence=++sequence;return out;
 }
};
// Borrow published images for an entire draw submission. The frame owners
// prevent slot reuse; the keyed mutex orders D3D reads against Vulkan writes.
// A second window may read the same completed image after key 0 is returned.
class SharedReads {
 ID3D11DeviceContext*context_;
 std::vector<std::shared_ptr<const SharedImage>> images_;
public:
 explicit SharedReads(ID3D11DeviceContext*context):context_(context){}
 ~SharedReads(){finish();}
 HRESULT acquire(std::shared_ptr<const SharedImage>image){
  for(const auto&prior:images_)if(prior==image)return S_OK;
  const HRESULT result=image->keyed->AcquireSync(image->consumed.load(std::memory_order_acquire)?0:1,0);
  if(result==S_OK)images_.push_back(std::move(image));return result;
 }
 bool finish(){
  if(images_.empty())return true;
  ID3D11ShaderResourceView*empty[2]{};context_->PSSetShaderResources(0,2,empty);
  context_->Flush();bool ok=true;
  for(const auto&image:images_){if(FAILED(image->keyed->ReleaseSync(0)))ok=false;else image->consumed.store(true,std::memory_order_release);}
  images_.clear();if(!ok)available()=false;return ok;
 }
};
}
#endif
