#include "LibMain.h"

#include <algorithm>
#include <cctype>
#include <cmath>

using namespace gigperformer::sdk;

namespace
{
LibMain *gLib = nullptr; // les fonctions GPScript sont des fonctions C libres

uint8_t toColor(int c)
{
    return static_cast<uint8_t>(std::clamp(c, 0, 127));
}

// --- Fonctions injectees dans GPScript -------------------------------------
// Les arguments sont depiles dans l'ordre inverse de leur declaration.

void KK_SetKey(GPRuntimeEngine *vm)
{
    const int color = GP_VM_PopInteger(vm);
    const int note = GP_VM_PopInteger(vm);
    gLib->device.setNote(note, toColor(color));
}

void KK_SetRange(GPRuntimeEngine *vm)
{
    const int color = GP_VM_PopInteger(vm);
    const int high = GP_VM_PopInteger(vm);
    const int low = GP_VM_PopInteger(vm);
    gLib->device.setRange(low, high, toColor(color));
}

void KK_Clear(GPRuntimeEngine *)
{
    gLib->device.clear();
}

void KK_Color(GPRuntimeEngine *vm)
{
    // hue 1..16 (1 = rouge ... 11 = bleu ...), 17 = blanc ; shade 0..3 (0 = sombre, 3 = clair)
    const int shade = std::clamp(GP_VM_PopInteger(vm), 0, 3);
    const int hue = std::clamp(GP_VM_PopInteger(vm), 0, 17);
    GP_VM_PushInteger(vm, hue == 0 ? 0 : hue * 4 + shade);
}

void KK_IsConnected(GPRuntimeEngine *vm)
{
    GP_VM_PushBoolean(vm, gLib->device.isConnected());
}

void KK_SetAutoClear(GPRuntimeEngine *vm)
{
    gLib->autoClearOnRackspace = GP_VM_PopBoolean(vm);
}

void KK_SetAutoZones(GPRuntimeEngine *vm)
{
    gLib->autoZones = GP_VM_PopBoolean(vm);
    if (gLib->autoZones)
        gLib->refreshZones();
}

void KK_RefreshZones(GPRuntimeEngine *)
{
    gLib->refreshZones();
}

void KK_SetZoneColor(GPRuntimeEngine *vm)
{
    const int color = GP_VM_PopInteger(vm);
    const int len = GP_VM_TopStringLength(vm);
    std::string handle(static_cast<size_t>(len) + 1, '\0');
    GP_VM_PopString(vm, handle.data(), len + 1);
    handle.resize(static_cast<size_t>(len));
    auto &ov = gLib->colorOverrides;
    ov.erase(std::remove_if(ov.begin(), ov.end(), [&](auto &p) { return p.first == handle; }), ov.end());
    ov.emplace_back(handle, toColor(color));
    gLib->refreshZones();
}

void KK_SetYield(GPRuntimeEngine *vm)
{
    gLib->device.setPaused(GP_VM_PopBoolean(vm));
}

ExternalAPI_GPScriptFunctionDefinition kFunctions[] = {
    {"KK_SetKey", "note : integer, color : integer", "",
     "Allume une touche du light guide (color = index de palette MK2, 0 = eteint)", KK_SetKey},
    {"KK_SetRange", "lowNote : integer, highNote : integer, color : integer", "",
     "Allume une plage de touches (bornes incluses)", KK_SetRange},
    {"KK_Clear", "", "", "Eteint tout le light guide", KK_Clear},
    {"KK_Color", "hue : integer, shade : integer", "Returns integer",
     "Construit un index de palette : hue 1..16 (17 = blanc), shade 0..3", KK_Color},
    {"KK_IsConnected", "", "Returns boolean", "Vrai si le clavier est detecte", KK_IsConnected},
    {"KK_SetAutoClear", "enabled : boolean", "",
     "Eteint automatiquement le light guide a chaque changement de rackspace", KK_SetAutoClear},
    {"KK_SetAutoZones", "enabled : boolean", "",
     "Active/desactive l'affichage automatique des zones des blocs MIDI In (actif par defaut)", KK_SetAutoZones},
    {"KK_RefreshZones", "", "", "Relit les blocs MIDI In et reaffiche les zones", KK_RefreshZones},
    {"KK_SetZoneColor", "blockHandle : string, color : integer", "",
     "Force la couleur de la zone d'un bloc MIDI In (par son handle)", KK_SetZoneColor},
    {"KK_SetYield", "yield : boolean", "",
     "true = laisse le light guide a Komplete Kontrol dans ce rackspace (remis a false a chaque rackspace)", KK_SetYield},
};
} // namespace

GigPerformerAPI *gigperformer::sdk::CreateGPExtension(LibraryHandle handle)
{
    return new LibMain(handle);
}

LibMain::~LibMain()
{
    device.stop();
    if (gLib == this)
        gLib = nullptr;
}

std::string LibMain::GetProductDescription()
{
    return "<Library>"
           "<Product Name=\"Kontrol Lights\" Version=\"0.1.0\" BuildDate=\"" __DATE__ "\"></Product>"
           "<Description>Controle le light guide des claviers Komplete Kontrol S-series MK2 depuis GPScript"
           "</Description>"
           "</Library>";
}

void LibMain::Initialization()
{
    gLib = this;
    registerCallback("OnOpen");
    registerCallback("OnClose");
    registerCallback("OnRackspaceActivated");
    registerCallback("OnVariationChanged");
    registerCallback("OnSongPartChanged");
    device.start();
}

int LibMain::RequestGPScriptFunctionSignatureList(GPScript_AllowedLocations, ExternalAPI_GPScriptFunctionDefinition **list)
{
    *list = kFunctions;
    return static_cast<int>(sizeof(kFunctions) / sizeof(kFunctions[0]));
}

void LibMain::OnOpen()
{
    consoleLog(device.isConnected()
                   ? "Kontrol Lights : " + device.modelName() + " connecte"
                   : std::string("Kontrol Lights : clavier MK2 non trouve. Nouvel essai toutes les 2 s ; "
                                 "menu Extensions > diagnostic pour le detail."));
}

void LibMain::OnClose()
{
    device.stop();
}

void LibMain::OnRackspaceActivated()
{
    device.setPaused(false); // KK_SetYield ne vaut que pour le rackspace qui l'appelle
    colorOverrides.clear(); // les couleurs forcees sont propres a chaque rackspace
    if (autoZones)
        refreshZones();
    else if (autoClearOnRackspace)
        device.clear();
}

void LibMain::OnVariationChanged(int, int)
{
    if (autoZones)
        refreshZones();
}

void LibMain::OnSongPartChanged(int, int)
{
    if (autoZones)
        refreshZones();
}

namespace
{
std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool has(const std::string &s, const char *w)
{
    return s.find(w) != std::string::npos;
}

// Teintes attribuees automatiquement, dans l'ordre des blocs : bleu, vert, orange, violet, jaune, rouge, cyan, rose
const int kAutoHues[] = {11, 7, 2, 13, 5, 1, 9, 16};
} // namespace

std::vector<Zone> LibMain::scanZones(bool logDetails)
{
    std::vector<std::string> handles;
    getPluginList(handles, false);

    struct Candidate
    {
        Zone zone;
        bool preferred; // bloc du Komplete Kontrol ou OMNI
    };
    std::vector<Candidate> found;

    for (const auto &h : handles)
    {
        const std::string name = getPluginName(h, false);
        const std::string lname = lower(name);
        if (!has(lname, "midi in"))
            continue;

        Zone z;
        z.handle = h;
        z.caption = getPluginCaption(h, false);
        int lowIdx = -1, highIdx = -1;
        const int count = getPluginParameterCount(h, false);
        for (int i = 0; i < count; ++i)
        {
            const std::string pn = lower(getPluginParameterName(h, i, false));
            const bool isKey = has(pn, "note") || has(pn, "key");
            if (logDetails && isKey && (has(pn, "min") || has(pn, "low") || has(pn, "max") || has(pn, "high")) &&
                !has(pn, "velocity"))
                consoleLog("  [" + h + "] " + getPluginParameterName(h, i, false) + " = " +
                           getPluginParameterText(h, i, false));
            if (isKey && !has(pn, "velocity") && lowIdx < 0 && (has(pn, "min") || has(pn, "low")))
                lowIdx = i;
            if (isKey && !has(pn, "velocity") && highIdx < 0 && (has(pn, "max") || has(pn, "high")))
                highIdx = i;
        }
        auto readNote = [&](int idx, int fallback) {
            if (idx < 0)
                return fallback;
            int n = zones::parseNote(getPluginParameterText(h, idx, false));
            if (n < 0)
                n = static_cast<int>(std::lround(getPluginParameter(h, idx, false) * 127.0));
            return std::clamp(n, 0, 127);
        };
        z.low = readNote(lowIdx, 0);
        z.high = readNote(highIdx, 127);
        if (z.low > z.high)
            std::swap(z.low, z.high);

        if (logDetails)
            consoleLog("Kontrol Lights : bloc '" + z.caption + "' (" + name + ") zone " + std::to_string(z.low) + "-" +
                       std::to_string(z.high) +
                       (lowIdx < 0 || highIdx < 0 ? "  [parametres de plage non trouves]" : ""));

        found.push_back({z, has(lname, "kontrol") || has(lname, "omni")});
    }

    // Si des blocs du Komplete Kontrol (ou OMNI) existent, on ignore les autres controleurs.
    const bool anyPreferred = std::any_of(found.begin(), found.end(), [](auto &c) { return c.preferred; });
    std::vector<Zone> out;
    int autoIndex = 0;
    for (auto &c : found)
    {
        if (anyPreferred && !c.preferred)
            continue;
        Zone z = c.zone;
        auto ov = std::find_if(colorOverrides.begin(), colorOverrides.end(), [&](auto &p) { return p.first == z.handle; });
        const int namedHue = zones::hueFromText(z.caption);
        if (ov != colorOverrides.end())
            z.color = ov->second;
        else if (namedHue > 0)
            z.color = zones::paletteColor(namedHue, zoneShade);
        else
            z.color = zones::paletteColor(kAutoHues[autoIndex++ % 8], zoneShade);
        out.push_back(z);
    }
    return out;
}

bool LibMain::readNoteMap(std::array<uint8_t, 128> &colors, bool &replace, bool logDetails)
{
    std::vector<std::string> handles;
    getPluginList(handles, false);
    for (const auto &h : handles)
    {
        if (!has(lower(getPluginName(h, false)), "kontrol lights map"))
            continue;
        int painted = 0;
        for (int n = 0; n < 128; ++n)
        {
            int v = zones::parseNote(getPluginParameterText(h, n, false)); // texte = nombre entier
            if (v < 0 || v > 71)
                v = static_cast<int>(std::lround(getPluginParameter(h, n, false) * 71.0));
            colors[n] = static_cast<uint8_t>(std::clamp(v, 0, 71));
            if (colors[n] != 0)
                ++painted;
        }
        replace = getPluginParameter(h, 128, false) > 0.5;
        if (logDetails)
            consoleLog("Kontrol Lights : carte de notes [" + h + "] : " + std::to_string(painted) + " note(s) reglee(s)" +
                       (replace ? ", remplace les zones" : ", par-dessus les zones"));
        return true;
    }
    if (logDetails)
        consoleLog("Kontrol Lights : aucun plugin Kontrol Lights Map avec handle dans ce rackspace");
    return false;
}

void LibMain::refreshZones()
{
    const auto image = zones::render(scanZones(false), layersInWhite);
    device.setAllNotes(image);
    device.publishZones(image);
    std::array<uint8_t, 128> map{};
    bool replace = false;
    const bool found = readNoteMap(map, replace, false);
    device.setOverlay(map, replace, found);
    device.markLiveSeen();
}

int LibMain::GetMenuCount()
{
    return 4;
}

std::string LibMain::GetMenuName(int index)
{
    switch (index)
    {
    case 0: return "Kontrol Lights : test arc-en-ciel";
    case 1: return "Kontrol Lights : tout eteindre";
    case 2: return "Kontrol Lights : reafficher les zones";
    default: return "Kontrol Lights : diagnostic des blocs MIDI In (console)";
    }
}

void LibMain::InvokeMenu(int index)
{
    if (index == 0)
    {
        const int low = device.lowestNote();
        for (int i = 0; i < device.keyCount(); ++i)
            device.setNote(low + i, static_cast<uint8_t>(((i % 16) + 1) * 4 + 2));
    }
    else if (index == 1)
    {
        device.clear();
    }
    else if (index == 2)
    {
        refreshZones();
    }
    else
    {
        consoleLog(std::string("Kontrol Lights : clavier ") +
                   (device.isConnected() ? "connecte (" + device.modelName() + ")" : "NON connecte"));
        consoleLog("Kontrol Lights : interfaces HID vues a la derniere recherche :\n" + device.probeReport());
        consoleLog("Kontrol Lights : " + device.errorReport());
        consoleLog("Kontrol Lights : diagnostic du rackspace courant");
        {
            std::vector<std::string> all;
            getPluginList(all, false);
            std::string names;
            for (const auto &h : all)
                names += "  [" + h + "] " + getPluginName(h, false) + "\n";
            consoleLog("Kontrol Lights : blocs avec handle dans ce rackspace :\n" +
                       (names.empty() ? std::string("  (aucun : donner un handle aux blocs MIDI In)\n") : names));
        }
        const auto zs = scanZones(true);
        consoleLog("Kontrol Lights : " + std::to_string(zs.size()) + " zone(s) affichee(s)");
        const auto image = zones::render(zs, layersInWhite);
        device.setAllNotes(image);
        device.publishZones(image);
        std::array<uint8_t, 128> map{};
        bool replace = false;
        const bool found = readNoteMap(map, replace, true);
        device.setOverlay(map, replace, found);
        device.markLiveSeen();
    }
}
