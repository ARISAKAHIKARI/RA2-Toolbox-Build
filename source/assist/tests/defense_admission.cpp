// Production predicates, raw combat reader and complete admission function
// execute over the tactical fixture's synthetic memory. No game calls.
static unsigned checks=0;
static void require(bool okay,const char*why){++checks;if(!okay){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
static uint32_t makeType(const char*id){auto t=alloc();std::memcpy(memory.data()+t+0x24,id,std::strlen(id)+1);set<bool>(t+0x231,true);set<bool>(t+0xD3A,true);set<bool>(t+0xD38,true);set<int>(t+0x684,-1);return t;}
static uint32_t makeActor(int k,uint32_t t,uint32_t id,uint32_t owner,int x){auto p=alloc();set<uint32_t>(p,k==3?0x7E3EBC:0x7EB058);set<uint32_t>(p+(k==3?0x520:0x6C0),t);set<uint32_t>(p+0x10,id);set<uint32_t>(p+0x21C,owner);set<int>(p+0x6C,100);set<bool>(p+0x74,true);set<bool>(p+0x90,true);set<int>(p+0x9C,x);set<int>(p+0x2FC,10);worldObjects.add(p,id);return p;}
int main(){
 sh=new Shared{};sh->flags=AntiTheft;snap.match=match;snap.frame=100;set<int>(0xA8ED84,100);
 const uint32_t enemy=0x30000,country=0x40000;set<uint32_t>(enemy+0x34,country);set<int>(enemy+0x30,1);
 const auto towerType=makeType("NALASR"),engineerType=makeType("ENGINEER"),oldType=makeType("OLDENG");
 set<bool>(engineerType+0xEC3,true);set<bool>(oldType+0xEC3,true);
 const auto weapon=alloc(),projectile=alloc(),warhead=alloc();set<uint32_t>(towerType+0x898,weapon);set<int>(weapon+0xB4,4096);set<int>(weapon+0xB8,256);set<uint32_t>(weapon+0xA0,projectile);set<uint32_t>(weapon+0xAC,warhead);set<bool>(projectile+0x2A5,true);for(int i=0;i<11;++i)set<double>(warhead+0xA0+i*8,1.);
 const auto tower=makeActor(3,towerType,1,lastHouse,1000),nearby=makeActor(1,engineerType,2,enemy,2000),old=makeActor(1,oldType,3,enemy,3000);set<uint32_t>(tower+0x2B4,old);
 planner::Order order{};order.unit=1;order.target=2;order.mission=1;order.purpose=planner::DefenseOrder;
 require(!externalOrderValid(order,lastHouse),"retain effective old threat");
 set<int>(old+0x9C,10000);require(externalOrderValid(order,lastHouse),"out-of-range old threat must allow nearby replacement");
 set<int>(old+0x9C,1100);require(externalOrderValid(order,lastHouse),"old threat inside minimum range must allow replacement");
 set<int>(old+0x9C,3000);set<int>(old+0xA4,10000);require(externalOrderValid(order,lastHouse),"old threat outside height range");set<int>(old+0xA4,0);
 set<bool>(oldType+0x233,true);require(externalOrderValid(order,lastHouse),"protected old threat");set<bool>(oldType+0x233,false);
 set<uint32_t>(old+0x11C,tower);require(externalOrderValid(order,lastHouse),"transported old threat");set<uint32_t>(old+0x11C,0);
 set<int>(old+0x220,2);require(externalOrderValid(order,lastHouse),"cloaked old threat");set<int>(old+0x220,0);
 set<int>(old+0x6C,0);require(externalOrderValid(order,lastHouse),"dead old threat");set<int>(old+0x6C,100);
 set<bool>(old+0x90,false);require(externalOrderValid(order,lastHouse),"off-map old threat");set<bool>(old+0x90,true);
 set<bool>(old+0x134,true);require(externalOrderValid(order,lastHouse),"old airborne threat incompatible with ground weapon");set<bool>(old+0x134,false);
 set<uint32_t>(tower+0x2B4,0);require(externalOrderValid(order,lastHouse),"no old target");
 set<int>(nearby+0x9C,10000);require(!externalOrderValid(order,lastHouse),"new target still needs current weapon range");set<int>(nearby+0x9C,2000);
 set<bool>(engineerType+0x233,true);require(!externalOrderValid(order,lastHouse),"new protected target rejected");set<bool>(engineerType+0x233,false);
 set<int>(nearby+0x220,2);require(!externalOrderValid(order,lastHouse),"new cloaked target rejected");set<int>(nearby+0x220,0);
 set<uint32_t>(nearby+0x11C,tower);require(!externalOrderValid(order,lastHouse),"new transported target rejected");set<uint32_t>(nearby+0x11C,0);
 order.token=1;require(!externalOrderValid(order,lastHouse),"manual version still enforced");order.token=0;
 sh->flags=0;require(!externalOrderValid(order,lastHouse),"disabled feature still rejected");sh->flags=AntiTheft;
 require(!nativeCalls&&!submissions,"admission must not issue native or event calls");
 require((productionCombatDemand()&worldframe::Combat)!=0,"defense alone must request combat facts on its first capture");
 sh->flags=0;require((productionCombatDemand()&worldframe::Combat)==0,"inactive defense adds no combat demand");
 std::printf("PASS defense admission: %u checks; replacement and retained-shot guards; no native calls\n",checks);
}
