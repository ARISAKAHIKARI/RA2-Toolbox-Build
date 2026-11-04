#include "../../external_reader/source/terrain_coverage.h"
#include <cassert>
#include <cstdio>
int main(){
 using external::terraincoverage::Region;
 auto coverage=Region::plan(1025,769,1920,1080,12000,8000);
 assert(coverage.ready&&coverage.left==768&&coverage.top==512&&coverage.right==3328&&coverage.bottom==2304);
 assert(coverage.contains(1025,769,1920,1080,12000,8000));
 assert(coverage.contains(800,600,1920,1080,12000,8000));
 assert(!coverage.contains(765,600,1920,1080,12000,8000)); // four native filter pixels remain covered
 assert(!coverage.contains(1025,769,1920,1080,12001,8000));
 assert(coverage.tile(300,900)); // an extra TMP image can reach into coverage
 assert(!coverage.rgb(300,900));
 assert(!coverage.tile(0,0)&&!coverage.tile(10000,7000));
 assert(coverage.rgb(740,600)&&!coverage.rgb(700,600));
 // Tile source position includes signed Level before admission. Its flat RGB
 // origin can be far away while raised pixels enter the actual viewport.
 assert(coverage.tile(1000,900)&&!coverage.rgb(1000,900+127*15));
 auto edge=Region::plan(-100,-50,640,480,1200,800);
 assert(edge.left==0&&edge.top==0&&edge.contains(-100,-50,640,480,1200,800));
 assert(edge.contains(-10000,-10000,100,100,1200,800)); // outside native backing is transparent
 auto empty=Region::plan(-10000,-10000,100,100,1200,800);
 assert(empty.ready&&empty.left==empty.right&&empty.top==empty.bottom);
 assert(empty.contains(-10000,-10000,100,100,1200,800)&&!empty.contains(0,0,100,100,1200,800));
 assert(!Region::plan(NAN,0,10,10,100,100).ready&&!Region::plan(0,0,10,10,0,100).ready);
 assert(!Region::plan(0,0,10,10,100,100,3).ready&&!Region::plan(0,0,10,10,100,100,3000).ready);
 assert(!Region{}.contains(0,0,10,10,100,100));
 // Large maps retain a bounded native neighborhood. Moving inside the bank
 // reuses it; crossing the covered boundary requests a new neighborhood.
 size_t admitted=0,total=0;
 for(int y=0;y<8000;y+=15)for(int x=0;x<12000;x+=30){++total;admitted+=coverage.tile(x,y);}
 assert(admitted<total/5&&admitted>0);
 std::printf("PASS: sparse native coverage, page alignment, filter halo, border transparency, signed-height/extra source reach and %zu/%zu bounded source candidates\n",admitted,total);
}
