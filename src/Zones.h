#pragma once
// Lecture automatique des zones de clavier à partir des blocs MIDI In du rackspace.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct Zone
{
    std::string handle;  // handle GP du bloc
    std::string caption; // nom affiché du bloc
    int low = 0;
    int high = 127;
    uint8_t color = 0;
};

namespace zones
{
// Couleurs de la palette MK2 : hue * 4 + shade (shade 0..3).
uint8_t paletteColor(int hue, int shade);
// Couleur nommee trouvee dans le texte (« bleu », « bleu clair », « red light »...) : index de palette, ou -1.
int colorFromText(const std::string &text);
// Convertit « C3 », « F#-1 », « Db4 » ou « 60 » en note MIDI (C3 = 60, convention GP). -1 si illisible.
int parseNote(const std::string &text);
// Construit l'image du clavier : chaque zone dans sa couleur, les chevauchements (layers) en blanc.
std::array<uint8_t, 128> render(const std::vector<Zone> &zones, bool layersInWhite);
} // namespace zones
