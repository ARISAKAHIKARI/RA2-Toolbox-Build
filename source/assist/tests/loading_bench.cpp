#include "../source/autoload.h"
#include <memory>
#include <chrono>
#include <iostream>
using namespace assist;
int main(){static assist::loading::Workspace workspace;auto c=std::make_unique<loading::Composition>();for(int count:{64,128,512})for(int mode:{0,1}){loading::Infantry p[512]{};int out[512]{};double total=0;for(int run=0;run<7;++run){loading::Vehicle v[128]{};for(int i=0;i<count;++i)p[i]={uint32_t(0x10000+i*16),uint32_t(i+1),(i%32)*128,(i/32)*128,1,uint32_t(i%8+1),1};for(int j=0;j<128;++j)v[j]={uint32_t(0x30000+j*16),uint32_t(1000+j),(j%16)*256,(j/16)*256,6,0,0,3.,0};c->clear();auto start=std::chrono::steady_clock::now();int n=loading::assign(p,count,v,128,mode,30,*c,workspace,out,[](const auto&,const auto&){return true;});auto end=std::chrono::steady_clock::now();if(n!=count)return 1;if(run)total+=std::chrono::duration<double,std::micro>(end-start).count();}std::cout<<count<<" passengers,128 carriers,mode="<<mode<<",mean_us="<<total/6<<"\n";}}
