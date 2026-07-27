cmake_minimum_required(VERSION 3.22)

project(PluginAnalyzer
    VERSION 1.0.0
    DESCRIPTION "Audio plugin analysis application"
    LANGUAGES CXX)

include(FetchContent)
include(GNUInstallDirs)

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
        Source/Application/AnalysisService.h
        Source/Application/AnalysisSession.h
        Source/Domain/AnalysisModel.h
        Source/AnalysisGraphComponent.cpp
        Source/AnalysisGraphComponent.h
        Source/TestSignalGenerator.h
        Source/OscilloscopeComponent.h
        Source/PluginScannerComponent.h
        Source/PluginScannerComponent.cpp
        Source/PluginScanIPC.h
        Source/PluginScanIPC.cpp
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
        Source/Application/AnalysisService.h
        Source/Application/AnalysisSession.h
        Source/Domain/AnalysisModel.h
        Source/AnalysisGraphComponent.cpp
        Source/AnalysisGraphComponent.h
        Source/TestSignalGenerator.h
        Source/OscilloscopeComponent.h
        Source/PluginScannerComponent.h
        Source/PluginScannerComponent.cpp
        Source/PluginScanIPC.h
        Source/PluginScanIPC.cpp
        Source/SettingsComponent.h
        Source/SSLLookAndFeel.h
)

install(TARGETS PluginAnalyzer
    BUNDLE DESTINATION . COMPONENT PluginAnalyzerApp
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT PluginAnalyzerApp
)
install(FILES README.md LICENSE.txt
    DESTINATION .
    COMPONENT PluginAnalyzerApp
)

include(CTest)
option(PLUGIN_ANALYZER_ENABLE_STRESS_TESTS
       "Enable the five-second sustained processing test" OFF)

if(BUILD_TESTING)
    juce_add_console_app(PluginAnalyzerTests
        PRODUCT_NAME "PluginAnalyzer Tests"
    )
    juce_generate_juce_header(PluginAnalyzerTests)

    target_sources(PluginAnalyzerTests
        PRIVATE
            Tests/AnalyzerTests.cpp
            Source/AnalyzerEngine.cpp
            Source/AnalyzerEngine.h
            Source/Application/AnalysisService.h
            Source/Application/AnalysisSession.h
            Source/Domain/AnalysisModel.h
            Source/TestSignalGenerator.h
    )
    target_compile_features(PluginAnalyzerTests PRIVATE cxx_std_17)
    target_compile_options(PluginAnalyzerTests
        PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    )
    target_compile_definitions(PluginAnalyzerTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_PLUGINHOST_VST3=1
    )
    target_link_libraries(PluginAnalyzerTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_test(NAME PluginAnalyzer.Unit COMMAND PluginAnalyzerTests)
    set_tests_properties(PluginAnalyzer.Unit PROPERTIES TIMEOUT 120)
    add_test(NAME PluginAnalyzer.AppSmoke COMMAND PluginAnalyzer --smoke-test)
    set_tests_properties(PluginAnalyzer.AppSmoke PROPERTIES
        TIMEOUT 30
        LABELS "smoke"
    )
    if(PLUGIN_ANALYZER_ENABLE_STRESS_TESTS)
        if(WIN32)
            add_test(NAME PluginAnalyzer.Stress
                COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/Tests/RunLongQualityTest.ps1"
                    -Executable "$<TARGET_FILE:PluginAnalyzerTests>"
            )
        else()
            add_test(NAME PluginAnalyzer.Stress COMMAND PluginAnalyzerTests --stress)
        endif()
        set_tests_properties(PluginAnalyzer.Stress PROPERTIES
            TIMEOUT 30
            LABELS "long;performance"
            RUN_SERIAL TRUE
        )
    endif()
endif()
