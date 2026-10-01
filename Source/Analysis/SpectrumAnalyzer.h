#pragma once
// POLOOYX RELEASE ENGINE — real-time spectrum.
// The audio thread only pushes samples into a lock-free FIFO; the FFT, smoothing, peak hold and
// freeze all happen on the UI thread, so the analyser never costs the audio thread more than a copy.

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../DSP/DspCommon.h"

namespace prl
{
// single-producer / single-consumer sample FIFO
class SampleFifo
{
public:
    explicit SampleFifo (int size = 1 << 16) : fifo (size), data ((size_t) size, 0.0f) {}
    void push (const float* x, int n) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (n, s1, n1, s2, n2);
        if (n1 > 0) std::copy (x, x + n1, data.begin() + s1);
        if (n2 > 0) std::copy (x + n1, x + n1 + n2, data.begin());
        fifo.finishedWrite (n1 + n2);   // drops samples if the UI is not reading (never blocks)
    }
    int pull (float* dst, int maxN) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (maxN, s1, n1, s2, n2);
        if (n1 > 0) std::copy (data.begin() + s1, data.begin() + s1 + n1, dst);
        if (n2 > 0) std::copy (data.begin() + s2, data.begin() + s2 + n2, dst + n1);
        fifo.finishedRead (n1 + n2);
        return n1 + n2;
    }
private:
    juce::AbstractFifo fifo;
    std::vector<float> data;
};

class SpectrumAnalyzer
{
public:
    static constexpr int kPoints = 240;   // log-spaced display points 20 Hz .. 20 kHz

    void setFftOrder (int order)
    {
        if (order == fftOrder && fft) return;
        fftOrder = order; N = 1 << order;
        fft = std::make_unique<juce::dsp::FFT> (order);
        win.assign ((size_t) N, 0.0f);
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * kPi * (float) i / (float) N);
        ring.assign ((size_t) N, 0.0f); work.assign ((size_t) N * 2, 0.0f);
        ringPos = 0; sinceLast = 0;
    }
    void setSampleRate (double s) { sr = s; }
    void setSmoothing (float s) { smoothing = s; }      // 0..0.95 (display averaging)
    void setFrozen (bool f) { frozen = f; }
    void setPeakHold (bool h) { hold = h; if (! h) peak.fill (-200.0f); }
    void resetPeaks() { peak.fill (-200.0f); }

    // pull new samples; returns true when a new frame was computed
    bool update (SampleFifo& src)
    {
        float tmp[4096];
        bool computed = false;
        int got;
        while ((got = src.pull (tmp, 4096)) > 0)
        {
            for (int i = 0; i < got; ++i) { ring[(size_t) ringPos] = tmp[i]; ringPos = (ringPos + 1) % N; }
            sinceLast += got;
        }
        if (frozen) { sinceLast = 0; return false; }
        if (sinceLast >= N / 4)   // 75 % overlap
        {
            sinceLast = 0;
            std::fill (work.begin(), work.end(), 0.0f);
            for (int i = 0; i < N; ++i) work[(size_t) i] = ring[(size_t) ((ringPos + i) % N)] * win[(size_t) i];
            fft->performFrequencyOnlyForwardTransform (work.data(), true);
            const float norm = 2.0f / (0.5f * (float) N);   // Hann coherent gain → 0 dB for a full-scale sine
            for (int p = 0; p < kPoints; ++p)
            {
                const double f = freqAt (p);
                const double flo = f * std::pow (2.0, -1.0 / 48.0), fhi = f * std::pow (2.0, 1.0 / 48.0);   // ±half the point spacing
                const int k0 = std::max (1, (int) std::ceil (flo * N / sr)), k1 = std::min (N / 2, (int) std::floor (fhi * N / sr));
                float m = 0.0f;
                if (k1 >= k0) for (int k = k0; k <= k1; ++k) m = std::max (m, work[(size_t) k]);
                else
                {   // fewer than one bin per display point (low frequencies): interpolate between neighbouring bins
                    const double kb = f * N / sr;
                    const int ka = juce::jlimit (1, N / 2 - 1, (int) std::floor (kb));
                    const float t = (float) (kb - ka);
                    m = work[(size_t) ka] + t * (work[(size_t) ka + 1] - work[(size_t) ka]);
                }
                const float db = gainToDb (m * norm);
                cur[(size_t) p] = smoothing * cur[(size_t) p] + (1.0f - smoothing) * db;
                if (hold) peak[(size_t) p] = std::max (peak[(size_t) p] - 0.05f, cur[(size_t) p]);
            }
            computed = true;
        }
        return computed;
    }

    static double freqAt (int p) { return 20.0 * std::pow (1000.0, (double) p / (kPoints - 1)); }
    const std::array<float, kPoints>& values() const noexcept { return cur; }
    const std::array<float, kPoints>& peaks() const noexcept { return peak; }

    SpectrumAnalyzer() { cur.fill (-200.0f); peak.fill (-200.0f); setFftOrder (12); }

private:
    std::unique_ptr<juce::dsp::FFT> fft;
    int fftOrder = 0, N = 4096, ringPos = 0, sinceLast = 0;
    double sr = 48000;
    float smoothing = 0.7f;
    bool frozen = false, hold = false;
    std::vector<float> win, ring, work;
    std::array<float, kPoints> cur {}, peak {};
};
} // namespace prl
