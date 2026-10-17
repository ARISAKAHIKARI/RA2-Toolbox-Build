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
}
