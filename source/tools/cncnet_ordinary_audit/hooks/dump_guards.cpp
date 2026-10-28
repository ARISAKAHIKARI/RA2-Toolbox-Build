#include "source/shared/source/patch_context.h"
#include "source/assist/source/signatures.h"
#include <cstdio>
int main(){
 for(auto&p:Signatures)std::printf("startup\t0\t%u\t%u\n",p.address,p.length);
 for(auto&p:patchcontext::Probes)std::printf("patchcontext\t%u\t%u\t%u\n",p.site,p.address,p.size);
 for(auto&p:directpatch::Sites)std::printf("direct-context\t%u\t%u\t12\n",p.address,p.address+p.size);
}
