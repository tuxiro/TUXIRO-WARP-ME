#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace tx
{
    inline juce::Colour bg()        { return juce::Colour (0xff070a08); }
    inline juce::Colour panel()     { return juce::Colour (0xff111612); }
    inline juce::Colour panelEdge() { return juce::Colour (0xff2a342c); }
    inline juce::Colour green()     { return juce::Colour (0xff44ff95); }
    inline juce::Colour greenMid()  { return juce::Colour (0xff1fb862); }
    inline juce::Colour greenDark() { return juce::Colour (0xff0c5a30); }
    inline juce::Colour text()      { return juce::Colour (0xffd9efe0); }
    inline juce::Colour dim()       { return juce::Colour (0xff7f9a88); }
    inline juce::Colour red()       { return juce::Colour (0xffff4a3d); }
    inline juce::Font font (float h, bool bold = true)
    { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
}

class TuxiroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TuxiroLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return tx::font (juce::jmin (15.f, (float) h * 0.48f)); }
};

class WaveformView : public juce::Component
{
public:
    explicit WaveformView (TuxiroWarpMeProcessor& p) : proc (p) {}
    void setData (std::shared_ptr<const TrackData>, std::shared_ptr<const BeatAnalysis>);
    void setWarped (bool w) { warped = w; repaint(); }
    void fit();
    void zoom (double factor, double anchor = -1.0);
    void ensureVisible (double seconds);
    void showTime (double seconds);
    double getViewStart() const { return viewStart; }
    double getViewEnd() const { return viewEnd; }
    std::function<void()> onViewChanged;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void clampView();
    double timeToX (double t) const { return (t - viewStart) / (viewEnd - viewStart) * getWidth(); }
    double xToTime (double x) const { return viewStart + x / getWidth() * (viewEnd - viewStart); }

    TuxiroWarpMeProcessor& proc;
    std::shared_ptr<const TrackData> track;
    std::shared_ptr<const BeatAnalysis> an;
    double viewStart = 0.0, viewEnd = 10.0, downViewStart = 0.0;
    bool warped = true, dragged = false;
};

class OverviewView : public juce::Component
{
public:
    explicit OverviewView (TuxiroWarpMeProcessor& p) : proc (p) {}
    void setData (std::shared_ptr<const TrackData> t) { track = std::move (t); repaint(); }
    void setViewRange (double s, double e) { vs = s; ve = e; repaint(); }
    std::function<void (double)> onCentre;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { pick (e); }
    void mouseDrag (const juce::MouseEvent& e) override { pick (e); }
private:
    void pick (const juce::MouseEvent& e);
    TuxiroWarpMeProcessor& proc;
    std::shared_ptr<const TrackData> track;
    double vs = 0, ve = 1;
};

class DragHandle : public juce::Component
{
public:
    explicit DragHandle (TuxiroWarpMeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    TuxiroWarpMeProcessor& proc;
};

class TuxiroWarpMeEditor : public juce::AudioProcessorEditor,
                           public juce::FileDragAndDropTarget,
                           private juce::Timer
{
public:
    explicit TuxiroWarpMeEditor (TuxiroWarpMeProcessor&);
    ~TuxiroWarpMeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    void timerCallback() override;
    void openFile();
    void showSettings();
    void jumpToNextCheck();
    void drawPanel (juce::Graphics&, juce::Rectangle<float>);

    TuxiroWarpMeProcessor& proc;
    TuxiroLookAndFeel lnf;
    std::shared_ptr<const TrackData> track;
    std::shared_ptr<const BeatAnalysis> an;
    double targetBpm = 120.0;
    bool warpedView = true;

    WaveformView wave;
    OverviewView overview;
    DragHandle dragHandle;
    juce::TextButton origBtn { "ORIGINAL" }, warpBtn { "WARPED" }, openBtn { "OPEN" }, gearBtn { "SET" },
                     prevBtn { "< 1" }, nextBtn { "1 >" }, zoomOutBtn { "-" }, zoomInBtn { "+" }, fitBtn { "FIT" },
                     playBtn { "PLAY" }, exportBtn { "EXPORT AUDIO" }, checkBtn { "NEXT" };
    juce::ToggleButton metroBtn { "METRONOME" };
    juce::Slider levelSlider;
    juce::Label tempoLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuxiroWarpMeEditor)
};
