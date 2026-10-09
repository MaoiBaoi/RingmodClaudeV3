#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SignalAnalysis.h"

//==============================================================================
// Ringmod Sidechain (Version 3)
//
//   Der Sidechain (z.B. Kick) wird gleichgerichtet (Betrag) und bestimmt, um
//   wie viel das Hauptsignal (z.B. Bass) Sample fuer Sample abgesenkt wird:
//
//       Ausgang = Signal x (1 - |Sidechain| / Threshold)
//
//   Attack / Hold / Release formen die Absenkkurve (alles 0 = reine Ringmod).
//   Optional wirkt die Absenkung nur unterhalb einer Trennfrequenz (Split).
//==============================================================================
class RingModProcessor : public juce::AudioProcessor
{
public:
    RingModProcessor();
    ~RingModProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RingModSidechain"; }
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

    // Parameter (Regler)
    juce::AudioProcessorValueTreeState apvts;

    // Fuer die Anzeigen im Fenster
    std::atomic<float> sidechainLevel { 0.0f };   // Pegel am Sidechain-Eingang
    std::atomic<float> duckAmount     { 0.0f };   // aktuelle Absenkung 0..1
    SignalRecorder     recorder;                  // Eingangssignale fuer die Wellenform
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Huellkurven-Folger mit Attack / Hold / Release
    struct Follower
    {
        float env = 0.0f;
        int   holdCount = 0;

        float process (float r, float attackCoef, float releaseCoef, int holdSamples) noexcept
        {
            if (r >= env)
            {
                env = env + (r - env) * (1.0f - attackCoef);
                holdCount = holdSamples;
            }
            else if (holdCount > 0)
            {
                --holdCount;
            }
            else
            {
                env = juce::jmax (r, env * releaseCoef);
            }

            return env;
        }

        void reset() noexcept { env = 0.0f; holdCount = 0; }
    };

    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* attackParam    = nullptr;
    std::atomic<float>* holdParam      = nullptr;
    std::atomic<float>* releaseParam   = nullptr;
    std::atomic<float>* mixParam       = nullptr;
    std::atomic<float>* gainParam      = nullptr;
    std::atomic<float>* modeParam      = nullptr;
    std::atomic<float>* splitParam     = nullptr;
    std::atomic<float>* splitFreqParam = nullptr;
    std::atomic<float>* scHpParam      = nullptr;
    std::atomic<float>* scLpParam      = nullptr;
    std::atomic<float>* linkParam      = nullptr;
    std::atomic<float>* listenParam    = nullptr;

    Follower ringFollower[2], compFollower[2];
    float    rmsState[2] = { 0.0f, 0.0f };
    double   currentSampleRate = 44100.0;

    juce::dsp::LinkwitzRileyFilter<float>  crossover;
    juce::dsp::StateVariableTPTFilter<float> scHighpass, scLowpass;

    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> gainSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RingModProcessor)
};
