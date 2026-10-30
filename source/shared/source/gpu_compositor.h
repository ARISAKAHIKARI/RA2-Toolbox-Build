#pragma once
#include "gpu_surface_win32.h"
#include "overlay_composition.h"
#ifdef _WIN32
#include <dcomp.h>
#include <d3dcompiler.h>
#include <set>
namespace gpupresent {
// Presenters serialize the shared immediate context without blocking a UI thread.
// CPU-created UI glyphs are immutable source textures, never a CPU-composited
// full-screen output. Main world sources arrive through shared GPU images.
class Compositor {
 struct Texture {Com<ID3D11Texture2D> image;Com<ID3D11ShaderResourceView> view;int w=0,h=0;};
 struct Cached {Texture texture;std::weak_ptr<const std::vector<uint32_t>> pixels;std::weak_ptr<const Resource> shared;uint64_t sequence=0;};
 std::shared_ptr<Device> owner_;Com<ID3D11DeviceContext> context_;Com<IDXGISwapChain1> swap_;Com<ID3D11RenderTargetView> target_;
 Com<IDCompositionDevice> composition_;Com<IDCompositionTarget> compositionTarget_;Com<IDCompositionVisual> visual_;
 Com<ID3D11VertexShader> vertex_;Com<ID3D11PixelShader> pixel_;Com<ID3D11Buffer> constants_,holes_;Com<ID3D11ShaderResourceView> holesView_;Com<ID3D11BlendState> blend_;Com<ID3D11RasterizerState> raster_;
 HMODULE compiler_=nullptr,dcomp_=nullptr;HWND window_=nullptr;int width_=0,height_=0;size_t holeCapacity_=0;bool ready_=false;
 std::map<uint64_t,Cached> cache_;std::string reason_;
 struct Parameters {float rect[4],output[4];uint32_t data[4];float drawRect[4];};
 static constexpr const char*Shader=R"HLSL(
cbuffer Params:register(b0){float4 rect;float4 output;uint4 data;float4 drawRect;}
Texture2D<float4> source:register(t0);StructuredBuffer<uint2> holes:register(t1);
struct Vertex {float4 position:SV_Position;};
Vertex vs(uint id:SV_VertexID){Vertex o;float2 corner=float2(id&1,id>>1);float2 p=drawRect.xy+corner*drawRect.zw;o.position=float4(p.x*2/output.x-1,1-p.y*2/output.y,0,1);return o;}
float4 ps(Vertex input):SV_Target{int2 p=int2(floor(input.position.xy*output.zw))-int2(rect.xy);
 if(data.w!=0&&(p.x<int(data.w)||p.y<int(data.w)||p.x>=int(rect.z)-int(data.w)||p.y>=int(rect.w)-int(data.w)))return float4(float((data.z>>16)&255u),float((data.z>>8)&255u),float(data.z&255u),255.)/255.;
 if(data.y!=0&&p.y>=0&&p.y<int(rect.w)){uint2 row=holes[p.y];for(uint n=0;n<row.y;++n){uint2 span=holes[row.x+n];if(p.x>=int(span.x)&&p.x<int(span.y))discard;}}
 if(any(p<int2(0,0))||any(p>=int2(rect.zw)))discard;
 return source.Load(int3(p,0))*(float(data.x)/255.0);
}
)HLSL";
 bool fail(const char*text){reason_=text;return false;}
 bool texture(Texture&out,int w,int h,const uint32_t*pixels=nullptr){
  out={};D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
  D3D11_SUBRESOURCE_DATA data{};data.pSysMem=pixels;data.SysMemPitch=UINT(w)*4;
  if(FAILED(owner_->device->CreateTexture2D(&d,pixels?&data:nullptr,out.image.put()))||FAILED(owner_->device->CreateShaderResourceView(out.image.get(),nullptr,out.view.put())))return fail("GPU layer texture allocation failed");out.w=w;out.h=h;return true;
 }
 bool shaders(){
  compiler_=systemModule(L"d3dcompiler_47.dll");if(!compiler_)return fail("System D3DCompiler47 unavailable");
  using Compile=HRESULT(WINAPI*)(LPCVOID,SIZE_T,LPCSTR,const D3D_SHADER_MACRO*,ID3DInclude*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);
  auto compile=reinterpret_cast<Compile>(GetProcAddress(compiler_,"D3DCompile"));if(!compile)return fail("D3DCompile unavailable");Com<ID3DBlob> vs,ps,error;
  if(FAILED(compile(Shader,std::strlen(Shader),"RA2 GPU compositor",nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,vs.put(),error.put()))||FAILED(compile(Shader,std::strlen(Shader),"RA2 GPU compositor",nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,ps.put(),error.put())))return fail("GPU compositor shader compilation failed");
  if(FAILED(owner_->device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,vertex_.put()))||FAILED(owner_->device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,pixel_.put())))return fail("GPU compositor shader creation failed");
  D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=sizeof(Parameters);buffer.Usage=D3D11_USAGE_DYNAMIC;buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;buffer.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
  if(FAILED(owner_->device->CreateBuffer(&buffer,nullptr,constants_.put())))return fail("GPU compositor constants unavailable");
  D3D11_BLEND_DESC blend{};auto&b=blend.RenderTarget[0];b.BlendEnable=TRUE;b.SrcBlend=D3D11_BLEND_ONE;b.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;b.BlendOp=D3D11_BLEND_OP_ADD;b.SrcBlendAlpha=D3D11_BLEND_ONE;b.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;b.BlendOpAlpha=D3D11_BLEND_OP_ADD;b.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
  D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.ScissorEnable=TRUE;raster.DepthClipEnable=TRUE;
  if(FAILED(owner_->device->CreateBlendState(&blend,blend_.put()))||FAILED(owner_->device->CreateRasterizerState(&raster,raster_.put())))return fail("GPU compositor pipeline unavailable");return true;
 }
 bool initialize(HWND window,std::shared_ptr<Device> owner){
  if(ready_&&owner_==owner&&window_==window)return true;reset();owner_=std::move(owner);window_=window;owner_->device->GetImmediateContext(context_.put());
  if(!context_||!shaders())return false;dcomp_=systemModule(L"dcomp.dll");if(!dcomp_)return fail("DirectComposition unavailable");
  using Create=HRESULT(WINAPI*)(IDXGIDevice*,REFIID,void**);auto create=reinterpret_cast<Create>(GetProcAddress(dcomp_,"DCompositionCreateDevice"));Com<IDXGIDevice> dxgi;
  if(!create||FAILED(owner_->device->QueryInterface(__uuidof(IDXGIDevice),reinterpret_cast<void**>(dxgi.put())))||FAILED(create(dxgi.get(),__uuidof(IDCompositionDevice),reinterpret_cast<void**>(composition_.put())))||FAILED(composition_->CreateTargetForHwnd(window,TRUE,compositionTarget_.put()))||FAILED(composition_->CreateVisual(visual_.put()))||FAILED(compositionTarget_->SetRoot(visual_.get())))return fail("DirectComposition target creation failed");ready_=true;return true;
 }
 bool resize(int w,int h){
  if(swap_&&w==width_&&h==height_)return true;context_->OMSetRenderTargets(0,nullptr,nullptr);target_.reset();
  if(swap_){if(FAILED(swap_->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)))return fail("Composition swap chain resize failed");}
  else{DXGI_SWAP_CHAIN_DESC1 d{};d.Width=w;d.Height=h;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.BufferCount=2;d.Scaling=DXGI_SCALING_STRETCH;d.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;d.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;d.Flags=DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
   if(FAILED(owner_->factory->CreateSwapChainForComposition(owner_->device.get(),&d,nullptr,swap_.put()))||FAILED(visual_->SetContent(swap_.get())))return fail("Composition swap chain creation failed");
   Com<IDXGISwapChain2> latency;if(SUCCEEDED(swap_->QueryInterface(__uuidof(IDXGISwapChain2),reinterpret_cast<void**>(latency.put()))))latency->SetMaximumFrameLatency(1);
  }
  Com<ID3D11Texture2D> back;if(FAILED(swap_->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(back.put())))||FAILED(owner_->device->CreateRenderTargetView(back.get(),nullptr,target_.put())))return fail("Composition back buffer unavailable");width_=w;height_=h;return true;
 }
 bool cutouts(const overlaycomposition::Surface&s){
  if(!s.cutouts||s.cutouts->empty())return true;
  std::vector<std::array<uint32_t,2>> words(size_t(s.rect.h));auto spans=*s.cutouts;std::sort(spans.begin(),spans.end(),[](const auto&a,const auto&b){return a.y!=b.y?a.y<b.y:a.x<b.x;});
  for(const auto&r:spans){auto&row=words[size_t(r.y)];if(!row[1])row[0]=uint32_t(words.size());++row[1];words.push_back({uint32_t(r.x),uint32_t(r.x+r.w)});}
  if(words.size()>holeCapacity_){holesView_.reset();holes_.reset();holeCapacity_=std::max<size_t>(256,words.size()*2);D3D11_BUFFER_DESC d{};d.ByteWidth=UINT(holeCapacity_*8);d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;d.StructureByteStride=8;
   if(FAILED(owner_->device->CreateBuffer(&d,nullptr,holes_.put())))return false;D3D11_SHADER_RESOURCE_VIEW_DESC v{};v.Format=DXGI_FORMAT_UNKNOWN;v.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;v.Buffer.NumElements=UINT(holeCapacity_);if(FAILED(owner_->device->CreateShaderResourceView(holes_.get(),&v,holesView_.put())))return false;}
  D3D11_BOX region{0,0,0,UINT(words.size()*8),1,1};context_->UpdateSubresource(holes_.get(),0,&region,words.data(),0,0);return true;
 }
 bool source(const overlaycomposition::Surface&s,Cached&cache){
  if(s.gpu){auto image=std::static_pointer_cast<const SharedImage>(s.gpu->resource);if(image->owner!=owner_)return fail("GPU layer adapter mismatch");
   if(cache.shared.lock()==image&&cache.sequence==image->sequence&&cache.texture.view)return true;
   if(!image->view)return fail("GPU shared frame view unavailable");
   cache.texture={};image->view->AddRef();*cache.texture.view.put()=image->view.get();
   cache.texture.w=s.rect.w;cache.texture.h=s.rect.h;
   cache.shared=image;cache.sequence=image->sequence;cache.pixels.reset();return true;
  }
  // Never upload a CPU fallback into a texture borrowed from Vulkan.
  if(cache.sequence){cache.texture={};cache.shared.reset();cache.sequence=0;}

  if(cache.pixels.lock()==s.pixels&&cache.texture.image)return true;
  if(!s.pixels||s.pixels->size()!=size_t(s.rect.w)*s.rect.h)return false;
  if(!cache.texture.image||cache.texture.w!=s.rect.w||cache.texture.h!=s.rect.h){if(!texture(cache.texture,s.rect.w,s.rect.h,s.pixels->data()))return false;}
  else context_->UpdateSubresource(cache.texture.image.get(),0,nullptr,s.pixels->data(),UINT(s.rect.w)*4,0);
  cache.pixels=s.pixels;cache.shared.reset();cache.sequence=0;return true;
 }
public:
 ~Compositor(){reset();}
 const std::string&reason()const{return reason_;}
 void reset(){cache_.clear();target_.reset();swap_.reset();visual_.reset();compositionTarget_.reset();composition_.reset();vertex_.reset();pixel_.reset();constants_.reset();holesView_.reset();holes_.reset();blend_.reset();raster_.reset();context_.reset();owner_.reset();window_=nullptr;ready_=false;width_=height_=0;holeCapacity_=0;if(compiler_)FreeLibrary(compiler_);if(dcomp_)FreeLibrary(dcomp_);compiler_=dcomp_=nullptr;}
 bool present(HWND window,const overlaycomposition::Context&view,std::vector<overlaycomposition::Surface>layers,int targetWidth=0,int targetHeight=0,uint32_t border=0){
  overlaycomposition::order(layers,view);auto world=std::find_if(layers.begin(),layers.end(),[](const auto&s){return bool(s.gpu);});
  std::shared_ptr<Device> owner=world!=layers.end()?std::static_pointer_cast<const SharedImage>(world->gpu->resource)->owner:owner_;
  if(!owner)return fail("GPU presentation device unavailable");
  std::unique_lock<std::mutex>contextLock(owner->presentationMutex,std::try_to_lock);
  if(!contextLock.owns_lock())return fail("GPU frame not ready");
  const int physicalW=targetWidth>0?targetWidth:view.width,physicalH=targetHeight>0?targetHeight:view.height;
  if(physicalW<1||physicalH<1||physicalW>8192||physicalH>8192||uint64_t(physicalW)*physicalH>16*1024*1024)return fail("GPU presentation dimensions invalid");
  if(!initialize(window,owner)||!resize(physicalW,physicalH))return false;
  SharedReads reads(context_.get());
  for(const auto&s:layers){
   if(s.gpu){const auto image=std::static_pointer_cast<const SharedImage>(s.gpu->resource);
    if(image->owner!=owner_)return fail("GPU layer adapter mismatch");
    const HRESULT result=reads.acquire(image);if(result!=S_OK)return fail(result==WAIT_TIMEOUT?"GPU frame not ready":"GPU shared frame acquire failed");
   }
   if(!source(s,cache_[s.id]))return false;
  }
  const float clear[4]{};context_->ClearRenderTargetView(target_.get(),clear);ID3D11RenderTargetView*target=target_.get();context_->OMSetRenderTargets(1,&target,nullptr);context_->OMSetBlendState(blend_.get(),nullptr,~0u);
  context_->IASetInputLayout(nullptr);context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);context_->VSSetShader(vertex_.get(),nullptr,0);context_->PSSetShader(pixel_.get(),nullptr,0);context_->RSSetState(raster_.get());
  D3D11_VIEWPORT viewport{0,0,float(physicalW),float(physicalH),0,1};context_->RSSetViewports(1,&viewport);
  for(const auto&s:layers){auto bounds=overlaycomposition::bounds(s,view);if(bounds.w<=0||bounds.h<=0)continue;if(!cutouts(s))return fail("GPU cutout upload failed");
   Parameters p{{float(s.rect.x),float(s.rect.y),float(s.rect.w),float(s.rect.h)},{float(view.width),float(view.height),float(view.width)/physicalW,float(view.height)/physicalH},{s.opacity,uint32_t(s.cutouts&&!s.cutouts->empty()),border,border?6u:0u},{float(bounds.x),float(bounds.y),float(bounds.w),float(bounds.h)}};
   D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(context_->Map(constants_.get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return fail("GPU compositor parameter upload failed");std::memcpy(mapped.pData,&p,sizeof p);context_->Unmap(constants_.get(),0);
   ID3D11Buffer*constants=constants_.get();context_->VSSetConstantBuffers(0,1,&constants);context_->PSSetConstantBuffers(0,1,&constants);ID3D11ShaderResourceView*views[]={cache_[s.id].texture.view.get(),p.data[1]?holesView_.get():nullptr};context_->PSSetShaderResources(0,2,views);
   D3D11_RECT clip{LONG(int64_t(bounds.x)*physicalW/view.width),LONG(int64_t(bounds.y)*physicalH/view.height),LONG((int64_t(bounds.x+bounds.w)*physicalW+view.width-1)/view.width),LONG((int64_t(bounds.y+bounds.h)*physicalH+view.height-1)/view.height)};context_->RSSetScissorRects(1,&clip);context_->Draw(4,0);
  }
  ID3D11ShaderResourceView*empty[2]{};context_->PSSetShaderResources(0,2,empty);context_->OMSetRenderTargets(0,nullptr,nullptr);
  if(!reads.finish())return fail("GPU shared frame release failed");
  const HRESULT result=swap_->Present(0,DXGI_PRESENT_DO_NOT_WAIT);if(result==DXGI_ERROR_WAS_STILL_DRAWING)return fail("GPU frame not ready");if(FAILED(result)||FAILED(composition_->Commit()))return fail("GPU composition present failed");
  for(auto i=cache_.begin();i!=cache_.end();)if(std::none_of(layers.begin(),layers.end(),[&](const auto&s){return s.id==i->first;}))i=cache_.erase(i);else ++i;
  reason_.clear();return true;
 }
 bool presentFrame(HWND window,std::shared_ptr<const Frame>frame,int w,int h,uint32_t border){
  if(!frame||!frame->resource)return fail("GPU frame unavailable");
  const int sourceW=frame->resource->width,sourceH=frame->resource->height;
  overlaycomposition::Context view{1,reinterpret_cast<uintptr_t>(window),0,0,sourceW,sourceH,0,0};
  overlaycomposition::Surface layer{1,0,view,{0,0,sourceW,sourceH},false,255,{}, {}, {},std::move(frame)};
  return present(window,view,{std::move(layer)},w,h,border);
 }

};
}
#endif
