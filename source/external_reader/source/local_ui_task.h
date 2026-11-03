#pragma once
#include "local_ui_renderer.h"
#include "feedback_orders.h"
#include "../../shared/source/latest_worker.h"
#include <functional>
namespace external {namespace localui {
// One reader/cache owner and replaceable pending input. Font discovery, text
// layout, string-table lookup and rasterization never run on the UI thread.
template<class Request,class Reader>class Task {
 struct Input {Request request;std::vector<nativefeedback::Object>objects;};
public:
 struct Image {nativefeedback::Rect bounds;std::shared_ptr<const std::vector<uint32_t>>pixels;};
 struct Result {Request request;mainview::View view;uint32_t sampledTick=0;std::array<Image,LayerCount>images;unsigned messages=0,timers=0,primary=0,rallies=0;};
private:
 Reader reader_;Engine engine_;uint32_t pid_=0,match_=0;std::function<uint32_t()>clock_;
 std::array<std::vector<nativefeedback::Primitive>,LayerCount>old_;std::array<Image,LayerCount>images_;
 asyncwork::Latest<Input,Result>worker_;
 std::shared_ptr<const Result>run(const Input&input){const auto&r=input.request;
  if(pid_!=r.pid||match_!=r.match){reader_.close();engine_.reset();for(auto&p:old_)p.clear();images_={};pid_=r.pid;match_=r.match;}
  if(!reader_.open(r.pid))return {};reader_.begin();auto fresh=[&](uint32_t p,void*out,size_t n){return reader_.readFresh(p,out,n);};auto read=[&](uint32_t p,void*out,size_t n){return reader_.read(p,out,n);};mainview::View before;
  if(!mainview::sample(fresh,before,r.view.transform)||!mainview::sameViewport(before,r.view)||before.frame<r.view.frame)return {};uint32_t sampled=clock_();Plan plan;
  if(!engine_.capture(read,fresh,before,input.objects,r.width,r.height,plan,sampled))return {};
  for(size_t lane=0;lane<plan.layers.size();++lane){auto&primitives=plan.layers[lane];if(primitives.empty()){images_[lane]={};old_[lane].clear();}
   else if(primitives!=old_[lane]){nativefeedback::Canvas canvas;nativefeedback::Rect bounds;if(!nativefeedback::paintInteractions(primitives,{},canvas,bounds)){images_[lane]={};old_[lane].clear();continue;}images_[lane]={bounds,std::make_shared<const std::vector<uint32_t>>(std::move(canvas.pixels))};old_[lane]=std::move(primitives);}
  }
  mainview::View after;if(!mainview::sample(fresh,after,r.view.transform)||!mainview::sameViewport(before,after)||after.frame<before.frame||uint32_t(clock_()-sampled)>150)return {};
  auto result=std::make_shared<Result>();result->request=r;result->view=before;result->sampledTick=sampled;result->images=images_;result->messages=plan.messages;result->timers=plan.timers;result->primary=plan.primary;result->rallies=plan.rallies;return result;
 }
public:
 explicit Task(std::function<uint32_t()>clock):clock_(std::move(clock)),worker_([this](const Input&i){return run(i);}){}
 void submit(const Request&r,const std::vector<nativefeedback::Object>&objects){Input input;input.request=r;for(const auto&o:objects)if(o.kind>=0&&o.kind<=3)input.objects.push_back(o);worker_.submit(std::move(input));}
 std::shared_ptr<const Result>get(){return worker_.get();}
 std::shared_ptr<const Result>visible(const Request&r,uint32_t now){auto result=worker_.get();if(!result||result->request.pid!=r.pid||result->request.match!=r.match||result->request.generation!=r.generation||result->request.width!=r.width||result->request.height!=r.height||result->request.context.token!=r.context.token||!mainview::sameViewport(result->view,r.view)||r.view.frame<result->view.frame||uint32_t(now-result->sampledTick)>150)return {};return result;}
 void invalidate(){worker_.invalidate();}void shutdown(){worker_.shutdown();}
};
}}
