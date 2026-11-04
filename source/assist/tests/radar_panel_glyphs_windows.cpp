#ifdef NDEBUG
#undef NDEBUG
#endif
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../source/radar_panel.h"
#include <cassert>
#include <cstdio>
#include <d3dcompiler.h>
#include RADAR_PANEL_SHADER
using gpupresent::Com;
struct Image {int width=100,height=40;std::vector<uint32_t>pixels=std::vector<uint32_t>(4000,0xff123456u);std::shared_ptr<const panelglyphs::Annotations>ranks;std::shared_ptr<const gpupresent::Frame>gpu;};
struct WARP {
 Com<ID3D11Device>device;Com<ID3D11DeviceContext>context;Com<ID3D11VertexShader>vs;Com<ID3D11PixelShader>ps;Com<ID3D11Buffer>constants;Com<ID3D11RasterizerState>raster;Com<ID3D11BlendState>blend;
 WARP(){HMODULE module=gpupresent::systemModule(L"d3d11.dll"),compiler=gpupresent::systemModule(L"d3dcompiler_47.dll");assert(module&&compiler);
  auto create=reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(GetProcAddress(module,"D3D11CreateDevice"));D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;assert(SUCCEEDED(create(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,&level,1,D3D11_SDK_VERSION,device.put(),nullptr,context.put())));
  using Compile=HRESULT(WINAPI*)(LPCVOID,SIZE_T,LPCSTR,const D3D_SHADER_MACRO*,ID3DInclude*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);auto compile=reinterpret_cast<Compile>(GetProcAddress(compiler,"D3DCompile"));Com<ID3DBlob>v,p,error;
  assert(SUCCEEDED(compile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,v.put(),error.put())));auto hr=compile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,p.put(),error.put());if(FAILED(hr)&&error)std::puts(static_cast<const char*>(error->GetBufferPointer()));assert(SUCCEEDED(hr));
  assert(SUCCEEDED(device->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,vs.put()))&&SUCCEEDED(device->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,ps.put())));
  D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(Parameters);cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;assert(SUCCEEDED(device->CreateBuffer(&cb,nullptr,constants.put())));
  D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.ScissorEnable=TRUE;r.DepthClipEnable=TRUE;assert(SUCCEEDED(device->CreateRasterizerState(&r,raster.put())));
  D3D11_BLEND_DESC b{};auto&t=b.RenderTarget[0];t.BlendEnable=TRUE;t.SrcBlend=D3D11_BLEND_ONE;t.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;t.BlendOp=D3D11_BLEND_OP_ADD;t.SrcBlendAlpha=D3D11_BLEND_ONE;t.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;t.BlendOpAlpha=D3D11_BLEND_OP_ADD;t.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;assert(SUCCEEDED(device->CreateBlendState(&b,blend.put())));
 }
 std::vector<uint32_t>render(const Image&image,const radargate::Visual&visual){
  const int w=visual.width,h=visual.height;Com<ID3D11Texture2D>target,readback;Com<ID3D11RenderTargetView>rtv;
  D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET;assert(SUCCEEDED(device->CreateTexture2D(&d,nullptr,target.put()))&&SUCCEEDED(device->CreateRenderTargetView(target.get(),nullptr,rtv.put())));
  auto*rt=rtv.get();context->OMSetRenderTargets(1,&rt,nullptr);context->OMSetBlendState(blend.get(),nullptr,~0u);context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);context->VSSetShader(vs.get(),nullptr,0);context->PSSetShader(ps.get(),nullptr,0);context->RSSetState(raster.get());D3D11_VIEWPORT vp{0,0,float(w),float(h),0,1};context->RSSetViewports(1,&vp);float clear[4]{};context->ClearRenderTargetView(rt,clear);
  auto draw=[&](const uint32_t*pixels,int sw,int sh,const Parameters&p,D3D11_RECT clip){Com<ID3D11Texture2D>texture;Com<ID3D11ShaderResourceView>srv;auto td=d;td.Width=sw;td.Height=sh;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{pixels,UINT(sw)*4,0};assert(SUCCEEDED(device->CreateTexture2D(&td,&data,texture.put()))&&SUCCEEDED(device->CreateShaderResourceView(texture.get(),nullptr,srv.put())));context->UpdateSubresource(constants.get(),0,nullptr,&p,0,0);auto*cb=constants.get();context->VSSetConstantBuffers(0,1,&cb);context->PSSetConstantBuffers(0,1,&cb);auto*view=srv.get();context->PSSetShaderResources(0,1,&view);context->RSSetScissorRects(1,&clip);context->Draw(4,0);};
  Parameters p{{0,0,float(image.width),float(image.height)},{float(image.width),float(image.height),float(image.width)/w,float(image.height)/h},{255,0,0xff9da173u,0},{0,0,float(image.width),float(image.height)}};p.panel[0]=float(w);p.panel[1]=float(h);p.panel[2]=1;p.panelView[0]=float(visual.view.left);p.panelView[1]=float(visual.view.top);p.panelView[2]=float(visual.view.right);p.panelView[3]=float(visual.view.bottom);draw(image.pixels.data(),image.width,image.height,p,{0,0,w,h});
  for(const auto&g:panelglyphs::place(image.ranks,w,h)){Parameters q{{0,0,float(g.sourceWidth),float(g.sourceHeight)},{float(w),float(h),1,1},{255,0,0,0},{float(g.x),float(g.y),float(g.width),float(g.height)}};q.panel[2]=-1;std::copy_n(p.panelView,4,q.panelView);draw(g.pixels->data(),g.sourceWidth,g.sourceHeight,q,{std::max(1,g.x),std::max(1,g.y),std::min(w-1,g.x+g.width),std::min(h-1,g.y+g.height)});}
  ID3D11ShaderResourceView*empty=nullptr;context->PSSetShaderResources(0,1,&empty);context->OMSetRenderTargets(0,nullptr,nullptr);d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.BindFlags=0;assert(SUCCEEDED(device->CreateTexture2D(&d,nullptr,readback.put())));context->CopyResource(readback.get(),target.get());D3D11_MAPPED_SUBRESOURCE mapped{};assert(SUCCEEDED(context->Map(readback.get(),0,D3D11_MAP_READ,0,&mapped)));std::vector<uint32_t>out(size_t(w)*h);for(int y=0;y<h;++y)std::memcpy(out.data()+size_t(y)*w,static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch,w*4);context->Unmap(readback.get(),0);return out;
 }
};
int main(){
 WARP gpu;radarpanel::Buffer buffer;radargate::PaintGate<Image>gate;Image image;auto ranks=std::make_shared<panelglyphs::Annotations>();ranks->worldWidth=1000;ranks->worldHeight=400;
 auto one=std::make_shared<const std::vector<uint32_t>>(std::initializer_list<uint32_t>{0xffffffff,0,0});auto three=std::make_shared<const std::vector<uint32_t>>(3,0xffffffffu);
 ranks->glyphs={{.25,.4,0,0,3,1,one},{.6,.65,-1,-1,3,1,three},{.99,.05,0,0,3,1,three},{.1,-.01,0,0,3,1,three}};image.ranks=ranks;
 HDC screen=GetDC(nullptr);assert(screen);unsigned cases=0;
 for(auto size:std::vector<std::pair<int,int>>{{52,22},{102,42},{202,82},{302,122},{1202,482}}){radargate::Visual v;v.width=size.first;v.height=size.second;v.real=true;v.view={int(size.first*.25)+3,int(size.second*.4)+2,int(size.first*.6),int(size.second*.7)};
  radarpanel::PixelBuffer target;assert(target.acquire(screen,v.width,v.height));assert(buffer.stage(screen,image,v)&&buffer.promote()&&buffer.present(target.acquire(screen,v.width,v.height),v.width,v.height));assert(GdiFlush());auto expected=gpu.render(image,v);for(size_t n=0;n<expected.size();++n)if((target.pixels()[n]&0xffffff)!=(expected[n]&0xffffff)){std::printf("mismatch size=%dx%d pixel=%zu cpu=%08x gpu=%08x\n",v.width,v.height,n,target.pixels()[n],expected[n]);assert(false);}++cases;
  // A failed new stage preserves the previously complete front.
  Image broken=image;broken.pixels.clear();assert(!buffer.stage(screen,broken,v));assert(buffer.present(target.acquire(screen,v.width,v.height),v.width,v.height));assert(GdiFlush());for(size_t n=0;n<expected.size();++n)assert((target.pixels()[n]&0xffffff)==(expected[n]&0xffffff));
 }
 auto small=panelglyphs::place(ranks,52,22),large=panelglyphs::place(ranks,302,122);assert(small[0].width==3&&large[0].width==3&&small[0].x!=large[0].x);
 auto first=std::make_shared<Image>(image);radargate::Visual v;v.image=first;v.width=102;v.height=42;assert(gate.needsVisual(v));gate.committedVisual(v);assert(!gate.needsVisual(v));auto next=std::make_shared<Image>(*first);next->ranks.reset();v.image=next;assert(gate.needsVisual(v));assert(first->ranks==ranks);
 ReleaseDC(nullptr,screen);std::printf("PASS %u final-panel sizes actual GDI Buffer vs production GPU shader WARP RGB equality; native one/three-star pixels, fractional resize, clipping/chrome, failed-stage retention, immutable publication repaint\n",cases);
}
