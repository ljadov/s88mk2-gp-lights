#pragma once
#include "KontrolDevice.h"
#include "Zones.h"
#include "gigperformer/sdk/GigPerformerAPI.h"

class LibMain : public gigperformer::sdk::GigPerformerAPI
{
  public:
    explicit LibMain(LibraryHandle handle) : GigPerformerAPI(handle) {}
    ~LibMain() override;

    void Initialization() override;
    std::string GetProductDescription() override;

    int RequestGPScriptFunctionSignatureList(GPScript_AllowedLocations location,
                                             ExternalAPI_GPScriptFunctionDefinition **list) override;

    void OnOpen() override;
    void OnClose() override;
    void OnRackspaceActivated() override;
    void OnVariationChanged(int oldIndex, int newIndex) override;
    void OnSongPartChanged(int oldIndex, int newIndex) override;

    // Lit les blocs MIDI In du rackspace courant et affiche leurs zones.
    void refreshZones();
    std::vector<Zone> scanZones(bool logDetails);
    // Lit le plugin "Kontrol Lights Map" du rackspace (par son handle). false s'il n'y en a pas.
    bool readNoteMap(std::array<uint8_t, 128> &colors, bool &replace, bool &yield, bool logDetails);

    int GetMenuCount() override;
    std::string GetMenuName(int index) override;
    void InvokeMenu(int index) override;

    KontrolDevice device;
    bool autoClearOnRackspace = false;
    bool autoZones = true;      // affichage automatique des zones MIDI In
    bool layersInWhite = true;  // touches couvertes par plusieurs zones en blanc
    int zoneShade = 2;          // luminosité 0..3
    std::vector<std::pair<std::string, uint8_t>> colorOverrides; // handle -> couleur (GPScript)
};
