#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TimeStretch.h"

TuxiroWarpMeProcessor::TuxiroWarpMeProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formats.registerBasicFormats();
    exportFolder = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("TUXIRO WARP ME");
    statusText = "Drop a track or press OPEN";
}

TuxiroWarpMeProcessor::~TuxiroWarpMeProcessor()
{
    const juce::ScopedLock sl (workerLock);
    if (loadWorker)   loadWorker->stopThread (30000);
    if (exportWorker) exportWorker->stopThread (30000);
}

void TuxiroWarpMeProcessor::prepareToPlay (double sr, int) { hostRate = sr; rtNextBeat = -1; }

bool TuxiroWarpMeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return out == l.getMainInputChannelSet();
}

// ------------------------------------------------------------------ audio
void TuxiroWarpMeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (wrapperType == wrapperType_Standalone) buffer.clear();   // no mic feedback in the standalone app

    {
        juce::SpinLock::ScopedTryLockType l (dataLock);
        if (l.isLocked() && rtGeneration != generation.load())
        {
            rtTrack = track; rtAnalysis = analysis; rtGeneration = generation.load(); rtNextBeat = -1;
        }
    }

    const double seek = seekTarget.exchange (-1.0);
    if (seek >= 0.0 && rtTrack) { rtPos = seek * rtTrack->sampleRate; rtNextBeat = -1; }
    if (!rtTrack || !playing.load()) return;

    const auto& audio = rtTrack->audio;
    const int64_t len = audio.getNumSamples();
    const double fsr = rtTrack->sampleRate, step = fsr / hostRate;
    const float* L = audio.getReadPointer (0);
    const float* R = audio.getNumChannels() > 1 ? audio.getReadPointer (1) : L;
    const std::vector<double>* beats = rtAnalysis ? &rtAnalysis->beats : nullptr;
    const bool metro = metronomeOn.load() && beats != nullptr && rtAnalysis->hasBeat;
    const float level = metronomeLevel.load();

    if (metro && rtNextBeat < 0)
        rtNextBeat = (int) (std::lower_bound (beats->begin(), beats->end(), rtPos / fsr) - beats->begin());

    const int numCh = buffer.getNumChannels();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const int64_t ip = (int64_t) rtPos;
        if (ip + 1 >= len) { playing = false; rtPos = 0.0; rtNextBeat = -1; posSeconds = 0.0; return; }
        const float f = (float) (rtPos - (double) ip);
        float l = L[ip] * (1.f - f) + L[ip + 1] * f;
        float r = R[ip] * (1.f - f) + R[ip + 1] * f;

        if (metro)
        {
            const double tSec = rtPos / fsr;
            while (rtNextBeat < (int) beats->size() && (*beats)[(size_t) rtNextBeat] <= tSec)
            {
                const int rel = rtNextBeat - rtAnalysis->downbeat;
                const bool accent = (((rel % 4) + 4) % 4) == 0;
                clickFreq = accent ? 1600.0 : 1000.0; clickAmp = accent ? 1.0 : 0.7; clickT = 0.0;
                ++rtNextBeat;
            }
            if (clickT < 0.04)
            {
                const float v = (float) (std::sin (juce::MathConstants<double>::twoPi * clickFreq * clickT)
                                         * std::exp (-clickT * 90.0) * clickAmp) * level * 0.7f;
                l += v; r += v; clickT += 1.0 / hostRate;
            }
        }
        buffer.addSample (0, i, l);
        if (numCh > 1) buffer.addSample (1, i, r);
        rtPos += step;
    }
    posSeconds = rtPos / fsr;
}

// ------------------------------------------------------------------ state
std::shared_ptr<const TrackData> TuxiroWarpMeProcessor::getTrack() const
{ const juce::SpinLock::ScopedLockType l (dataLock); return track; }

std::shared_ptr<const BeatAnalysis> TuxiroWarpMeProcessor::getAnalysis() const
{ const juce::SpinLock::ScopedLockType l (dataLock); return analysis; }

juce::String TuxiroWarpMeProcessor::getStatus() const { const juce::ScopedLock l (miscLock); return statusText; }
juce::File TuxiroWarpMeProcessor::getLastExport() const { const juce::ScopedLock l (miscLock); return lastExport; }
juce::File TuxiroWarpMeProcessor::getExportFolder() const { const juce::ScopedLock l (miscLock); return exportFolder; }
void TuxiroWarpMeProcessor::setExportFolder (const juce::File& f) { const juce::ScopedLock l (miscLock); exportFolder = f; }
void TuxiroWarpMeProcessor::setStatus (const juce::String& s) { const juce::ScopedLock l (miscLock); statusText = s; }
void TuxiroWarpMeProcessor::setStage (Stage s, const juce::String& text) { stage = (int) s; setStatus (text); }

void TuxiroWarpMeProcessor::publishTrack (std::shared_ptr<const TrackData> t)
{
    {
        const juce::SpinLock::ScopedLockType l (dataLock);
        if (track) graveyard.push_back (track);
        if (analysis) graveyard.push_back (analysis);
        track = std::move (t); analysis.reset();
        while (graveyard.size() > 4) graveyard.erase (graveyard.begin());
    }
    playing = false; posSeconds = 0.0; seekTarget = 0.0;
    ++generation;
}

void TuxiroWarpMeProcessor::publishAnalysis (std::shared_ptr<const BeatAnalysis> a)
{
    {
        const juce::SpinLock::ScopedLockType l (dataLock);
        if (analysis) graveyard.push_back (analysis);
        analysis = std::move (a);
        while (graveyard.size() > 4) graveyard.erase (graveyard.begin());
    }
    ++generation;
}

void TuxiroWarpMeProcessor::setPlaying (bool p)
{
    if (p)
    {
        auto t = getTrack();
        if (!t) return;
        if (posSeconds.load() > t->durationSeconds() - 0.2) seekSeconds (0.0);
    }
    playing = p;
}

void TuxiroWarpMeProcessor::seekSeconds (double s)
{
    auto t = getTrack();
    if (!t) return;
    s = juce::jlimit (0.0, juce::jmax (0.0, t->durationSeconds() - 0.1), s);
    posSeconds = s; seekTarget = s;
}

void TuxiroWarpMeProcessor::shiftDownbeat (int delta)
{
    auto a = getAnalysis();
    if (!a || a->beats.empty()) return;
    auto copy = std::make_shared<BeatAnalysis> (*a);
    copy->downbeat = ((copy->downbeat + delta) % 4 + 4) % 4;
    publishAnalysis (copy);
}

// ------------------------------------------------------------------ load + analyse
void TuxiroWarpMeProcessor::loadFile (const juce::File& file)
{
    const juce::ScopedLock sl (workerLock);
    if (loadWorker) loadWorker->stopThread (30000);
    playing = false;
    { const juce::ScopedLock l (miscLock); lastFile = file; }
    stage = (int) Stage::Loading; progress = 0.f;
    setStatus ("Loading " + file.getFileName() + "...");
    loadWorker = std::make_unique<FnThread> ("TWM load", [this, file] (FnThread& t) { runLoad (file, t); });
    loadWorker->startThread();
}

static void computePeaks (TrackData& td)
{
    const int block = td.peakBlock, len = td.audio.getNumSamples(), nch = td.audio.getNumChannels();
    const int nBlocks = (len + block - 1) / block;
    td.peakFull.assign ((size_t) nBlocks, 0.f); td.peakMid.assign ((size_t) nBlocks, 0.f); td.peakHigh.assign ((size_t) nBlocks, 0.f);
    const float a = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 3000.0 / td.sampleRate);
    float y[2] = { 0.f, 0.f };
    for (int b = 0; b < nBlocks; ++b)
    {
        float pf = 0.f, pm = 0.f, ph = 0.f;
        const int end = juce::jmin (len, (b + 1) * block);
        for (int i = b * block; i < end; ++i)
            for (int c = 0; c < nch; ++c)
            {
                const float x = td.audio.getReadPointer (c)[i];
                y[c] = a * y[c] + (1.f - a) * x;
                pf = juce::jmax (pf, std::abs (x)); pm = juce::jmax (pm, std::abs (y[c])); ph = juce::jmax (ph, std::abs (x - y[c]));
            }
        td.peakFull[(size_t) b] = pf; td.peakMid[(size_t) b] = pm; td.peakHigh[(size_t) b] = ph;
    }
}

void TuxiroWarpMeProcessor::runLoad (juce::File file, juce::Thread& t)
{
    auto fail = [this] (const juce::String& m) { setStage (Stage::Failed, m); };

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0) { fail ("Could not open this file"); return; }
    const double fsr = reader->sampleRate;
    if (reader->lengthInSamples > (juce::int64) (fsr * 60.0 * 30.0)) { fail ("Files are limited to 30 minutes"); return; }
    if (reader->lengthInSamples < (juce::int64) (fsr * 4.0)) { fail ("This file is too short"); return; }

    auto td = std::make_shared<TrackData>();
    const int nch = (int) juce::jmin (2u, reader->numChannels);
    const int len = (int) reader->lengthInSamples;
    td->audio.setSize (nch, len);
    td->sampleRate = fsr; td->name = file.getFileNameWithoutExtension(); td->file = file;

    const int chunk = 1 << 18;
    for (int pos = 0; pos < len; pos += chunk)
    {
        if (t.threadShouldExit()) return;
        reader->read (&td->audio, pos, juce::jmin (chunk, len - pos), pos, true, nch > 1);
        progress = 0.25f * (float) pos / (float) len;
    }
    reader.reset();
    computePeaks (*td);
    if (t.threadShouldExit()) return;

    publishTrack (td);
    setStage (Stage::Analysing, "Finding every beat...");

    std::vector<float> mono ((size_t) len);
    for (int i = 0; i < len; ++i)
    {
        float s = 0.f;
        for (int c = 0; c < nch; ++c) s += td->audio.getReadPointer (c)[i];
        mono[(size_t) i] = s / (float) nch;
    }
    auto result = std::make_shared<BeatAnalysis> (BeatAnalyzer::analyse (
        mono, fsr, [&t] { return t.threadShouldExit(); },
        [this] (float p) { progress = 0.25f + 0.75f * p; }));
    if (t.threadShouldExit()) return;

    publishAnalysis (result);
    if (result->hasBeat)
        setStage (Stage::Ready, "Warped: " + juce::String (result->bpm, 2) + " BPM, the 1 at "
                                    + juce::String (result->beats[(size_t) result->downbeat], 2) + " s.");
    else
        setStage (Stage::Ready, "No clear beat found in this file.");
    progress = 1.f;
}

// ------------------------------------------------------------------ export
void TuxiroWarpMeProcessor::startExport (double bpm)
{
    if (exporting.load()) return;
    auto a = getAnalysis();
    if (!a || !a->hasBeat) { setStatus ("Nothing to export yet"); return; }
    const juce::ScopedLock sl (workerLock);
    if (exportWorker) exportWorker->stopThread (30000);
    exporting = true; progress = 0.f;
    setStatus ("Exporting...");
    const auto folder = getExportFolder();
    const bool aiff = exportAiff.load();
    exportWorker = std::make_unique<FnThread> ("TWM export", [this, bpm, folder, aiff] (FnThread& t) { runExport (bpm, folder, aiff, t); });
    exportWorker->startThread();
}

void TuxiroWarpMeProcessor::runExport (double bpm, juce::File folder, bool aiff, juce::Thread& t)
{
    struct Done { std::atomic<bool>& f; ~Done() { f = false; } } done { exporting };
    auto tr = getTrack(); auto an = getAnalysis();
    if (!tr || !an) { setStatus ("Nothing to export yet"); return; }

    std::vector<const float*> chans;
    for (int c = 0; c < tr->audio.getNumChannels(); ++c) chans.push_back (tr->audio.getReadPointer (c));
    auto out = TimeStretch::renderToGrid (chans, tr->audio.getNumSamples(), tr->sampleRate, an->beats, an->downbeat, bpm,
                                          [&t] { return t.threadShouldExit(); }, [this] (float p) { progress = p; });
    if (out.empty()) { if (!t.threadShouldExit()) setStatus ("Export failed"); return; }

    juce::AudioBuffer<float> buf ((int) out.size(), (int) out[0].size());
    for (int c = 0; c < buf.getNumChannels(); ++c)
        juce::FloatVectorOperations::copy (buf.getWritePointer (c), out[(size_t) c].data(), buf.getNumSamples());

    if (!folder.createDirectory().wasOk()) { setStatus ("Cannot create the export folder"); return; }
    const auto base = tr->name + " [" + juce::String (bpm, 2) + " BPM]";
    const auto outFile = folder.getNonexistentChildFile (base, aiff ? ".aif" : ".wav");

    juce::WavAudioFormat wav; juce::AiffAudioFormat aif;
    juce::AudioFormat& fmt = aiff ? static_cast<juce::AudioFormat&> (aif) : static_cast<juce::AudioFormat&> (wav);
    std::unique_ptr<juce::FileOutputStream> os (outFile.createOutputStream());
    if (os == nullptr) { setStatus ("Cannot write to the export folder"); return; }
    std::unique_ptr<juce::AudioFormatWriter> writer (fmt.createWriterFor (os.get(), tr->sampleRate, (unsigned) buf.getNumChannels(), 24, {}, 0));
    if (writer == nullptr) { setStatus ("Cannot create the audio file"); return; }
    os.release();
    writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    writer.reset();

    { const juce::ScopedLock l (miscLock); lastExport = outFile; }
    setStatus ("Exported " + juce::String (bpm, 2) + " BPM: " + outFile.getFileName());
    progress = 1.f;
}

// ------------------------------------------------------------------ host state
void TuxiroWarpMeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement xml ("TUXIROWARPME");
    { const juce::ScopedLock l (miscLock);
      xml.setAttribute ("file", lastFile.getFullPathName());
      xml.setAttribute ("folder", exportFolder.getFullPathName()); }
    xml.setAttribute ("aiff", exportAiff.load());
    xml.setAttribute ("metro", metronomeOn.load());
    xml.setAttribute ("level", (double) metronomeLevel.load());
    copyXmlToBinary (xml, dest);
}

void TuxiroWarpMeProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || !xml->hasTagName ("TUXIROWARPME")) return;
    if (xml->getStringAttribute ("folder").isNotEmpty()) setExportFolder (juce::File (xml->getStringAttribute ("folder")));
    exportAiff = xml->getBoolAttribute ("aiff", false);
    metronomeOn = xml->getBoolAttribute ("metro", true);
    metronomeLevel = (float) xml->getDoubleAttribute ("level", 0.6);
    const juce::File f (xml->getStringAttribute ("file"));
    if (f.existsAsFile()) loadFile (f);
}

juce::AudioProcessorEditor* TuxiroWarpMeProcessor::createEditor() { return new TuxiroWarpMeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TuxiroWarpMeProcessor(); }
