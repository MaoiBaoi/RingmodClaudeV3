#pragma once

#include "PluginProcessor.h"
#include "WaveformScope.h"

//==============================================================================
class RingModEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit RingModEditor (RingModProcessor&);
    ~RingModEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& title);
    void setupToggle (juce::ToggleButton&, const juce::String& text);
    void setupCombo (juce::ComboBox&);
    int  getScopeWindowSamples() const;

    RingModProcessor& processor;

    WaveformScope scope;
    ScopeResult   scopeResult;

    juce::Slider thresholdSlider, attackSlider, holdSlider, releaseSlider, mixSlider;
    juce::Slider splitFreqSlider, hpSlider, lpSlider, gainSlider;
    juce::Label  thresholdLabel, attackLabel, holdLabel, releaseLabel, mixLabel;
    juce::Label  splitFreqLabel, hpLabel, lpLabel, gainLabel, statusLabel, modeLabel;

    juce::ComboBox modeBox, scopeWindowBox;
    juce::ToggleButton splitButton, linkButton, listenButton, normButton;

    juce::Rectangle<int> meterArea;
    float displayedDuck = 0.0f;
    bool  lastNorm = true;

    std::unique_ptr<SliderAttachment> thresholdAtt, attackAtt, holdAtt, releaseAtt, mixAtt;
    std::unique_ptr<SliderAttachment> splitFreqAtt, hpAtt, lpAtt, gainAtt;
    std::unique_ptr<ButtonAttachment> splitAtt, linkAtt, listenAtt, normAtt;
    std::unique_ptr<ComboAttachment>  modeAtt, scopeWindowAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RingModEditor)
};
