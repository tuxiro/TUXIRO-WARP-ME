#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct BeatAnalysis
{
    std::vector<double> beats;        // beat positions in seconds (whole track)
    std::vector<int>    checkBeats;   // beat indices (bar starts) worth a listen
    int    downbeat   = 0;            // phase of "the 1": index 0..3 into beats
    double bpm        = 0.0;
    float  confidence = 0.f;
    bool   hasBeat    = false;
    bool   straight   = false;        // one constant-tempo grid fits the track
    std::string drift = "-";
};

namespace BeatAnalyzer
{
    BeatAnalysis analyse (const std::vector<float>& mono, double sampleRate,
                          const std::function<bool()>& shouldStop,
                          const std::function<void (float)>& progress);
}
