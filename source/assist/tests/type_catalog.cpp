#include "../source/type_catalog.h"
#include <cassert>
#include <cstring>
#include <string>
static assist::Type item(const char* id,int kind,int index,const char* name){
 assist::Type t{};std::strncpy(t.id,id,sizeof(t.id)-1);
 t.kind=kind;t.index=index;
 for(size_t i=0;i<std::strlen(name)&&i<39;++i)t.name[i]=name[i];
 return t;
}
int main(){
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
