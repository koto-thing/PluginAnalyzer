#pragma once

#include <JuceHeader.h>
#include "AnalyzerEngine.h"
#include "AnalysisGraphComponent.h"
#include "SSLLookAndFeel.h"
#include "SettingsComponent.h"
#include "PluginScannerComponent.h"

class OscilloscopeComponent;


class MainComponent : public juce::AudioAppComponent,
                      public juce::Timer,
                      public juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    
    void loadPluginClicked();
    
    void showSettingsDialog();
    void applySettings(const SettingsComponent::Settings& newSettings);
    void showPluginBrowser();
    
    void currentTabChanged(int newCurrentTabIndex, const juce::String& newCurrentTabName);

private:
    AnalyzerEngine engine;
    
    SSLLookAndFeel sslLookAndFeel;
    
    juce::TabbedButtonBar tabs { juce::TabbedButtonBar::TabsAtTop };
    
    std::unique_ptr<AnalysisGraphComponent> graphComponent;
    std::unique_ptr<OscilloscopeComponent> scopeComponent;

    juce::Component* currentContentComp = nullptr;
    
    juce::TextButton loadButton { "Load Plugin..." };
    juce::ToggleButton showPhaseButton { "Show Phase" };
    juce::Label pluginNameLabel;
    
    // Settings
    juce::TextButton settingsButton { "Settings" };
    juce::TextButton browserButton { "Browser" };
    SettingsComponent::Settings currentSettings;
    
    // THD/IMD
    juce::Slider amplitudeSlider;
    juce::Label amplitudeLabel;
    juce::Slider frequencySlider;
    juce::Label frequencyLabel;
    juce::Label thdLabel;
    juce::Label thdValueLabel;
    
    // Dynamics
    juce::Label dynamicsLabel;
    juce::Label compressionRatioLabel;
    juce::Label envelopeLabel;
    juce::Label attackTimeLabel;
    
    // Performance
    juce::Label performanceLabel;
    juce::Label avgProcessingTimeLabel;
    juce::Label peakProcessingTimeLabel;
    juce::Label cpuUsageLabel;
    
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
