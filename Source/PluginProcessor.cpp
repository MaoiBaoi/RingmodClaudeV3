#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float rmsTimeSeconds = 0.025f;   // Glaettung der "Compress"-Huellkurve

    float coefFromMs (double sampleRate, float ms) noexcept
    {
        return ms <= 0.0f ? 0.0f : std::exp (-1.0f / (float) (sampleRate * 0.001 * (double) ms));
    }
}

//==============================================================================
RingModProcessor::RingModProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    thresholdParam = apvts.getRawParameterValue ("threshold");
    attackParam    = apvts.getRawParameterValue ("attack");
    holdParam      = apvts.getRawParameterValue ("hold");
    releaseParam   = apvts.getRawParameterValue ("release");
    mixParam       = apvts.getRawParameterValue ("mix");
    gainParam      = apvts.getRawParameterValue ("gain");
    modeParam      = apvts.getRawParameterValue ("mode");
    splitParam     = apvts.getRawParameterValue ("split");
    splitFreqParam = apvts.getRawParameterValue ("splitFreq");
    scHpParam      = apvts.getRawParameterValue ("scHP");
    scLpParam      = apvts.getRawParameterValue ("scLP");
    linkParam      = apvts.getRawParameterValue ("link");
    listenParam    = apvts.getRawParameterValue ("listen");
}

juce::AudioProcessorValueTreeState::ParameterLayout RingModProcessor::createLayout()
{
    using juce::ParameterID;
    using Float  = juce::AudioParameterFloat;
    using Range  = juce::NormalisableRange<float>;
    using Attrs  = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<Float> (ParameterID { "threshold", 1 }, "Threshold",
                                         Range (-36.0f, 0.0f, 0.1f), -6.0f, Attrs().withLabel ("dB")));
    layout.add (std::make_unique<Float> (ParameterID { "attack", 1 }, "Attack",
                                         Range (0.0f, 50.0f, 0.1f, 0.5f), 0.0f, Attrs().withLabel ("ms")));
    layout.add (std::make_unique<Float> (ParameterID { "hold", 1 }, "Hold",
                                         Range (0.0f, 200.0f, 0.1f, 0.5f), 0.0f, Attrs().withLabel ("ms")));
    layout.add (std::make_unique<Float> (ParameterID { "release", 1 }, "Release",
                                         Range (0.0f, 500.0f, 0.1f, 0.4f), 0.0f, Attrs().withLabel ("ms")));
    layout.add (std::make_unique<Float> (ParameterID { "mix", 1 }, "Wet/Dry",
                                         Range (0.0f, 100.0f, 0.1f), 100.0f, Attrs().withLabel ("%")));
    layout.add (std::make_unique<Float> (ParameterID { "gain", 1 }, "Volume",
                                         Range (-24.0f, 24.0f, 0.1f), 0.0f, Attrs().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                                         juce::StringArray { "Ringmod", "Compress", "Combined" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "split", 1 }, "Split", false));
    layout.add (std::make_unique<Float> (ParameterID { "splitFreq", 1 }, "Split Frequenz",
                                         Range (30.0f, 1000.0f, 1.0f, 0.4f), 150.0f, Attrs().withLabel ("Hz")));

    layout.add (std::make_unique<Float> (ParameterID { "scHP", 1 }, "Sidechain Hochpass",
                                         Range (20.0f, 500.0f, 1.0f, 0.4f), 20.0f, Attrs().withLabel ("Hz")));
    layout.add (std::make_unique<Float> (ParameterID { "scLP", 1 }, "Sidechain Tiefpass",
                                         Range (200.0f, 20000.0f, 1.0f, 0.3f), 20000.0f, Attrs().withLabel ("Hz")));

    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "link", 1 }, "Stereo Link", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "listen", 1 }, "Listen", false));

    // Einstellungen der Wellenform-Anzeige
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "scopeWindow", 1 }, "Scope Fenster",
                                         juce::StringArray { "25 ms", "50 ms", "100 ms", "200 ms", "400 ms" }, 2));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "scopeNorm", 1 }, "Scope Normalisieren", true));

    return layout;
}

//==============================================================================
void RingModProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    mixSmoothed.reset (sampleRate, 0.03);
    gainSmoothed.reset (sampleRate, 0.03);
    mixSmoothed.setCurrentAndTargetValue (mixParam->load() * 0.01f);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };

    crossover.prepare (spec);
    crossover.reset();

    scHighpass.prepare (spec);
    scHighpass.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    scHighpass.reset();

    scLowpass.prepare (spec);
    scLowpass.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    scLowpass.reset();

    for (int ch = 0; ch < 2; ++ch)
    {
        ringFollower[ch].reset();
        compFollower[ch].reset();
        rmsState[ch] = 0.0f;
    }

    sidechainLevel.store (0.0f);
    duckAmount.store (0.0f);
    recorder.clear();
}

bool RingModProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    const auto& sc = layouts.getChannelSet (true, 1);

    if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

//==============================================================================
void RingModProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBuffer = getBusBuffer (buffer, true, 0);   // Haupteingang (= Ausgang, in-place)
    auto scBuffer   = getBusBuffer (buffer, true, 1);   // Sidechain-Eingang

    const int numSamples = buffer.getNumSamples();
    const int numMainCh  = juce::jmin (2, mainBuffer.getNumChannels());

    auto* scBus = getBus (true, 1);
    const bool scEnabled = scBus != nullptr && scBus->isEnabled();
    const int  numScCh   = scEnabled ? juce::jmin (2, scBuffer.getNumChannels()) : 0;

    float*       mainPtr[2] = { nullptr, nullptr };
    const float* scPtr[2]   = { nullptr, nullptr };

    for (int ch = 0; ch < numMainCh; ++ch)
        mainPtr[ch] = mainBuffer.getWritePointer (ch);

    for (int ch = 0; ch < numScCh; ++ch)
        scPtr[ch] = scBuffer.getReadPointer (ch);

    // ---- Parameter lesen ----
    const double sr = currentSampleRate;

    const float invThreshold = 1.0f / juce::Decibels::decibelsToGain (thresholdParam->load());
    const float attackCoef   = coefFromMs (sr, attackParam->load());
    const float releaseCoef  = coefFromMs (sr, releaseParam->load());
    const int   holdSamples  = (int) (holdParam->load() * 0.001 * sr);
    const float rmsCoef      = std::exp (-1.0f / (float) (sr * rmsTimeSeconds));

    const int  mode   = (int) modeParam->load();          // 0 Ringmod, 1 Compress, 2 Combined
    const bool split  = splitParam->load()  > 0.5f;
    const bool link   = linkParam->load()   > 0.5f;
    const bool listen = listenParam->load() > 0.5f;

    const float hpFreq = scHpParam->load();
    const float lpFreq = scLpParam->load();
    const bool  hpOn   = hpFreq > 20.5f;
    const bool  lpOn   = lpFreq < 19990.0f;

    const float nyquistLimit = (float) (sr * 0.45);
    crossover.setCutoffFrequency (juce::jmin (splitFreqParam->load(), nyquistLimit));
    scHighpass.setCutoffFrequency (juce::jmin (hpFreq, nyquistLimit));
    scLowpass.setCutoffFrequency (juce::jmin (lpFreq, nyquistLimit));

    mixSmoothed.setTargetValue (mixParam->load() * 0.01f);
    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));

    if (numScCh == 0)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            ringFollower[ch].reset();
            compFollower[ch].reset();
            rmsState[ch] = 0.0f;
        }
    }

    const int64_t recPos = recorder.getWritePos();
    float scPeak = 0.0f;
    float maxReduction = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix  = mixSmoothed.getNextValue();
        const float gain = gainSmoothed.getNextValue();

        // ---- Sidechain lesen und fuer die Erkennung filtern ----
        float scRaw[2] = { 0.0f, 0.0f };
        float scF[2]   = { 0.0f, 0.0f };

        for (int c = 0; c < numScCh; ++c)
        {
            scRaw[c] = scPtr[c][i];
            float x = scRaw[c];

            if (hpOn) x = scHighpass.processSample (c, x);
            if (lpOn) x = scLowpass.processSample (c, x);

            scF[c] = x;
            scPeak = juce::jmax (scPeak, std::abs (scRaw[c]));
        }

        // ---- Eingangssignale fuer die Wellenform-Anzeige aufzeichnen ----
        const float inMono = numMainCh > 1 ? 0.5f * (mainPtr[0][i] + mainPtr[1][i]) : mainPtr[0][i];
        const float scMono = numScCh > 1 ? 0.5f * (scRaw[0] + scRaw[1]) : scRaw[0];
        recorder.write (recPos + i, inMono, scMono);

        // ---- Gleichrichten (Betrag) ----
        float r[2] = { 0.0f, 0.0f };

        for (int ch = 0; ch < numMainCh && numScCh > 0; ++ch)
            r[ch] = std::abs (scF[juce::jmin (ch, numScCh - 1)]);

        if (link && numMainCh > 1)
            r[0] = r[1] = juce::jmax (r[0], r[1]);

        for (int ch = 0; ch < numMainCh; ++ch)
        {
            const float dry = mainPtr[ch][i];

            if (listen)
            {
                mainPtr[ch][i] = numScCh > 0 ? scF[juce::jmin (ch, numScCh - 1)] : 0.0f;
                continue;
            }

            float duck = 0.0f;   // 0 = keine Absenkung, 1 = komplett stumm

            if (numScCh > 0)
            {
                // Ringmod-Kurve: folgt dem gleichgerichteten Sidechain
                const float envRing = ringFollower[ch].process (r[ch], attackCoef, releaseCoef, holdSamples);
                const float duckRing = juce::jmin (1.0f, envRing * invThreshold);

                // Compress-Kurve: folgt der geglaetteten Lautstaerke (RMS)
                rmsState[ch] += (r[ch] * r[ch] - rmsState[ch]) * (1.0f - rmsCoef);
                const float level   = std::sqrt (rmsState[ch]) * 1.41421356f;
                const float envComp = compFollower[ch].process (level, attackCoef, releaseCoef, holdSamples);
                const float duckComp = juce::jmin (1.0f, envComp * invThreshold);

                if (mode == 0)       duck = duckRing;
                else if (mode == 1)  duck = duckComp;
                else                 duck = 1.0f - (1.0f - duckRing) * (1.0f - duckComp);
            }

            const float reduction = duck * mix;       // Wet/Dry = Tiefe der Absenkung
            maxReduction = juce::jmax (maxReduction, reduction);

            float out;

            if (split)
            {
                float low = 0.0f, high = 0.0f;
                crossover.processSample (ch, dry, low, high);
                out = low * (1.0f - reduction) + high;   // nur der Bass-Bereich wird abgesenkt
            }
            else
            {
                out = dry * (1.0f - reduction);
            }

            mainPtr[ch][i] = out * gain;
        }
    }

    recorder.publish (recPos + numSamples);

    // Anzeigen: schnell hoch, langsam runter
    sidechainLevel.store (juce::jmax (scPeak, sidechainLevel.load() * 0.9f));
    duckAmount.store (juce::jmax (maxReduction, duckAmount.load() * 0.9f));
}

//==============================================================================
juce::AudioProcessorEditor* RingModProcessor::createEditor()
{
    return new RingModEditor (*this);
}

void RingModProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RingModProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// Wird vom Host aufgerufen, um das Plugin zu erzeugen
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RingModProcessor();
}
