#include "../../external_reader/source/game_archive.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace external::assets;
static std::vector<uint8_t> hex(const std::string&s){std::vector<uint8_t>b;for(size_t i=0;i<s.size();i+=2)b.push_back(uint8_t(std::stoul(s.substr(i,2),nullptr,16)));return b;}
static std::vector<uint8_t> file(const std::string&s){std::ifstream f(s,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
 assert(argc==2);std::string root=argv[1];std::ifstream vectors(root+"/vectors.txt");assert(vectors);std::string mode,a,b,c;unsigned count=0;
 while(vectors>>mode>>a>>b){auto input=hex(a),expected=hex(b);
  if(mode=="K"){uint8_t out[56];assert(input.size()==80&&expected.size()==56);mixcrypto::key(input.data(),out);assert(std::equal(expected.begin(),expected.end(),out));}
  else{vectors>>c;auto result=hex(c);mixcrypto::Blowfish fish(input.data(),input.size());fish.decrypt(expected.data(),expected.size());assert(expected==result);}++count;
 }
 std::map<std::string,std::vector<uint8_t>> disk;
 for(auto n:{"EXPANDMD99.MIX","RA2MD.MIX","ECACHEMD02.MIX"})disk[n]=file(root+"/"+n);
 auto read=[&](const std::string&nm,uint64_t at,void*out,size_t size){auto i=disk.find(nm);if(i==disk.end()||at>i->second.size()||size>i->second.size()-at)return false;memcpy(out,i->second.data()+at,size);return true;};
 auto build=[&](Directory&d){std::vector<Slice> files;for(auto&[n,v]:disk)files.push_back({n,0,v.size()});d.build(files,{},read);};
 Directory d;build(d);std::vector<uint8_t> out;
 auto check=[&](const char*nm,const char*expected){assert(d.load(nm,out,read,0));assert(std::string(out.begin(),out.end())==expected);};
 check("SLOPE01.TEM","expanded terrain");check("ONLYBASE.TEM","base fallback");check("HTNK.VXL","expanded voxel");check("UNITTEM.PAL","base palette");check("MODUNIT.SHP","custom named child");
 assert(d.count()==8);
 // Truncated encryption blocks and corrupt extents must never read outside
 // their enclosing MIX. The fixture has no live process or decoded index.
 auto valid=disk["EXPANDMD99.MIX"];disk.clear();
 for(size_t size:{size_t(0),size_t(10),size_t(83),size_t(91),valid.size()-1}){disk["EXPANDMD99.MIX"]={valid.begin(),valid.begin()+size};build(d);assert(d.count()==0);}
 disk["EXPANDMD99.MIX"]=file(root+"/BAD-EXTENT.MIX");build(d);assert(d.count()==0);
 disk["EXPANDMD99.MIX"]=file(root+"/BAD-DATABASE.MIX");build(d);assert(d.count()==1);assert(!d.load("MODUNIT.SHP",out,read,0));
 std::cout<<"PASS "<<count<<" independent RSA/Blowfish vectors; encrypted nested MIX, same-name fallback, custom XCC child, bounds\n";
}
