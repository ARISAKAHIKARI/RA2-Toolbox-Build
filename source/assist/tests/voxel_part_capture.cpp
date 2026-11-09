#include "source/external_reader/source/radar_scene.h"
using namespace external;
static constexpr uint32_t Actor=0x10000,Type=0x20000,Owner=0x30000,Tile=0x40000;
static void makeVoxel(uint32_t v,int color,float lowZ,float highZ){
 uint32_t tail=v+0x1000,body=v+0x2000,hva=v+0x3000,matrix=v+0x4000;
 word(memory,v+4,1);word(memory,v+8,1);word(memory,v+12,13);word(memory,v+20,tail);word(memory,v+24,body);
 word(memory,tail,body);word(memory,tail+4,body+4);word(memory,tail+8,body+8);word(memory,body,0);word(memory,body+4,4);
 memory[body+8]=0;memory[body+9]=1;memory[body+10]=uint8_t(color);memory[body+11]=0;memory[body+12]=1;
 for(int i=0;i<8;++i){float bounds[3]={float((i&1)?20:0),float((i&2)?20:0),(i&4)?highZ:lowZ};memcpy(memory.data()+tail+0x40+i*12,bounds,sizeof bounds);}
 memory[tail+0xA0]=memory[tail+0xA1]=memory[tail+0xA2]=1;memory[tail+0xA3]=4;
 word(memory,hva+4,1);word(memory,hva+8,1);word(memory,hva+12,matrix);float m[12]={1,0,0,0,0,1,0,0,0,0,1,0};memcpy(memory.data()+matrix,m,sizeof m);
}
static void setup(){
 std::fill(memory.begin(),memory.end(),0);disk.clear();resetFault();
 word(memory,Actor,0x7F5C70);word(memory,Actor+0x10,42);word(memory,Actor+0x6C,100);word(memory,Actor+0x6C4,Type);word(memory,Actor+0x21C,Owner);
 memcpy(memory.data()+Type+0x24,"TEST",5);memcpy(memory.data()+Type+0x1F8,"TEST",5);
 word(memory,Type+0xB0,0x710000);word(memory,Type+0xB4,0x713000);word(memory,Type+0xB8,0x720000);word(memory,Type+0xBC,0x723000);
 makeVoxel(0x710000,3,0,8);makeVoxel(0x720000,4,8,24);
 memcpy(memory.data()+0x7E1B78+68,"ISOTEM",7);memcpy(memory.data()+0x7E1B78+78,"TEM",4);memcpy(memory.data()+0x7E1B78+48,"TEMPERAT",9);memory[0x7E1B78+86]='T';
 memcpy(memory.data()+Tile+0x2F5,"CLEAR01.TEM",12);
 auto&unit=disk["UNITTEM.PAL"];unit.assign(768,32);unit[9]=63;unit[10]=0;unit[11]=0;unit[12]=0;unit[13]=63;unit[14]=0;
 auto&iso=disk["ISOTEM.PAL"];iso.assign(768,16);
 std::vector<uint8_t>tmp(20+52+1800);word(tmp,0,1);word(tmp,4,1);word(tmp,8,60);word(tmp,12,30);word(tmp,16,20);word(tmp,32,952);word(tmp,56,2);std::fill(tmp.begin()+72,tmp.begin()+972,5);disk["CLEAR01.TEM"]=tmp;
 const std::string rules="[TEST]\nNoShadow=yes\n";disk["RULESMD.INI"]={rules.begin(),rules.end()};disk["ARTMD.INI"]={};
}
static void initialize(gameassets::Context&c,unsigned match){c.identity(pid,match);c.presentationLighting(false,false);}
static radar::Frame frame(){
 radar::Frame f;auto t=std::make_shared<radar::Terrain>();t->geo={8,8,4,4,8,16,1,0x60000,Owner,512*512,{8,8,0,0,8,8},8,16};t->reveal=true;t->colors.resize(512*512);t->art.resize(512*512);t->shade.resize(512*512);
 for(int y=4;y<8;++y)for(int x=4;x<8;++x){size_t at=y*512+x;t->colors[at]=0x204020;t->art[at].type=Tile;}
 f.terrain=t;f.tick=100;f.frame=1;f.posesPrepared=true;radar::Marker m;m.object=Actor;m.type=Type;m.owner=Owner;m.id=42;m.x=m.y=6*256;m.kind=2;m.layer=2;m.pose.variant=-1;m.pose.voxel=true;f.markers={m};return f;
}
static unsigned checks=0,failures=0;static uint32_t generation=900;static uint64_t healthyDigest=1469598103934665603ull;
static void check(bool ok,const char*message){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<message<<'\n';}}
static void configure(bool extra,bool barrel){
 setup();if(barrel){word(memory,Type+0xB8,0);word(memory,Type+0xBC,0);word(memory,Type+0xC0,0x720000);word(memory,Type+0xC4,0x723000);}
 if(extra){word(memory,Type+0x808,1);uint32_t slot=Type+(barrel?0x158:0xC8);word(memory,slot,0x720000);word(memory,slot+4,0x723000);}
}
static void captures(){
 for(bool extra:{false,true})for(bool barrel:{false,true})for(uint32_t missing:{0x720000u,0x721000u,0x722000u,0x723000u,0x724000u}){
  configure(extra,barrel);gameassets::Context c(externalReader);initialize(c,++generation);gameassets::ActorArt art;art.type=0x123456;
  setFault(missing,4096,true);check(!c.captureActor(Actor,Type,42,art),"nonempty failed component rejects capture");check(art.type==0x123456,"failed capture leaves output untouched");check(faultHits>0,"resource fault exercised");resetFault();
  check(c.captureActor(Actor,Type,42,art)&&art.model.faces.size()==12,"component recovers on same context");
 }
 for(uint32_t missing:{Type+0xB8,Type+0xBC,Type+0x360,Type+0x368,Type+0x720,Type+0x808,Type+0xCD5,Actor+0x124,Type+0xC8,Type+0xCC}){
  configure(missing==Type+0xCD5||missing==Actor+0x124||missing==Type+0xC8||missing==Type+0xCC,false);gameassets::Context c(externalReader);initialize(c,++generation);gameassets::ActorArt art;art.type=0x123456;
  setFault(missing,1);check(!c.captureActor(Actor,Type,42,art)&&art.type==0x123456,"unavailable binding cannot commit default geometry");check(faultHits>0,"binding fault exercised");resetFault();check(c.captureActor(Actor,Type,42,art)&&art.model.faces.size()==12,"binding recovers");
 }
 setup();gameassets::Context c(externalReader);initialize(c,++generation);gameassets::ActorArt art;
 setFault(0x712000,4096,true);check(!c.captureActor(Actor,Type,42,art),"failed body rejected");resetFault();
 word(memory,Type+0xB8,0);word(memory,Type+0xBC,0);check(c.captureActor(Actor,Type,42,art)&&art.model.faces.size()==6,"verified absent turret accepted");
 word(memory,Type+0x808,1);check(c.captureActor(Actor,Type,42,art)&&art.model.faces.size()==6,"verified absent extra components accepted");
 word(memory,Type+0x808,0);word(memory,Type+0xB8,0x720000);check(!c.captureActor(Actor,Type,42,art),"nonnull voxel with missing HVA rejected");
 word(memory,Type+0xB8,0);word(memory,Type+0xBC,0x723000);check(c.captureActor(Actor,Type,42,art)&&art.model.faces.size()==6,"null voxel is absent even with an unused HVA");
 setFault(Type+0xBC,4);check(c.captureActor(Actor,Type,42,art)&&faultHits==0,"absent voxel does not require its unused HVA slot");resetFault();
 word(memory,Type+0x808,1);word(memory,Type+0xCC,0x723000);setFault(Type+0xCC,4);check(c.captureActor(Actor,Type,42,art)&&art.model.faces.size()==6&&faultHits==0,"absent extra voxel does not require its unused HVA");resetFault();
 setup();gameassets::Context fallback(externalReader);initialize(fallback,++generation);disk["TESTTUR.VXL"]={0};disk["TESTTUR.HVA"]={0};check(fallback.captureActor(Actor,Type,42,art)&&art.model.faces.size()==12,"invalid disk component retains healthy live fallback");
 setup();word(memory,Type+0xB0,0);word(memory,Type+0xB4,0x713000);disk["TEST.SHP"]=partShape(2,0,3);gameassets::Context shp(externalReader);initialize(shp,++generation);check(shp.captureActor(Actor,Type,42,art)&&!art.voxel&&!art.shape.empty(),"null voxel body retains regular SHP capture");
}
static void scenes(){
 size_t differences=0;unsigned scenes=0;
 for(bool extra:{false,true})for(bool barrel:{false,true})for(bool live:{false,true}){
  configure(extra,barrel);gameassets::Context warmed(externalReader),fresh(externalReader);uint32_t match=++generation;initialize(warmed,match);initialize(fresh,match);radar::SceneRenderer<gameassets::Context>wr,fr;auto f=frame();if(extra)f.markers.front().pose.variant=0;
  const radar::SceneRegion region{0,0,240,240,true};setFault(0x722000,4096,true);wr.render(f,240,240,warmed.read,warmed,region,1,true,live,false,true);
  check(!wr.materialsReady(),"failed component cannot publish ready materials");check(faultHits>0,"scene exercises failed resource page");resetFault();
  f.tick+=20;++f.frame;auto recovered=wr.render(f,240,240,warmed.read,warmed,region,1,true,live,false,true);auto reference=fr.render(f,240,240,fresh.read,fresh,region,1,true,live,false,true);
  check(recovered&&reference&&wr.materialsReady()&&fr.materialsReady(),"same renderer recovers after page returns");
  if(reference)for(auto pixel:reference->pixels)healthyDigest=(healthyDigest^pixel)*1099511628211ull;
  size_t delta=0;if(recovered&&reference&&recovered->pixels.size()==reference->pixels.size())for(size_t n=0;n<recovered->pixels.size();++n)delta+=recovered->pixels[n]!=reference->pixels[n];else ++delta;
  check(delta==0,"recovered scene equals fresh healthy renderer");differences+=delta;++scenes;
 }
 std::cout<<"recovered_scenes="<<scenes<<" pixel_mismatches="<<differences<<'\n';
}
int main(){captures();scenes();std::cout<<"healthy_digest="<<std::hex<<healthyDigest<<std::dec<<" checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;}
