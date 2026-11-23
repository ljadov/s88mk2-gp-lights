#pragma once
// Accès HID direct au light guide des Komplete Kontrol S-series MK2.
// Protocole (non officiel, documenté par SynthesiaKontrol / LightKontrol) :
//   VID 0x17CC, PID 0x1610 (S49), 0x1620 (S61), 0x1630 (S88)
//   init       : [0xA0, 0x00, 0x00]
//   light guide: [0x81, c0, c1, ... cN-1]   un octet de palette par touche
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

struct hid_device_;

class KontrolDevice
{
  public:
    static constexpr int kMaxKeys = 88;

    KontrolDevice();
    ~KontrolDevice();

    void start(); // lance le thread d'écriture (reconnexion auto)
    void stop();  // éteint les LEDs et ferme le périphérique

    // Toutes ces méthodes sont non bloquantes : elles modifient un tampon
    // que le thread d'écriture envoie au clavier (~30 fois/s max).
    void setNote(int midiNote, uint8_t color);
    void setRange(int lowNote, int highNote, uint8_t color);
    void clear();
    // Remplace tout le clavier d'un coup ; colorsByNote[n] = couleur de la note MIDI n.
    void setAllNotes(const std::array<uint8_t, 128> &colorsByNote);

    bool isConnected() const { return fConnected.load(); }
    std::string modelName() const;
    // Compte rendu de la derniere recherche du clavier (interfaces HID vues, erreurs).
    std::string probeReport() const;
    std::string errorReport() const;
    // En pause, l'extension n'ecrit plus rien : Komplete Kontrol garde le light guide.
    void setPaused(bool paused);

    // Couche "carte de notes" (plugin Kontrol Lights Map), posee par-dessus les zones.
    // Valeurs : 0 = transparent, 1 = eteint force, 4..71 = couleur. replace = la carte remplace les zones.
    void setOverlay(const std::array<uint8_t, 128> &colors, bool replace, bool active);
    // Ignore les donnees "en direct" deja presentes (appele au changement de rackspace).
    void markLiveSeen();
    // Publie l'image des zones pour l'editeur du plugin Kontrol Lights Map (Windows).
    void publishZones(const std::array<uint8_t, 128> &colorsByNote);
    int lowestNote() const { return fLowestNote.load(); }
    int keyCount() const { return fKeyCount.load(); }

  private:
    void run();
    bool tryOpen();
    void closeDevice();

    hid_device_ *fDev = nullptr;
    std::array<uint8_t, kMaxKeys> fKeys{};
    mutable std::mutex fMutex;
    std::condition_variable fCv;
    bool fDirty = true;
    std::atomic<bool> fRunning{false};
    std::atomic<bool> fConnected{false};
    std::atomic<int> fLowestNote{21}; // A0 sur un S88
    std::atomic<int> fKeyCount{88};
    std::string fModel;
    std::array<uint8_t, 128> fOverlay{};
    bool fOverlayReplace = false;
    bool fOverlayActive = false;
    uint32_t fLiveSeen = 0;
    void *fLiveMapping = nullptr;
    void *fZonesMapping = nullptr;
    void *fZonesView = nullptr;
    const void *fLiveView = nullptr;
    void pollLiveMap(); // lit le canal direct du plugin (Windows)
    bool fPaused = false;
    std::chrono::steady_clock::time_point fLastInit{};
    int fWriteErrors = 0;
    std::string fLastError;
    std::string fProbeReport = "(pas encore de recherche)";
    std::thread fThread;
};
