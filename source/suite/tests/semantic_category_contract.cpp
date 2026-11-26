#include <cassert>
#include "../source/settings_categories.h"
int main(){
 using namespace suite::categories;
 const int sim[]={9494},autoFilter[]={9182,9183},repair[]={780,312},load[]={700,701},combat[]={458},tech[]={9097};
 assert(row(3,sim,1)==Rules); // simulation rate is distinct from rendering FPS
 assert(row(2,autoFilter,2)==Automatic); // native targeting stays by low-damage filter
 assert(row(2,repair,2)==Automatic);
 assert(row(2,load,2)==Entry); // AutoBoard is part of the entry and unload workflow
 assert(row(1,combat,1)==Control);
 assert(row(4,tech,1)==Rules); // keep native technology eligibility in rules
}
