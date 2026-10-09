#pragma once

#include "CompareProcessor.h"
#include "LevelHistoryView.h"

//==============================================================================
class CompareEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit CompareEditor (CompareProcessor&);
    ~CompareEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void setupCombo (juce::ComboBox&);

    CompareProcessor& processor;

    LevelHistoryView view;
    juce::ComboBox windowBox, smoothBox;
    juce::Label windowLabel, smoothLabel, mainReadout, sideReadout, diffReadout, statusLabel;

    std::unique_ptr<ComboAttachment> windowAtt, smoothAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompareEditor)
};
