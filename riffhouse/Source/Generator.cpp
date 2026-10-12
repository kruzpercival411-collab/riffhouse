#include "Generator.h"
#include <algorithm>
#include <cmath>

namespace rh
{
namespace
{
// ------------------------------------------------------------------ tables
const int kScales[6][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },   // (auto -> resolved before use)
    { 0, 2, 4, 5, 7, 9, 11 },   // Major
    { 0, 2, 3, 5, 7, 8, 10 },   // Minor
    { 0, 2, 4, 5, 7, 9, 10 },   // Mixolydian
    { 0, 2, 3, 5, 7, 9, 10 },   // Dorian
    { 0, 1, 3, 5, 7, 8, 10 },   // Phrygian
};

const char* kNotes[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
const char* kSharps[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char* kFlats[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

// spell chords with flats in flat keys (Ab major -> Db, not C#)
bool useFlats (int keyPc, int mode)
{
    static const int toRelMajor[6] = { 0, 0, 3, 5, 10, 8 };
    const int maj = (keyPc + toRelMajor[mode]) % 12;
    return maj == 5 || maj == 10 || maj == 3 || maj == 8 || maj == 1 || maj == 6;
}

enum VoicingMode { V_Power = 0, V_Full, V_Mixed, V_Shimmer, V_Blues };

struct Borrow { int semis; Quality q; float w; };

struct StyleDef
{
    const char* name;
    int defMode, bpm, voicing;
    bool lowTune;
    float swing;
    float degW[7];
    std::vector<Borrow> borrows;
    int g[NumSecTypes], b[NumSecTypes], d[NumSecTypes];
};

//                         Intro      Verse      Pre        Chorus     Bridge       Outro
const std::vector<StyleDef>& styles()
{
    static const std::vector<StyleDef> s = {
        { "Classic Rock", 1, 126, V_Mixed, false, 0.0f, { 3, .5f, .3f, 3, 2.5f, 1.5f, 0 },
          { { 10, Maj, 2.f }, { 3, Maj, .6f }, { 8, Maj, .6f } },
          { G_Stabs, G_Chug8, G_Drive8, G_Strum, G_Half, G_LetRing },
          { B_Root8, B_Root8, B_Root8, B_Pump, B_Half, B_Whole },
          { D_Rock8, D_Rock8, D_Build, D_Ride, D_Half, D_Rock8 } },

        { "Punk", 1, 180, V_Power, false, 0.0f, { 3, .3f, .2f, 3, 3, 1.5f, 0 },
          { { 10, Maj, 1.f }, { 8, Maj, .5f } },
          { G_Drive8, G_Drive8, G_Drive8, G_Drive8, G_Half, G_Drive8 },
          { B_Root8, B_Root8, B_Root8, B_Root8, B_Half, B_Root8 },
          { D_Punk, D_Rock8, D_Build, D_Punk, D_Half, D_Punk } },

        { "Grunge", 1, 110, V_Power, false, 0.0f, { 3, .2f, .2f, 1.5f, .6f, .4f, 0 },
          { { 3, Maj, 2.5f }, { 8, Maj, 2.f }, { 10, Maj, 2.f }, { 1, Maj, .5f }, { 2, Maj, .8f } },
          { G_Half, G_Arp, G_Chug8, G_Drive8, G_Half, G_LetRing },
          { B_Whole, B_Kick, B_Root8, B_Root8, B_Half, B_Whole },
          { D_Sparse, D_Sparse, D_Build, D_Ride, D_Half, D_Rock8 } },

        { "Alt / Indie", 1, 132, V_Full, false, 0.0f, { 3, 1, 1, 2.5f, 2, 2.5f, 0 },
          { { 10, Maj, .8f } },
          { G_Arp, G_Arp, G_Strum, G_Strum, G_Half, G_LetRing },
          { B_Whole, B_Kick, B_Root8, B_Pump, B_Half, B_Whole },
          { D_Sparse, D_Rock8, D_Build, D_Four, D_Half, D_Rock8 } },

        { "Stadium Anthem", 1, 118, V_Mixed, false, 0.0f, { 3, .5f, .4f, 3, 2.5f, 2.5f, 0 },
          { { 10, Maj, .6f } },
          { G_LetRing, G_Chug8, G_Drive8, G_Strum, G_Half, G_LetRing },
          { B_Whole, B_Root8, B_Root8, B_Pump, B_Half, B_Whole },
          { D_Half, D_Rock8, D_Build, D_Ride, D_Half, D_Ride } },

        { "Blues Rock", 3, 112, V_Blues, false, 0.33f, { 3, .3f, .1f, 2.5f, .3f, .3f, 1.5f },
          { { 3, Maj, .8f }, { 7, Maj, 2.f } },
          { G_Stabs, G_Boogie, G_Boogie, G_Stabs, G_Half, G_LetRing },
          { B_Walk, B_Walk, B_Walk, B_Walk, B_Half, B_Whole },
          { D_Shuffle, D_Shuffle, D_Shuffle, D_Shuffle, D_Half, D_Shuffle } },

        { "Pop-Punk", 1, 170, V_Power, false, 0.0f, { 3, .4f, .2f, 3, 3, 3, 0 },
          { { 10, Maj, .4f } },
          { G_Drive8, G_Chug8, G_Stabs, G_Drive8, G_Half, G_Drive8 },
          { B_Root8, B_Root8, B_Root8, B_Root8, B_Half, B_Root8 },
          { D_Rock8, D_Rock8, D_Build, D_Punk, D_Half, D_Punk } },

        { "Metal", 2, 150, V_Power, true, 0.0f, { 3, 0, 1, .8f, .6f, 2, 2 },
          { { 1, Maj, 1.5f } },
          { G_Gallop, G_Gallop, G_Chug8, G_Half, G_Breakdown, G_LetRing },
          { B_Kick, B_Kick, B_Kick, B_Root8, B_Kick, B_Whole },
          { D_Double, D_Rock8, D_Build, D_Double, D_Breakdown, D_Half } },

        { "Stoner / Doom", 2, 72, V_Power, true, 0.0f, { 3, 0, 1, 1.5f, .5f, 1.5f, 2 },
          { { 1, Maj, .5f } },
          { G_LetRing, G_Half, G_Half, G_Half, G_Chug8, G_LetRing },
          { B_Whole, B_Half, B_Half, B_Half, B_Root8, B_Whole },
          { D_Sparse, D_Half, D_Half, D_Half, D_Rock8, D_Half } },

        { "Shoegaze", 1, 124, V_Shimmer, false, 0.0f, { 3, 1, 1, 2.5f, 1.5f, 2, 0 },
          { { 10, Maj, .5f } },
          { G_LetRing, G_Strum, G_LetRing, G_Strum, G_LetRing, G_LetRing },
          { B_Whole, B_Root8, B_Half, B_Root8, B_Whole, B_Whole },
          { D_Sparse, D_Rock8, D_Build, D_Ride, D_Half, D_Ride } },
    };
    return s;
}

const std::vector<std::vector<SecType>>& structures()
{
    static const std::vector<std::vector<SecType>> s = {
        { Intro, Verse, Pre, Chorus, Verse, Pre, Chorus, Bridge, Chorus, Outro },
        { Intro, Verse, Chorus, Verse, Chorus, Chorus, Outro },
        { Intro, Verse, Verse, Pre, Chorus, Bridge, Chorus, Chorus, Outro },
        { Intro, Verse, Chorus, Verse, Chorus, Bridge, Chorus, Outro },
        { Verse, Chorus },
    };
    return s;
}

const float kEnergy[NumSecTypes] = { 0.82f, 0.78f, 0.88f, 1.0f, 0.85f, 0.95f };

enum Drum { K = 36, S = 38, CH = 42, OH = 46, CR = 49, RD = 51 };
const int kToms[6] = { 50, 48, 47, 45, 43, 41 };

// ------------------------------------------------------------------ theory
juce::String romanFor (int semis, Quality baseQ, Quality shownQ)
{
    static const int majorSemis[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static const char* nums[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    juce::String r;
    for (int d = 0; d < 7; ++d)
        if (majorSemis[d] == semis) r = nums[d];
    if (r.isEmpty())
        for (int d = 0; d < 7; ++d)
            if (majorSemis[d] == semis + 1) r = juce::String ("b") + nums[d];
    if (baseQ == Min || baseQ == Dim) r = r.toLowerCase();
    if (baseQ == Dim) r << "dim";
    if (shownQ == Dom7) r << "7";
    return r;
}

juce::String chordName (int pc, Quality q, bool flats)
{
    juce::String n = flats ? kFlats[pc] : kSharps[pc];
    switch (q)
    {
        case Power: n << "5"; break;
        case Min:   n << "m"; break;
        case Dim:   n << "dim"; break;
        case Sus2:  n << "sus2"; break;
        case Sus4:  n << "sus4"; break;
        case Add9:  n << "add9"; break;
        case Dom7:  n << "7"; break;
        case Min7:  n << "m7"; break;
        case Maj:   default: break;
    }
    return n;
}

struct Opt { int semis; Quality q; float w; };

std::vector<Opt> buildOptions (const StyleDef& st, int mode)
{
    const int* sc = kScales[mode];
    std::vector<Opt> o;
    for (int d = 0; d < 7; ++d)
    {
        const int third = (sc[(d + 2) % 7] - sc[d] + 12) % 12;
        const int fifth = (sc[(d + 4) % 7] - sc[d] + 12) % 12;
        if (fifth == 8) continue;
        Quality q = third == 4 ? Maj : (fifth == 6 ? Dim : Min);
        float w = st.degW[d];
        if (q == Dim) w = 0;
        if (w > 0) o.push_back ({ sc[d], q, w });
    }
    for (auto& b : st.borrows)
    {
        bool merged = false;
        for (auto& x : o)
            if (x.semis == b.semis && x.q == b.q) { x.w += b.w; merged = true; }
        if (! merged) o.push_back ({ b.semis, b.q, b.w });
    }
    return o;
}

int pickWeighted (const std::vector<float>& w, juce::Random& r)
{
    float total = 0;
    for (auto x : w) total += x;
    if (total <= 0) return r.nextInt ((int) w.size());
    float p = r.nextFloat() * total;
    for (size_t i = 0; i < w.size(); ++i)
    {
        p -= w[i];
        if (p <= 0) return (int) i;
    }
    return (int) w.size() - 1;
}

std::vector<int> makeProgression (const std::vector<Opt>& opts, SecType t, int n, juce::Random& r)
{
    std::vector<int> idx;
    auto in = [] (int v, std::initializer_list<int> l) { return std::find (l.begin(), l.end(), v) != l.end(); };

    for (int i = 0; i < n; ++i)
    {
        std::vector<float> w (opts.size());
        const int prev = i > 0 ? idx[(size_t) i - 1] : -1;
        for (size_t j = 0; j < opts.size(); ++j)
        {
            float x = opts[j].w;
            const int s = opts[j].semis;
            const bool tonic = s == 0;
            if ((int) j == prev) x = 0;
            if (i >= 2 && (int) j == idx[(size_t) i - 2]) x *= 0.5f;
            if (prev >= 0)
            {
                const int iv = (s - opts[(size_t) prev].semis + 12) % 12;
                if (iv == 5 || iv == 7) x *= 1.5f;
                if (iv == 2 || iv == 10) x *= 1.3f;
            }
            if (i == 0)
            {
                if (t == Verse) x *= tonic ? 14.f : 0.4f;
                if (t == Chorus) x *= in (s, { 0, 5, 8, 9 }) ? 2.f : 0.7f;
                if (t == Bridge) x *= tonic ? 0.1f : (in (s, { 2, 3, 4, 8, 9 }) ? 1.8f : 1.f);
                if (t == Pre) x *= tonic ? 0.2f : 1.f;
            }
            if (i == n - 1 && n > 1)
            {
                if (t == Pre) x *= s == 7 ? 5.f : (s == 5 ? 2.5f : (tonic ? 0.1f : 1.f));
                if (t == Chorus || t == Verse) x *= in (s, { 5, 7, 10 }) ? 2.f : (tonic ? 0.4f : 1.f);
                if (n > 2 && (int) j == idx[0]) x *= 0.3f;
            }
            w[j] = x;
        }
        idx.push_back (pickWeighted (w, r));
    }
    return idx;
}

Quality colour (Quality base, int vm, SecType t, juce::Random& r)
{
    switch (vm)
    {
        case V_Power: return Power;
        case V_Mixed:
            if (t == Verse || t == Pre || t == Intro) return Power;
            [[fallthrough]];
        case V_Full:
        {
            const float x = r.nextFloat();
            if (base == Maj && x < 0.10f) return Sus4;
            if (base == Maj && x < 0.18f) return Add9;
            return base;
        }
        case V_Shimmer:
            if (base == Maj) return r.nextFloat() < 0.6f ? Add9 : Sus2;
            return r.nextFloat() < 0.5f ? Min7 : Sus2;
        case V_Blues:
            if (base == Maj) return r.nextFloat() < 0.6f ? Dom7 : Power;
            return Min7;
        default: return base;
    }
}

Chord makeChord (const Opt& o, int keyPc, Quality shown, bool flats)
{
    Chord c;
    c.semis = o.semis;
    c.pc = (keyPc + o.semis) % 12;
    c.q = shown;
    c.roman = romanFor (o.semis, o.q, shown);
    c.name = chordName (c.pc, shown, flats || c.roman.startsWithIgnoreCase ("b"));
    return c;
}

int guitarRoot (int pc, bool low) { return low ? 38 + ((pc - 2 + 12) % 12) : 40 + ((pc - 4 + 12) % 12); }
int bassRoot   (int pc, bool low) { return low ? 26 + ((pc - 2 + 12) % 12) : 28 + ((pc - 4 + 12) % 12); }

std::vector<int> fullVoicing (const Chord& c, bool low)
{
    const int r = guitarRoot (c.pc, low);
    switch (c.q)
    {
        case Power: return { r, r + 7, r + 12 };
        case Min:   return { r, r + 7, r + 12, r + 15, r + 19, r + 24 };
        case Dim:   return { r, r + 6, r + 12, r + 15, r + 18 };
        case Sus2:  return { r, r + 7, r + 12, r + 14, r + 19, r + 24 };
        case Sus4:  return { r, r + 7, r + 12, r + 17, r + 19, r + 24 };
        case Add9:  return { r, r + 7, r + 14, r + 16, r + 19 };
        case Dom7:  return { r, r + 7, r + 10, r + 16, r + 19, r + 24 };
        case Min7:  return { r, r + 7, r + 10, r + 15, r + 19, r + 24 };
        case Maj:   default: return { r, r + 7, r + 12, r + 16, r + 19, r + 24 };
    }
}

// ------------------------------------------------------------------ bar builder
struct Ctx
{
    std::vector<NoteEv>& out;
    juce::Random& hr;
    double bar0 = 0;
    float energy = 1, hum = 0.4f, swing = 0;

    double sw (double b) const
    {
        if (swing <= 0) return b;
        const double f = b - std::floor (b);
        if (std::abs (f - 0.5) < 1e-6) return std::floor (b) + 0.5 + swing * 0.5;
        return b;
    }

    void add (Part p, double beat, double len, int note, float vel, juce::uint8 art = ArtNormal)
    {
        double t = bar0 + sw (beat) + (hr.nextFloat() * 2.f - 1.f) * hum * 0.012;
        t = std::max (0.0, t);
        float v = vel * energy * (1.f + (hr.nextFloat() * 2.f - 1.f) * hum * 0.12f);
        out.push_back ({ t, std::max (0.03, len), juce::jlimit (0, 127, note), juce::jlimit (0.05f, 1.f, v), p, art });
    }

    void strum (double beat, const std::vector<int>& notes, double len, float vel, juce::uint8 art = ArtNormal, bool down = true)
    {
        std::vector<int> n = notes;
        if (! down)
        {
            std::reverse (n.begin(), n.end());
            if (n.size() > 4) n.resize (4);
        }
        const double spread = down ? 0.014 : 0.010;
        for (size_t i = 0; i < n.size(); ++i)
            add (Part::Guitar, beat + (double) i * spread, len - (double) i * spread, n[i],
                 vel * (down ? 1.f : 0.72f) * (1.f - (float) i * 0.03f), art);
    }
};

std::vector<double> drumBar (Ctx& c, int pat, int var, int barInSec, int secBars,
                             bool firstBar, bool lastBar, bool fill, juce::Random& r)
{
    std::vector<double> kicks;
    auto D = [&] (double b, int n, float v) { c.add (Part::Drums, b, 0.2, n, v); };
    const double fillStart = (lastBar && fill) ? ((var % 2 == 0) ? 3.0 : 2.0) : 4.0;
    auto kick = [&] (double b, float v) { if (b < fillStart) { D (b, K, v); kicks.push_back (b); } };
    auto snare = [&] (double b, float v) { if (b < fillStart) D (b, S, v); };
    auto cym = [&] (double b, int n, float v) { if (b < fillStart && ! (firstBar && b == 0.0)) D (b, n, v); };

    switch (pat)
    {
        case D_Rock8:
        case D_Ride:
        {
            static const std::vector<std::vector<double>> kv = { { 0, 2 }, { 0, 2, 2.5 }, { 0, 1.5, 2 }, { 0, 2, 2.5, 3.5 } };
            for (double b : kv[(size_t) var % kv.size()]) kick (b, 0.95f);
            snare (1, 0.95f); snare (3, 0.95f);
            for (int i = 0; i < 8; ++i) cym (i * 0.5, pat == D_Ride ? RD : CH, i % 2 == 0 ? 0.75f : 0.55f);
            break;
        }
        case D_Punk:
            for (int i = 0; i < 4; ++i) { kick (i, 0.95f); snare (i + 0.5, 0.9f); }
            for (int i = 0; i < 8; ++i) cym (i * 0.5, CH, 0.75f);
            break;
        case D_Half:
            kick (0, 1.f);
            if (var % 2) kick (1.5, 0.85f);
            if (var % 3 == 2) kick (3.5, 0.8f);
            snare (2, 1.f);
            for (int i = 0; i < 8; ++i) cym (i * 0.5, CH, i % 2 == 0 ? 0.7f : 0.5f);
            break;
        case D_Four:
            for (int i = 0; i < 4; ++i) { kick (i, 0.95f); cym (i, CH, 0.5f); cym (i + 0.5, OH, 0.65f); }
            snare (1, 0.95f); snare (3, 0.95f);
            break;
        case D_Shuffle:
            kick (0, 0.95f); kick (2, 0.9f);
            if (var % 2) kick (2.0 + 2.0 / 3.0, 0.75f);
            snare (1, 0.9f); snare (3, 0.9f);
            for (int i = 0; i < 4; ++i) { cym (i, RD, 0.7f); cym (i + 2.0 / 3.0, RD, 0.5f); }
            break;
        case D_Double:
            for (int i = 0; i < 16; ++i) kick (i * 0.25, i % 4 == 0 ? 0.9f : 0.72f);
            snare (1, 1.f); snare (3, 1.f);
            for (int i = 0; i < 4; ++i) cym (i, RD, 0.75f);
            break;
        case D_Sparse:
            kick (0, 0.85f); kick (2.5, 0.75f);
            if (var % 2) kick (1.5, 0.6f);
            snare (1, 0.5f); snare (3, 0.5f);
            for (int i = 0; i < 4; ++i) cym (i, CH, 0.5f);
            break;
        case D_Build:
        {
            const float prog = (float) barInSec / (float) std::max (1, secBars);
            const double step = prog < 0.5f ? 1.0 : (prog < 0.75f ? 0.5 : 0.25);
            for (int i = 0; i < 4; ++i) kick (i, 0.9f);
            for (double b = 0; b < 4.0; b += step) snare (b, 0.5f + 0.5f * (prog + (float) b / 16.f));
            break;
        }
        case D_Breakdown:
        {
            static const std::vector<std::vector<double>> kv = { { 0, 0.75, 1.5, 2.75, 3.5 }, { 0, 0.5, 1.5, 2.5, 3.25 }, { 0, 1, 1.75, 2.5, 3.5 } };
            for (double b : kv[(size_t) var % kv.size()]) kick (b, 1.f);
            snare (2, 1.f);
            for (int i = 0; i < 4; ++i) cym (i, CR, 0.55f);
            break;
        }
        default: break;
    }

    if (firstBar) { D (0, CR, 0.95f); if (kicks.empty() || kicks.front() > 0.0) { D (0, K, 0.95f); kicks.insert (kicks.begin(), 0.0); } }
    if ((pat == D_Ride || pat == D_Double) && ! firstBar && barInSec % 4 == 0) D (0, CR, 0.8f);

    if (fillStart < 4.0)
    {
        const int type = r.nextInt (3);
        D (fillStart, K, 0.9f);
        kicks.push_back (fillStart);
        int k = 0;
        if (type == 2)
        {
            for (double b = fillStart; b < 4.0; b += 0.5, ++k)
                D (b, k % 2 == 0 ? S : kToms[std::min (5, k)], 0.85f);
        }
        else
        {
            const int steps = (int) ((4.0 - fillStart) / 0.25);
            for (int i = 0; i < steps; ++i)
            {
                const double b = fillStart + i * 0.25;
                const float v = 0.55f + 0.45f * (float) i / (float) steps;
                if (type == 0) D (b, S, v);
                else D (b, kToms[std::min (5, i * 6 / steps)], v);
            }
        }
    }
    std::sort (kicks.begin(), kicks.end());
    return kicks;
}

void guitarBar (Ctx& c, int pat, int var, const Chord& ch, bool low, const std::vector<double>& kicks)
{
    const auto full = fullVoicing (ch, low);
    const int r = guitarRoot (ch.pc, low);
    const std::vector<int> pm = { r, r + 7 };
    const std::vector<int> pw = { r, r + 7, r + 12 };

    switch (pat)
    {
        case G_Stabs:
        {
            static const std::vector<std::vector<std::pair<double, double>>> rv = {
                { { 0, 1.4 }, { 1.5, 0.45 }, { 2.5, 0.4 }, { 3, 0.9 } },
                { { 0, 0.4 }, { 0.75, 0.4 }, { 1.5, 1.4 }, { 3, 0.4 }, { 3.5, 0.45 } },
                { { 0, 1.9 }, { 2, 0.4 }, { 2.5, 1.4 } } };
            for (auto& p : rv[(size_t) var % rv.size()]) c.strum (p.first, full, p.second, 0.9f);
            break;
        }
        case G_Chug8:
            for (int i = 0; i < 8; ++i)
            {
                const bool accent = i == 0 || (var % 2 == 1 && i == 5);
                if (accent) c.strum (i * 0.5, pw, 0.45, 0.95f);
                else c.strum (i * 0.5, pm, 0.3, 0.72f, ArtPalmMute);
            }
            break;
        case G_Drive8:
            for (int i = 0; i < 8; ++i) c.strum (i * 0.5, full.size() > 3 && var % 2 ? full : pw, 0.46, i % 2 == 0 ? 0.88f : 0.76f);
            break;
        case G_Strum:
        {
            static const std::vector<std::vector<std::pair<double, bool>>> sv = {
                { { 0, true }, { 1, true }, { 1.5, false }, { 2.5, false }, { 3, true }, { 3.5, false } },
                { { 0, true }, { 0.5, false }, { 1, true }, { 1.5, false }, { 2, true }, { 2.5, false }, { 3, true }, { 3.5, false } } };
            const auto& s = sv[(size_t) var % sv.size()];
            for (size_t i = 0; i < s.size(); ++i)
            {
                const double next = i + 1 < s.size() ? s[i + 1].first : 4.0;
                c.strum (s[i].first, full, next - s[i].first, 0.85f, ArtNormal, s[i].second);
            }
            break;
        }
        case G_LetRing:
            if (var % 2 == 0) c.strum (0, full, 3.95, 0.85f, ArtLetRing);
            else { c.strum (0, full, 2.45, 0.85f, ArtLetRing); c.strum (2.5, full, 1.45, 0.7f, ArtLetRing, false); }
            break;
        case G_Half:
            c.strum (0, full, 1.9, 0.9f, ArtLetRing);
            c.strum (2, full, 1.9, 0.82f, ArtLetRing);
            break;
        case G_Arp:
        {
            static const std::vector<std::vector<int>> order = { { 0, 2, 3, 4, 5, 4, 3, 2 }, { 0, 3, 2, 4, 1, 3, 5, 3 } };
            const auto& o = order[(size_t) var % order.size()];
            for (int i = 0; i < 8; ++i)
                c.add (Part::Guitar, i * 0.5, 1.0, full[(size_t) o[(size_t) i] % full.size()], i == 0 ? 0.8f : 0.65f, ArtLetRing);
            break;
        }
        case G_Gallop:
            for (int b = 0; b < 4; ++b)
            {
                const double offs[2][3] = { { 0, 0.5, 0.75 }, { 0, 0.25, 0.5 } };
                for (int k = 0; k < 3; ++k)
                {
                    const double t = b + offs[var % 2][k];
                    if (b == 0 && k == 0) c.strum (t, pw, 0.24, 0.95f);
                    else c.strum (t, pm, 0.2, 0.74f, ArtPalmMute);
                }
            }
            break;
        case G_Boogie:
            for (int i = 0; i < 8; ++i)
            {
                const int beat = i / 2;
                int top = (beat % 2 == 0) ? 7 : 9;
                if (var % 2 == 1 && beat == 2) top = 10;
                c.strum (i * 0.5, { r, r + top }, 0.42, i % 2 == 0 ? 0.88f : 0.72f);
            }
            break;
        case G_Breakdown:
            for (double k : kicks)
            {
                if (k == 0.0) c.strum (k, pw, 0.4, 1.f);
                else c.strum (k, pm, 0.22, 0.85f, ArtPalmMute);
            }
            break;
        default: break;
    }
}

void bassBar (Ctx& c, int pat, int var, const Chord& ch, const Chord& next, bool low, const std::vector<double>& kicks)
{
    const int r = bassRoot (ch.pc, low);
    auto B = [&] (double b, double len, int n, float v) { c.add (Part::Bass, b, len, n, v); };
    switch (pat)
    {
        case B_Root8:
            for (int i = 0; i < 8; ++i)
            {
                int n = r;
                if (i == 7 && var % 2 == 1 && next.pc != ch.pc) n = r + 7;
                B (i * 0.5, 0.45, n, i % 2 == 0 ? 0.85f : 0.75f);
            }
            break;
        case B_Kick:
            for (size_t i = 0; i < kicks.size(); ++i)
            {
                const double end = i + 1 < kicks.size() ? kicks[i + 1] : 4.0;
                B (kicks[i], std::min (1.0, end - kicks[i] - 0.04), r, 0.85f);
            }
            break;
        case B_Whole:
            B (0, 3.9, r, 0.85f);
            break;
        case B_Walk:
        {
            const int third = (ch.q == Min || ch.q == Min7) ? 3 : 4;
            const int steps[8] = { 0, third, 7, 9, 10, 9, 7, third };
            for (int i = 0; i < 8; ++i) B (i * 0.5, 0.45, r + steps[i], i % 2 == 0 ? 0.85f : 0.72f);
            break;
        }
        case B_Pump:
            for (int i = 0; i < 8; ++i)
            {
                const bool oct = i == 7 || (var % 2 == 1 && i == 3);
                B (i * 0.5, 0.45, oct ? r + 12 : r, i % 2 == 0 ? 0.86f : 0.74f);
            }
            break;
        case B_Half:
            B (0, 1.9, r, 0.85f);
            B (2, 1.9, r, 0.8f);
            break;
        default: break;
    }
}
} // namespace

// ====================================================================== public
juce::StringArray styleNames()
{
    juce::StringArray a;
    for (auto& s : styles()) a.add (s.name);
    return a;
}
juce::StringArray keyNames() { juce::StringArray a; for (auto n : kNotes) a.add (n); return a; }
juce::StringArray modeNames() { return { "Auto", "Major", "Minor", "Mixolydian", "Dorian", "Phrygian" }; }
juce::StringArray structureNames() { return { "Verse / Chorus", "Short & Fast", "Slow Build", "No Pre-Chorus", "Jam Loop" }; }
juce::StringArray voicingNames() { return { "Auto", "Power Chords", "Full Chords", "Shimmer" }; }
int styleTempo (int style) { return styles()[(size_t) juce::jlimit (0, (int) styles().size() - 1, style)].bpm; }

juce::String sectionName (SecType t)
{
    static const char* n[NumSecTypes] = { "INTRO", "VERSE", "PRE-CHORUS", "CHORUS", "BRIDGE", "OUTRO" };
    return n[t];
}
juce::String guitarPatName (int p)
{
    static const char* n[NumGuitarPats] = { "chord stabs", "palm-muted 8th chugs", "driving 8th downstrokes", "open strumming",
                                            "big let-ring chords", "half-note hits", "picked arpeggios", "metal gallop",
                                            "blues boogie riff", "breakdown chugs locked to the kick" };
    return n[juce::jlimit (0, NumGuitarPats - 1, p)];
}
juce::String bassPatName (int p)
{
    static const char* n[NumBassPats] = { "driving 8th roots", "locked to the kick", "long whole notes", "shuffle walk",
                                          "pumping 8ths with octaves", "half notes" };
    return n[juce::jlimit (0, NumBassPats - 1, p)];
}
juce::String drumPatName (int p)
{
    static const char* n[NumDrumPats] = { "straight rock beat", "punk two-step", "half-time", "four-on-the-floor", "shuffle",
                                          "double kick", "sparse groove", "snare build-up", "rock beat on the ride", "breakdown" };
    return n[juce::jlimit (0, NumDrumPats - 1, p)];
}

juce::String sectionTip (const Song& song, int si)
{
    if (si < 0 || si >= (int) song.sections.size()) return {};
    const auto& s = song.sections[(size_t) si];
    static const char* tips[NumSecTypes] = {
        "Hook them fast: the intro previews the verse chords - try your catchiest riff here.",
        "Leave room for the vocal: keep guitars tight and mid-range, save the big sounds.",
        "Build tension: energy rises and it ends on a chord that pulls into the chorus.",
        "The payoff: open chords, crashes and double-tracked guitars. Put your title line here.",
        "Contrast: new chords and feel. Strip back or go half-time before the last chorus.",
        "Bring the chorus home, then end on one big ringing chord." };
    return tips[s.type];
}

Song generateSong (const GenSettings& gs)
{
    const auto& sty = styles();
    const int si = juce::jlimit (0, (int) sty.size() - 1, gs.style);
    const auto& st = sty[(size_t) si];
    const int mode = gs.mode <= 0 ? st.defMode : juce::jlimit (1, 5, gs.mode);
    const int vm = gs.voicing == 1 ? V_Power : gs.voicing == 2 ? V_Full : gs.voicing == 3 ? V_Shimmer : st.voicing;
    const int keyPc = ((gs.key % 12) + 12) % 12;
    const bool flats = useFlats (keyPc, mode);

    Song song;
    song.keyPc = keyPc; song.modeIndex = mode; song.styleIndex = si; song.suggestedBpm = st.bpm;

    const auto opts = buildOptions (st, mode);

    // progressions per section type (repeats share them)
    std::array<std::vector<Chord>, NumSecTypes> progs;
    std::array<int, NumSecTypes> varG {}, varB {}, varD {};
    for (int t = 0; t < NumSecTypes; ++t)
    {
        juce::Random r (gs.seeds[(size_t) t] * 7919 + 17 * t + 1);
        const int n = t == Pre ? 2 : 4;
        const auto idx = makeProgression (opts, (SecType) t, n, r);
        for (int i : idx) progs[(size_t) t].push_back (makeChord (opts[(size_t) i], keyPc, colour (opts[(size_t) i].q, vm, (SecType) t, r), flats));
        varG[(size_t) t] = r.nextInt (6); varB[(size_t) t] = r.nextInt (6); varD[(size_t) t] = r.nextInt (6);
    }
    progs[Intro] = progs[Verse];
    {
        // outro = chorus progression + tonic ending
        auto o = progs[Chorus];
        bool hasTonic = false;
        for (auto& c : o) hasTonic |= c.semis == 0;
        if (! hasTonic)
        {
            Opt tonic { 0, kScales[mode][2] == 4 ? Maj : Min, 1 };
            juce::Random r (gs.seeds[Outro] + 5);
            o.push_back (makeChord (tonic, keyPc, colour (tonic.q, vm, Chorus, r), flats));
        }
        progs[Outro] = o;
    }

    // sections
    const auto& structure = structures()[(size_t) juce::jlimit (0, (int) structures().size() - 1, gs.structure)];
    int bar = 0;
    for (auto t : structure)
    {
        Section s;
        s.type = t;
        s.startBar = bar;
        s.chords = progs[t];
        s.gPat = st.g[t]; s.bPat = st.b[t]; s.dPat = st.d[t];
        s.energy = kEnergy[t];
        switch (t)
        {
            case Intro:  s.barChord = { 0, 1, 2, 3 }; break;
            case Verse:
            case Chorus: s.barChord = { 0, 1, 2, 3, 0, 1, 2, 3 }; break;
            case Pre:    s.barChord = { 0, 0, 1, 1 }; break;
            case Bridge: s.barChord = { 0, 0, 1, 1, 2, 2, 3, 3 }; break;
            case Outro:
            {
                int tonic = 0;
                for (size_t i = 0; i < s.chords.size(); ++i) if (s.chords[i].semis == 0) { tonic = (int) i; break; }
                s.barChord = { 0, 1, 2, 3, tonic };
                break;
            }
            default: break;
        }
        s.bars = (int) s.barChord.size();
        bar += s.bars;
        song.sections.push_back (s);
    }
    song.totalBars = bar;
    song.totalBeats = bar * 4.0;

    // parts
    int instance = 0;
    for (size_t sIdx = 0; sIdx < song.sections.size(); ++sIdx)
    {
        const auto& s = song.sections[sIdx];
        juce::Random hr (gs.seeds[s.type] * 31 + (juce::int64) (++instance) * 104729);
        Ctx c { song.notes, hr };
        c.energy = s.energy;
        c.hum = gs.humanize;
        c.swing = st.swing;
        const bool isLastSection = sIdx + 1 == song.sections.size();

        for (int b = 0; b < s.bars; ++b)
        {
            c.bar0 = (s.startBar + b) * 4.0;
            const auto& ch = s.chords[(size_t) s.barChord[(size_t) b]];
            const Chord& next = b + 1 < s.bars ? s.chords[(size_t) s.barChord[(size_t) b + 1]]
                                               : (! isLastSection ? song.sections[sIdx + 1].chords[(size_t) song.sections[sIdx + 1].barChord[0]] : ch);
            const bool finalHit = s.type == Outro && b == s.bars - 1;
            if (finalHit)
            {
                c.add (Part::Drums, 0, 0.2, K, 1.f);
                c.add (Part::Drums, 0, 0.2, CR, 1.f);
                c.strum (0, fullVoicing (ch, st.lowTune), 4.0, 0.95f, ArtLetRing);
                c.add (Part::Bass, 0, 4.0, bassRoot (ch.pc, st.lowTune), 0.9f);
                continue;
            }
            // build-up into the chorus: crescendo
            if (s.type == Pre) c.energy = s.energy + 0.12f * (float) b / (float) s.bars;
            const bool fill = (b == s.bars - 1) && ! (s.type == Outro);
            const auto kicks = drumBar (c, s.dPat, varD[s.type], b, s.bars, b == 0, b == s.bars - 1, fill, hr);
            guitarBar (c, s.gPat, varG[s.type], ch, st.lowTune, kicks);
            bassBar (c, s.bPat, varB[s.type], ch, next, st.lowTune, kicks);
        }
    }

    // stop same-note overlaps per part so note-offs never cut the next hit
    std::sort (song.notes.begin(), song.notes.end(), [] (const NoteEv& a, const NoteEv& b)
    {
        if (a.part != b.part) return (int) a.part < (int) b.part;
        if (a.note != b.note) return a.note < b.note;
        return a.beat < b.beat;
    });
    for (size_t i = 0; i + 1 < song.notes.size(); ++i)
    {
        auto& a = song.notes[i];
        const auto& n = song.notes[i + 1];
        if (a.part == n.part && a.note == n.note && a.beat + a.len > n.beat - 0.01)
            a.len = std::max (0.02, n.beat - a.beat - 0.01);
    }
    for (auto& n : song.notes)
        n.len = std::min (n.len, song.totalBeats - n.beat - 0.001);

    std::sort (song.notes.begin(), song.notes.end(), [] (const NoteEv& a, const NoteEv& b) { return a.beat < b.beat; });

    for (auto& n : song.notes)
    {
        song.trigs.push_back ({ n.beat, true, n.note, n.vel, n.part, n.art });
        song.trigs.push_back ({ n.beat + n.len, false, n.note, 0, n.part, n.art });
    }
    std::stable_sort (song.trigs.begin(), song.trigs.end(), [] (const Trig& a, const Trig& b)
    {
        if (a.beat != b.beat) return a.beat < b.beat;
        return (! a.on) && b.on;
    });
    return song;
}

juce::MidiFile toMidiFile (const Song& song, int partMask, double bpm)
{
    constexpr int tpq = 960;
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (tpq);

    juce::MidiMessageSequence meta;
    meta.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / juce::jmax (20.0, bpm))), 0);
    meta.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    mf.addTrack (meta);

    static const char* names[3] = { "Riffhouse Guitar", "Riffhouse Bass", "Riffhouse Drums" };
    static const int chans[3] = { 1, 2, 10 };
    for (int p = 0; p < 3; ++p)
    {
        if (! (partMask & (1 << p))) continue;
        juce::MidiMessageSequence seq;
        auto nm = juce::MidiMessage::textMetaEvent (3, names[p]);
        nm.setTimeStamp (0);
        seq.addEvent (nm);
        for (auto& n : song.notes)
        {
            if ((int) n.part != p) continue;
            const double on = std::round (n.beat * tpq);
            const double off = std::round ((n.beat + (p == 2 ? 0.1 : n.len)) * tpq);
            const auto vel = (juce::uint8) juce::jlimit (1, 127, (int) std::round (n.vel * 127.f));
            seq.addEvent (juce::MidiMessage::noteOn (chans[p], n.note, vel), on);
            seq.addEvent (juce::MidiMessage::noteOff (chans[p], n.note), std::max (on + 1, off));
        }
        seq.updateMatchedPairs();
        seq.sort();
        mf.addTrack (seq);
    }
    return mf;
}
} // namespace rh
