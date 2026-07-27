#pragma once

#include <JuceHeader.h>
#include "PluginScanIPC.h"

class PluginScannerComponent final : public juce::Component,
                                     public juce::ListBoxModel,
                                     private juce::Thread,
                                     private juce::Timer
{
public:
    PluginScannerComponent(const juce::StringArray& scanPaths,
                           juce::PropertiesFile& properties);
    ~PluginScannerComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void(const juce::PluginDescription&)> onPluginSelected;

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;

private:
    void startScan();
    void run() override;
    void timerCallback() override;
    void loadPersistedList();
    void publishFinishedScan();
    juce::File getDeadMansPedalFile() const;

    juce::StringArray pathsToScan;
    juce::PropertiesFile& properties;
    juce::ListBox pluginList;
    juce::TextButton scanButton { "Scan Plugins" };
    juce::TextButton clearBlacklistButton { "Clear Blacklist" };
    juce::ProgressBar progressBar;
    juce::Label statusLabel;
    juce::Label blacklistLabel;

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    juce::KnownPluginList scanResults;

    std::atomic<double> progress { 0.0 };
    double displayedProgress = 0.0;
    std::atomic<bool> scanFinished { false };
    juce::CriticalSection stateLock;
    juce::String currentPlugin;
    juce::StringArray failedFiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginScannerComponent)
};
