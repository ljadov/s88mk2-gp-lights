#pragma once
// Canal direct plugin "Kontrol Lights Map" -> extension, pour voir les couleurs en direct
// pendant l'edition. Memoire partagee Windows nommee ; inactive sur les autres systemes.
// Valeurs de couleur par note : 0 = transparent (la zone MIDI In reste visible),
// 1 = eteint force, 4..71 = index de palette MK2 (teinte * 4 + nuance).
#include <cstdint>

namespace livemap
{
constexpr uint32_t kMagic = 0x4B4C4D31; // "KLM1"
constexpr const char *kName = "Local\\KontrolLightsMapLive";
constexpr uint8_t kTransparent = 0;
constexpr uint8_t kForcedOff = 1;
constexpr int kMaxColor = 71;

struct Block
{
    uint32_t magic;
    volatile uint32_t seq; // impair = ecriture en cours
    uint8_t replaceZones;  // 1 = la carte remplace les zones (transparent = eteint)
    uint8_t colors[128];
};

// Sens inverse : l'extension publie l'image des zones MIDI In du rackspace actif,
// pour que l'editeur du plugin les affiche sous les touches transparentes.
constexpr uint32_t kZonesMagic = 0x4B4C5A31; // "KLZ1"
constexpr const char *kZonesName = "Local\\KontrolLightsZones";

struct ZonesBlock
{
    uint32_t magic;
    volatile uint32_t seq; // impair = ecriture en cours
    uint8_t colors[128];   // couleur de palette par note (0 = aucune zone)
};
} // namespace livemap
