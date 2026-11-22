#include "PluginProcessor.h"
#include "PluginEditor.h"

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace
{
juce::String paramId(int note)
{
    return "n" + juce::String(note);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout MapProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // Index des parametres = numero de note MIDI (0..127), puis "replace" en 128.
    for (int n = 0; n < 128; ++n)
        layout.add(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{paramId(n), 1}, "Note " + juce::String(n) + " " + juce::MidiMessage::getMidiNoteName(n, true, true, 3),
            0, livemap::kMaxColor, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"replace", 1}, "Replace zones", false));
    return layout;
}

MapProcessor::MapProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "KontrolLightsMap", makeLayout())
{
#if JUCE_WINDOWS
    HANDLE h = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(livemap::Block), livemap::kName);
    if (h)
    {
        fMapping = h;
        fBlock = static_cast<livemap::Block *>(MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(livemap::Block)));
    }
#endif
}

MapProcessor::~MapProcessor()
{
#if JUCE_WINDOWS
    if (fBlock)
        UnmapViewOfFile(fBlock);
    if (fMapping)
        CloseHandle(static_cast<HANDLE>(fMapping));
#endif
}

int MapProcessor::getNoteColor(int note) const
{
    if (auto *p = state.getRawParameterValue(paramId(note)))
        return juce::roundToInt(p->load());
    return 0;
}

void MapProcessor::setNoteColor(int note, int color)
{
    if (auto *p = state.getParameter(paramId(note)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(color)));
        p->endChangeGesture();
    }
}

bool MapProcessor::getReplace() const
{
    return state.getRawParameterValue("replace")->load() > 0.5f;
}

void MapProcessor::setReplace(bool r)
{
    auto *p = state.getParameter("replace");
    p->beginChangeGesture();
    p->setValueNotifyingHost(r ? 1.0f : 0.0f);
    p->endChangeGesture();
}

void MapProcessor::publishLive()
{
    if (!fBlock)
        return;
    const uint32_t seq = fBlock->seq;
    fBlock->seq = (seq | 1u); // ecriture en cours
    std::atomic_thread_fence(std::memory_order_seq_cst);
    fBlock->magic = livemap::kMagic;
    fBlock->replaceZones = getReplace() ? 1 : 0;
    for (int n = 0; n < 128; ++n)
        fBlock->colors[n] = static_cast<uint8_t>(getNoteColor(n));
    std::atomic_thread_fence(std::memory_order_seq_cst);
    fBlock->seq = (seq | 1u) + 1u; // pair = stable, et different du precedent
}

void MapProcessor::getStateInformation(juce::MemoryBlock &dest)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void MapProcessor::setStateInformation(const void *data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor *MapProcessor::createEditor()
{
    return new MapEditor(*this);
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MapProcessor();
}
