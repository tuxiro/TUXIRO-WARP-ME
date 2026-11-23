#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace TimeStretch
{
    using Channels = std::vector<std::vector<float>>;

    // Re-times audio so that beats[startBeat], beats[startBeat+1], ... land on a
    // constant grid of targetBpm, starting at output sample 0 (pitch preserved, WSOLA).
    // Returns an empty result if cancelled or the input is unusable.
    Channels renderToGrid (const std::vector<const float*>& in, int64_t length, double sampleRate,
                           const std::vector<double>& beats, int startBeat, double targetBpm,
                           const std::function<bool()>& shouldStop,
                           const std::function<void (float)>& progress);
}
