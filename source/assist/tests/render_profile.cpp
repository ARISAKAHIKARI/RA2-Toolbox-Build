#include "../../external_reader/source/render_profile.h"
#include "../../suite/source/settings_categories.h"
#include "../../suite/source/display_settings.h"
#include "../source/shroud_policy.h"
#include <cassert>
#include <thread>
#include <set>
#include <iostream>
struct Assets {
 bool gpu=false,voxel=false,sprite=false,precision=false;int detail=0,sw=0,sh=0,w=0,h=0;external::pixelupgrade::PaletteRange palette;
 void presentationGPU(bool value){gpu=value;}void presentationDetail(int value){detail=value;}void presentationColorPrecision(bool value){precision=value;}
 void presentationLighting(bool v,bool s){voxel=v;sprite=s;}void presentationResolution(int a,int b,int c,int d){sw=a;sh=b;w=c;h=d;}
 void presentationPalette(external::pixelupgrade::PaletteRange value){palette=value;}
};
int main(){
 external::presentation::Profile p;assert(p.particles&&p.lasers&&p.voxelLighting&&p.spriteLighting&&!p.gpu&&!p.material);
 external::presentation::Settings settings;settings.publish(p);assert(settings.get().revision==0);
 p.gpu=true;p.detail=1;p.precision=1;p.palette=1;p.scale=3;p.spriteLighting=false;settings.publish(p);assert(settings.get().revision==1);settings.publish(p);assert(settings.get().revision==1);
 Assets main,watch,map;for(auto*a:{&main,&watch,&map})external::presentation::apply(settings.get().profile,*a,640,512,1280,1024);
 for(auto*a:{&main,&watch,&map})assert(a->gpu&&a->detail==1&&a->precision&&a->voxel&&!a->sprite&&a->palette==external::pixelupgrade::PaletteRange::FullRGB&&a->w==1280);
 std::thread writer([&]{for(int i=0;i<10000;++i){auto q=p;q.lasers=q.particles=bool(i%2);settings.publish(q);}});for(int i=0;i<10000;++i){auto value=settings.get();assert(value.profile.lasers==value.profile.particles);}writer.join();
 std::set<int>ids;for(const auto&s:suite::displaySpecs)assert(ids.insert(s.id).second);assert(suite::DisplayOptionCount==62);
 for(auto o:{suite::RenderParticles,suite::RenderLasers,suite::RenderVoxelLighting,suite::RenderSpriteLighting})assert(suite::validDisplayEdit(o,0)&&suite::validDisplayEdit(o,1)&&!suite::validDisplayEdit(o,2));
 assert(suite::categories::Count==5);for(int id:{420,428,808,809,814,815}){assert(suite::categories::rule(id));assert(suite::categories::row(0,&id,1)==suite::categories::Rules);}
 int automatic[]={0,9042},control[]={9170},video[]={9300},experimental[]={0};assert(suite::categories::row(0,automatic,2)==suite::categories::Automatic);assert(suite::categories::row(4,control,1)==suite::categories::Control);assert(suite::categories::row(3,video,1)==suite::categories::Video);assert(suite::categories::row(4,experimental,1)==suite::categories::Experimental);
 for(int mode=0;mode<=6;++mode)assert(assist::shroudpolicy::fullPreview(mode)==(mode==3||mode==5||mode==6));
 std::cout<<"PASS default-on independent effects, coherent concurrent profile, three viewport propagation, unique owner routing, five categories, material fog preview\n";
}
