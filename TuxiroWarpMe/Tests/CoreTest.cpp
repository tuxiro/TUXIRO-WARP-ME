// Offline sanity test for the beat analyser and the grid renderer (no JUCE needed).
#include "../Source/BeatAnalyzer.h"
#include "../Source/TimeStretch.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

static const double kPi = 3.14159265358979323846;

static std::vector<float> synth (double sr, double dur, double bpm, double drift, double offset, std::vector<double>& truth)
{
    std::mt19937 rng (1);
    std::uniform_real_distribution<float> u (-1.f, 1.f);
    const int64_t n = (int64_t) (sr * dur);
    std::vector<float> x ((size_t) n, 0.f);
    truth.clear();
    for (double t = offset; t < dur - 0.6; t += 60.0 / (bpm * (1 + drift * std::sin (2 * kPi * t / 40.0))))
        truth.push_back (t);
    for (size_t k = 0; k < truth.size(); ++k)
    {
        const int64_t s = (int64_t) (truth[k] * sr);
        const double amp = (k % 4 == 0) ? 1.0 : 0.85;
        for (int i = 0; i < (int) (0.25 * sr) && s + i < n; ++i)
        {
            const double t = i / sr;
            x[(size_t) (s + i)] += (float) (amp * std::sin (2 * kPi * (45 * t + 90 * (1 - std::exp (-35 * t)) / 35)) * std::exp (-14 * t));
        }
        if (k % 4 == 0)
            for (int i = 0; i < (int) (0.4 * sr) && s + i < n; ++i)
                x[(size_t) (s + i)] += (float) (0.35 * std::sin (2 * kPi * 55 * i / sr) * std::exp (-4.0 * i / sr));
        if (k % 4 == 1 || k % 4 == 3)
            for (int i = 0; i < (int) (0.08 * sr) && s + i < n; ++i)
                x[(size_t) (s + i)] += 0.5f * u (rng) * (float) std::exp (-60.0 * i / sr);
        const double nextGap = (k + 1 < truth.size() ? truth[k + 1] - truth[k] : 0.4);
        const int64_t h = s + (int64_t) (0.5 * nextGap * sr);
        float prev = 0;
        for (int i = 0; i < (int) (0.03 * sr) && h + i < n; ++i)
        {
            const float v = u (rng); x[(size_t) (h + i)] += 0.2f * (v - prev) * (float) std::exp (-200.0 * i / sr); prev = v;
        }
    }
    float pk = 0; for (float v : x) pk = std::max (pk, std::abs (v));
    for (auto& v : x) v *= 0.8f / pk;
    return x;
}

static void report (const char* name, const BeatAnalysis& a, const std::vector<double>& truth)
{
    std::printf ("[%s] hasBeat=%d conf=%.2f bpm=%.3f straight=%d drift=%s beats=%zu downbeat=%d check=%zu\n",
                 name, a.hasBeat, a.confidence, a.bpm, a.straight, a.drift.c_str(), a.beats.size(), a.downbeat, a.checkBeats.size());
    double err = 0, mx = 0; int cnt = 0;
    for (double b : a.beats)
    {
        double best = 1e9; for (double t : truth) best = std::min (best, std::abs (t - b));
        if (best < 0.2) { err += best; mx = std::max (mx, best); ++cnt; }
    }
    std::printf ("    mean |err| = %.2f ms, max = %.2f ms over %d beats\n", cnt ? 1000 * err / cnt : -1.0, 1000 * mx, cnt);
    if (!a.beats.empty())
    {
        const double d = a.beats[(size_t) a.downbeat];
        size_t kk = 0; double best = 1e9;
        for (size_t k = 0; k < truth.size(); ++k) if (std::abs (truth[k] - d) < best) { best = std::abs (truth[k] - d); kk = k; }
        std::printf ("    the 1 at %.3f s -> true beat index %zu (mod 4 = %zu, want 0)\n", d, kk, kk % 4);
    }
}

int main()
{
    const double sr = 44100.0;
    auto stop = [] { return false; };
    auto prog = [] (float) {};
    std::vector<double> t1, t2;

    auto x1 = synth (sr, 150, 128, 0.0, 0.37, t1);
    auto a1 = BeatAnalyzer::analyse (x1, sr, stop, prog);
    report ("house 128 steady", a1, t1);

    auto x2 = synth (sr, 150, 118, 0.03, 0.81, t2);
    auto a2 = BeatAnalyzer::analyse (x2, sr, stop, prog);
    report ("funk 118 drifting +-3%", a2, t2);

    // render the drifting track to a 118 grid and re-analyse it
    std::vector<const float*> ch { x2.data() };
    auto out = TimeStretch::renderToGrid (ch, (int64_t) x2.size(), sr, a2.beats, a2.downbeat, 118.0, stop, prog);
    std::printf ("rendered %zu samples (%.1f s)\n", out.empty() ? 0 : out[0].size(), out.empty() ? 0.0 : out[0].size() / sr);
    if (!out.empty())
    {
        auto a3 = BeatAnalyzer::analyse (out[0], sr, stop, prog);
        std::vector<double> grid; for (int k = 0; k < 400; ++k) grid.push_back (k * 60.0 / 118.0);
        report ("re-analysed grid render", a3, grid);
    }
    return 0;
}
