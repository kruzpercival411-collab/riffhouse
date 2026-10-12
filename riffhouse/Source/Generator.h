#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

// =====================================================================
//  RIFFHOUSE song generator
//  Pure rule-based music theory: styles, progressions, guitar/bass/drum
//  parts. Deterministic from seeds, so a song can be saved and recalled.
// =====================================================================
namespace rh
{
enum class Part : int { Guitar = 0, Bass = 1, Drums = 2 };

enum Art : juce::uint8 { ArtNormal = 0, ArtPalmMute = 1, ArtLetRing = 2 };

enum Quality { Power, Maj, Min, Dim, Sus2, Sus4, Add9, Dom7, Min7 };

enum SecType { Intro = 0, Verse, Pre, Chorus, Bridge, Outro, NumSecTypes };

enum GuitarPat { G_Stabs, G_Chug8, G_Drive8, G_Strum, G_LetRing, G_Half, G_Arp, G_Gallop, G_Boogie, G_Breakdown, NumGuitarPats };
enum BassPat   { B_Root8, B_Kick, B_Whole, B_Walk, B_Pump, B_Half, NumBassPats };
enum DrumPat   { D_Rock8, D_Punk, D_Half, D_Four, D_Shuffle, D_Double, D_Sparse, D_Build, D_Ride, D_Breakdown, NumDrumPats };

struct Chord
{
    int semis = 0;          // offset from tonic
    int pc = 0;             // absolute pitch class 0..11 (C = 0)
    Quality q = Maj;
    juce::String roman, name;
};

struct NoteEv
{
    double beat = 0, len = 0.5;
    int note = 60;
    float vel = 0.8f;
    Part part = Part::Guitar;
    juce::uint8 art = ArtNormal;
};

struct Trig
{
    double beat;
    bool on;
    int note;
    float vel;
    Part part;
    juce::uint8 art;
};

struct Section
{
    SecType type = Verse;
    int startBar = 0, bars = 8;
    std::vector<Chord> chords;      // unique progression
    std::vector<int> barChord;      // chord index for every bar
    int gPat = 0, bPat = 0, dPat = 0;
    float energy = 1.0f;
};

struct Song
{
    std::vector<Section> sections;
    std::vector<NoteEv> notes;
    std::vector<Trig> trigs;        // sorted; offs before ons at equal beat
    int totalBars = 0;
    double totalBeats = 0;
    int keyPc = 0, modeIndex = 1, styleIndex = 0, suggestedBpm = 120;
};

struct GenSettings
{
    int style = 0, key = 4, mode = 0, structure = 0, voicing = 0;
    float humanize = 0.4f;
    std::array<juce::int64, NumSecTypes> seeds {};
};

// tables for UI
juce::StringArray styleNames();
juce::StringArray keyNames();
juce::StringArray modeNames();        // index 0 = "Auto"
juce::StringArray structureNames();
juce::StringArray voicingNames();
juce::String sectionName (SecType t);
juce::String guitarPatName (int p);
juce::String bassPatName (int p);
juce::String drumPatName (int p);
juce::String sectionTip (const Song& song, int sectionIndex);
int styleTempo (int style);

Song generateSong (const GenSettings& s);

// partMask bits: 1 guitar, 2 bass, 4 drums
juce::MidiFile toMidiFile (const Song& song, int partMask, double bpm);
} // namespace rh
