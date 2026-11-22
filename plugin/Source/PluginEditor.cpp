#include "PluginEditor.h"
#include "Palette.h"

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace
{
constexpr int kW = 1000, kH = 380;
constexpr float kKbX = 16, kKbY = 64, kWhiteH = 150, kBlackH = 94;
const juce::Colour kBg(0xff1e1f24), kText(0xffe8e8ea), kQuiet(0xff9a9ca3), kSel(0xff4aa3ff);

bool isBlack(int n)
{
    const int pc = n % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

int whiteIndex(int n) // rang de la touche blanche a gauche (ou sous) la note
{
    int w = 0;
    for (int k = MapProcessor::kFirstKey; k < n; ++k)
        if (!isBlack(k))
            ++w;
    return w;
}

float whiteW()
{
    return (kW - 2 * kKbX) / 52.0f;
}

juce::String u8(const char *s)
{
    return juce::String::fromUTF8(s);
}

juce::String noteName(int n)
{
    return juce::MidiMessage::getMidiNoteName(n, true, true, 3); // convention GP : C3 = 60
}
} // namespace

juce::Colour MapEditor::displayColour(int idx)
{
    if (idx < 4)
        return juce::Colours::transparentBlack;
    const int hue = idx / 4, shade = idx % 4;
    const float bright[] = {0.35f, 0.55f, 0.8f, 1.0f};
    return juce::Colour(0xff000000 | palette::kHues[juce::jlimit(1, 17, hue) - 1].rgb).withMultipliedBrightness(bright[shade]);
}

MapEditor::MapEditor(MapProcessor &p) : AudioProcessorEditor(p), proc(p)
{
    replaceToggle.setButtonText(u8("Remplacer les zones MIDI In"));
    replaceToggle.setToggleState(proc.getReplace(), juce::dontSendNotification);
    replaceToggle.onClick = [this] {
        proc.setReplace(replaceToggle.getToggleState());
        proc.publishLive();
    };
    transparentButton.setButtonText("Transparent");
    transparentButton.setTooltip(u8("La zone du bloc MIDI In reste visible sur ces touches"));
    transparentButton.onClick = [this] { applyColor(livemap::kTransparent); };
    offButton.setButtonText(u8("Éteint"));
    offButton.onClick = [this] { applyColor(livemap::kForcedOff); };
    clearButton.setButtonText(u8("Tout effacer"));
    clearButton.onClick = [this] {
        for (int n = 0; n < 128; ++n)
            if (proc.getNoteColor(n) != 0)
                proc.setNoteColor(n, 0);
        proc.publishLive();
    };
    allButton.setButtonText(u8("Tout sélectionner"));
    allButton.onClick = [this] { selectRange(MapProcessor::kFirstKey, MapProcessor::kLastKey); repaint(); };
    for (auto *c : std::initializer_list<juce::Component *>{&replaceToggle, &transparentButton, &offButton, &clearButton, &allButton})
        addAndMakeVisible(c);
    setSize(kW, kH);
    startTimerHz(10);
    proc.publishLive(); // l'editeur s'ouvre : on montre cette carte sur le clavier
}

MapEditor::~MapEditor()
{
#if JUCE_WINDOWS
    if (zonesView)
        UnmapViewOfFile(zonesView);
    if (zonesMapping)
        CloseHandle(static_cast<HANDLE>(zonesMapping));
#endif
}

void MapEditor::readZones()
{
#if JUCE_WINDOWS
    if (!zonesView)
    {
        HANDLE h = OpenFileMappingA(FILE_MAP_READ, FALSE, livemap::kZonesName);
        if (!h)
            return;
        zonesView = MapViewOfFile(h, FILE_MAP_READ, 0, 0, sizeof(livemap::ZonesBlock));
        if (!zonesView)
        {
            CloseHandle(h);
            return;
        }
        zonesMapping = h;
    }
    const auto *b = static_cast<const livemap::ZonesBlock *>(zonesView);
    const uint32_t seq = b->seq;
    if (b->magic != livemap::kZonesMagic || (seq & 1u))
        return;
    std::array<uint8_t, 128> tmp;
    std::copy(b->colors, b->colors + 128, tmp.begin());
    if (b->seq == seq)
    {
        zones = tmp;
        zonesKnown = true;
    }
#endif
}

void MapEditor::timerCallback()
{
    readZones();
    repaint();
}

juce::Rectangle<float> MapEditor::keyBounds(int n) const
{
    const float ww = whiteW();
    if (!isBlack(n))
        return {kKbX + whiteIndex(n) * ww, kKbY, ww, kWhiteH};
    const float bw = ww * 0.62f;
    return {kKbX + whiteIndex(n) * ww - bw / 2, kKbY, bw, kBlackH};
}

int MapEditor::noteAt(juce::Point<float> pt) const
{
    for (int n = MapProcessor::kFirstKey; n <= MapProcessor::kLastKey; ++n)
        if (isBlack(n) && keyBounds(n).contains(pt))
            return n;
    for (int n = MapProcessor::kFirstKey; n <= MapProcessor::kLastKey; ++n)
        if (!isBlack(n) && keyBounds(n).contains(pt))
            return n;
    return -1;
}

juce::Rectangle<float> MapEditor::swatchBounds(int hue, int shade) const
{
    return {kKbX + (hue - 1) * 34.0f, 262.0f + (3 - shade) * 26.0f, 30.0f, 22.0f};
}

int MapEditor::swatchAt(juce::Point<float> pt) const
{
    for (int h = 1; h <= 17; ++h)
        for (int s = 0; s < 4; ++s)
            if (swatchBounds(h, s).contains(pt))
                return h * 4 + s;
    return -1;
}

void MapEditor::selectRange(int a, int b)
{
    selection.clear();
    for (int n = std::min(a, b); n <= std::max(a, b); ++n)
        selection.insert(n);
}

void MapEditor::applyColor(int color)
{
    for (int n : selection)
        proc.setNoteColor(n, color);
    proc.publishLive();
    repaint();
}

void MapEditor::mouseDown(const juce::MouseEvent &e)
{
    const auto pt = e.position;
    if (const int sw = swatchAt(pt); sw >= 0)
    {
        applyColor(sw);
        return;
    }
    const int n = noteAt(pt);
    if (n < 0)
        return;
    if (e.mods.isShiftDown() && anchor >= 0)
        selectRange(anchor, n);
    else if (e.mods.isCtrlDown())
    {
        if (!selection.erase(n))
            selection.insert(n);
        anchor = n;
    }
    else
    {
        anchor = n;
        selectRange(n, n);
    }
    repaint();
}

void MapEditor::mouseDrag(const juce::MouseEvent &e)
{
    if (anchor < 0 || e.mods.isCtrlDown())
        return;
    if (const int n = noteAt(e.position); n >= 0)
    {
        selectRange(anchor, n);
        repaint();
    }
}

void MapEditor::mouseMove(const juce::MouseEvent &e)
{
    juce::String t;
    if (const int sw = swatchAt(e.position); sw >= 0)
        t = u8("Couleur : ") + juce::String::fromUTF8(palette::labelFr(sw).c_str()) + u8("  (n° ") + juce::String(sw) + ")";
    else if (const int n = noteAt(e.position); n >= 0)
    {
        const int c = proc.getNoteColor(n);
        t = noteName(n) + " : " + (c == 0 ? juce::String("transparent") : c == 1 ? u8("éteint") : juce::String::fromUTF8(palette::labelFr(c).c_str()));
        if (c == 0 && !proc.getReplace() && zones[n] >= 4)
            t += u8(" (zone MIDI In : ") + juce::String::fromUTF8(palette::labelFr(zones[n]).c_str()) + ")";
    }
    if (t != hoverText)
    {
        hoverText = t;
        repaint();
    }
}

void MapEditor::mouseExit(const juce::MouseEvent &)
{
    hoverText.clear();
    repaint();
}

juce::String MapEditor::selectionText() const
{
    if (selection.empty())
        return u8("Cliquez une touche, glissez ou Maj+clic pour une plage, Ctrl+clic pour ajouter des notes.");
    if (selection.size() == 1)
        return u8("Sélection : ") + noteName(*selection.begin());
    const bool contiguous = (*selection.rbegin() - *selection.begin() + 1) == static_cast<int>(selection.size());
    return u8("Sélection : ") + (contiguous ? noteName(*selection.begin()) + u8(" à ") + noteName(*selection.rbegin())
                                            : juce::String("plusieurs notes")) +
           " (" + juce::String(static_cast<int>(selection.size())) + " notes)";
}

void MapEditor::paint(juce::Graphics &g)
{
    g.fillAll(kBg);
    g.setColour(kText);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("Kontrol Lights Map", 16, 12, 400, 24, juce::Justification::left);
    g.setFont(13.0f);
    g.setColour(kQuiet);
    g.drawText(selectionText(), 16, 36, kW - 360, 20, juce::Justification::left);
    g.drawText(zonesKnown ? u8("Hachuré = zone d'un bloc MIDI In (vue par l'extension)")
                          : u8("Zones MIDI In : extension non détectée"),
               kW - 356, 36, 340, 20, juce::Justification::right);

    auto drawKey = [&](int n) {
        const auto r = keyBounds(n);
        const int c = proc.getNoteColor(n);
        juce::Colour face = isBlack(n) ? juce::Colour(0xff2b2c31) : juce::Colour(0xffd9dade);
        const bool showZone = c == livemap::kTransparent && !proc.getReplace() && zones[n] >= 4;
        if (c >= 4)
            face = displayColour(c);
        else if (c == livemap::kForcedOff || (c == livemap::kTransparent && proc.getReplace()))
            face = juce::Colour(0xff000000);
        else if (showZone)
            face = displayColour(zones[n]);
        g.setColour(face);
        g.fillRoundedRectangle(r.reduced(1.0f), 3.0f);
        if (showZone)
        {
            // Hachures : couleur venant d'une zone MIDI In, pas de la carte
            g.saveState();
            g.reduceClipRegion(r.reduced(2.0f).toNearestInt());
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            for (float d = -r.getHeight(); d < r.getWidth(); d += 7.0f)
                g.drawLine(r.getX() + d, r.getBottom(), r.getX() + d + r.getHeight(), r.getY(), 1.2f);
            g.restoreState();
        }
        if (c == livemap::kForcedOff)
        {
            g.setColour(kQuiet);
            g.drawLine(r.getX() + 4, r.getBottom() - 8, r.getRight() - 4, r.getBottom() - 8, 1.0f);
        }
        g.setColour(selection.count(n) ? kSel : juce::Colour(0xff0e0f12));
        g.drawRoundedRectangle(r.reduced(1.0f), 3.0f, selection.count(n) ? 2.5f : 1.0f);
        if (n % 12 == 0)
        {
            g.setColour(juce::Colour(0xff55575e));
            g.setFont(10.0f);
            g.drawText(noteName(n), r.withTrimmedTop(r.getHeight() - 18), juce::Justification::centred);
        }
    };
    for (int n = MapProcessor::kFirstKey; n <= MapProcessor::kLastKey; ++n)
        if (!isBlack(n))
            drawKey(n);
    for (int n = MapProcessor::kFirstKey; n <= MapProcessor::kLastKey; ++n)
        if (isBlack(n))
            drawKey(n);

    g.setColour(kQuiet);
    g.setFont(13.0f);
    g.drawText(u8("Couleur à appliquer (lignes : clair, vif, moyen, sombre) :"), 16, 236, 380, 20,
               juce::Justification::left);
    g.setColour(kText);
    g.drawText(hoverText, 400, 236, kW - 416, 20, juce::Justification::left);
    for (int h = 1; h <= 17; ++h)
        for (int s = 0; s < 4; ++s)
        {
            g.setColour(displayColour(h * 4 + s));
            g.fillRoundedRectangle(swatchBounds(h, s), 3.0f);
        }
}

void MapEditor::resized()
{
    const int x = 16 + 17 * 34 + 24;
    transparentButton.setBounds(x, 262, 150, 28);
    offButton.setBounds(x + 160, 262, 150, 28);
    allButton.setBounds(x, 298, 150, 28);
    clearButton.setBounds(x + 160, 298, 150, 28);
    replaceToggle.setBounds(x, 334, 320, 24);
}
