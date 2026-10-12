#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Generator.h"
#include "Engines.h"

class RiffhouseProcessor : public juce::AudioProcessor,
                           public juce::ChangeBroadcaster,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::AsyncUpdater
{
public:
    RiffhouseProcessor();
    ~RiffhouseProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Riffhouse Studio"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- message-thread API for the editor
    std::shared_ptr<const rh::Song> getSong();
    void generateNew();                 // new seeds for every section
    void rerollSection (int secType);   // new seed for one section type
    juce::File writeMidiFile (int partMask, const juce::File& folder);
    juce::String describeSong();

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<double> uiBeat { 0 };
    std::atomic<bool> uiPlaying { false };
    std::atomic<bool> preview { false };
    std::atomic<double> lastBpm { 120.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float) override;
    void handleAsyncUpdate() override;
    void regenerate();
    rh::GenSettings settings() const;

    struct Ev { int sample; int seq; bool on; int note; float vel; rh::Part part; juce::uint8 art; };
    void dispatch (const Ev& e);
    void allNotesOff();

    std::array<juce::int64, rh::NumSecTypes> seeds {};

    juce::SpinLock songLock;
    std::shared_ptr<const rh::Song> song, audioSong;
    std::vector<std::shared_ptr<const rh::Song>> retired;

    rh::GuitarEngine guitar;
    rh::BassEngine bass;
    rh::DrumKit drums;
    rh::GuitarAmp guitarAmp;
    rh::BassAmp bassAmp;
    rh::DrumBus drumBus;

    juce::AudioBuffer<float> gBuf, bBuf, dBuf;
    std::vector<Ev> events;

    double previewBeat = 0, expectedBeat = -1;
    bool wasPlaying = false;

    std::atomic<float>* pAuto = nullptr; std::atomic<float>* pMidiIn = nullptr;
    std::atomic<float>* pGDrive = nullptr; std::atomic<float>* pGTone = nullptr; std::atomic<float>* pGWidth = nullptr; std::atomic<float>* pGVol = nullptr;
    std::atomic<float>* pBDrive = nullptr; std::atomic<float>* pBTone = nullptr; std::atomic<float>* pBVol = nullptr;
    std::atomic<float>* pDPunch = nullptr; std::atomic<float>* pDRoom = nullptr; std::atomic<float>* pDVol = nullptr;
    std::atomic<float>* pMaster = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiffhouseProcessor)
};
