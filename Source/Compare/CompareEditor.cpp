#include "CompareEditor.h"

namespace
{
    const juce::Colour accent     { 0xffff8a3d };
    const juce::Colour background { 0xff1b1d22 };

    const int windowSeconds[] = { 5, 10, 30, 60 };
    const int smoothFrames[]  = { 1, 15, 50 };   // 20 ms, 300 ms, 1 s

    juce::String dbText (float db)
    {
        return db <= -99.0f ? juce::String ("-inf dB") : juce::String (db, 1) + " dB";
    }
}

//==============================================================================
CompareEditor::CompareEditor (CompareProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    addAndMakeVisible (view);

    setupCombo (windowBox);
    windowBox.addItemList ({ "5 s", "10 s", "30 s", "60 s" }, 1);
    windowAtt = std::make_unique<ComboAttachment> (processor.apvts, "window", windowBox);

    setupCombo (smoothBox);
    smoothBox.addItemList ({ "Schnell (20 ms)", "Mittel (300 ms)", "Langsam (1 s)" }, 1);
    smoothAtt = std::make_unique<ComboAttachment> (processor.apvts, "smooth", smoothBox);

    auto setupLabel = [this] (juce::Label& l, const juce::String& text, juce::Colour colour, juce::Justification j)
    {
        l.setText (text, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, colour);
        l.setJustificationType (j);
        addAndMakeVisible (l);
    };

    const auto grey = juce::Colours::white.withAlpha (0.8f);
    setupLabel (windowLabel, "ZEITFENSTER", grey, juce::Justification::centredRight);
    setupLabel (smoothLabel, "GLAETTUNG",   grey, juce::Justification::centredRight);
    setupLabel (mainReadout, "", LevelHistoryView::mainColour, juce::Justification::centredLeft);
    setupLabel (sideReadout, "", LevelHistoryView::sideColour, juce::Justification::centredLeft);
    setupLabel (diffReadout, "", juce::Colours::white, juce::Justification::centredLeft);
    setupLabel (statusLabel, "", grey, juce::Justification::centred);

    for (auto* l : { &mainReadout, &sideReadout, &diffReadout })
        l->setFont (juce::FontOptions (15.0f, juce::Font::bold));

    setSize (760, 420);
    startTimerHz (30);
    timerCallback();
}

CompareEditor::~CompareEditor()
{
    stopTimer();
}

void CompareEditor::setupCombo (juce::ComboBox& c)
{
    c.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2d35));
    c.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
    c.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff3a3d46));
    c.setColour (juce::ComboBox::arrowColourId,      accent);
    addAndMakeVisible (c);
}

//==============================================================================
void CompareEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    g.setColour (accent);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("SIDECHAIN COMPARE", getLocalBounds().removeFromTop (44), juce::Justification::centred);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (20.0f, 44.0f, (float) getWidth() - 20.0f, 44.0f, 1.0f);
}

void CompareEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 0);
    area.removeFromTop (50);

    auto top = area.removeFromTop (30);
    smoothBox.setBounds (top.removeFromRight (150).reduced (0, 2));
    smoothLabel.setBounds (top.removeFromRight (90));
    top.removeFromRight (16);
    windowBox.setBounds (top.removeFromRight (80).reduced (0, 2));
    windowLabel.setBounds (top.removeFromRight (100));
    statusLabel.setBounds (top);

    area.removeFromTop (6);

    auto bottom = area.removeFromBottom (34);
    area.removeFromBottom (6);
    view.setBounds (area);

    const int w = bottom.getWidth() / 3;
    mainReadout.setBounds (bottom.removeFromLeft (w));
    sideReadout.setBounds (bottom.removeFromLeft (w));
    diffReadout.setBounds (bottom);
}

//==============================================================================
void CompareEditor::timerCallback()
{
    const int wIdx = juce::jlimit (0, 3, (int) processor.apvts.getRawParameterValue ("window")->load());
    const int sIdx = juce::jlimit (0, 2, (int) processor.apvts.getRawParameterValue ("smooth")->load());

    const int seconds = windowSeconds[wIdx];
    std::vector<float> m, s;
    processor.history.getSeries (seconds * LevelHistory::framesPerSecond, smoothFrames[sIdx], m, s);

    const float nowMain = m.back(), nowSide = s.back();
    view.setData (std::move (m), std::move (s), (float) seconds);

    mainReadout.setText ("SIGNAL  " + dbText (nowMain), juce::dontSendNotification);
    sideReadout.setText ("SIDECHAIN  " + dbText (nowSide), juce::dontSendNotification);

    if (nowMain > -99.0f && nowSide > -99.0f)
    {
        const float diff = nowSide - nowMain;
        diffReadout.setText ("Sidechain ist " + juce::String (std::abs (diff), 1) + " dB "
                             + (diff >= 0.0f ? "lauter" : "leiser"), juce::dontSendNotification);
    }
    else
    {
        diffReadout.setText ("", juce::dontSendNotification);
    }

    auto* sc = processor.getBus (true, 1);

    if (sc == nullptr || ! sc->isEnabled())
    {
        statusLabel.setText ("Sidechain: nicht aktiv", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe05555));
    }
    else if (nowSide > -80.0f)
    {
        statusLabel.setText ("Sidechain: Signal liegt an", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff5fd075));
    }
    else
    {
        statusLabel.setText ("Sidechain: kein Signal", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe0b050));
    }
}
