#include "TimeStretch.h"

#include <algorithm>
#include <cmath>

namespace TimeStretch
{
namespace
{
void fetch (const std::vector<float>& src, int64_t start, int len, std::vector<float>& dst)
{
    dst.assign ((size_t) len, 0.f);
    const int64_t s0 = std::max<int64_t> (0, start);
    const int64_t s1 = std::min<int64_t> ((int64_t) src.size(), start + len);
    if (s1 > s0) std::copy (src.begin() + s0, src.begin() + s1, dst.begin() + (s0 - start));
}

int64_t floorDiv (int64_t a, int d) { return a >= 0 ? a / d : -((-a + d - 1) / d); }

double ncc (const float* a, const float* b, int n)
{
    double c = 0, e = 1e-9;
    for (int i = 0; i < n; ++i) { c += (double) a[i] * b[i]; e += (double) b[i] * b[i]; }
    return c / std::sqrt (e);
}
} // namespace

Channels renderToGrid (const std::vector<const float*>& in, int64_t length, double sr,
                       const std::vector<double>& beats, int startBeat, double targetBpm,
                       const std::function<bool()>& stop,
                       const std::function<void (float)>& progress)
{
    Channels out;
    const int nch = (int) in.size();
    const int nb = (int) beats.size() - startBeat;
    if (nch == 0 || nb < 2 || length < 4096 || targetBpm < 20.0) return out;

    const double outBeat = 60.0 / targetBpm * sr;
    std::vector<double> outT ((size_t) nb), inS ((size_t) nb);
    for (int j = 0; j < nb; ++j) { outT[(size_t) j] = j * outBeat; inS[(size_t) j] = beats[(size_t) (startBeat + j)] * sr; }
    const double lastOut = outT.back(), lastIn = inS.back();
    const int64_t outLen = (int64_t) (lastOut + std::max (0.0, (double) length - lastIn));
    if (outLen < 4096) return out;

    auto mapIn = [&] (double o) -> double
    {
        if (o >= lastOut) return lastIn + (o - lastOut);
        int j = (int) (std::upper_bound (outT.begin(), outT.end(), o) - outT.begin()) - 1;
        j = std::clamp (j, 0, nb - 2);
        const double u = (o - outT[(size_t) j]) / (outT[(size_t) j + 1] - outT[(size_t) j]);
        return inS[(size_t) j] + u * (inS[(size_t) j + 1] - inS[(size_t) j]);
    };

    const int D = 4;
    std::vector<float> mono ((size_t) length);
    for (int64_t i = 0; i < length; ++i)
    {
        float s = 0; for (int c = 0; c < nch; ++c) s += in[(size_t) c][i];
        mono[(size_t) i] = s / (float) nch;
    }
    const int64_t decLen = length / D;
    std::vector<float> dec ((size_t) decLen);
    for (int64_t i = 0; i < decLen; ++i)
        dec[(size_t) i] = (mono[(size_t) (D * i)] + mono[(size_t) (D * i + 1)] + mono[(size_t) (D * i + 2)] + mono[(size_t) (D * i + 3)]) / (float) D;

    const int N = sr <= 52000.0 ? 2048 : 4096;
    const int Hs = N / 2, tol = N / 4, Ld = Hs / D, tolD = tol / D;
    std::vector<float> win ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * i / N));

    out.assign ((size_t) nch, std::vector<float> ((size_t) outLen, 0.f));
    const int64_t nFrames = (outLen + Hs - 1) / Hs + 1;
    int64_t prevP = 0;
    std::vector<float> segT, segC;

    for (int64_t m = 0; m < nFrames; ++m)
    {
        if ((m & 127) == 0)
        {
            if (stop()) return {};
            progress ((float) m / (float) nFrames);
        }
        const int64_t outStart = (m - 1) * (int64_t) Hs;
        int64_t p = (int64_t) std::llround (mapIn ((double) outStart + N * 0.5)) - N / 2;

        if (m > 0)
        {
            const int64_t tmplStart = prevP + Hs;
            const int64_t tD = floorDiv (tmplStart, D), iD = floorDiv (p, D);
            fetch (dec, tD, Ld, segT);
            fetch (dec, iD - tolD, 2 * tolD + Ld, segC);
            double bestScore = ncc (segT.data(), segC.data() + tolD, Ld);
            int bestLag = 0;
            for (int lag = -tolD; lag <= tolD; ++lag)
            {
                if (lag == 0) continue;
                const double sc = ncc (segT.data(), segC.data() + tolD + lag, Ld);
                if (sc > bestScore) { bestScore = sc; bestLag = lag; }
            }
            p = (iD + bestLag) * D;

            fetch (mono, tmplStart, Hs, segT);
            fetch (mono, p - D, 2 * D + Hs, segC);
            double bs = ncc (segT.data(), segC.data() + D, Hs);
            int bo = 0;
            for (int off = -D; off <= D; ++off)
            {
                if (off == 0) continue;
                const double sc = ncc (segT.data(), segC.data() + D + off, Hs);
                if (sc > bs) { bs = sc; bo = off; }
            }
            p += bo;
        }

        for (int c = 0; c < nch; ++c)
        {
            const float* src = in[(size_t) c];
            float* dst = out[(size_t) c].data();
            for (int i = 0; i < N; ++i)
            {
                const int64_t idx = p + i, o = outStart + i;
                if (idx < 0 || idx >= length || o < 0 || o >= outLen) continue;
                dst[o] += src[idx] * win[(size_t) i];
            }
        }
        prevP = p;
    }
    progress (1.0f);
    return out;
}
}
