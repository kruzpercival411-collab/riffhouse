#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

// =====================================================================
//  RIFFHOUSE sound engines
//  Guitar + bass: Karplus-Strong plucked strings (no samples needed).
//  Drums: synthesized rock kit. Each has its own bus processing.
// =====================================================================
namespace rh
{
inline float midiHz (float n) { return 440.f * std::pow (2.f, (n - 69.f) / 12.f); }
inline float decayCoef (float seconds, double sr) { return (float) std::exp (-6.9 / (std::max (0.001, (double) seconds) * sr)); }

struct Noise
{
    juce::uint32 s = 0x9E3779B9u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) ((double) s / 4294967295.0 * 2.0 - 1.0); }
};

// ---------------------------------------------------------------- string
class KSString
{
public:
    void prepare (double sampleRate, juce::uint32 seed)
    {
        sr = sampleRate;
        buf.assign ((size_t) (sr / 16.0) + 16, 0.f);
        scratch.reserve (buf.size());   // no allocation on the audio thread
        w = 0; lp = 0; env = 0; dcX = dcY = 0; pending = -1;
        noise.s = seed | 1u;
    }

    // schedule a pluck 'delaySamples' from now (used for double-tracking)
    void pluck (float freq, float vel, float bright, float decaySec, float damp, int delaySamples)
    {
        pf = freq; pv = vel; pb = bright; pdec = decaySec; pdamp = damp;
        pending = std::max (0, delaySamples);
        if (pending == 0) doPluck();
    }

    void release (float seconds = 0.07f)
    {
        if (pending > 0) { pending = -1; }
        damp = std::max (damp, 0.6f);
        setDecay (freq, seconds);
    }

    bool isActive() const { return pending >= 0 || env > 1.0e-4f; }

    float process()
    {
        if (pending > 0 && --pending == 0) doPluck();
        if (env <= 1.0e-5f && pending < 0) return 0.f;

        const int N = (int) buf.size();
        float rp = (float) w - delay;
        while (rp < 0) rp += (float) N;
        const int i0 = (int) rp;
        const float fr = rp - (float) i0;
        const float s = buf[(size_t) (i0 % N)] * (1.f - fr) + buf[(size_t) ((i0 + 1) % N)] * fr;
        lp = s + damp * (lp - s);
        const float y = lp * g;
        buf[(size_t) w] = y;
        if (++w >= N) w = 0;

        // dc blocker
        const float out = y - dcX + 0.995f * dcY;
        dcX = y; dcY = out;
        env = std::max (std::abs (out), env * 0.9997f);
        return out;
    }

    float freq = 110.f;

private:
    void setDecay (float f, float seconds)
    {
        g = std::pow (0.001f, 1.f / std::max (1.f, f * seconds));
    }

    void doPluck()
    {
        pending = -1;
        freq = pf;
        damp = pdamp;
        const float period = (float) sr / pf;
        delay = juce::jlimit (2.f, (float) buf.size() - 4.f, period - damp / (1.f - damp));
        setDecay (pf, pdec);

        const int len = std::min ((int) buf.size() - 2, (int) std::ceil (period) + 2);
        const int N = (int) buf.size();
        const int pick = std::max (1, (int) (period * 0.13f));
        const float c = 1.f - juce::jlimit (0.05f, 0.98f, pb);       // excitation lowpass
        float l = 0;
        std::vector<float>& e = scratch;
        e.assign ((size_t) len, 0.f);
        for (int i = 0; i < len; ++i) { l = l + (1.f - c) * (noise.next() - l); e[(size_t) i] = l; }
        for (int i = len - 1; i >= pick; --i) e[(size_t) i] -= e[(size_t) (i - pick)];   // pick position comb
        float peak = 1.0e-6f;
        for (auto v : e) peak = std::max (peak, std::abs (v));
        for (int i = 0; i < len; ++i)
        {
            int idx = w - len + i;
            while (idx < 0) idx += N;
            buf[(size_t) idx] = e[(size_t) i] / peak * pv * 0.5f;
        }
        lp = 0;
        env = pv;
    }

    double sr = 44100;
    std::vector<float> buf, scratch;
    int w = 0, pending = -1;
    float delay = 100, damp = 0.3f, g = 0.999f, lp = 0, env = 0, dcX = 0, dcY = 0;
    float pf = 110, pv = 0.8f, pb = 0.5f, pdec = 2.f, pdamp = 0.3f;
    Noise noise;
};

// ---------------------------------------------------------------- guitar
class GuitarEngine
{
public:
    static constexpr int numVoices = 24;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (int i = 0; i < numVoices; ++i)
        {
            voices[i].l.prepare (sr, 1234u + (juce::uint32) i * 77u);
            voices[i].r.prepare (sr, 98765u + (juce::uint32) i * 131u);
            voices[i].note = -1;
        }
    }

    void noteOn (int note, float vel, int art)
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (x.note == note) { v = &x; break; }
        if (! v) for (auto& x : voices) if (! x.l.isActive() && ! x.r.isActive()) { v = &x; break; }
        if (! v)
        {
            v = &voices[0];
            for (auto& x : voices) if (x.age < v->age) v = &x;
        }
        v->note = note;
        v->age = ++counter;
        const float f = midiHz ((float) note);
        const bool pm = art == 1;
        const float bright = pm ? 0.38f : 0.35f + 0.55f * vel;
        float dec = 4.5f * std::pow (0.5f, ((float) note - 40.f) / 24.f);
        if (art == 2) dec *= 1.5f;
        if (pm) dec = 0.22f;
        const float damp = pm ? 0.78f : 0.18f;
        const float amp = vel * (pm ? 1.1f : 1.f);
        const int dl = (int) (rng.nextFloat() * 0.002f * (float) sr);
        const int dr = (int) ((0.007f + rng.nextFloat() * 0.008f) * (float) sr);
        const float detune = std::pow (2.f, (3.f + rng.nextFloat() * 4.f) / 1200.f);
        v->l.pluck (f, amp, bright, dec, damp, dl);
        v->r.pluck (f * detune, amp, juce::jlimit (0.05f, 0.98f, bright + 0.05f), dec, damp, dr);
    }

    void noteOff (int note)
    {
        for (auto& v : voices) if (v.note == note) { v.l.release(); v.r.release(); v.note = -1; }
    }

    void allOff() { for (auto& v : voices) { v.l.release (0.03f); v.r.release (0.03f); v.note = -1; } }

    void render (float& outL, float& outR)
    {
        float a = 0, b = 0;
        for (auto& v : voices) { a += v.l.process(); b += v.r.process(); }
        outL = a; outR = b;
    }

private:
    struct Voice { KSString l, r; int note = -1; juce::uint64 age = 0; };
    Voice voices[numVoices];
    juce::uint64 counter = 0;
    double sr = 44100;
    juce::Random rng { 42 };
};

// ---------------------------------------------------------------- bass
class BassEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (int i = 0; i < 2; ++i) voices[i].s.prepare (sr, 555u + (juce::uint32) i * 999u);
    }

    void noteOn (int note, float vel)
    {
        // monophonic: release whatever is ringing, take the other slot
        for (auto& v : voices) if (v.note >= 0) { v.s.release (0.02f); v.note = -1; v.subRel = true; }
        cur = 1 - cur;
        auto& v = voices[cur];
        v.note = note;
        const float f = midiHz ((float) note);
        v.s.pluck (f, vel, 0.25f + 0.4f * vel, 3.0f, 0.35f, 0);
        v.subF = f; v.subPh = 0; v.subEnv = vel * 0.55f; v.subRel = false;
    }

    void noteOff (int note)
    {
        for (auto& v : voices) if (v.note == note) { v.s.release (0.05f); v.note = -1; v.subRel = true; }
    }

    void allOff() { for (auto& v : voices) { v.s.release (0.03f); v.note = -1; v.subRel = true; } }

    float render()
    {
        float o = 0;
        const float kSus = decayCoef (2.5f, sr), kRel = decayCoef (0.05f, sr);
        for (auto& v : voices)
        {
            o += v.s.process();
            if (v.subEnv > 1.0e-5f)
            {
                v.subPh += v.subF / (float) sr;
                if (v.subPh >= 1.f) v.subPh -= 1.f;
                o += std::sin (v.subPh * juce::MathConstants<float>::twoPi) * v.subEnv;
                v.subEnv *= v.subRel ? kRel : kSus;
            }
        }
        return o;
    }

private:
    struct Voice { KSString s; int note = -1; float subF = 55, subPh = 0, subEnv = 0; bool subRel = true; };
    Voice voices[2];
    int cur = 0;
    double sr = 44100;
};

// ---------------------------------------------------------------- drums
class DrumKit
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        auto hp = [&] (float f) { return juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, f, 0.7f); };
        hatHP.coefficients = hp (7000.f);
        cymHP.coefficients = hp (4200.f);
        rideHP.coefficients = hp (5200.f);
        snHP.coefficients = hp (1300.f);
        hatHP.reset(); cymHP.reset(); rideHP.reset(); snHP.reset();
        for (int i = 0; i < 6; ++i) metalPh[i] = 0;
    }

    void setPunch (float p) { punch = p; }

    void hit (int note, float vel)
    {
        const float v = vel * vel * 0.5f + vel * 0.5f;
        switch (note)
        {
            case 35: case 36: kAmp = v; kPitch = 1.f; kClick = v; kPh = 0; break;
            case 37: case 38: case 40: sTone = v * 0.8f; sNoise = v; sPh1 = sPh2 = 0; break;
            case 42: case 44: hClosed = v * 0.6f; hOpen *= 0.05f; break;
            case 46: hOpen = v * 0.55f; break;
            case 49: case 57: case 52: case 55: crash = v * 0.75f; break;
            case 51: case 59: case 53: ride = v * 0.5f; break;
            case 41: case 43: case 45: case 47: case 48: case 50:
            {
                static const std::pair<int, float> map[] = { { 50, 210 }, { 48, 175 }, { 47, 150 }, { 45, 125 }, { 43, 100 }, { 41, 84 } };
                for (auto& m : map)
                    if (m.first == note)
                    {
                        auto& t = toms[tomNext]; tomNext = (tomNext + 1) % 3;
                        t.f = m.second; t.amp = v; t.pitch = 1.f; t.ph = 0;
                        t.pan = juce::jmap ((float) (m.second - 84.f) / (210.f - 84.f), -0.4f, 0.4f);
                    }
                break;
            }
            default: break;
        }
    }

    void allOff() { hOpen = 0; }

    void render (float& outL, float& outR)
    {
        const float isr = 1.f / (float) sr;
        float l = 0, r = 0;

        // kick
        if (kAmp > 1.0e-5f)
        {
            const float f = 46.f + (90.f + 90.f * punch) * kPitch;
            kPh += f * isr; if (kPh >= 1.f) kPh -= 1.f;
            float k = std::sin (kPh * juce::MathConstants<float>::twoPi) * kAmp;
            k += noise.next() * kClick * (0.3f + 0.6f * punch);
            k = std::tanh (k * 1.6f);
            l += k; r += k;
            kAmp *= decayCoef (0.42f, sr); kPitch *= decayCoef (0.045f, sr); kClick *= decayCoef (0.004f, sr);
        }
        // snare
        if (sTone > 1.0e-5f || sNoise > 1.0e-5f)
        {
            sPh1 += 185.f * isr; if (sPh1 >= 1.f) sPh1 -= 1.f;
            sPh2 += 330.f * isr; if (sPh2 >= 1.f) sPh2 -= 1.f;
            const float tone = (std::sin (sPh1 * juce::MathConstants<float>::twoPi) + 0.6f * std::sin (sPh2 * juce::MathConstants<float>::twoPi)) * sTone * 0.6f;
            const float nz = snHP.processSample (noise.next()) * sNoise * 0.9f;
            const float s = std::tanh ((tone + nz) * 1.3f);
            l += s; r += s;
            sTone *= decayCoef (0.12f, sr); sNoise *= decayCoef (0.24f, sr);
        }
        // metallic source (shared)
        float metal = 0;
        const bool cymActive = hClosed > 1.0e-5f || hOpen > 1.0e-5f || crash > 1.0e-5f || ride > 1.0e-5f;
        if (cymActive)
        {
            static const float mf[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f };
            for (int i = 0; i < 6; ++i)
            {
                metalPh[i] += mf[i] * 2.1f * isr; if (metalPh[i] >= 1.f) metalPh[i] -= 1.f;
                metal += metalPh[i] < 0.5f ? 1.f : -1.f;
            }
            metal *= 0.16f;
            const float nz = noise.next();
            const float hat = hatHP.processSample (metal * 0.6f + nz * 0.5f) * (hClosed + hOpen);
            const float cy = cymHP.processSample (nz * 0.6f + metal * 0.4f) * crash;
            const float rd = rideHP.processSample (metal * 0.75f + nz * 0.25f) * ride;
            l += hat * 0.75f + cy * 1.25f + rd * 0.7f;
            r += hat * 1.25f + cy * 0.75f + rd * 1.3f;
            hClosed *= decayCoef (0.055f, sr); hOpen *= decayCoef (0.5f, sr);
            crash *= decayCoef (2.2f, sr); ride *= decayCoef (1.0f, sr);
        }
        // toms
        for (auto& t : toms)
        {
            if (t.amp < 1.0e-5f) continue;
            t.ph += t.f * (1.f + 0.5f * t.pitch) * isr; if (t.ph >= 1.f) t.ph -= 1.f;
            const float s = std::tanh ((std::sin (t.ph * juce::MathConstants<float>::twoPi) + noise.next() * 0.08f * t.pitch) * t.amp * 1.4f);
            l += s * (1.f - t.pan); r += s * (1.f + t.pan);
            t.amp *= decayCoef (0.45f, sr); t.pitch *= decayCoef (0.07f, sr);
        }
        outL = l; outR = r;
    }

private:
    struct Tom { float f = 100, amp = 0, pitch = 0, ph = 0, pan = 0; };
    double sr = 44100;
    Noise noise;
    float punch = 0.5f;
    float kAmp = 0, kPitch = 0, kClick = 0, kPh = 0;
    float sTone = 0, sNoise = 0, sPh1 = 0, sPh2 = 0;
    float hClosed = 0, hOpen = 0, crash = 0, ride = 0;
    float metalPh[6] {};
    Tom toms[3];
    int tomNext = 0;
    juce::dsp::IIR::Filter<float> hatHP, cymHP, rideHP, snHP;
};

// ---------------------------------------------------------------- buses
class GuitarAmp
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        os.initProcessing ((size_t) maxBlock);
        os.reset();
        for (int c = 0; c < 2; ++c)
        {
            preHP[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 90.f, 0.7f);
            postHP[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 75.f, 0.7f);
            thump[c].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 120.f, 0.9f, 1.4f);
            presence[c].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 2200.f, 1.0f, 1.5f);
            preHP[c].reset(); postHP[c].reset(); thump[c].reset(); presence[c].reset();
            for (auto& f : cab[c]) f.reset();
        }
        lastTone = -1;
    }

    void process (juce::AudioBuffer<float>& b, int n, float drive, float tone, float width)
    {
        if (std::abs (tone - lastTone) > 0.001f)
        {
            lastTone = tone;
            const float fc = 2200.f * std::pow (3.6f, tone);
            for (int c = 0; c < 2; ++c)
                for (auto& f : cab[c]) f.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, fc, 0.707f);
        }
        for (int c = 0; c < 2; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i) d[i] = preHP[c].processSample (d[i]);
        }

        // waveshaper, 4x oversampled
        const float gain = juce::Decibels::decibelsToGain (drive * 44.f);
        const float makeup = juce::jmap (drive, 1.0f, 0.32f);
        const float bias = 0.12f, tb = std::tanh (bias);
        juce::dsp::AudioBlock<float> block (b.getArrayOfWritePointers(), 2, (size_t) n);
        auto up = os.processSamplesUp (block);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                const float x = d[i] * gain;
                const float y1 = std::tanh (x + bias) - tb;
                d[i] = std::tanh (y1 * 1.5f) * makeup;   // two cascaded stages = tube-ish
            }
        }
        os.processSamplesDown (block);

        for (int c = 0; c < 2; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                float x = postHP[c].processSample (d[i]);
                x = thump[c].processSample (x);
                x = presence[c].processSample (x);
                for (auto& f : cab[c]) x = f.processSample (x);
                d[i] = x;
            }
        }
        auto* L = b.getWritePointer (0);
        auto* R = b.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]) * width * 1.4f;
            L[i] = m + s; R[i] = m - s;
        }
    }

private:
    double sr = 44100;
    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };
    juce::dsp::IIR::Filter<float> preHP[2], postHP[2], thump[2], presence[2], cab[2][2];
    float lastTone = -1;
};

class BassAmp
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 35.f, 0.7f);
        hp.reset(); lp.reset();
        comp.prepare ({ sr, (juce::uint32) maxBlock, 1 });
        comp.setThreshold (-20.f); comp.setRatio (4.f); comp.setAttack (6.f); comp.setRelease (90.f);
        lastTone = -1;
    }

    void process (float* d, int n, float drive, float tone)
    {
        if (std::abs (tone - lastTone) > 0.001f)
        {
            lastTone = tone;
            lp.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 500.f * std::pow (10.f, tone), 0.75f);
        }
        const float g = 1.f + drive * 9.f;
        const float mk = 1.f / std::sqrt (g);
        for (int i = 0; i < n; ++i)
        {
            float x = hp.processSample (d[i]);
            x = std::tanh (x * g) * mk * 1.3f;
            d[i] = lp.processSample (x);
        }
        float* chans[1] = { d };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
        comp.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

private:
    double sr = 44100;
    juce::dsp::IIR::Filter<float> hp, lp;
    juce::dsp::Compressor<float> comp;
    float lastTone = -1;
};

class DrumBus
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        verb.prepare ({ sampleRate, (juce::uint32) maxBlock, 2 });
        verb.reset();
        comp.prepare ({ sampleRate, (juce::uint32) maxBlock, 2 });
        comp.setThreshold (-14.f); comp.setRatio (3.f); comp.setAttack (10.f); comp.setRelease (120.f);
    }

    void process (juce::AudioBuffer<float>& b, int n, float punch, float room)
    {
        juce::dsp::AudioBlock<float> block (b.getArrayOfWritePointers(), 2, (size_t) n);
        const float g = 1.f + punch * 1.5f;
        for (int c = 0; c < 2; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i) d[i] = std::tanh (d[i] * g) / std::tanh (g) * 0.9f;
        }
        comp.process (juce::dsp::ProcessContextReplacing<float> (block));
        juce::dsp::Reverb::Parameters p;
        p.roomSize = 0.38f; p.damping = 0.55f; p.width = 0.9f;
        p.wetLevel = room * 0.38f; p.dryLevel = 1.f;
        verb.setParameters (p);
        verb.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

private:
    juce::dsp::Reverb verb;
    juce::dsp::Compressor<float> comp;
};
} // namespace rh
