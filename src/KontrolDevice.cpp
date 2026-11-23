#include "KontrolDevice.h"
#include "LiveMap.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <chrono>
#include <hidapi.h>
#include <algorithm>
#include <cstdio>
#include <vector>

namespace
{
constexpr unsigned short kVendorNI = 0x17CC;

struct Model
{
    unsigned short pid;
    const char *name;
    int keys;
    int lowestNote;
};

constexpr Model kModels[] = {
    {0x1630, "Komplete Kontrol S88 MK2", 88, 21},
    {0x1620, "Komplete Kontrol S61 MK2", 61, 36},
    {0x1610, "Komplete Kontrol S49 MK2", 49, 36},
};

constexpr auto kFrameInterval = std::chrono::milliseconds(33);
constexpr auto kRetryInterval = std::chrono::milliseconds(500);
constexpr auto kReassertInterval = std::chrono::milliseconds(250);
constexpr auto kInitInterval = std::chrono::milliseconds(1000);
} // namespace

KontrolDevice::KontrolDevice()
{
    hid_init();
}

KontrolDevice::~KontrolDevice()
{
    stop();
    hid_exit();
}

void KontrolDevice::start()
{
    if (fRunning.exchange(true))
        return;
    tryOpen(); // premier essai tout de suite, pour que OnOpen sache deja si le clavier est la
    fThread = std::thread([this] { run(); });
}

void KontrolDevice::stop()
{
    if (!fRunning.exchange(false))
        return;
    fCv.notify_all();
    if (fThread.joinable())
        fThread.join();
    closeDevice();
}

void KontrolDevice::setNote(int midiNote, uint8_t color)
{
    const int idx = midiNote - fLowestNote.load();
    if (idx < 0 || idx >= fKeyCount.load())
        return;
    std::lock_guard<std::mutex> lock(fMutex);
    if (fKeys[idx] != color)
    {
        fKeys[idx] = color;
        fDirty = true;
        fCv.notify_one();
    }
}

void KontrolDevice::setRange(int lowNote, int highNote, uint8_t color)
{
    if (lowNote > highNote)
        std::swap(lowNote, highNote);
    std::lock_guard<std::mutex> lock(fMutex);
    for (int n = lowNote; n <= highNote; ++n)
    {
        const int idx = n - fLowestNote.load();
        if (idx >= 0 && idx < fKeyCount.load())
            fKeys[idx] = color;
    }
    fDirty = true;
    fCv.notify_one();
}

void KontrolDevice::clear()
{
    std::lock_guard<std::mutex> lock(fMutex);
    fKeys.fill(0);
    fDirty = true;
    fCv.notify_one();
}

void KontrolDevice::setAllNotes(const std::array<uint8_t, 128> &colorsByNote)
{
    std::lock_guard<std::mutex> lock(fMutex);
    const int low = fLowestNote.load();
    for (int i = 0; i < fKeyCount.load(); ++i)
        fKeys[i] = (low + i < 128) ? colorsByNote[low + i] : 0;
    fDirty = true;
    fCv.notify_one();
}

std::string KontrolDevice::modelName() const
{
    std::lock_guard<std::mutex> lock(fMutex);
    return fModel;
}

std::string KontrolDevice::probeReport() const
{
    std::lock_guard<std::mutex> lock(fMutex);
    return fProbeReport;
}

namespace
{
std::string narrow(const wchar_t *w)
{
    std::string out;
    if (!w)
        return "(aucun message)";
    for (; *w; ++w)
        out += (*w < 128) ? static_cast<char>(*w) : '?';
    return out;
}

std::string hex4(unsigned v)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%04X", v & 0xFFFF);
    return buf;
}
} // namespace

bool KontrolDevice::tryOpen()
{
    // On enumere TOUTES les interfaces HID Native Instruments. Sous Windows, le clavier
    // expose plusieurs collections HID : hid_open() prend la premiere venue, qui n'est pas
    // forcement celle du light guide. On essaie donc chaque chemin, la page vendeur (0xFFxx) d'abord.
    struct Candidate
    {
        std::string path;
        const Model *model;
        std::string desc;
        bool vendorPage;
    };
    std::vector<Candidate> cands;
    std::string report;

    hid_device_info *list = hid_enumerate(kVendorNI, 0);
    for (hid_device_info *d = list; d; d = d->next)
    {
        const Model *model = nullptr;
        for (const auto &m : kModels)
            if (m.pid == d->product_id)
                model = &m;
        std::string desc = "PID " + hex4(d->product_id) + " interface " + std::to_string(d->interface_number) +
                           " usage " + hex4(d->usage_page) + ":" + hex4(d->usage) + " (" +
                           narrow(d->product_string) + ")";
        if (!model)
        {
            report += "  ignore : " + desc + "\n";
            continue;
        }
        cands.push_back({d->path, model, desc, d->usage_page >= 0xFF00});
    }
    hid_free_enumeration(list);

    if (cands.empty() && report.empty())
        report = "  aucun peripherique HID Native Instruments (VID 17CC) vu par Windows\n";

    std::stable_sort(cands.begin(), cands.end(), [](const Candidate &a, const Candidate &b) {
        return a.vendorPage && !b.vendorPage;
    });

    bool ok = false;
    for (const auto &c : cands)
    {
        hid_device *dev = hid_open_path(c.path.c_str());
        if (!dev)
        {
            report += "  echec ouverture : " + c.desc + " : " + narrow(hid_error(nullptr)) + "\n";
            continue;
        }
        const unsigned char init[] = {0xA0, 0x00, 0x00};
        if (hid_write(dev, init, sizeof(init)) < 0)
        {
            report += "  echec ecriture : " + c.desc + " : " + narrow(hid_error(dev)) + "\n";
            hid_close(dev);
            continue;
        }
        report += "  OK : " + c.desc + "\n";
        fDev = dev;
        fKeyCount = c.model->keys;
        fLowestNote = c.model->lowestNote;
        {
            std::lock_guard<std::mutex> lock(fMutex);
            fModel = c.model->name;
            fDirty = true; // renvoyer l'etat courant apres (re)connexion
        }
        fConnected = true;
        ok = true;
        break;
    }

    std::lock_guard<std::mutex> lock(fMutex);
    fProbeReport = report;
    return ok;
}

void KontrolDevice::closeDevice()
{
    if (fDev)
    {
        std::vector<unsigned char> off(1 + fKeyCount.load(), 0);
        off[0] = 0x81;
        hid_write(fDev, off.data(), off.size());
        hid_close(fDev);
        fDev = nullptr;
    }
    fConnected = false;
}

void KontrolDevice::run()
{
    while (fRunning)
    {
        if (!fDev && !tryOpen())
        {
            std::unique_lock<std::mutex> lock(fMutex);
            fCv.wait_for(lock, kRetryInterval, [this] { return !fRunning; });
            continue;
        }

        pollLiveMap();

        std::vector<unsigned char> frame;
        {
            std::unique_lock<std::mutex> lock(fMutex);
            // Reaffirmation : meme sans changement, on renvoie l'image regulierement, pour
            // reprendre la main si Komplete Kontrol (NIHIA) a ecrit sur le light guide entre-temps.
            fCv.wait_for(lock, kReassertInterval, [this] { return (fDirty && !fPaused) || !fRunning; });
            if (!fRunning)
                break;
            if (fPaused)
                continue; // on laisse la main a Komplete Kontrol
            frame.resize(1 + fKeyCount.load());
            frame[0] = 0x81;
            const int low = fLowestNote.load();
            for (int i = 0; i < fKeyCount.load(); ++i)
            {
                uint8_t c = fKeys[i];
                const int note = low + i;
                if (fOverlayActive && note < 128)
                {
                    const uint8_t o = fOverlay[note];
                    if (o == livemap::kForcedOff)
                        c = 0;
                    else if (o >= 4)
                        c = o;
                    else if (fOverlayReplace)
                        c = 0;
                }
                frame[1 + i] = c;
            }
            fDirty = false;
        }

        // Quand aucun hote Komplete Kontrol n'est actif, le service NI remet le clavier en
        // "mode MIDI" (touches bleues) et notre image est ignoree. On renvoie donc regulierement
        // le message d'entree en mode light guide (A0 00 00) juste avant l'image.
        const auto now = std::chrono::steady_clock::now();
        if (now - fLastInit >= kInitInterval)
        {
            const unsigned char init[] = {0xA0, 0x00, 0x00};
            hid_write(fDev, init, sizeof(init));
            fLastInit = now;
        }

        if (hid_write(fDev, frame.data(), frame.size()) < 0)
        {
            // Clavier debranche, ou interface reprise par le logiciel NI : on ferme et on retente vite.
            std::string err = narrow(hid_error(fDev));
            hid_close(fDev);
            fDev = nullptr;
            fConnected = false;
            std::lock_guard<std::mutex> lock(fMutex);
            ++fWriteErrors;
            fLastError = err;
            fDirty = true;
            continue;
        }
        std::this_thread::sleep_for(kFrameInterval); // limite le debit USB
    }
}

void KontrolDevice::setPaused(bool paused)
{
    std::lock_guard<std::mutex> lock(fMutex);
    if (fPaused != paused)
    {
        fPaused = paused;
        fDirty = true;
        fCv.notify_one();
    }
}

std::string KontrolDevice::errorReport() const
{
    std::lock_guard<std::mutex> lock(fMutex);
    if (fWriteErrors == 0)
        return "aucune erreur d'ecriture";
    return std::to_string(fWriteErrors) + " erreur(s) d'ecriture, derniere : " + fLastError;
}

void KontrolDevice::setOverlay(const std::array<uint8_t, 128> &colors, bool replace, bool active)
{
    std::lock_guard<std::mutex> lock(fMutex);
    fOverlay = colors;
    fOverlayReplace = replace;
    fOverlayActive = active;
    fDirty = true;
    fCv.notify_one();
}

#ifdef _WIN32
static const livemap::Block *liveBlock(void *&mapping, const void *&view)
{
    if (!view)
    {
        HANDLE h = OpenFileMappingA(FILE_MAP_READ, FALSE, livemap::kName);
        if (!h)
            return nullptr;
        view = MapViewOfFile(h, FILE_MAP_READ, 0, 0, sizeof(livemap::Block));
        if (!view)
        {
            CloseHandle(h);
            return nullptr;
        }
        mapping = h;
    }
    return static_cast<const livemap::Block *>(view);
}
#endif

void KontrolDevice::markLiveSeen()
{
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(fMutex);
    if (const auto *b = liveBlock(fLiveMapping, fLiveView))
        fLiveSeen = b->seq;
#endif
}

void KontrolDevice::pollLiveMap()
{
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(fMutex);
    const auto *b = liveBlock(fLiveMapping, fLiveView);
    if (!b || b->magic != livemap::kMagic)
        return;
    const uint32_t seq = b->seq;
    if ((seq & 1u) || seq == fLiveSeen)
        return;
    std::array<uint8_t, 128> colors;
    std::copy(b->colors, b->colors + 128, colors.begin());
    const bool replace = b->replaceZones != 0;
    const bool yield = b->yieldToKK != 0;
    if (b->seq != seq)
        return; // ecriture pendant la copie : on relira au prochain tour
    fLiveSeen = seq;
    fOverlay = colors;
    fOverlayReplace = replace;
    fOverlayActive = true;
    fPaused = yield; // case "Laisser les lumieres a Komplete Kontrol" cochee/decochee en direct
    fDirty = true;
#endif
}

void KontrolDevice::publishZones(const std::array<uint8_t, 128> &colorsByNote)
{
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(fMutex);
    if (!fZonesView)
    {
        HANDLE h = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(livemap::ZonesBlock),
                                      livemap::kZonesName);
        if (!h)
            return;
        fZonesView = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(livemap::ZonesBlock));
        if (!fZonesView)
        {
            CloseHandle(h);
            return;
        }
        fZonesMapping = h;
    }
    auto *b = static_cast<livemap::ZonesBlock *>(fZonesView);
    const uint32_t seq = b->seq | 1u;
    b->seq = seq;
    MemoryBarrier();
    b->magic = livemap::kZonesMagic;
    std::copy(colorsByNote.begin(), colorsByNote.end(), b->colors);
    MemoryBarrier();
    b->seq = seq + 1u;
#else
    (void)colorsByNote;
#endif
}
