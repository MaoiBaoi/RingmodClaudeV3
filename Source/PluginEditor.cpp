#include "PluginEditor.h"

namespace
{
    const juce::Colour accent     { 0xffff8a3d };
    const juce::Colour background { 0xff1b1d22 };

    juce::String formatHz (double v)
    {
        return v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
    }
}

//==============================================================================
RingModEditor::RingModEditor (RingModProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    addAndMakeVisible (scope);

    setupKnob (thresholdSlider, thresholdLabel, "THRESHOLD");
    setupKnob (attackSlider,    attackLabel,    "ATTACK");
    setupKnob (holdSlider,      holdLabel,      "HOLD");
    setupKnob (releaseSlider,   releaseLabel,   "RELEASE");
    setupKnob (mixSlider,       mixLabel,       "WET / DRY");
    setupKnob (splitFreqSlider, splitFreqLabel, "SPLIT FREQ");
    setupKnob (hpSlider,        hpLabel,        "SC HOCHPASS");
    setupKnob (lpSlider,        lpLabel,        "SC TIEFPASS");
    setupKnob (gainSlider,      gainLabel,      "VOLUME");

    auto& v = processor.apvts;
    thresholdAtt = std::make_unique<SliderAttachment> (v, "threshold", thresholdSlider);
    attackAtt    = std::make_unique<SliderAttachment> (v, "attack",    attackSlider);
    holdAtt      = std::make_unique<SliderAttachment> (v, "hold",      holdSlider);
    releaseAtt   = std::make_unique<SliderAttachment> (v, "release",   releaseSlider);
    mixAtt       = std::make_unique<SliderAttachment> (v, "mix",       mixSlider);
    splitFreqAtt = std::make_unique<SliderAttachment> (v, "splitFreq", splitFreqSlider);
    hpAtt        = std::make_unique<SliderAttachment> (v, "scHP",      hpSlider);
    lpAtt        = std::make_unique<SliderAttachment> (v, "scLP",      lpSlider);
    gainAtt      = std::make_unique<SliderAttachment> (v, "gain",      gainSlider);

    // Der Attachment ueberschreibt die Textanzeige -> Einheiten danach selbst setzen
    auto unit = [] (juce::Slider& s, const juce::String& suffix, int decimals)
    {
        s.textFromValueFunction = [suffix, decimals] (double x) { return juce::String (x, decimals) + suffix; };
        s.updateText();
    };
    auto hz = [] (juce::Slider& s)
    {
        s.textFromValueFunction = [] (double x) { return formatHz (x); };
        s.updateText();
    };

    unit (thresholdSlider, " dB", 1);
    unit (attackSlider,    " ms", 1);
    unit (holdSlider,      " ms", 0);
    unit (releaseSlider,   " ms", 0);
    unit (mixSlider,       " %",  0);
    unit (gainSlider,      " dB", 1);
    hz (splitFreqSlider);
    hz (hpSlider);
    hz (lpSlider);

    thresholdSlider.setDoubleClickReturnValue (true, -6.0);
    attackSlider.setDoubleClickReturnValue    (true, 0.0);
    holdSlider.setDoubleClickReturnValue      (true, 0.0);
    releaseSlider.setDoubleClickReturnValue   (true, 0.0);
    mixSlider.setDoubleClickReturnValue       (true, 100.0);
    splitFreqSlider.setDoubleClickReturnValue (true, 150.0);
    hpSlider.setDoubleClickReturnValue        (true, 20.0);
    lpSlider.setDoubleClickReturnValue        (true, 20000.0);
    gainSlider.setDoubleClickReturnValue      (true, 0.0);

    // Auswahlfelder: erst Eintraege anlegen, dann verbinden
    setupCombo (modeBox);
    modeBox.addItemList ({ "Ringmod", "Compress", "Combined" }, 1);
    modeAtt = std::make_unique<ComboAttachment> (v, "mode", modeBox);

    setupCombo (scopeWindowBox);
    scopeWindowBox.addItemList ({ "25 ms", "50 ms", "100 ms", "200 ms", "400 ms" }, 1);
    scopeWindowAtt = std::make_unique<ComboAttachment> (v, "scopeWindow", scopeWindowBox);

    modeLabel.setText ("MODE", juce::dontSendNotification);
    modeLabel.setJustificationType (juce::Justification::centred);
    modeLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    addAndMakeVisible (modeLabel);

    setupToggle (splitButton,  "SPLIT");
    setupToggle (linkButton,   "ST. LINK");
    setupToggle (listenButton, "LISTEN");
    setupToggle (normButton,   "NORM");
    splitAtt  = std::make_unique<ButtonAttachment> (v, "split",     splitButton);
    linkAtt   = std::make_unique<ButtonAttachment> (v, "link",      linkButton);
    listenAtt = std::make_unique<ButtonAttachment> (v, "listen",    listenButton);
    normAtt   = std::make_unique<ButtonAttachment> (v, "scopeNorm", normButton);

    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);

    setSize (740, 624);
    startTimerHz (30);
    timerCallback();
}

RingModEditor::~RingModEditor()
{
    stopTimer();
}

void RingModEditor::setupKnob (juce::Slider& s, juce::Label& label, const juce::String& title)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    s.setColour (juce::Slider::rotarySliderFillColourId,    accent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3d46));
    s.setColour (juce::Slider::thumbColourId,               juce::Colours::white);
    s.setColour (juce::Slider::textBoxTextColourId,         juce::Colours::white);
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    label.setText (title, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    addAndMakeVisible (label);
}

void RingModEditor::setupToggle (juce::ToggleButton& b, const juce::String& text)
{
    b.setButtonText (text);
    b.setColour (juce::ToggleButton::textColourId, juce::Colours::white.withAlpha (0.9f));
    b.setColour (juce::ToggleButton::tickColourId, accent);
    b.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff6a6d76));
    addAndMakeVisible (b);
}

void RingModEditor::setupCombo (juce::ComboBox& c)
{
    c.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2d35));
    c.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
    c.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff3a3d46));
    c.setColour (juce::ComboBox::arrowColourId,      accent);
    addAndMakeVisible (c);
}

int RingModEditor::getScopeWindowSamples() const
{
    static constexpr double windowsMs[] = { 25.0, 50.0, 100.0, 200.0, 400.0 };
    const int idx = juce::jlimit (0, 4, (int) processor.apvts.getRawParameterValue ("scopeWindow")->load());
    return (int) (windowsMs[idx] * 0.001 * processor.getCurrentSampleRate());
}

//==============================================================================
void RingModEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    g.setColour (accent);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("RING MOD SIDECHAIN", getLocalBounds().removeFromTop (44), juce::Justification::centred);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (20.0f, 44.0f, (float) getWidth() - 20.0f, 44.0f, 1.0f);

    // Absenk-Anzeige
    auto bar = meterArea.toFloat();
    g.setColour (juce::Colour (0xff2a2d35));
    g.fillRoundedRectangle (bar, 4.0f);

    g.setColour (accent);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, displayedDuck)), 4.0f);

    g.setColour (juce::Colours::white.withAlpha (0.75f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("ABSENKUNG", meterArea, juce::Justification::centred);
}

void RingModEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 0);
    area.removeFromTop (48);

    statusLabel.setBounds (area.removeFromTop (24));

    // Wellenform-Anzeige + Einstellungen darunter
    scope.setBounds (area.removeFromTop (200));
    {
        auto row = area.removeFromTop (28).reduced (0, 3);
        scopeWindowBox.setBounds (row.removeFromRight (90));
        row.removeFromRight (10);
        normButton.setBounds (row.removeFromRight (80));
    }

    meterArea = area.removeFromTop (18);
    area.removeFromTop (10);

    const int rowH = (area.getHeight() - 6) / 2;
    auto row1 = area.removeFromTop (rowH);
    area.removeFromTop (6);
    auto row2 = area;

    auto place = [] (juce::Rectangle<int> r, int cols, int idx, juce::Label& label, juce::Slider& slider)
    {
        auto cell = r.withWidth (r.getWidth() / cols).withX (r.getX() + idx * (r.getWidth() / cols));
        label.setBounds (cell.removeFromTop (20));
        slider.setBounds (cell.reduced (6, 0));
    };

    // Reihe 1: Absenkung formen
    place (row1, 5, 0, thresholdLabel, thresholdSlider);
    place (row1, 5, 1, attackLabel,    attackSlider);
    place (row1, 5, 2, holdLabel,      holdSlider);
    place (row1, 5, 3, releaseLabel,   releaseSlider);
    place (row1, 5, 4, mixLabel,       mixSlider);

    // Reihe 2: Modus + Schalter, Split, Sidechain-Filter, Volume
    {
        auto cell = row2.withWidth (row2.getWidth() / 5);
        cell = cell.reduced (10, 0);
        modeLabel.setBounds (cell.removeFromTop (20));
        modeBox.setBounds (cell.removeFromTop (26));
        cell.removeFromTop (4);
        splitButton.setBounds (cell.removeFromTop (22));
        linkButton.setBounds (cell.removeFromTop (22));
        listenButton.setBounds (cell.removeFromTop (22));
    }

    place (row2, 5, 1, splitFreqLabel, splitFreqSlider);
    place (row2, 5, 2, hpLabel,        hpSlider);
    place (row2, 5, 3, lpLabel,        lpSlider);
    place (row2, 5, 4, gainLabel,      gainSlider);
}

//==============================================================================
void RingModEditor::timerCallback()
{
    displayedDuck = processor.duckAmount.load();
    repaint (meterArea);

    // Wellenform-Anzeige aktualisieren
    const double sr = processor.getCurrentSampleRate();

    const bool norm = normButton.getToggleState();

    if (ScopeAnalyzer::analyse (processor.recorder, sr, getScopeWindowSamples(), scopeResult) || norm != lastNorm)
    {
        lastNorm = norm;
        scope.setData (scopeResult, norm);
    }

    // Status des Sidechains
    auto* sc = processor.getBus (true, 1);

    if (sc == nullptr || ! sc->isEnabled())
    {
        statusLabel.setText ("Sidechain: nicht aktiv", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe05555));
    }
    else if (processor.sidechainLevel.load() > 0.0005f)
    {
        statusLabel.setText (listenButton.getToggleState() ? "Sidechain: Signal liegt an  (LISTEN aktiv: Ausgang = Sidechain)"
                                                           : "Sidechain: Signal liegt an",
                             juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff5fd075));
    }
    else
    {
        statusLabel.setText ("Sidechain: kein Signal", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe0b050));
    }
}
