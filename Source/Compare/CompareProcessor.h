#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../SignalAnalysis.h"

//==============================================================================
// SidechainCompare: reiner Analyzer (veraendert das Audio NICHT).
// Zeigt die Lautstaerke des Signals auf der Spur und des Sidechain-Signals
// als zwei Linien im Zeitverlauf an.
//==============================================================================
class CompareProcessor : public juce::AudioProcessor
{
public:
    CompareProcessor();
    ~CompareProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SidechainCompare"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    LevelHistory history;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    double sumMain = 0.0, sumSide = 0.0;
    int    frameCount = 0;
    int    frameLength = 960;   // Samples pro Anzeige-Wert (1/50 s)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompareProcessor)
};
