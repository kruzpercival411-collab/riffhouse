#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace rhui
{
namespace col
{
const juce::Colour bg { 0xff14110e }, panel { 0xff1f1b17 }, panel2 { 0xff2a241e }, edge { 0xff3a322a },
                   amber { 0xffe8a33d }, red { 0xffd2492a }, text { 0xffeee6d8 }, dim { 0xff8d8478 };
}

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
};

class Timeline : public juce::Component
{
public:
    std::function<void (int)> onSelect;
    void setSong (std::shared_ptr<const rh::Song> s) { song = std::move (s); repaint(); }
    void setPlayhead (double beat, bool playing) { if (beat != head || playing != isPlaying) { head = beat; isPlaying = playing; repaint(); } }
    void setSelected (int i) { selected = i; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int selected = 1;

private:
    std::shared_ptr<const rh::Song> song;
    double head = 0;
    bool isPlaying = false;
};

class DragTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    DragTile (juce::String t, juce::Colour c) : title (std::move (t)), accent (c) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    std::function<juce::File()> makeFile;
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; repaint(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::String title;
    juce::Colour accent;
    bool dragging = false;
};

struct Knob
{
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};
} // namespace rhui

class RiffhouseEditor : public juce::AudioProcessorEditor, private juce::Timer, private juce::ChangeListener
{
public:
    explicit RiffhouseEditor (RiffhouseProcessor&);
    ~RiffhouseEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshSong();
    void addCombo (juce::ComboBox&, juce::Label&, const juce::String& caption, const juce::String& paramId, const juce::StringArray& items,
                   std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&);
    void addKnob (rhui::Knob&, const juce::String& caption, const juce::String& paramId);
    juce::File midiFolder() const;

    RiffhouseProcessor& proc;
    rhui::Look look;

    juce::ComboBox style, key, mode, structure, voicing, midiIn;
    juce::Label styleL, keyL, modeL, structureL, voicingL, midiInL;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleA, keyA, modeA, structureA, voicingA, midiInA;
    rhui::Knob humanize, gDrive, gTone, gWidth, gVol, bDrive, bTone, bVol, dPunch, dRoom, dVol, master;

    juce::TextButton generateBtn { "GENERATE SONG" }, rerollBtn { "RE-ROLL SECTION" }, previewBtn { "PREVIEW" }, saveBtn { "SAVE MIDI..." };
    juce::ToggleButton autoPlay { "Auto-play" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoPlayA;

    rhui::Timeline timeline;
    juce::Label info, tempoHint;
    rhui::DragTile gTile { "GUITAR MIDI", rhui::col::amber }, bTile { "BASS MIDI", rhui::col::amber },
                   dTile { "DRUMS MIDI", rhui::col::amber }, songTile { "FULL SONG MIDI", rhui::col::red };

    juce::Rectangle<int> gPanel, bPanel, dPanel, sPanel;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::TooltipWindow> tips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiffhouseEditor)
};
