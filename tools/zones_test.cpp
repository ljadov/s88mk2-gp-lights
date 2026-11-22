#include "Zones.h"
#include "Palette.h"
#include <string>
#include <cassert>
#include <cstdio>
int main(){
 using namespace zones;
 assert(parseNote("C3")==60); assert(parseNote("A-1")==21); assert(parseNote("C#3")==61); assert(parseNote("Db3")==61);
 assert(parseNote("G8")==127); assert(parseNote("C-2")==0); assert(parseNote("64")==64); assert(parseNote("xx")==-1);
 assert(colorFromText("Basse bleu")==46); assert(colorFromText("MIDI In (Komplete Kontrol - 1)")==-1);
 assert(colorFromText("Pad bleu clair")==47); assert(colorFromText("Lead Rouge sombre")==4); assert(colorFromText("pad green light")==31);
 assert(colorFromText("Corde Doré")==18); assert(colorFromText("Piano blanc 1")==69); assert(colorFromText("Bip jaune")==22);
 assert(colorFromText("Synth or Pad")==-1); assert(colorFromText("Vertige")==-1); assert(colorFromText("Rose pâle")==67);
 assert(palette::nameFr(47)=="bleu clair"); assert(palette::nameFr(46)=="bleu");
 std::vector<Zone> z={{"a","",21,59,paletteColor(11,2)},{"b","",55,108,paletteColor(7,2)}};
 auto r=render(z,true); assert(r[21]==46); assert(r[56]==paletteColor(17,2)); assert(r[100]==30); assert(r[20]==0);
 puts("tests OK");}
