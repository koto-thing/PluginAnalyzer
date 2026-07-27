#pragma once

#include <JuceHeader.h>

// The scanner worker is the same executable launched in JUCE child-process
// mode. Keeping plug-in discovery in this process prevents a faulty module from
// taking down the analyser UI.
class PluginScanWorker final : private juce::ChildProcessWorker,
                               private juce::AsyncUpdater
{
public:
    PluginScanWorker();
    bool initialise(const juce::String& commandLine);

private:
    void handleMessageFromCoordinator(const juce::MemoryBlock&) override;
    void handleConnectionLost() override;
    void handleAsyncUpdate() override;
    juce::OwnedArray<juce::PluginDescription> scan(const juce::MemoryBlock&);
    void sendResults(const juce::OwnedArray<juce::PluginDescription>&);

    std::mutex mutex;
    std::queue<juce::MemoryBlock> pendingMessages;
    juce::AudioPluginFormatManager formatManager;
};

std::unique_ptr<juce::KnownPluginList::CustomScanner> createIsolatedPluginScanner();

