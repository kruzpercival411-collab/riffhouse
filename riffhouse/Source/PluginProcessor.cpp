#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const char* kGenIds[] = { "style", "key", "mode", "structure", "voicing", "humanize" };
}

RiffhouseProcessor::RiffhouseProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RIFFHOUSE", createLayout())
{
    for (auto id : kGenIds) apvts.addParameterListener (id, this);
    pAuto = apvts.getRawParameterValue ("autoplay");
    pMidiIn = apvts.getRawParameterValue ("midiIn");
    pGDrive = apvts.getRawParameterValue ("gDrive");
    pGTone = apvts.getRawParameterValue ("gTone");
    pGWidth = apvts.getRawParameterValue ("gWidth");
    pGVol = apvts.getRawParameterValue ("gVol");
    pBDrive = apvts.getRawParameterValue ("bDrive");
    pBTone = apvts.getRawParameterValue ("bTone");
    pBVol = apvts.getRawParameterValue ("bVol");
    pDPunch = apvts.getRawParameterValue ("dPunch");
    pDRoom = apvts.getRawParameterValue ("dRoom");
    pDVol = apvts.getRawParameterValue ("dVol");
    pMaster = apvts.getRawParameterValue ("master");

    juce::Random r;
    for (auto& s : seeds) s = r.nextInt64() & 0x7fffffff;
    regenerate();
    events.reserve (8192);
}

RiffhouseProcessor::~RiffhouseProcessor()
{
    cancelPendingUpdate();
    for (auto id : kGenIds) apvts.removeParameterListener (id, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout RiffhouseProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto choice = [&] (const char* id, const char* name, StringArray items, int def)
    { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto knob = [&] (const char* id, const char* name, float def)
    { p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.f, 1.f), def)); };
    auto db = [&] (const char* id, const char* name, float lo, float hi, float def)
    { p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.1f), def,
                                                           AudioParameterFloatAttributes().withLabel ("dB"))); };

    choice ("style", "Style", rh::styleNames(), 0);
    choice ("key", "Key", rh::keyNames(), 4);
    choice ("mode", "Mode", rh::modeNames(), 0);
    choice ("structure", "Structure", rh::structureNames(), 0);
    choice ("voicing", "Voicing", rh::voicingNames(), 0);
    knob ("humanize", "Humanize", 0.35f);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "autoplay", 1 }, "Auto-Play Song", true));
    choice ("midiIn", "MIDI In Plays", { "By Channel", "Guitar", "Bass", "Drums" }, 0);

    knob ("gDrive", "Guitar Drive", 0.55f);
    knob ("gTone", "Guitar Tone", 0.5f);
    knob ("gWidth", "Guitar Width", 0.8f);
    db ("gVol", "Guitar Volume", -36.f, 6.f, -7.f);
    knob ("bDrive", "Bass Drive", 0.3f);
    knob ("bTone", "Bass Tone", 0.45f);
    db ("bVol", "Bass Volume", -36.f, 6.f, -5.f);
    knob ("dPunch", "Drum Punch", 0.5f);
    knob ("dRoom", "Drum Room", 0.3f);
    db ("dVol", "Drum Volume", -36.f, 6.f, -4.f);
    db ("master", "Master", -24.f, 6.f, 0.f);
    return { p.begin(), p.end() };
}

bool RiffhouseProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void RiffhouseProcessor::prepareToPlay (double sr, int block)
{
    guitar.prepare (sr);
    bass.prepare (sr);
    drums.prepare (sr);
    guitarAmp.prepare (sr, block);
    bassAmp.prepare (sr, block);
    drumBus.prepare (sr, block);
    gBuf.setSize (2, block); bBuf.setSize (1, block); dBuf.setSize (2, block);
    wasPlaying = false;
    expectedBeat = -1;
}

// ------------------------------------------------------------------ song management
rh::GenSettings RiffhouseProcessor::settings() const
{
    rh::GenSettings s;
    auto idx = [&] (const char* id) { return (int) apvts.getRawParameterValue (id)->load(); };
    s.style = idx ("style");
    s.key = idx ("key");
    s.mode = idx ("mode");
    s.structure = idx ("structure");
    s.voicing = idx ("voicing");
    s.humanize = apvts.getRawParameterValue ("humanize")->load();
    s.seeds = seeds;
    return s;
}

void RiffhouseProcessor::regenerate()
{
    auto fresh = std::make_shared<const rh::Song> (rh::generateSong (settings()));
    std::shared_ptr<const rh::Song> old;
    {
        const juce::SpinLock::ScopedLockType l (songLock);
        old = song;
        song = fresh;
    }
    // keep old songs alive until the audio thread has let go (never free on the audio thread)
    retired.erase (std::remove_if (retired.begin(), retired.end(), [] (auto& s) { return s.use_count() == 1; }), retired.end());
    if (old) retired.push_back (std::move (old));
    sendChangeMessage();
}

std::shared_ptr<const rh::Song> RiffhouseProcessor::getSong()
{
    const juce::SpinLock::ScopedLockType l (songLock);
    return song;
}

void RiffhouseProcessor::generateNew()
{
    juce::Random r;
    for (auto& s : seeds) s = r.nextInt64() & 0x7fffffff;
    regenerate();
}

void RiffhouseProcessor::rerollSection (int t)
{
    if (t < 0 || t >= rh::NumSecTypes) return;
    seeds[(size_t) t] = juce::Random().nextInt64() & 0x7fffffff;
    regenerate();
}

void RiffhouseProcessor::parameterChanged (const juce::String&, float) { triggerAsyncUpdate(); }
void RiffhouseProcessor::handleAsyncUpdate() { regenerate(); }

juce::String RiffhouseProcessor::describeSong()
{
    auto s = getSong();
    if (! s) return {};
    return rh::keyNames()[s->keyPc] + " " + rh::modeNames()[s->modeIndex] + " " + rh::styleNames()[s->styleIndex];
}

juce::File RiffhouseProcessor::writeMidiFile (int mask, const juce::File& folder)
{
    auto s = getSong();
    if (! s) return {};
    folder.createDirectory();
    juce::String part = mask == 1 ? "Guitar" : mask == 2 ? "Bass" : mask == 4 ? "Drums" : "Full Song";
    auto name = juce::File::createLegalFileName ("Riffhouse " + part + " - " + describeSong().replaceCharacter ('/', '-').replace ("#", " sharp") + ".mid");
    auto f = folder.getChildFile (name);
    f.deleteFile();
    auto mf = rh::toMidiFile (*s, mask, lastBpm.load());
    juce::FileOutputStream out (f);
    if (! out.openedOk()) return {};
    mf.writeTo (out, 1);
    out.flush();
    return f;
}

// ------------------------------------------------------------------ audio
void RiffhouseProcessor::dispatch (const Ev& e)
{
    switch (e.part)
    {
        case rh::Part::Guitar: if (e.on) guitar.noteOn (e.note, e.vel, e.art); else guitar.noteOff (e.note); break;
        case rh::Part::Bass:   if (e.on) bass.noteOn (e.note, e.vel); else bass.noteOff (e.note); break;
        case rh::Part::Drums:  if (e.on) drums.hit (e.note, e.vel); break;
    }
}

void RiffhouseProcessor::allNotesOff()
{
    guitar.allOff();
    bass.allOff();
    drums.allOff();
}

void RiffhouseProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n <= 0) return;
    if (gBuf.getNumSamples() < n)
    {
        gBuf.setSize (2, n, false, false, true); bBuf.setSize (1, n, false, false, true); dBuf.setSize (2, n, false, false, true);
    }
    gBuf.clear(); bBuf.clear(); dBuf.clear();

    {
        const juce::SpinLock::ScopedTryLockType l (songLock);
        if (l.isLocked() && audioSong != song) audioSong = song;
    }
    const rh::Song* s = audioSong.get();

    // --- transport
    bool hostPlaying = false;
    double ppq = 0, bpm = lastBpm.load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 400.0, *b);
            hostPlaying = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppq = *p;
        }
    lastBpm = bpm;

    const double spb = getSampleRate() * 60.0 / bpm;     // samples per beat
    const double blockBeats = n / spb;
    const bool autoPlay = pAuto->load() > 0.5f;
    if (! preview.load() && ! hostPlaying) previewBeat = 0;
    const bool playing = autoPlay && s != nullptr && s->totalBeats > 0 && (hostPlaying || preview.load());
    const double startBeat = hostPlaying ? ppq : previewBeat;

    events.clear();
    int seq = 0;

    if (playing)
    {
        if (! wasPlaying || std::abs (startBeat - expectedBeat) > 0.05) allNotesOff();
        const double L = s->totalBeats;
        double s0 = std::fmod (startBeat, L);
        if (s0 < 0) s0 += L;
        auto collect = [&] (double from, double to, double sampleBase)
        {
            auto it = std::lower_bound (s->trigs.begin(), s->trigs.end(), from, [] (const rh::Trig& t, double b) { return t.beat < b; });
            for (; it != s->trigs.end() && it->beat < to; ++it)
            {
                if (events.size() >= events.capacity()) break;
                const int smp = juce::jlimit (0, n - 1, (int) (sampleBase + (it->beat - from) * spb));
                events.push_back ({ smp, seq++, it->on, it->note, it->vel, it->part, it->art });
            }
        };
        const double e0 = s0 + blockBeats;
        if (e0 <= L) collect (s0, e0, 0);
        else { collect (s0, L, 0); collect (0, e0 - L, (L - s0) * spb); }

        uiBeat = s0;
        if (! hostPlaying) previewBeat += blockBeats;
    }
    else if (wasPlaying)
    {
        allNotesOff();
    }
    wasPlaying = playing;
    expectedBeat = startBeat + blockBeats;
    uiPlaying = playing;

    // --- incoming MIDI plays the engines too
    const int target = (int) pMidiIn->load();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! (m.isNoteOn() || m.isNoteOff())) { if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff(); continue; }
        rh::Part part = rh::Part::Guitar;
        if (target == 0) part = m.getChannel() == 10 ? rh::Part::Drums : (m.getChannel() == 2 ? rh::Part::Bass : rh::Part::Guitar);
        else part = (rh::Part) (target - 1);
        if (events.size() < events.capacity())
            events.push_back ({ juce::jlimit (0, n - 1, meta.samplePosition), seq++, m.isNoteOn(), m.getNoteNumber(),
                                m.getFloatVelocity(), part, rh::ArtNormal });
    }
    std::sort (events.begin(), events.end(), [] (const Ev& a, const Ev& b) { return a.sample != b.sample ? a.sample < b.sample : a.seq < b.seq; });

    drums.setPunch (pDPunch->load());

    // --- render voices
    auto* gl = gBuf.getWritePointer (0); auto* gr = gBuf.getWritePointer (1);
    auto* bb = bBuf.getWritePointer (0);
    auto* dl = dBuf.getWritePointer (0); auto* dr = dBuf.getWritePointer (1);
    size_t ei = 0;
    for (int i = 0; i < n; ++i)
    {
        while (ei < events.size() && events[ei].sample <= i) dispatch (events[ei++]);
        guitar.render (gl[i], gr[i]);
        bb[i] = bass.render();
        drums.render (dl[i], dr[i]);
    }

    // --- buses
    guitarAmp.process (gBuf, n, pGDrive->load(), pGTone->load(), pGWidth->load());
    bassAmp.process (bb, n, pBDrive->load(), pBTone->load());
    drumBus.process (dBuf, n, pDPunch->load(), pDRoom->load());

    const float gv = juce::Decibels::decibelsToGain (pGVol->load(), -35.9f) * 0.9f;
    const float bv = juce::Decibels::decibelsToGain (pBVol->load(), -35.9f) * 0.8f;
    const float dv = juce::Decibels::decibelsToGain (pDVol->load(), -35.9f) * 0.7f;
    const float mv = juce::Decibels::decibelsToGain (pMaster->load());

    const int outCh = buffer.getNumChannels();
    auto* oL = buffer.getWritePointer (0);
    auto* oR = outCh > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        float l = (gl[i] * gv + bb[i] * bv + dl[i] * dv) * mv;
        float r = (gr[i] * gv + bb[i] * bv + dr[i] * dv) * mv;
        l = std::tanh (l * 0.9f) / 0.9f * 0.95f;   // gentle safety clip
        r = std::tanh (r * 0.9f) / 0.9f * 0.95f;
        if (oR) { oL[i] = l; oR[i] = r; }
        else oL[i] = 0.5f * (l + r);
    }
}

// ------------------------------------------------------------------ state
void RiffhouseProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::StringArray sa;
    for (auto s : seeds) sa.add (juce::String (s));
    state.setProperty ("seeds", sa.joinIntoString (","), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void RiffhouseProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto vt = juce::ValueTree::fromXml (*xml);
        if (! vt.isValid() || ! vt.hasType (apvts.state.getType())) return;
        auto sa = juce::StringArray::fromTokens (vt.getProperty ("seeds").toString(), ",", "");
        for (int i = 0; i < sa.size() && i < (int) seeds.size(); ++i) seeds[(size_t) i] = sa[i].getLargeIntValue();
        apvts.replaceState (vt);
        if (juce::MessageManager::existsAndIsCurrentThread()) { cancelPendingUpdate(); regenerate(); }
        else triggerAsyncUpdate();
    }
}

juce::AudioProcessorEditor* RiffhouseProcessor::createEditor() { return new RiffhouseEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RiffhouseProcessor(); }
