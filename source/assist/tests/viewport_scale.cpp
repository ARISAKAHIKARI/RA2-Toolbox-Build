#include "../../shared/source/viewport_scale.h"
#include <cassert>
#include <cstdio>
int main(){
 viewport::Scale s;
 assert(s.set(2560,1440,3840,2160));assert(s.width(44)==66&&s.height(13)==20); // unit icon and label font, both 1.5x
 assert(s.left(200)==300&&s.top(400)==600);
 assert(s.set(1920,1080,3840,2160));assert(s.width(44)==88&&s.height(13)==26);
 assert(s.set(2560,1440,2560,1440));assert(s.width(44)==44&&s.height(13)==13);
 assert(s.set(1920,1080,1280,720));assert(s.width(60)==40&&s.height(48)==32);
 assert(!s.set(0,0,3840,2160));assert(!s.set(2560,1440,0,0));
 puts("PASS: shared game-pixel layout scaling of positions/font/icon/bounds at 1x, 1.5x, 2x, 2/3x; invalid view rejected");
}
