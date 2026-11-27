#include "../source/type_catalog.h"
#include "../../external_reader/source/catalogue_stamp.h"
#include <cassert>
#include <cstring>
#include <string>
static assist::Type item(const char* id,int kind,int index,const char* name){
 assist::Type t{};std::strncpy(t.id,id,sizeof(t.id)-1);
 t.kind=kind;t.index=index;
 for(size_t i=0;i<std::strlen(name)&&i<39;++i)t.name[i]=name[i];
 return t;
}
struct DependencyFixture {
 std::vector<uint32_t> retained;
 uint32_t missing=0;
 int calls=0;
};
struct CatalogueRead {
 DependencyFixture* fixture;
 bool operator()(uint32_t at,void*out,size_t n)const{
  ++fixture->calls;
  if(at==fixture->missing)return false;
  std::memset(out,0,n);
  if(at==0xA8B218+4&&n==16){
   const uint32_t header[]={0x30000,1,0,1};
   std::memcpy(out,header,n);
  }else if(at==0x30000&&n==4){
   const uint32_t pointer=0x40000;std::memcpy(out,&pointer,n);
  }else if(at==0x40024&&n==24){
   const char id[]="TEST";std::memcpy(out,id,sizeof id);
  }
  return true;
 }
 void retainWorldPages(const std::vector<uint32_t>&pages)const{
  fixture->retained.insert(fixture->retained.end(),pages.begin(),pages.end());
 }
};
static void projectionDependencies(){
 external::catalogue::Dependencies deps;
 DependencyFixture fixture;CatalogueRead read{&fixture};
 auto retained=[&](uint32_t page){return std::find(fixture.retained.begin(),fixture.retained.end(),page)!=fixture.retained.end();};
 auto project=[&](uint32_t at,size_t n){
  return deps.captureProjection(read,[&](auto tracked){
   unsigned char bytes[32]{};return tracked(at,bytes,n);
  });
 };
 assert(project(0x50ff8,16)); // Cross-page field.
 fixture.retained.clear();int calls=fixture.calls;deps.maintain(read);
 assert(retained(0x50000)&&retained(0x51000)&&fixture.calls==calls);
 external::catalogue::Stamp stamp;
 assert(deps.capture(read,stamp)&&stamp.count==1);
 fixture.retained.clear();deps.maintain(read);
 assert(retained(0x50000)&&retained(0x51000)&&retained(0x40000));
 fixture.missing=0x60000;
 assert(!project(0x60000,8));
 fixture.retained.clear();deps.maintain(read);
 assert(retained(0x60000)&&retained(0x50000)&&retained(0x51000));
 fixture.missing=0x61000;
 assert(!project(0x61000,8));
 fixture.retained.clear();deps.maintain(read);
 assert(retained(0x60000)&&retained(0x61000)&&retained(0x50000));
 fixture.missing=0;
 assert(project(0x62000,8));
 fixture.retained.clear();deps.maintain(read);
 assert(retained(0x62000)&&retained(0x40000)&&!retained(0x60000)&&!retained(0x50000));
 deps.clear();fixture.retained.clear();deps.maintain(read);
 assert(fixture.retained.empty());
}
int main(){
 projectionDependencies();
 typecatalog::ObservedCatalogue observed;
 assist::Type rows[]={item("E1",16,3,"Engineer"),item("MTNK",40,2,"Tank")};
 assert(observed.changed(1,rows,2,0));
 assert(!observed.changed(1,rows,2,250));
 rows[0].cost=500;
 assert(!observed.changed(1,rows,2,251));
 assert(observed.changed(1,rows,2,500));
 assert(!observed.changed(1,rows,2,750));
 std::swap(rows[0],rows[1]);
 assert(observed.changed(1,rows,2,1000));
 assert(observed.changed(2,rows,2,1001));
 assert(observed.changed(2,rows,1,1002));
 assert(!observed.changed(2,nullptr,1,2000));
 typecatalog::ObservedCatalogue wrapObserved;
 assert(wrapObserved.changed(1,rows,2,0xfffffff0u));
 rows[0].index++;
 assert(!wrapObserved.changed(1,rows,2,0u));
 assert(wrapObserved.changed(1,rows,2,234u));
 typecatalog::WriteState write;
 assert(write.due(1,0));
 write.attempted(0);
 assert(!write.due(1,999)&&write.due(1,1000));
 assert(write.needsWrite(1)); // failure is still pending
 write.attempted(1000);write.completed(1);
 assert(!write.due(1,9000));
 assert(!write.due(2,1999)&&write.due(2,2000));
 write.completed(2);assert(!write.needsWrite(2));
 typecatalog::WriteState wrap;
 wrap.attempted(0xfffffff0u);
 assert(!wrap.due(1,0u)&&wrap.due(1,984u));
 typecatalog::Catalogue c;
 assert(c.ensure("E1"));assert(c.types.size()==1);
 assert(!c.ensure("E1"));assert(c.types[0].kind==-1);
 auto e=item("E1",16,3,"Engineer");
 assert(c.remember(e));assert(c.types.size()==1);
 assert(c.types[0].kind==16&&c.types[0].index==3);
 assert(!c.remember(e)); // no redundant disk writes each game frame
 auto tank=item("MTNK",40,2,"Tank");assert(c.remember(tank));
 assert(c.types.size()==2);
 auto newer=item("E1",16,7,"Engineer");assert(c.remember(newer));
 assert(c.types[0].index==7); // previously saved ID survives reordered type indices
 assert(c.find("E1",16)==0);
 assert(c.find("E1",40)<0);
 assert(!c.ensure(""));assert(!c.ensure("bad key"));
 auto bad=item("E1",40,9,"Vehicle");assert(c.remember(bad));
 assert(c.types.size()==3); // never collapse *real* same-ID distinct kinds
 int row=99;
 assert(!c.remember(bad,&row)&&row==-1);
 auto next=item("NEW",40,12,"New");assert(c.remember(next,&row)&&row==3);
 assert(c.ensure("FUTURE"));auto future=item("FUTURE",16,2,"Future");
 assert(c.remember(future,&row)&&row==4&&c.types.size()==5);
 future.index=3;assert(c.remember(future,&row)&&row==4);
 auto invalid=item("invalid id",40,0,"Invalid");
 assert(!c.remember(invalid,&row)&&row==-1);
 auto unterminated=item("bad",40,0,"Bad");
 std::memset(unterminated.id,'X',sizeof unterminated.id);
 assert(typecatalog::idView(unterminated).empty());
 assert(!c.remember(unterminated,&row)&&row==-1);
 // Exact kind always takes priority over an earlier placeholder.
 typecatalog::Catalogue ordered;
 ordered.types.push_back(typecatalog::placeholder("E1"));
 ordered.types.push_back(e);
 assert(ordered.remember(newer,&row)&&row==1);
 assert(ordered.types[0].kind==-1&&ordered.types.size()==2);
}
