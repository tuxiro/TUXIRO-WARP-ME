#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include "BeatAnalyzer.h"

struct TrackData
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    juce::String name;
    juce::File file;
    int peakBlock = 128;
    std::vector<float> peakFull, peakMid, peakHigh;

    double durationSeconds() const { return sampleRate > 0 ? (double) audio.getNumSamples() / sampleRate : 0.0; }
};

class FnThread : public juce::Thread
{
public:
    FnThread (const juce::String& n, std::function<void (FnThread&)> f) : juce::Thread (n), fn (std::move (f)) {}
    void run() override { fn (*this); }
private:
    std::function<void (FnThread&)> fn;
};

class TuxiroWarpMeProcessor : public juce::AudioProcessor
{
public:
    enum class Stage { Idle = 0, Loading, Analysing, Ready, Failed };

    TuxiroWarpMeProcessor();
    ~TuxiroWarpMeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "TUXIRO WARP ME"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- UI API (message thread) ----
    void loadFile (const juce::File&);
    std::shared_ptr<const TrackData> getTrack() const;
    std::shared_ptr<const BeatAnalysis> getAnalysis() const;
    Stage getStage() const { return (Stage) stage.load(); }
    float getProgress() const { return progress.load(); }
    juce::String getStatus() const;
    bool isExporting() const { return exporting.load(); }

    void setPlaying (bool);
    bool isPlaying() const { return playing.load(); }
    void seekSeconds (double);
    double getPositionSeconds() const { return posSeconds.load(); }
    void shiftDownbeat (int delta);
    void startExport (double bpm);
    juce::File getLastExport() const;

    juce::File getExportFolder() const;
    void setExportFolder (const juce::File&);
    bool getExportAiff() const { return exportAiff.load(); }
    void setExportAiff (bool b) { exportAiff = b; }

    std::atomic<bool>  metronomeOn { true };
    std::atomic<float> metronomeLevel { 0.6f };

private:
    void runLoad (juce::File, juce::Thread&);
    void runExport (double bpm, juce::File folder, bool aiff, juce::Thread&);
    void publishTrack (std::shared_ptr<const TrackData>);
    void publishAnalysis (std::shared_ptr<const BeatAnalysis>);
    void setStatus (const juce::String&);
    void setStage (Stage, const juce::String& status);

    juce::AudioFormatManager formats;

    mutable juce::SpinLock dataLock;
    std::shared_ptr<const TrackData> track;
    std::shared_ptr<const BeatAnalysis> analysis;
    std::vector<std::shared_ptr<const void>> graveyard;   // retired data is freed off the audio thread
    std::atomic<int> generation { 0 };

    mutable juce::CriticalSection miscLock;               // status, lastExport, settings
    juce::String statusText;
    juce::File lastExport, lastFile, exportFolder;
    std::atomic<bool> exportAiff { false };

    std::atomic<int>  stage { (int) Stage::Idle };
    std::atomic<float> progress { 0.f };
    std::atomic<bool> exporting { false };

    juce::CriticalSection workerLock;
    std::unique_ptr<FnThread> loadWorker, exportWorker;

    // playback
    std::atomic<bool>   playing { false };
    std::atomic<double> posSeconds { 0.0 };
    std::atomic<double> seekTarget { -1.0 };

    // audio-thread state
    std::shared_ptr<const TrackData> rtTrack;
    std::shared_ptr<const BeatAnalysis> rtAnalysis;
    int rtGeneration = -1;
    double rtPos = 0.0, hostRate = 44100.0;
    int rtNextBeat = -1;
    double clickT = 1e9, clickFreq = 1000.0, clickAmp = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuxiroWarpMeProcessor)
};
