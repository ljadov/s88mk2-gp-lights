#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LiveMap.h"

// Plugin utilitaire : ne traite pas le son. Il garde une couleur par note (128 parametres)
// que l'extension Kontrol Lights lit au changement de rackspace, et qu'il publie en direct
// pendant l'edition.
class MapProcessor : public juce::AudioProcessor
{
  public:
    static constexpr int kFirstKey = 21, kLastKey = 108; // S88 MK2

    MapProcessor();
    ~MapProcessor() override;

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &) const override { return true; }
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Kontrol Lights Map"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}

    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;

    int getNoteColor(int note) const;
    void setNoteColor(int note, int color); // geste d'edition complet, notifie l'hote
    bool getReplace() const;
    void setReplace(bool);
    void publishLive(); // envoie la carte a l'extension (edition en direct)

    juce::AudioProcessorValueTreeState state;

  private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void *fMapping = nullptr;
    livemap::Block *fBlock = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MapProcessor)
};
