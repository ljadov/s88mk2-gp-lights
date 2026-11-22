#include "Zones.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace zones;
 assert(parseNote("C3")==60); assert(parseNote("A-1")==21); assert(parseNote("C#3")==61); assert(parseNote("Db3")==61);
 assert(parseNote("G8")==127); assert(parseNote("C-2")==0); assert(parseNote("64")==64); assert(parseNote("xx")==-1);
 assert(hueFromText("Basse bleu")==11); assert(hueFromText("MIDI In (Komplete Kontrol - 1)")==-1);
 std::vector<Zone> z={{"a","",21,59,paletteColor(11,2)},{"b","",55,108,paletteColor(7,2)}};
 auto r=render(z,true); assert(r[21]==46); assert(r[56]==paletteColor(17,2)); assert(r[100]==30); assert(r[20]==0);
 puts("tests OK");}
