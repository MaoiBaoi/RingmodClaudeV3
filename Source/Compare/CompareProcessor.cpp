#include "CompareProcessor.h"
#include "CompareEditor.h"

//==============================================================================
CompareProcessor::CompareProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CompareProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "window", 1 }, "Zeitfenster",
                    juce::StringArray { "5 s", "10 s", "30 s", "60 s" }, 1));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "smooth", 1 }, "Glaettung",
                    juce::StringArray { "Schnell (20 ms)", "Mittel (300 ms)", "Langsam (1 s)" }, 1));

    return layout;
}

//==============================================================================
void CompareProcessor::prepareToPlay (double sampleRate, int)
{
    frameLength = juce::jmax (1, (int) (sampleRate / (double) LevelHistory::framesPerSecond));
    sumMain = sumSide = 0.0;
    frameCount = 0;
    history.clear();
}

bool CompareProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    const auto& sc = layouts.getChannelSet (true, 1);

    return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

//==============================================================================
void CompareProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Das Audio selbst bleibt unveraendert (Eingang = Ausgang); wir messen nur.
    auto mainBuffer = getBusBuffer (buffer, true, 0);
    auto scBuffer   = getBusBuffer (buffer, true, 1);

    const int numSamples = buffer.getNumSamples();
    const int numMainCh  = juce::jmin (2, mainBuffer.getNumChannels());

    auto* scBus = getBus (true, 1);
    const bool scEnabled = scBus != nullptr && scBus->isEnabled();
    const int  numScCh   = scEnabled ? juce::jmin (2, scBuffer.getNumChannels()) : 0;

    const float* mainPtr[2] = { nullptr, nullptr };
    const float* scPtr[2]   = { nullptr, nullptr };

    for (int ch = 0; ch < numMainCh; ++ch) mainPtr[ch] = mainBuffer.getReadPointer (ch);
    for (int ch = 0; ch < numScCh; ++ch)   scPtr[ch]   = scBuffer.getReadPointer (ch);

    for (int i = 0; i < numSamples; ++i)
    {
        float pm = 0.0f, ps = 0.0f;

        for (int ch = 0; ch < numMainCh; ++ch)
            pm += mainPtr[ch][i] * mainPtr[ch][i];

        for (int ch = 0; ch < numScCh; ++ch)
            ps += scPtr[ch][i] * scPtr[ch][i];

        sumMain += numMainCh > 0 ? pm / (float) numMainCh : 0.0f;
        sumSide += numScCh   > 0 ? ps / (float) numScCh   : 0.0f;

        if (++frameCount >= frameLength)
        {
            const double meanMain = sumMain / frameCount, meanSide = sumSide / frameCount;

            history.push (meanMain > 1e-10 ? (float) (10.0 * std::log10 (meanMain)) : LevelHistory::floorDb,
                          meanSide > 1e-10 ? (float) (10.0 * std::log10 (meanSide)) : LevelHistory::floorDb);

            sumMain = sumSide = 0.0;
            frameCount = 0;
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* CompareProcessor::createEditor()
{
    return new CompareEditor (*this);
}

void CompareProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CompareProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CompareProcessor();
}
