#pragma once
#include "../../external_reader/source/sidebar_indicators.h"
namespace sidebarhud {
constexpr uint64_t Layer=24;
struct Request {uint64_t generation=0;uint32_t pid=0,match=0;bool harvesters=false,power=false;external::mainview::View view;overlaycompositor::Context context;};
struct Result {Request request;uint32_t sampled=0;external::nativefeedback::Rect bounds;std::shared_ptr<const std::vector<uint32_t>>pixels;};
static external::ReadSession reader;static external::sidebarindicators::Engine engine;static uint32_t readerPid=0,readerMatch=0,submittedTick=0;static uint64_t generation=1;static int submittedFrame=-1;static unsigned submittedFlags=0;
static asyncwork::Latest<Request,Result>worker([](const Request&r)->std::shared_ptr<const Result>{
 if(readerPid!=r.pid||readerMatch!=r.match){reader.close();engine.reset();readerPid=r.pid;readerMatch=r.match;}
 if(!reader.open(r.pid))return {};reader.begin();auto read=[](uint32_t p,void*out,size_t n){return reader.read(p,out,n);};auto fresh=[](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};
 external::mainview::View view;if(!external::mainview::sample(fresh,view,nativeviewportprojection::sample(shared))||view.house!=r.view.house||view.screenW!=r.view.screenW||view.screenH!=r.view.screenH||view.physicalX()!=r.view.physicalX()||view.physicalWidth()!=r.view.physicalWidth())return {};
 auto result=std::make_shared<Result>();result->request=r;result->sampled=GetTickCount();external::sidebarindicators::Sample sample;
 if(!external::sidebarindicators::capture(fresh,view.house,view.frame,sample)||!engine.render(read,view,sample,r.context.width,r.context.height,r.harvesters,r.power,result->bounds,result->pixels))return {};
 return result;
});
static void stop(){++generation;submittedFrame=-1;submittedFlags=0;worker.invalidate();overlaycompositor::host().hide(Layer);}
static void shutdown(){stop();worker.shutdown();}
static void update(uint32_t pid,uint32_t match,const external::mainview::View&view,const overlaycompositor::Context&context,bool harvesters,bool power){
 if(!harvesters&&!power){if(submittedFlags)stop();return;}
 unsigned flags=unsigned(harvesters)|(unsigned(power)<<1);Request request{generation,pid,match,harvesters,power,view,context};auto result=worker.get();uint32_t now=GetTickCount();
 bool geometry=result&&result->request.view.screenW==view.screenW&&result->request.view.screenH==view.screenH&&result->request.view.physicalX()==view.physicalX()&&result->request.view.physicalWidth()==view.physicalWidth();
 if(result&&geometry&&result->request.generation==generation&&result->request.pid==pid&&result->request.match==match&&result->request.context.token==context.token&&result->request.harvesters==harvesters&&result->request.power==power&&uint32_t(now-result->sampled)<250&&result->pixels){auto b=result->bounds;overlaycompositor::host().publish({Layer,70,context,{b.x,b.y,b.w,b.h},false,255,result->pixels});}
 else overlaycompositor::host().hide(Layer);
 if(submittedFrame!=view.frame||submittedFlags!=flags||!geometry||result->request.context.token!=context.token||uint32_t(now-submittedTick)>=100){submittedFrame=view.frame;submittedFlags=flags;submittedTick=now;worker.submit(std::move(request));}
}
}
