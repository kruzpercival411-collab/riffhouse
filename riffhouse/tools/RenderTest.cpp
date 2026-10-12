// Offline test: renders songs through the real processor to WAV + prints the arrangement.
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

static void render (int style, int key, const juce::String& out)
{
    RiffhouseProcessor p;
    auto set = [&] (const char* id, float v) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
    set ("style", (float) style); set ("key", (float) key);
    p.generateNew();
    auto song = p.getSong();
    std::cout << "== " << rh::styleNames()[style] << " in " << p.describeSong() << " : " << song->totalBars << " bars, "
              << song->notes.size() << " notes\n";
    for (auto& s : song->sections)
    {
        juce::StringArray c; for (auto& ch : s.chords) c.add (ch.name + "(" + ch.roman + ")");
        std::cout << "  " << rh::sectionName (s.type) << " [" << s.bars << "] " << c.joinIntoString (" ") << "  | " << rh::guitarPatName (s.gPat) << "\n";
    }
    const double sr = 44100; const int bs = 512;
    p.setPlayConfigDetails (0, 2, sr, bs);
    p.prepareToPlay (sr, bs);
    p.preview = true;
    const double bpm = 120.0;
    const int total = (int) (std::min (song->totalBeats, 64.0) * 60.0 / bpm * sr);
    juce::AudioBuffer<float> all (2, total), blk (2, bs);
    juce::MidiBuffer m;
    float peak = 0; double rms = 0;
    for (int pos = 0; pos < total; pos += bs)
    {
        const int n = std::min (bs, total - pos);
        blk.setSize (2, n, false, false, true);
        p.processBlock (blk, m);
        for (int c = 0; c < 2; ++c) all.copyFrom (c, pos, blk, c, 0, n);
    }
    for (int c = 0; c < 2; ++c) { peak = std::max (peak, all.getMagnitude (c, 0, total)); rms += all.getRMSLevel (c, 0, total); }
    bool bad = false;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < total; ++i) if (! std::isfinite (all.getSample (c, i))) bad = true;
    std::cout << "  peak " << peak << "  rms " << rms / 2 << (bad ? "  NaN!" : "") << "\n";
    juce::File f (out); f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 16, {}, 0));
    w->writeFromAudioSampleBuffer (all, 0, total);
    // midi export check
    auto mid = p.writeMidiFile (7, f.getParentDirectory());
    std::cout << "  midi " << mid.getFileName() << " " << mid.getSize() << " bytes\n";
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String dir = argc > 1 ? argv[1] : ".";
    if (argc > 2)
    {
        RiffhouseProcessor p;
        p.generateNew();
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (dir + "/ui.png"); f.deleteFile();
        juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::cout << "ui " << img.getWidth() << "x" << img.getHeight() << "\n";
        return 0;
    }
    for (int s = 0; s < 10; ++s) render (s, s == 7 ? 4 : (s * 5) % 12, dir + "/style" + juce::String (s) + ".wav");
    return 0;
}
