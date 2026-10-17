#include "../source/feature_type_config.h"
#include <cassert>
#include <string>
using namespace assist;
int main(){static featuretypes::Config c;assert(featuretypes::valid(c));
 for(int i=0;i<featuretypes::Count;++i){assert(featuretypes::key(i)[0]&&featuretypes::name(i)[0]);assert(featuretypes::allowed(c,i,"ANY"));
  auto&f=c.filters[i];assert(oreconfig::parse(L"unit_a,Unit-B；unit_a",f.ids,f.count));assert(f.count==2);f.custom=1;
  assert(featuretypes::allowed(c,i,"UNIT_A")&&featuretypes::allowed(c,i,"unit-b")&&!featuretypes::allowed(c,i,"OTHER"));
  for(int j=i+1;j<featuretypes::Count;++j)assert(featuretypes::allowed(c,j,"OTHER"));
  auto old=f;assert(!oreconfig::parse(L"bad/id",f.ids,f.count));assert(!memcmp(&old,&f,sizeof f));
  assert(oreconfig::parse(L"",f.ids,f.count));assert(!featuretypes::allowed(c,i,"UNIT_A"));
 }
 assert(featuretypes::valid(c));c.filters[0].count=513;assert(!featuretypes::valid(c));c.filters[0].count=1;
 memset(c.filters[0].ids[0],'X',24);assert(!featuretypes::valid(c));strcpy(c.filters[0].ids[0],"BAD/ID");assert(!featuretypes::valid(c));
 strcpy(c.filters[0].ids[0],"GOOD_ID");assert(featuretypes::valid(c));c.filters[0].custom=2;assert(!featuretypes::valid(c));
 assert(!featuretypes::allowed(c,-1,"X")&&!featuretypes::allowed(c,featuretypes::Count,"X"));
}
