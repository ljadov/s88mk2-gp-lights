#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <set>

class MapEditor : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit MapEditor(MapProcessor &);
    ~MapEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;

    static juce::Colour displayColour(int paletteIndex);

  private:
    void timerCallback() override;
    void readZones();
    std::array<uint8_t, 128> zones{}; // zones MIDI In publiees par l'extension
    bool zonesKnown = false;
    void *zonesMapping = nullptr;
    const void *zonesView = nullptr;
    int noteAt(juce::Point<float>) const;
    juce::Rectangle<float> keyBounds(int note) const;
    int swatchAt(juce::Point<float>) const; // index de palette, ou -1
    juce::Rectangle<float> swatchBounds(int hue, int shade) const;
    void applyColor(int color);
    void selectRange(int a, int b);
    juce::String selectionText() const;

    MapProcessor &proc;
    std::set<int> selection;
    int anchor = -1;
    juce::ToggleButton replaceToggle;
    juce::TextButton transparentButton, offButton, clearButton, allButton;
};
