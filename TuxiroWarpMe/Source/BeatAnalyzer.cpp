#include "BeatAnalyzer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace
{
constexpr double kPi  = 3.14159265358979323846;
constexpr int    kFFT = 1024;
constexpr int    kHop = 256;

class Fft
{
public:
    Fft()
    {
        tw.resize (kFFT / 2);
        for (int i = 0; i < kFFT / 2; ++i)
            tw[(size_t) i] = std::polar (1.0f, (float) (-2.0 * kPi * i / kFFT));
        rev.resize (kFFT);
        for (int i = 0; i < kFFT; ++i)
        {
            int r = 0;
            for (int b = 0; b < 10; ++b)
                if (i & (1 << b)) r |= 1 << (9 - b);
            rev[(size_t) i] = r;
        }
        buf.resize (kFFT);
    }

    void magnitudes (const float* in, float* mag)
    {
        for (int i = 0; i < kFFT; ++i)
            buf[(size_t) rev[(size_t) i]] = std::complex<float> (in[i], 0.0f);

        for (int len = 2; len <= kFFT; len <<= 1)
        {
            const int half = len / 2, step = kFFT / len;
            for (int i = 0; i < kFFT; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const auto u = buf[(size_t) (i + j)];
                    const auto v = buf[(size_t) (i + j + half)] * tw[(size_t) (j * step)];
                    buf[(size_t) (i + j)]        = u + v;
                    buf[(size_t) (i + j + half)] = u - v;
                }
        }
        for (int k = 0; k <= kFFT / 2; ++k)
            mag[k] = std::abs (buf[(size_t) k]);
    }

private:
    std::vector<std::complex<float>> tw, buf;
    std::vector<int> rev;
};

void normaliseStd (std::vector<float>& v)
{
    double s = 0, s2 = 0;
    for (float a : v) { s += a; s2 += (double) a * a; }
    const double m  = s / (double) v.size();
    const double sd = std::sqrt (std::max (1e-12, s2 / (double) v.size() - m * m));
    for (auto& a : v) a = (float) (a / sd);
}

// Multi-band spectral-flux onset strength, one value per hop.
std::vector<float> onsetEnvelope (const std::vector<float>& x, double sr,
                                  const std::function<bool()>& stop,
                                  const std::function<void (float)>& progress)
{
    const int64_t n = (int64_t) x.size();
    if (n < (int64_t) kFFT * 8) return {};
    const int64_t frames = (n - kFFT) / kHop + 1;

    Fft fft;
    std::vector<float> win ((size_t) kFFT);
    for (int i = 0; i < kFFT; ++i)
        win[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / kFFT));

    const double binHz = sr / kFFT;
    const int bins = kFFT / 2 + 1;
    const int b200 = std::max (2, (int) std::lround (200.0 / binHz));
    const int b2k  = std::max (b200 + 1, (int) std::lround (2000.0 / binHz));
    const int bTop = std::min (bins - 1, (int) std::lround (10000.0 / binHz));

    std::vector<float> low ((size_t) frames, 0.f), mid ((size_t) frames, 0.f), high ((size_t) frames, 0.f);
    std::vector<float> frame ((size_t) kFFT), mag ((size_t) bins), prev ((size_t) bins, 0.f);

    for (int64_t f = 0; f < frames; ++f)
    {
        if ((f & 2047) == 0)
        {
            if (stop()) return {};
            progress (0.6f * (float) f / (float) frames);
        }
        const float* src = x.data() + f * kHop;
        for (int i = 0; i < kFFT; ++i) frame[(size_t) i] = src[i] * win[(size_t) i];
        fft.magnitudes (frame.data(), mag.data());

        float fl = 0, fm = 0, fh = 0;
        for (int b = 1; b <= bTop; ++b)
        {
            const float m = std::log1p (mag[(size_t) b] * 0.1f);
            const float d = m - prev[(size_t) b];
            prev[(size_t) b] = m;
            if (d > 0.f)
            {
                if (b < b200) fl += d; else if (b < b2k) fm += d; else fh += d;
            }
        }
        low[(size_t) f] = fl; mid[(size_t) f] = fm; high[(size_t) f] = fh;
    }

    normaliseStd (low); normaliseStd (mid); normaliseStd (high);
    std::vector<float> env ((size_t) frames);
    for (size_t i = 0; i < env.size(); ++i)
        env[i] = 1.0f * low[i] + 0.7f * mid[i] + 0.5f * high[i];

    // remove local mean, half-wave rectify
    const double fps = sr / kHop;
    const int half = std::max (1, (int) std::lround (0.2 * fps));
    std::vector<double> cs (env.size() + 1, 0.0);
    for (size_t i = 0; i < env.size(); ++i) cs[i + 1] = cs[i] + env[i];
    std::vector<float> out (env.size());
    for (int i = 0; i < (int) env.size(); ++i)
    {
        const int lo = std::max (0, i - half), hi = std::min ((int) env.size() - 1, i + half);
        const double mean = (cs[(size_t) hi + 1] - cs[(size_t) lo]) / (double) (hi - lo + 1);
        out[(size_t) i] = (float) std::max (0.0, env[(size_t) i] - mean);
    }
    for (int i = 0; i < 3 && i < (int) out.size(); ++i) out[(size_t) i] = 0.f;
    normaliseStd (out);
    return out;
}

struct Tempo { double period = 0; float significance = 0; };

Tempo estimatePeriod (const std::vector<float>& env, double fps)
{
    const int N = (int) env.size();
    const int minLag = std::max (3, (int) std::floor (fps * 60.0 / 190.0));
    const int maxLag = (int) std::ceil (fps * 60.0 / 60.0);
    const int lagTop = 2 * maxLag;
    if (N < lagTop * 4) return {};

    std::vector<double> ac ((size_t) lagTop + 2, 0.0);
    for (int lag = minLag; lag <= lagTop; ++lag)
    {
        double s = 0;
        for (int i = 0; i + lag < N; ++i) s += (double) env[(size_t) i] * env[(size_t) (i + lag)];
        ac[(size_t) lag] = s / (double) (N - lag);
    }

    std::vector<double> score ((size_t) maxLag + 2, 0.0);
    double sum = 0; int cnt = 0, best = -1; double bestV = -1e30;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        const double bpm = 60.0 * fps / lag;
        const double o = std::log2 (bpm / 122.0) / 0.85;
        const double prior = std::exp (-0.5 * o * o);
        const double v = (ac[(size_t) lag] + 0.5 * ac[(size_t) (2 * lag)]) * prior;
        score[(size_t) lag] = v; sum += v; ++cnt;
        if (v > bestV) { bestV = v; best = lag; }
    }
    if (best < 0 || bestV <= 0) return {};

    double p = best;
    if (best > minLag && best < maxLag)
    {
        const double a = score[(size_t) best - 1], b = score[(size_t) best], c = score[(size_t) best + 1];
        const double d = a - 2 * b + c;
        if (d < 0) p += std::clamp (0.5 * (a - c) / d, -1.0, 1.0);
    }
    Tempo t; t.period = p; t.significance = (float) (bestV / (sum / cnt));
    return t;
}

// Dynamic-programming beat tracker (Ellis 2007), tolerant to tempo wander.
std::vector<int> trackBeats (const std::vector<float>& env, double period)
{
    const int N = (int) env.size();
    const int pmin = std::max (1, (int) std::lround (period * 0.5));
    const int pmax = (int) std::lround (period * 2.0);
    constexpr float alpha = 120.f;
    std::vector<float> pen ((size_t) pmax + 1, 0.f);
    for (int d = pmin; d <= pmax; ++d)
    {
        const double l = std::log ((double) d / period);
        pen[(size_t) d] = (float) (alpha * l * l);
    }
    std::vector<float> cum ((size_t) N, 0.f);
    std::vector<int> back ((size_t) N, -1);
    for (int t = 0; t < N; ++t)
    {
        float best = -1e30f; int bi = -1;
        const int lo = std::max (0, t - pmax), hi = t - pmin;
        for (int p = lo; p <= hi; ++p)
        {
            const float v = cum[(size_t) p] - pen[(size_t) (t - p)];
            if (v > best) { best = v; bi = p; }
        }
        if (bi >= 0 && best > 0.f) { cum[(size_t) t] = env[(size_t) t] + best; back[(size_t) t] = bi; }
        else                       { cum[(size_t) t] = env[(size_t) t];        back[(size_t) t] = -1; }
    }
    int t = std::max (0, N - pmax);
    for (int i = t; i < N; ++i) if (cum[(size_t) i] > cum[(size_t) t]) t = i;
    std::vector<int> frames;
    while (t >= 0) { frames.push_back (t); t = back[(size_t) t]; }
    std::reverse (frames.begin(), frames.end());
    return frames;
}

// Sample-accurate attack position near an estimate (largest short-term energy rise).
double refine (const std::vector<float>& x, double c, double sr)
{
    const int64_t n = (int64_t) x.size();
    const int W = (int) std::lround (0.045 * sr);
    const int R = std::max (8, (int) std::lround (0.004 * sr));
    const int64_t centre = (int64_t) std::llround (c);
    const int64_t a = std::max<int64_t> (R, centre - W);
    const int64_t b = std::min<int64_t> (n - R - 1, centre + W);
    if (b <= a) return c;
    const int64_t s0 = a - R, s1 = b + R;
    const int len = (int) (s1 - s0 + 1);
    std::vector<double> ps ((size_t) len + 1, 0.0);
    for (int i = 0; i < len; ++i)
    {
        const double v = x[(size_t) (s0 + i)];
        ps[(size_t) i + 1] = ps[(size_t) i] + v * v;
    }
    auto E = [&] (int64_t from, int64_t to) { return ps[(size_t) (to - s0)] - ps[(size_t) (from - s0)]; };
    double bestScore = 0; int64_t bestN = centre;
    for (int64_t k = a; k <= b; ++k)
    {
        const double rise = E (k, k + R) - E (k - R, k);
        const double wgt = 1.0 - 0.3 * std::abs ((double) (k - centre)) / (double) W;
        const double sc = rise * wgt;
        if (sc > bestScore) { bestScore = sc; bestN = k; }
    }
    return (double) bestN;
}

struct Fit { double t0 = 0, T = 0, within10 = 0; bool ok = false; };

Fit fitGrid (const std::vector<double>& t, const std::vector<int>& n)
{
    Fit f;
    const size_t M = t.size();
    if (M < 8) return f;
    std::vector<char> use (M, 1);
    const double thr[] = { 1e9, 0.030, 0.020, 0.015 };
    for (int it = 0; it < 4; ++it)
    {
        double sk = 0, st = 0, skk = 0, skt = 0; int m = 0;
        for (size_t i = 0; i < M; ++i)
            if (use[i]) { const double k = n[i]; sk += k; st += t[i]; skk += k * k; skt += k * t[i]; ++m; }
        if (m < 8) return f;
        const double den = m * skk - sk * sk;
        if (std::abs (den) < 1e-9) return f;
        f.T = (m * skt - sk * st) / den;
        f.t0 = (st - f.T * sk) / m;
        if (it < 3)
            for (size_t i = 0; i < M; ++i)
                use[i] = std::abs (t[i] - (f.t0 + f.T * n[i])) < thr[it + 1];
    }
    int w = 0;
    for (size_t i = 0; i < M; ++i)
        if (std::abs (t[i] - (f.t0 + f.T * n[i])) < 0.010) ++w;
    f.within10 = (double) w / (double) M;
    f.ok = true;
    return f;
}

double medianOf (std::vector<double> v)
{
    if (v.empty()) return 0.0;
    std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
    return v[v.size() / 2];
}


// Low-band and broadband energy just after each beat (kick + bass weight the "1").
void beatEnergies (const std::vector<float>& x, double sr, const std::vector<double>& beats,
                   std::vector<double>& low, std::vector<double>& broad)
{
    const int64_t n = (int64_t) x.size();
    const double a = std::exp (-2.0 * kPi * 150.0 / sr);
    const int len = (int) (0.09 * sr), warm = (int) (0.1 * sr), pre = (int) (0.005 * sr);
    low.assign (beats.size(), 0.0); broad.assign (beats.size(), 0.0);
    for (size_t i = 0; i < beats.size(); ++i)
    {
        const int64_t s0 = (int64_t) std::llround (beats[i] * sr) - pre;
        const int64_t from = std::max<int64_t> (0, s0 - warm), to = std::min<int64_t> (n, s0 + len);
        double y = 0, el = 0, eb = 0;
        for (int64_t j = from; j < to; ++j)
        {
            const double v = x[(size_t) j];
            y = a * y + (1.0 - a) * v;
            if (j >= s0) { el += y * y; eb += v * v; }
        }
        low[i] = el; broad[i] = eb;
    }
}
} // namespace

namespace BeatAnalyzer
{
BeatAnalysis analyse (const std::vector<float>& mono, double sr,
                      const std::function<bool()>& stop,
                      const std::function<void (float)>& progress)
{
    BeatAnalysis res;
    const double dur = (double) mono.size() / sr;
    auto env = onsetEnvelope (mono, sr, stop, progress);
    if (env.empty() || stop()) return res;

    const double fps = sr / kHop;
    const auto tempo = estimatePeriod (env, fps);
    progress (0.65f);
    if (tempo.period <= 0) return res;

    const auto frames = trackBeats (env, tempo.period);
    progress (0.75f);
    if (frames.size() < 8 || stop()) return res;

    const double Tsec = tempo.period / fps;
    std::vector<double> raw;
    for (int f : frames)
        raw.push_back (refine (mono, (double) f * kHop + 0.65 * kFFT, sr) / sr);
    std::sort (raw.begin(), raw.end());
    {
        std::vector<double> clean { raw[0] };
        for (size_t i = 1; i < raw.size(); ++i)
            if (raw[i] - clean.back() > 0.45 * Tsec) clean.push_back (raw[i]);
        raw.swap (clean);
    }
    if (raw.size() < 8) return res;
    progress (0.85f);

    std::vector<int> num (raw.size(), 0);
    for (size_t i = 1; i < raw.size(); ++i)
        num[i] = num[i - 1] + std::max (1, (int) std::lround ((raw[i] - raw[i - 1]) / Tsec));

    const auto fit = fitGrid (raw, num);
    const bool straight = fit.ok && fit.within10 >= 0.93 && fit.T > 0
                          && std::abs (fit.T / Tsec - 1.0) < 0.1;

    std::vector<double> beats, dev;
    double medInterval = 0;
    if (straight)
    {
        const double T = fit.T;
        const int kMin = (int) std::ceil ((0.0 - fit.t0) / T - 1e-9);
        const int kMax = (int) std::floor ((dur - 0.05 - fit.t0) / T);
        for (int k = kMin; k <= kMax; ++k) beats.push_back (fit.t0 + k * T);
        dev.assign (beats.size(), 0.0);
        for (size_t i = 0; i < raw.size(); ++i)
        {
            const int j = num[i] - kMin;
            if (j >= 0 && j < (int) beats.size()) dev[(size_t) j] = std::abs (raw[i] - beats[(size_t) j]);
        }
        res.bpm = 60.0 / T; res.drift = "steady"; medInterval = T;
    }
    else
    {
        std::vector<double> b;
        for (size_t i = 0; i < raw.size(); ++i)
        {
            if (i > 0)
            {
                const double gap = raw[i] - raw[i - 1];
                const int m = (int) std::lround (gap / Tsec);
                if (m >= 2 && m <= 4)
                    for (int q = 1; q < m; ++q) b.push_back (raw[i - 1] + gap * q / m);
            }
            b.push_back (raw[i]);
        }
        std::vector<double> s = b;
        for (size_t i = 1; i + 1 < b.size(); ++i) s[i] = 0.25 * b[i - 1] + 0.5 * b[i] + 0.25 * b[i + 1];

        std::vector<double> iv;
        for (size_t i = 1; i < s.size(); ++i) iv.push_back (s[i] - s[i - 1]);
        double mean = 0, var = 0;
        for (double d : iv) mean += d;
        mean /= (double) iv.size();
        for (double d : iv) var += (d - mean) * (d - mean);
        const double cv = std::sqrt (var / (double) iv.size()) / mean;
        res.drift = cv < 0.015 ? "slight drift" : (cv < 0.04 ? "drifting" : "wandering");
        res.bpm = 60.0 * (double) (s.size() - 1) / (s.back() - s.front());
        medInterval = medianOf (iv);

        const size_t k = std::min<size_t> (8, iv.size());
        const double Tf = medianOf (std::vector<double> (iv.begin(), iv.begin() + (long) k));
        const double Tl = medianOf (std::vector<double> (iv.end() - (long) k, iv.end()));
        std::vector<double> head, tail;
        for (double t = s.front() - Tf; t >= 0.0 && Tf > 0.05; t -= Tf) head.push_back (t);
        std::reverse (head.begin(), head.end());
        for (double t = s.back() + Tl; t < dur - 0.05 && Tl > 0.05; t += Tl) tail.push_back (t);
        beats = head;
        beats.insert (beats.end(), s.begin(), s.end());
        beats.insert (beats.end(), tail.begin(), tail.end());
        dev.assign (beats.size(), 0.0);
    }
    if (beats.size() < 8) return res;

    // "The 1": the beat phase whose kick/bass energy is most often the strongest.
    std::vector<double> low, broad;
    beatEnergies (mono, sr, beats, low, broad);
    auto localMean = [&] (const std::vector<double>& v, int i)
    {
        const int lo = std::max (0, i - 8), hi = std::min ((int) v.size() - 1, i + 8);
        double s = 0; for (int j = lo; j <= hi; ++j) s += v[(size_t) j];
        return s / (double) (hi - lo + 1) + 1e-12;
    };
    std::array<double, 4> score {}; std::array<int, 4> cnt {};
    for (int i = 0; i < (int) beats.size(); ++i)
    {
        score[(size_t) (i % 4)] += low[(size_t) i] / localMean (low, i) + broad[(size_t) i] / localMean (broad, i);
        ++cnt[(size_t) (i % 4)];
    }
    int bestPhase = 0; double bestScore = -1;
    for (int p = 0; p < 4; ++p)
        if (cnt[(size_t) p] > 0 && score[(size_t) p] / cnt[(size_t) p] > bestScore)
        { bestScore = score[(size_t) p] / cnt[(size_t) p]; bestPhase = p; }
    res.downbeat = bestPhase;

    // Bars worth a listen.
    for (int i = res.downbeat; i + 3 < (int) beats.size(); i += 4)
    {
        bool flag = false;
        if (straight)
        {
            int bad = 0;
            for (int q = 0; q < 4; ++q) if (dev[(size_t) (i + q)] > 0.025) ++bad;
            flag = bad >= 2;
        }
        else
        {
            for (int q = 0; q < 3; ++q)
                if (std::abs ((beats[(size_t) (i + q + 1)] - beats[(size_t) (i + q)]) / medInterval - 1.0) > 0.09) flag = true;
        }
        if (flag) res.checkBeats.push_back (i);
    }

    res.beats = std::move (beats);
    res.straight = straight;
    res.confidence = tempo.significance;
    res.hasBeat = tempo.significance > 1.2f && res.beats.size() >= 16;
    progress (1.0f);
    return res;
}
}
