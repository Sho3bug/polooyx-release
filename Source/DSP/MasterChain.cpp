#include "MasterChain.h"

namespace prl
{
void MasterChain::OsSection::prepare (int maxBlock)
{
    for (int k = 0; k < 3; ++k)
    {
        os[(size_t) k] = std::make_unique<juce::dsp::Oversampling<float>> (2, (size_t) (k + 1),
                             juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        os[(size_t) k]->initProcessing ((size_t) maxBlock);
        lat[(size_t) k] = (int) std::lround (os[(size_t) k]->getLatencyInSamples());
    }
    maxLat = *std::max_element (lat.begin(), lat.end());
    for (auto& p : pad) p.prepare (maxLat + maxBlock + 8);
    for (auto& p : bypass) p.prepare (maxLat + maxBlock + 8);
    curFactor = 0;
}

void MasterChain::OsSection::reset()
{
    for (auto& o : os) if (o) o->reset();
    for (auto& p : pad) p.clear();
    for (auto& p : bypass) p.clear();
    wasActive = false;
}

void MasterChain::prepare (double sampleRate)
{
    sr = sampleRate;
    gainStage.prepare (sr);
    eq.prepare (sr);
    deq.prepare (sr);
    mb.prepare (sr);
    glue.prepare (sr);
    punch.prepare (sr);
    stereo.prepare (sr);
    character.prepare (sr);
    limiter.prepare (sr, 4);
    osColor.prepare (kMaxBlock);
    osClip.prepare (kMaxBlock);
    sat.prepare (sr * 2.0);
    satFactor = 2;
    latency = osColor.maxLat + osClip.maxLat + limiter.getLatency();
    for (auto& d : dryDelay) d.prepare (latency + kMaxBlock + 8);
    reset();
}

void MasterChain::reset()
{
    gainStage.reset(); eq.reset(); deq.reset(); mb.reset(); glue.reset(); punch.reset();
    sat.reset(); stereo.reset(); character.reset(); clipper.reset(); limiter.reset();
    osColor.reset(); osClip.reset();
    for (auto& d : dryDelay) d.clear();
    outGain.snap (1.0f); dryWet.snap (1.0f);
    tel = {};
}

template <typename Fn>
void MasterChain::runOversampled (OsSection& sec, float* L, float* R, int n, int factor, bool active, Fn&& fn) noexcept
{
    // exact, latency-matched copy of the input: used whenever the section's module is idle, so an
    // idle module is bit-transparent (the oversampling filters are not in the signal path then)
    float byL[kMaxBlock], byR[kMaxBlock];
    for (int i = 0; i < n; ++i)
    {
        sec.bypass[0].push (L[i]); sec.bypass[1].push (R[i]);
        byL[i] = sec.bypass[0].readInt (sec.maxLat); byR[i] = sec.bypass[1].readInt (sec.maxLat);
    }
    const bool run = active || sec.wasActive;   // finish the module's fade-out before switching paths
    sec.wasActive = active;

    const int k = factor == 2 ? 0 : factor == 4 ? 1 : factor == 8 ? 2 : -1;
    if (k + 1 != sec.curFactor && k >= 0) sec.os[(size_t) k]->reset();   // factor changed: start that path clean
    sec.curFactor = k + 1;
    if (k < 0)
    {
        float* ch[2] = { L, R };
        if (run) fn (ch, n);
        for (int i = 0; i < n; ++i)
        {
            sec.pad[0].push (L[i]); sec.pad[1].push (R[i]);
            L[i] = sec.pad[0].readInt (sec.maxLat); R[i] = sec.pad[1].readInt (sec.maxLat);
        }
    }
    else
    {
        // the oversampling filters always run so re-engaging the module is seamless
        float* io[2] = { L, R };
        juce::dsp::AudioBlock<float> blk (io, 2, (size_t) n);
        auto up = sec.os[(size_t) k]->processSamplesUp (blk);
        float* upPtr[2] = { up.getChannelPointer (0), up.getChannelPointer (1) };
        if (run) fn (upPtr, (int) up.getNumSamples());
        sec.os[(size_t) k]->processSamplesDown (blk);
        const int padLen = sec.maxLat - sec.lat[(size_t) k];
        if (padLen > 0)
            for (int i = 0; i < n; ++i)
            {
                sec.pad[0].push (L[i]); sec.pad[1].push (R[i]);
                L[i] = sec.pad[0].readInt (padLen); R[i] = sec.pad[1].readInt (padLen);
            }
    }
    if (! run)
        for (int i = 0; i < n; ++i) { L[i] = byL[i]; R[i] = byR[i]; }
}

void MasterChain::process (float* L, float* R, int n, const ChainSettings& s) noexcept
{
    // dry copy (pre everything) for DRY/WET, delayed by the full chain latency
    float dryL[kMaxBlock], dryR[kMaxBlock];
    for (int i = 0; i < n; ++i)
    {
        dryDelay[0].push (L[i]); dryDelay[1].push (R[i]);
        dryL[i] = dryDelay[0].readInt (latency); dryR[i] = dryDelay[1].readInt (latency);
    }

    gainStage.process (L, R, n, s.gain);
    eq.process (L, R, n, s.eq);
    deq.process (L, R, n, s.deq);
    mb.process (L, R, n, s.mb);
    glue.process (L, R, n, s.glue);
    punch.process (L, R, n, s.punch);

    const int f = s.osFactor;
    if (f != satFactor) { sat.prepare (sr * f); satFactor = f; }
    runOversampled (osColor, L, R, n, f, s.color.amount * s.color.mix > 0.0f || std::abs (s.color.outDb * s.color.amount) > 0.001f, [this, &s] (float* const* ch, int m) { sat.process (ch, m, s.color); });

    stereo.process (L, R, n, s.stereo);
    character.process (L, R, n, s.character);

    runOversampled (osClip, L, R, n, f, s.clip.amount * s.clip.mix > 0.0f, [this, &s] (float* const* ch, int m) { clipper.process (ch, m, s.clip); });

    limiter.process (L, R, n, s.lim);

    outGain.set (dbToGain (s.outGainDb)); dryWet.set (s.dryWet);
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.at (i, n), w = dryWet.at (i, n);
        L[i] = dryL[i] + w * (L[i] * g - dryL[i]);
        R[i] = dryR[i] + w * (R[i] * g - dryR[i]);
    }

    tel.glueGr = glue.grDb; tel.clipDb = clipper.clipDb; tel.limGr = limiter.grDb; tel.punchDb = punch.lastGainDb;
    tel.deqGr = deq.grOut; tel.mbGr = mb.grOut; tel.dcOffset = gainStage.dcOffset(); tel.osFactor = f;
}
} // namespace prl
