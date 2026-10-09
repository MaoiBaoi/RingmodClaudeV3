cmake_minimum_required(VERSION 3.22)

# Windows: Laufzeitbibliothek fest einbinden, damit auf deinem Rechner
# keine zusaetzliche "Visual C++ Redistributable" installiert sein muss.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(RingModSidechain VERSION 3.0.0)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

#------------------------------------------------------------------------------
# Plugin 1: RingModSidechain  (der eigentliche Ringmod-Sidechain-Effekt)
#------------------------------------------------------------------------------
juce_add_plugin(RingModSidechain
    COMPANY_NAME             "MyCompany"
    PLUGIN_MANUFACTURER_CODE Mycp
    PLUGIN_CODE              Rmsc
    FORMATS                  VST3
    PRODUCT_NAME             "RingModSidechain"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    VST3_CATEGORIES          Fx Modulation
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

target_sources(RingModSidechain PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/SignalAnalysis.h
    Source/WaveformScope.h
)

target_link_libraries(RingModSidechain
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

#------------------------------------------------------------------------------
# Plugin 2: SidechainCompare  (Analyzer, veraendert das Audio nicht)
#------------------------------------------------------------------------------
juce_add_plugin(SidechainCompare
    COMPANY_NAME             "MyCompany"
    PLUGIN_MANUFACTURER_CODE Mycp
    PLUGIN_CODE              Scmp
    FORMATS                  VST3
    PRODUCT_NAME             "SidechainCompare"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    VST3_CATEGORIES          Fx Analyzer
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

target_sources(SidechainCompare PRIVATE
    Source/Compare/CompareProcessor.cpp
    Source/Compare/CompareProcessor.h
    Source/Compare/CompareEditor.cpp
    Source/Compare/CompareEditor.h
    Source/Compare/LevelHistoryView.h
    Source/SignalAnalysis.h
)

target_link_libraries(SidechainCompare
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

#------------------------------------------------------------------------------
foreach(plugin RingModSidechain SidechainCompare)
    target_compile_features(${plugin} PRIVATE cxx_std_17)
    target_compile_definitions(${plugin} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
endforeach()
