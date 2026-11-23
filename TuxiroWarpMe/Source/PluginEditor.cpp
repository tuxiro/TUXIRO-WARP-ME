#include "PluginEditor.h"
#include <cmath>

namespace
{
juce::String fmtTime (double s)
{
    if (s < 0) s = 0;
    const int m = (int) (s / 60.0);
    return juce::String (m) + ":" + juce::String (s - m * 60.0, 1).paddedLeft ('0', 4);
}
int floorDiv (int a, int d) { return (int) std::floor ((double) a / d); }
int posMod (int a, int m) { return ((a % m) + m) % m; }
}

// ====================================================================== look and feel
TuxiroLookAndFeel::TuxiroLookAndFeel()
{
    setColour (juce::Label::textColourId, tx::green());
    setColour (juce::TextEditor::backgroundColourId, tx::bg());
    setColour (juce::TextEditor::textColourId, tx::green());
    setColour (juce::TextEditor::highlightColourId, tx::greenDark());
    setColour (juce::PopupMenu::backgroundColourId, tx::panel());
    setColour (juce::PopupMenu::textColourId, tx::text());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, tx::greenDark());
    setColour (juce::PopupMenu::highlightedTextColourId, tx::green());
}

void TuxiroLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const auto id = b.getComponentID();
    const bool on = b.getToggleState();
    const float alpha = b.isEnabled() ? 1.f : 0.4f;

    if (id == "play")
    {
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        auto c = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c3d31), c.getCentreX(), c.getY(),
                                                 juce::Colour (0xff0b100c), c.getCentreX(), c.getBottom(), false));
        g.fillEllipse (c);
        g.setColour (tx::greenMid().withAlpha (0.8f)); g.drawEllipse (c.reduced (2.f), 2.f);
        auto in = c.reduced (d * 0.14f);
        g.setGradientFill (juce::ColourGradient (tx::green().brighter (0.1f), in.getCentreX(), in.getY(),
                                                 tx::greenMid().darker (0.2f), in.getCentreX(), in.getBottom(), false));
        g.fillEllipse (down ? in.reduced (1.5f) : in);
        return;
    }
    if (id == "export")
    {
        g.setColour (juce::Colour (0xff040605)); g.fillRoundedRectangle (r, 6.f);
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addRoundedRectangle (r, 6.f); g.reduceClipRegion (clip);
        for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 24.f)
        {
            juce::Path p;
            p.addQuadrilateral (x, r.getBottom(), x + 12.f, r.getBottom(), x + 12.f + r.getHeight(), r.getY(), x + r.getHeight(), r.getY());
            g.setColour (tx::green().withAlpha (0.85f * alpha)); g.fillPath (p);
        }
        auto in = r.reduced (6.f);
        g.setGradientFill (juce::ColourGradient (down ? tx::greenMid().darker() : tx::greenMid().brighter (hover ? 0.2f : 0.f), in.getX(), in.getY(),
                                                 tx::greenDark(), in.getX(), in.getBottom(), false));
        g.fillRoundedRectangle (in, 4.f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (in, 4.f, 1.f);
        return;
    }

    juce::Colour top = on ? tx::green().darker (0.1f) : juce::Colour (0xff1d261f);
    juce::Colour bot = on ? tx::greenMid().darker (0.3f) : juce::Colour (0xff0d120e);
    if (hover) { top = top.brighter (0.15f); bot = bot.brighter (0.1f); }
    if (down)  { top = top.darker (0.2f); }
    g.setGradientFill (juce::ColourGradient (top.withAlpha (alpha), r.getX(), r.getY(), bot.withAlpha (alpha), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.f);
    g.setColour ((on ? tx::green() : tx::greenMid().withAlpha (0.55f)).withAlpha (alpha));
    g.drawRoundedRectangle (r, 4.f, 1.2f);
}

void TuxiroLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const auto id = b.getComponentID();
    juce::Colour c = tx::green();
    if (id == "play") c = juce::Colour (0xff031208);
    else if (id == "export") c = juce::Colours::white;
    else if (b.getToggleState()) c = juce::Colour (0xff031208);
    if (!b.isEnabled()) c = c.withAlpha (0.4f);
    g.setColour (c);
    float h = (float) b.getHeight();
    g.setFont (tx::font (id == "play" ? h * 0.2f : (id == "export" ? h * 0.36f : juce::jmin (15.f, h * 0.48f))));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

void TuxiroLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    const float h = (float) b.getHeight();
    auto led = juce::Rectangle<float> (h * 0.55f, h * 0.55f).withCentre ({ h * 0.5f, h * 0.5f });
    g.setColour (juce::Colour (0xff050806)); g.fillEllipse (led.expanded (2.f));
    g.setColour (b.getToggleState() ? tx::green() : tx::greenDark().darker()); g.fillEllipse (led);
    g.setColour (tx::text()); g.setFont (tx::font (h * 0.5f));
    g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft ((int) (h * 1.1f)), juce::Justification::centredLeft, false);
}

void TuxiroLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                          juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (juce::Colour (0xff050806)); g.fillRoundedRectangle ((float) x, cy - 4.f, (float) w, 8.f, 3.f);
    g.setColour (tx::greenMid()); g.fillRoundedRectangle ((float) x, cy - 3.f, pos - (float) x, 6.f, 3.f);
    auto th = juce::Rectangle<float> (12.f, (float) h * 0.8f).withCentre ({ pos, cy });
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a4a3e), 0, th.getY(), juce::Colour (0xff1a231c), 0, th.getBottom(), false));
    g.fillRoundedRectangle (th, 2.f);
    g.setColour (tx::green()); g.drawRoundedRectangle (th, 2.f, 1.f);
}

// ====================================================================== waveform
void WaveformView::setData (std::shared_ptr<const TrackData> t, std::shared_ptr<const BeatAnalysis> a)
{
    const bool newTrack = t != track;
    track = std::move (t); an = std::move (a);
    if (newTrack) fit(); else repaint();
}

void WaveformView::fit()
{
    viewStart = 0.0; viewEnd = track ? juce::jmax (1.0, track->durationSeconds()) : 10.0;
    if (onViewChanged) onViewChanged();
    repaint();
}

void WaveformView::clampView()
{
    const double dur = track ? track->durationSeconds() : 10.0;
    double span = juce::jlimit (0.25, juce::jmax (1.0, dur), viewEnd - viewStart);
    viewStart = juce::jlimit (0.0, juce::jmax (0.0, dur - span), viewStart);
    viewEnd = viewStart + span;
}

void WaveformView::zoom (double factor, double anchor)
{
    if (!track) return;
    if (anchor < 0) anchor = (viewStart + viewEnd) * 0.5;
    const double span = (viewEnd - viewStart) * factor;
    const double u = (anchor - viewStart) / (viewEnd - viewStart);
    viewStart = anchor - u * span; viewEnd = viewStart + span;
    clampView();
    if (onViewChanged) onViewChanged();
    repaint();
}

void WaveformView::ensureVisible (double s)
{
    const double span = viewEnd - viewStart;
    if (span >= (track ? track->durationSeconds() : 0.0) - 0.01) return;
    if (s > viewStart + span * 0.92 || s < viewStart)
    {
        viewStart = s - span * 0.08; viewEnd = viewStart + span;
        clampView();
        if (onViewChanged) onViewChanged();
    }
}

void WaveformView::showTime (double s)
{
    const double span = viewEnd - viewStart;
    viewStart = s - span * 0.5; viewEnd = viewStart + span;
    clampView();
    if (onViewChanged) onViewChanged();
    repaint();
}

void WaveformView::mouseDown (const juce::MouseEvent& e) { dragged = false; downViewStart = viewStart; juce::ignoreUnused (e); }

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (!track) return;
    if (std::abs (e.getDistanceFromDragStartX()) > 3) dragged = true;
    if (!dragged) return;
    const double span = viewEnd - viewStart;
    viewStart = downViewStart - (double) e.getDistanceFromDragStartX() / (double) getWidth() * span;
    viewEnd = viewStart + span;
    clampView();
    if (onViewChanged) onViewChanged();
    repaint();
}

void WaveformView::mouseUp (const juce::MouseEvent& e)
{
    if (!dragged && track) proc.seekSeconds (xToTime ((double) e.x));
    dragged = false;
}

void WaveformView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (!track) return;
    if (std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        const double span = viewEnd - viewStart;
        viewStart -= w.deltaX * span * 0.5; viewEnd = viewStart + span;
        clampView();
        if (onViewChanged) onViewChanged();
        repaint();
    }
    else
        zoom (std::exp (-w.deltaY * 0.8), xToTime ((double) e.x));
}

void WaveformView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    g.setColour (juce::Colour (0xff050806)); g.fillRect (b);
    if (!track)
    {
        g.setColour (tx::dim()); g.setFont (tx::font (16.f));
        g.drawText ("DROP A TRACK HERE  -  OR PRESS OPEN", b, juce::Justification::centred);
        return;
    }

    const float w = (float) b.getWidth(), h = (float) b.getHeight(), mid = h * 0.5f;
    const double sr = track->sampleRate, span = viewEnd - viewStart, spp = span / (double) w;
    const double dur = track->durationSeconds();
    const int blocks = (int) track->peakFull.size();
    const double blockSec = (double) track->peakBlock / sr;

    g.setColour (juce::Colour (0xff2d4a38)); g.fillRect (0.f, mid, w, 1.f);
    for (int x = 0; x < b.getWidth(); ++x)
    {
        const double t0 = viewStart + x * spp, t1 = t0 + spp;
        if (t1 < 0.0 || t0 > dur) continue;
        const int b0 = juce::jlimit (0, blocks - 1, (int) std::floor (t0 / blockSec));
        const int b1 = juce::jlimit (b0, blocks - 1, (int) std::floor (t1 / blockSec));
        const int stride = juce::jmax (1, (b1 - b0 + 1) / 48);
        float pf = 0.f, pm = 0.f, ph = 0.f;
        for (int i = b0; i <= b1; i += stride)
        {
            pf = juce::jmax (pf, track->peakFull[(size_t) i]);
            pm = juce::jmax (pm, track->peakMid[(size_t) i]);
            ph = juce::jmax (ph, track->peakHigh[(size_t) i]);
        }
        auto bar = [&] (float v, juce::Colour c)
        {
            const float hh = juce::jlimit (0.f, mid - 6.f, v * (mid - 8.f));
            if (hh < 0.5f) return;
            g.setColour (c); g.fillRect ((float) x, mid - hh, 1.f, hh * 2.f);
        };
        bar (pf, juce::Colour (0xff12794a));
        bar (pm * 0.95f, juce::Colour (0xff2fdc7e));
        bar (ph * 1.5f, juce::Colour (0xddeafff2));
    }

    // beat grid
    const double sp = juce::jmax (0.001, span);
    if (an && an->hasBeat && !an->beats.empty())
    {
        const auto& beats = an->beats;
        const double T = 60.0 / an->bpm;
        const double pxPerBeat = (double) w / sp * T;
        const int step = pxPerBeat >= 7.0 ? 1 : (pxPerBeat * 4 >= 7.0 ? 4 : (pxPerBeat * 16 >= 7.0 ? 16 : 64));
        auto drawLine = [&] (double t, int rel)
        {
            if (posMod (rel, step) != 0) return;
            const float x = (float) timeToX (t);
            const bool bar = posMod (rel, 4) == 0;
            g.setColour ((warped ? tx::green() : juce::Colour (0xff9bb5a4)).withAlpha (bar ? 0.55f : 0.2f));
            g.fillRect (x, 0.f, 1.f, h);
            if (bar && pxPerBeat * 4 >= 26.0)
            {
                juce::Path flag; flag.addTriangle (x, 0.f, x + 15.f, 0.f, x, 15.f);
                g.setColour (warped ? tx::greenMid() : juce::Colour (0xff6f8a79)); g.fillPath (flag);
                g.setColour (juce::Colour (0xff031208)); g.setFont (tx::font (10.f));
                g.drawText (juce::String (floorDiv (rel, 4) + 1), (int) x + 1, 0, 14, 12, juce::Justification::centredLeft, false);
            }
        };
        if (warped)
        {
            const int i0 = juce::jmax (0, (int) (std::lower_bound (beats.begin(), beats.end(), viewStart) - beats.begin()) - 1);
            for (int i = i0; i < (int) beats.size(); ++i)
            {
                if (beats[(size_t) i] > viewEnd) break;
                drawLine (beats[(size_t) i], i - an->downbeat);
            }
        }
        else
        {
            const double t0 = beats[(size_t) juce::jmin ((int) beats.size() - 1, an->downbeat)];
            for (int k = (int) std::ceil ((viewStart - t0) / T) - 1; t0 + k * T <= viewEnd; ++k)
                drawLine (t0 + k * T, k);
        }
    }

    // playhead
    const double pos = proc.getPositionSeconds();
    if (pos >= viewStart && pos <= viewEnd)
    {
        const float x = (float) timeToX (pos);
        g.setColour (tx::red()); g.fillRect (x - 1.f, 0.f, 2.f, h);
        juce::Path tri; tri.addTriangle (x - 6.f, 0.f, x + 6.f, 0.f, x, 9.f); g.fillPath (tri);
    }

    g.setFont (tx::font (11.f)); g.setColour (tx::dim());
    g.drawText (warped ? "WARPED // EVERY BEAT ON THE GRID" : "ORIGINAL // ONE FIXED TEMPO - SEE THE DRIFT",
                8, b.getHeight() - 18, 380, 14, juce::Justification::centredLeft, false);
    if (an && an->hasBeat && !an->beats.empty())
    {
        const auto& beats = an->beats;
        int idx = (int) (std::upper_bound (beats.begin(), beats.end(), pos) - beats.begin()) - 1;
        const int rel = idx - an->downbeat;
        g.setColour (tx::green());
        g.drawText ("BAR " + juce::String (idx < 0 ? 0 : floorDiv (rel, 4) + 1) + "  BEAT " + juce::String (idx < 0 ? 0 : posMod (rel, 4) + 1)
                        + "  //  " + juce::String (an->bpm, 2) + " BPM",
                    b.getWidth() - 308, b.getHeight() - 18, 300, 14, juce::Justification::centredRight, false);
    }
}

// ====================================================================== overview
void OverviewView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff050806)); g.fillRoundedRectangle (b, 3.f);
    if (!track) return;
    const int w = getWidth(); const float mid = b.getCentreY(), hh = b.getHeight() * 0.42f;
    const int blocks = (int) track->peakFull.size();
    for (int x = 0; x < w; ++x)
    {
        const int b0 = (int) ((int64_t) x * blocks / w), b1 = juce::jmax (b0, (int) ((int64_t) (x + 1) * blocks / w) - 1);
        float pk = 0.f;
        for (int i = b0; i <= b1 && i < blocks; i += juce::jmax (1, (b1 - b0) / 24)) pk = juce::jmax (pk, track->peakFull[(size_t) i]);
        g.setColour (juce::Colour (0xffd9ffe8).withAlpha (0.85f));
        g.fillRect ((float) x, mid - pk * hh, 1.f, juce::jmax (1.f, pk * hh * 2.f));
    }
    const double dur = juce::jmax (0.001, track->durationSeconds());
    auto vp = juce::Rectangle<float> ((float) (vs / dur * w), 1.f, juce::jmax (4.f, (float) ((ve - vs) / dur * w)), b.getHeight() - 2.f);
    g.setColour (tx::green().withAlpha (0.15f)); g.fillRect (vp);
    g.setColour (tx::green()); g.drawRect (vp, 1.5f);
    const float px = (float) (proc.getPositionSeconds() / dur * w);
    g.setColour (tx::red()); g.fillRect (px, 0.f, 1.5f, b.getHeight());
}

void OverviewView::pick (const juce::MouseEvent& e)
{
    if (!track || !onCentre) return;
    onCentre ((double) e.x / (double) getWidth() * track->durationSeconds());
}

// ====================================================================== drag handle
void DragHandle::paint (juce::Graphics& g)
{
    const bool ok = proc.getLastExport().existsAsFile();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour (0xff0d120e)); g.fillRoundedRectangle (r, 4.f);
    g.setColour (tx::greenMid().withAlpha (ok ? 0.9f : 0.3f)); g.drawRoundedRectangle (r, 4.f, 1.2f);
    g.setColour (ok ? tx::green() : tx::dim()); g.setFont (tx::font (12.f));
    g.drawText (ok ? "DRAG LAST EXPORT INTO LIVE" : "EXPORT FIRST, THEN DRAG IT TO LIVE", getLocalBounds(), juce::Justification::centred);
}

void DragHandle::mouseDrag (const juce::MouseEvent& e)
{
    const auto f = proc.getLastExport();
    if (f.existsAsFile() && e.getDistanceFromDragStart() > 6)
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
}

// ====================================================================== editor
TuxiroWarpMeEditor::TuxiroWarpMeEditor (TuxiroWarpMeProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), wave (p), overview (p), dragHandle (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (wave);
    addAndMakeVisible (overview);
    for (auto* b : { &origBtn, &warpBtn, &openBtn, &gearBtn, &prevBtn, &nextBtn, &zoomOutBtn, &zoomInBtn, &fitBtn,
                     &playBtn, &exportBtn, &checkBtn })
        addAndMakeVisible (*b);
    addAndMakeVisible (metroBtn); addAndMakeVisible (levelSlider); addAndMakeVisible (tempoLabel); addAndMakeVisible (dragHandle);

    playBtn.setComponentID ("play"); exportBtn.setComponentID ("export");
    origBtn.setRadioGroupId (7); warpBtn.setRadioGroupId (7);
    origBtn.setClickingTogglesState (true); warpBtn.setClickingTogglesState (true);
    warpBtn.setToggleState (true, juce::dontSendNotification);
    origBtn.onClick = [this] { warpedView = false; wave.setWarped (false); };
    warpBtn.onClick = [this] { warpedView = true;  wave.setWarped (true); };
    openBtn.onClick = [this] { openFile(); };
    gearBtn.onClick = [this] { showSettings(); };
    prevBtn.onClick = [this] { proc.shiftDownbeat (-1); };
    nextBtn.onClick = [this] { proc.shiftDownbeat (1); };
    zoomOutBtn.onClick = [this] { wave.zoom (1.6); };
    zoomInBtn.onClick  = [this] { wave.zoom (1.0 / 1.6); };
    fitBtn.onClick = [this] { wave.fit(); };
    checkBtn.onClick = [this] { jumpToNextCheck(); };
    playBtn.onClick = [this] { proc.setPlaying (!proc.isPlaying()); };
    exportBtn.onClick = [this] { if (an && an->hasBeat) proc.startExport (targetBpm); };

    metroBtn.setToggleState (proc.metronomeOn.load(), juce::dontSendNotification);
    metroBtn.onClick = [this] { proc.metronomeOn = metroBtn.getToggleState(); };
    levelSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    levelSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    levelSlider.setRange (0.0, 1.0, 0.01);
    levelSlider.setValue (proc.metronomeLevel.load(), juce::dontSendNotification);
    levelSlider.onValueChange = [this] { proc.metronomeLevel = (float) levelSlider.getValue(); };

    tempoLabel.setFont (tx::font (50.f));
    tempoLabel.setJustificationType (juce::Justification::centredLeft);
    tempoLabel.setColour (juce::Label::textColourId, tx::green());
    tempoLabel.setColour (juce::Label::textWhenEditingColourId, tx::green());
    tempoLabel.setEditable (false, true, false);
    tempoLabel.setText ("--", juce::dontSendNotification);
    tempoLabel.onTextChange = [this]
    {
        const double v = tempoLabel.getText().getDoubleValue();
        if (v >= 40.0 && v <= 250.0) targetBpm = v;
        tempoLabel.setText (juce::String (targetBpm, 2), juce::dontSendNotification);
    };

    overview.onCentre = [this] (double t) { wave.showTime (t); };
    wave.onViewChanged = [this] { overview.setViewRange (wave.getViewStart(), wave.getViewEnd()); };

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (1024.0 / 677.0);
    setResizeLimits (640, 423, 1700, 1124);
    setSize (1024, 677);
    startTimerHz (30);
}

TuxiroWarpMeEditor::~TuxiroWarpMeEditor() { stopTimer(); setLookAndFeel (nullptr); }

bool TuxiroWarpMeEditor::isInterestedInFileDrag (const juce::StringArray& f)
{
    for (auto& s : f)
        if (juce::File (s).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void TuxiroWarpMeEditor::filesDropped (const juce::StringArray& f, int, int)
{
    for (auto& s : f)
        if (juce::File (s).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) { proc.loadFile (juce::File (s)); return; }
}

void TuxiroWarpMeEditor::openFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a track",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    juce::Component::SafePointer<TuxiroWarpMeEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f.existsAsFile()) safe->proc.loadFile (f);
                          });
}

void TuxiroWarpMeEditor::showSettings()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Export folder");
    m.addItem (1, proc.getExportFolder().getFullPathName(), false, false);
    m.addItem (2, "Choose export folder...");
    m.addSeparator();
    m.addItem (3, "Export as WAV 24-bit", true, !proc.getExportAiff());
    m.addItem (4, "Export as AIFF 24-bit", true, proc.getExportAiff());
    juce::Component::SafePointer<TuxiroWarpMeEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (gearBtn), [safe] (int r)
    {
        if (safe == nullptr) return;
        if (r == 3) safe->proc.setExportAiff (false);
        if (r == 4) safe->proc.setExportAiff (true);
        if (r == 2)
        {
            safe->chooser = std::make_unique<juce::FileChooser> ("Export folder", safe->proc.getExportFolder());
            safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                        [safe] (const juce::FileChooser& fc)
                                        { if (safe != nullptr && fc.getResult().isDirectory()) safe->proc.setExportFolder (fc.getResult()); });
        }
    });
}

void TuxiroWarpMeEditor::jumpToNextCheck()
{
    if (!an || an->checkBeats.empty()) return;
    const double pos = proc.getPositionSeconds();
    double target = -1.0;
    for (int i : an->checkBeats)
        if (an->beats[(size_t) i] > pos + 0.1) { target = an->beats[(size_t) i]; break; }
    if (target < 0) target = an->beats[(size_t) an->checkBeats.front()];
    proc.seekSeconds (juce::jmax (0.0, target - 0.5));
    wave.showTime (target);
}

void TuxiroWarpMeEditor::timerCallback()
{
    auto t = proc.getTrack(); auto a = proc.getAnalysis();
    const bool changed = (t != track) || (a != an);
    if (a != an && a && a->hasBeat && (!an || a->bpm != an->bpm))
    {
        targetBpm = std::round (a->bpm * 100.0) / 100.0;
        if (!tempoLabel.isBeingEdited()) tempoLabel.setText (juce::String (targetBpm, 2), juce::dontSendNotification);
    }
    if (t != track && !t) tempoLabel.setText ("--", juce::dontSendNotification);
    track = t; an = a;
    if (changed) { wave.setData (track, an); overview.setData (track); }

    const bool busy = proc.getStage() == TuxiroWarpMeProcessor::Stage::Loading || proc.getStage() == TuxiroWarpMeProcessor::Stage::Analysing
                      || proc.isExporting();
    playBtn.setButtonText (proc.isPlaying() ? "PAUSE" : "PLAY");
    exportBtn.setEnabled (an && an->hasBeat && !proc.isExporting());
    checkBtn.setEnabled (an && !an->checkBeats.empty());
    prevBtn.setEnabled (an && an->hasBeat); nextBtn.setEnabled (an && an->hasBeat);

    if (proc.isPlaying()) wave.ensureVisible (proc.getPositionSeconds());
    if (proc.isPlaying() || busy || changed)
    {
        repaint();
        wave.repaint(); overview.repaint();
    }
    dragHandle.repaint();
}

void TuxiroWarpMeEditor::resized()
{
    const float s = (float) getWidth() / 1024.f;
    auto R = [s] (int x, int y, int w, int h)
    { return juce::Rectangle<int> ((int) std::lround (x * s), (int) std::lround (y * s), (int) std::lround (w * s), (int) std::lround (h * s)); };

    origBtn.setBounds (R (664, 24, 92, 26)); warpBtn.setBounds (R (762, 24, 92, 26));
    openBtn.setBounds (R (866, 22, 98, 30)); gearBtn.setBounds (R (972, 22, 38, 30));
    wave.setBounds (R (28, 80, 966, 266));
    prevBtn.setBounds (R (40, 88, 48, 22)); nextBtn.setBounds (R (934, 88, 48, 22));
    overview.setBounds (R (24, 372, 784, 34));
    zoomOutBtn.setBounds (R (818, 372, 40, 34)); zoomInBtn.setBounds (R (862, 372, 40, 34)); fitBtn.setBounds (R (906, 372, 64, 34));
    tempoLabel.setBounds (R (36, 468, 190, 58));
    tempoLabel.setFont (tx::font (50.f * s));
    checkBtn.setBounds (R (236, 628, 56, 20));
    playBtn.setBounds (R (352, 466, 84, 84));
    metroBtn.setBounds (R (350, 574, 150, 28)); levelSlider.setBounds (R (520, 574, 150, 28));
    exportBtn.setBounds (R (738, 486, 242, 60));
    dragHandle.setBounds (R (738, 560, 242, 34));
}

void TuxiroWarpMeEditor::drawPanel (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff151b16), r.getX(), r.getY(), tx::panel().darker (0.3f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (tx::panelEdge()); g.drawRoundedRectangle (r, 6.f, 1.5f);
    for (auto p : { r.getTopLeft() + juce::Point<float> (9, 9), r.getTopRight() + juce::Point<float> (-9, 9),
                    r.getBottomLeft() + juce::Point<float> (9, -9), r.getBottomRight() + juce::Point<float> (-9, -9) })
    { g.setColour (juce::Colour (0xff050806)); g.fillEllipse (p.x - 2.5f, p.y - 2.5f, 5.f, 5.f); }
}

void TuxiroWarpMeEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / 1024.f;
    g.fillAll (tx::bg());
    g.addTransform (juce::AffineTransform::scale (s));

    // header
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff131a15), 0, 0, tx::bg(), 0, 76, false));
    g.fillRect (0, 0, 1024, 76);
    g.setColour (tx::greenMid()); g.drawEllipse (26.f, 20.f, 40.f, 40.f, 4.f);
    g.setColour (tx::green()); g.fillEllipse (40.f, 34.f, 12.f, 12.f);
    g.setFont (tx::font (31.f)); g.setColour (tx::green());
    g.drawText ("TUXIRO WARP ME", 78, 12, 340, 40, juce::Justification::centredLeft, false);
    g.setFont (tx::font (11.f)); g.setColour (tx::dim());
    g.drawText ("EVERY BEAT ON THE GRID", 80, 50, 300, 14, juce::Justification::centredLeft, false);
    g.setFont (tx::font (14.f)); g.setColour (tx::text());
    g.drawText (track ? track->name : juce::String ("No track loaded"), 400, 22, 256, 30, juce::Justification::centred);
    g.setColour (tx::panelEdge()); g.fillRect (0, 74, 1024, 2);

    g.setColour (juce::Colour (0xff0a0e0b)); g.fillRoundedRectangle (22.f, 76.f, 980.f, 276.f, 6.f);
    g.setColour (tx::greenMid().withAlpha (0.5f)); g.drawRoundedRectangle (22.f, 76.f, 980.f, 276.f, 6.f, 1.5f);

    // panels
    const juce::Rectangle<float> p1 (24, 448, 284, 212);
const juce::Rectangle<float> p2 (326, 448, 368, 212);
const juce::Rectangle<float> p3 (712, 448, 288, 212);
    drawPanel (g, p1); drawPanel (g, p2); drawPanel (g, p3);

    const auto stage = proc.getStage();
    const bool hasBeat = an && an->hasBeat;
    g.setFont (tx::font (11.f)); g.setColour (tx::dim());
    g.drawText ("TEMPO", 40, 458, 100, 14, juce::Justification::centredLeft, false);
    g.setFont (tx::font (16.f)); g.setColour (tx::dim());
    g.drawText ("BPM", 232, 500, 60, 20, juce::Justification::centredLeft, false);
    if (hasBeat)
    {
        g.setFont (tx::font (10.f));
        g.drawText ("detected " + juce::String (an->bpm, 2) + "  -  click the number to edit", 40, 526, 250, 12, juce::Justification::centredLeft, false);
    }

    const bool busy = stage == TuxiroWarpMeProcessor::Stage::Loading || stage == TuxiroWarpMeProcessor::Stage::Analysing;
    juce::String type = hasBeat ? "FULL TRACK" : (an ? "NO CLEAR BEAT" : (busy ? "..." : "-"));
    juce::String drift = an ? juce::String (an->drift) : "-";
    juce::String one = hasBeat ? "at " + juce::String (an->beats[(size_t) an->downbeat], 2) + " s" : "-";
    juce::String markers = hasBeat ? juce::String ((int) an->beats.size()) + " warp markers" : "-";
    juce::String check = !hasBeat ? "-" : (an->checkBeats.empty() ? "all clear" : juce::String ((int) an->checkBeats.size()) + " bars to check");
    const char* labels[] = { "TYPE", "DRIFT", "THE 1", "MARKERS", "CHECK" };
    const juce::String values[] = { type, drift, one, markers, check };
    for (int i = 0; i < 5; ++i)
    {
        const float y = 548.f + (float) i * 20.f;
        g.setFont (tx::font (12.f)); g.setColour (tx::dim());
        g.drawText (labels[i], 40, (int) y, 80, 18, juce::Justification::centredLeft, false);
        g.setColour (i == 4 && hasBeat && an->checkBeats.empty() ? tx::greenMid() : (i == 0 ? tx::green() : tx::text()));
        g.setFont (tx::font (13.f));
        g.drawText (values[i], 122, (int) y, 170, 18, juce::Justification::centredLeft, false);
    }

    // play panel
    const double pos = proc.getPositionSeconds();
    g.setColour (tx::text()); g.setFont (tx::font (34.f));
    g.drawText (fmtTime (pos), 452, 468, 220, 40, juce::Justification::centredLeft, false);
    g.setColour (tx::dim()); g.setFont (tx::font (13.f));
    g.drawText ("of " + fmtTime (track ? track->durationSeconds() : 0.0), 454, 510, 200, 18, juce::Justification::centredLeft, false);
    g.setFont (tx::font (11.f));
    g.drawText ("LEVEL", 520, 556, 80, 14, juce::Justification::centredLeft, false);

    // export panel
    const bool exporting = proc.isExporting();
    const bool failed = stage == TuxiroWarpMeProcessor::Stage::Failed;
    const float pulse = 0.55f + 0.45f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.008f);
    juce::Colour led = failed ? tx::red() : ((busy || exporting) ? juce::Colour (0xffffc24a).withAlpha (pulse)
                                           : (stage == TuxiroWarpMeProcessor::Stage::Ready ? tx::green() : tx::dim()));
    g.setColour (led); g.fillEllipse (728.f, 462.f, 9.f, 9.f);
    g.setFont (tx::font (11.f)); g.setColour (tx::dim());
    g.drawText (failed ? "ERROR" : (busy ? "ANALYSING" : (exporting ? "EXPORTING" : (stage == TuxiroWarpMeProcessor::Stage::Ready ? "READY" : "IDLE"))),
                744, 458, 120, 16, juce::Justification::centredLeft, false);
    if (busy || exporting)
    {
        g.setColour (juce::Colour (0xff050806)); g.fillRoundedRectangle (738.f, 474.f, 242.f, 4.f, 2.f);
        g.setColour (tx::green()); g.fillRoundedRectangle (738.f, 474.f, 242.f * juce::jlimit (0.f, 1.f, proc.getProgress()), 4.f, 2.f);
    }
    g.setColour (failed ? tx::red() : tx::greenMid()); g.setFont (tx::font (12.f));
    g.drawFittedText (proc.getStatus(), 738, 604, 244, 46, juce::Justification::topLeft, 3);
}
