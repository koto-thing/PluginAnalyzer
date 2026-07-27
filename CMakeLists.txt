cmake_minimum_required(VERSION 3.22)

project(PluginAnalyzer
    VERSION 1.0.0
    DESCRIPTION "Audio plugin analysis application"
    LANGUAGES CXX)

include(FetchContent)

# Keep JUCE reproducible across local builds and CI. Developers may override
# FETCHCONTENT_SOURCE_DIR_JUCE to use an existing local checkout.
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.13
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(PluginAnalyzer
    PRODUCT_NAME "PluginAnalyzer"
    COMPANY_NAME "koto-thing"
    BUNDLE_ID "com.kotothing.pluginanalyzer"
)
juce_generate_juce_header(PluginAnalyzer)

target_sources(PluginAnalyzer
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/MainComponent.h
        Source/AnalyzerEngine.cpp
        Source/AnalyzerEngine.h
        Source/AnalysisGraphComponent.cpp
        Source/AnalysisGraphComponent.h
        Source/TestSignalGenerator.h
        Source/OscilloscopeComponent.h
        Source/PluginScannerComponent.h
        Source/SettingsComponent.h
        Source/SSLLookAndFeel.h
)

target_compile_features(PluginAnalyzer PRIVATE cxx_std_17)

target_compile_options(PluginAnalyzer
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

target_compile_definitions(PluginAnalyzer
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1
        $<$<PLATFORM_ID:Darwin>:JUCE_PLUGINHOST_AU=1>
        $<$<PLATFORM_ID:Linux>:JUCE_PLUGINHOST_LADSPA=1>
        $<$<PLATFORM_ID:Linux>:JUCE_PLUGINHOST_LV2=1>
)

target_link_libraries(PluginAnalyzer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    PREFIX "Source"
    FILES
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/MainComponent.h
        Source/AnalyzerEngine.cpp
        Source/AnalyzerEngine.h
        Source/AnalysisGraphComponent.cpp
        Source/AnalysisGraphComponent.h
        Source/TestSignalGenerator.h
        Source/OscilloscopeComponent.h
        Source/PluginScannerComponent.h
        Source/SettingsComponent.h
        Source/SSLLookAndFeel.h
)

include(CTest)
