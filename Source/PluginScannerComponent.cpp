#include "PluginScannerComponent.h"

namespace
{
constexpr auto pluginListKey = "knownPluginList";

/**
 * @brief
 * @param manager
 */
void addPluginFormats(juce::AudioPluginFormatManager& manager)
{
   #if JUCE_PLUGINHOST_VST3
    manager.addFormat(std::make_unique<juce::VST3PluginFormat>());
   #endif
   #if JUCE_PLUGINHOST_AU && JUCE_MAC
    manager.addFormat(std::make_unique<juce::AudioUnitPluginFormat>());
   #endif
   #if JUCE_PLUGINHOST_LADSPA
    manager.addFormat(std::make_unique<juce::LADSPAPluginFormat>());
   #endif
   #if JUCE_PLUGINHOST_LV2
    manager.addFormat(std::make_unique<juce::LV2PluginFormat>());
   #endif
}
}

/**
 * @brief
 * @param scanPaths
 * @param settings
 */
PluginScannerComponent::PluginScannerComponent(const juce::StringArray& scanPaths,
                                               juce::PropertiesFile& settings)
    : juce::Thread("Plugin scanner"),
      pathsToScan(scanPaths),
      properties(settings),
      progressBar(displayedProgress)
{
    addPluginFormats(formatManager);
    scanResults.setCustomScanner(createIsolatedPluginScanner());
    loadPersistedList();

    addAndMakeVisible(pluginList);
    pluginList.setModel(this);
    pluginList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0d0d0d));

    addAndMakeVisible(scanButton);
    scanButton.onClick = [this] { startScan(); };

    addAndMakeVisible(clearBlacklistButton);
    clearBlacklistButton.onClick = [this]
    {
        knownPluginList.clearBlacklistedFiles();
        scanResults.clearBlacklistedFiles();
        getDeadMansPedalFile().deleteFile();
        if (auto xml = knownPluginList.createXml())
            properties.setValue(pluginListKey, xml.get());
        blacklistLabel.setText("Blacklist: empty", juce::dontSendNotification);
    };

    addAndMakeVisible(progressBar);
    addAndMakeVisible(statusLabel);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setText("Ready — " + juce::String(knownPluginList.getNumTypes())
                        + " plug-ins", juce::dontSendNotification);

    addAndMakeVisible(blacklistLabel);
    blacklistLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    blacklistLabel.setText("Blacklist: "
                           + juce::String(knownPluginList.getBlacklistedFiles().size()),
                           juce::dontSendNotification);

    setSize(700, 500);
}

PluginScannerComponent::~PluginScannerComponent()
{
    stopTimer();
    signalThreadShouldExit();
    stopThread(5000);
    scanResults.scanFinished();
    pluginList.setModel(nullptr);
}

/**
 * @brief
 * @param g
 */
void PluginScannerComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
}

/**
 * @brief
 */
void PluginScannerComponent::resized()
{
    auto area = getLocalBounds().reduced(6);
    auto buttons = area.removeFromTop(34);
    scanButton.setBounds(buttons.removeFromLeft(150).reduced(2));
    clearBlacklistButton.setBounds(buttons.removeFromLeft(150).reduced(2));
    blacklistLabel.setBounds(buttons.reduced(4));
    progressBar.setBounds(area.removeFromTop(22).reduced(2));
    statusLabel.setBounds(area.removeFromTop(28).reduced(2));
    pluginList.setBounds(area.reduced(2));
}

/**
 * @brief
 * @return 
 */
int PluginScannerComponent::getNumRows()
{
    return knownPluginList.getNumTypes();
}

/**
 * @brief
 * @param row
 * @param g
 * @param width
 * @param height
 * @param selected
 */
void PluginScannerComponent::paintListBoxItem(int row, juce::Graphics& g,
                                              int width, int height, bool selected)
{
    if (selected)
        g.fillAll(juce::Colour(0xff00a0ff).withAlpha(0.3f));

    const auto types = knownPluginList.getTypes();
    if (juce::isPositiveAndBelow(row, types.size()))
    {
        const auto& type = types.getReference(row);
        g.setColour(juce::Colours::white);
        g.drawText(type.name + "  ·  " + type.manufacturerName
                   + "  (" + type.pluginFormatName + ")",
                   6, 0, width - 12, height, juce::Justification::centredLeft, true);
    }
}

/**
 * @brief
 * @param row
 */
void PluginScannerComponent::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    const auto types = knownPluginList.getTypes();
    if (onPluginSelected && juce::isPositiveAndBelow(row, types.size()))
        onPluginSelected(types.getReference(row));
}

/**
 * @brief
 */
void PluginScannerComponent::startScan()
{
    if (isThreadRunning())
        return;

    scanResults.clear();
    if (auto saved = properties.getXmlValue(pluginListKey))
        scanResults.recreateFromXml(*saved);
    scanResults.setCustomScanner(createIsolatedPluginScanner());
    juce::PluginDirectoryScanner::applyBlacklistingsFromDeadMansPedal(
        scanResults, getDeadMansPedalFile());

    progress.store(0.0);
    scanFinished.store(false);
    failedFiles.clear();
    scanButton.setEnabled(false);
    pluginList.setEnabled(false);
    statusLabel.setText("Preparing scan…", juce::dontSendNotification);
    startTimerHz(20);
    startThread();
}

/**
 * @brief
 */
void PluginScannerComponent::run()
{
    juce::FileSearchPath searchPath;
    for (const auto& path : pathsToScan)
        searchPath.add(juce::File(path));

    // Empty user paths still use each format's platform defaults.
    for (auto* format : formatManager.getFormats())
    {
        auto formatPaths = searchPath;
        if (formatPaths.getNumPaths() == 0)
            formatPaths = format->getDefaultLocationsToSearch();

        juce::PluginDirectoryScanner scanner(scanResults, *format, formatPaths, true,
                                             getDeadMansPedalFile(), false);
        juce::String name;
        while (!threadShouldExit() && scanner.scanNextFile(true, name))
        {
            {
                const juce::ScopedLock lock(stateLock);
                currentPlugin = name;
            }
            progress.store(scanner.getProgress(), std::memory_order_release);
        }

        {
            const juce::ScopedLock lock(stateLock);
            failedFiles.addArray(scanner.getFailedFiles());
        }
        if (threadShouldExit())
            return;
    }

    scanResults.scanFinished();
    progress.store(1.0, std::memory_order_release);
    scanFinished.store(true, std::memory_order_release);
}

/**
 * @brief
 */
void PluginScannerComponent::timerCallback()
{
    displayedProgress = progress.load(std::memory_order_acquire);
    progressBar.repaint();
    {
        const juce::ScopedLock lock(stateLock);
        if (isThreadRunning())
            statusLabel.setText("Scanning: " + currentPlugin, juce::dontSendNotification);
    }

    if (scanFinished.exchange(false, std::memory_order_acq_rel))
        publishFinishedScan();
}

/**
 * @brief
 */
void PluginScannerComponent::loadPersistedList()
{
    if (auto saved = properties.getXmlValue(pluginListKey))
        knownPluginList.recreateFromXml(*saved);
    juce::PluginDirectoryScanner::applyBlacklistingsFromDeadMansPedal(
        knownPluginList, getDeadMansPedalFile());
}

/**
 * @brief
 */
void PluginScannerComponent::publishFinishedScan()
{
    stopTimer();
    if (auto xml = scanResults.createXml())
    {
        knownPluginList.recreateFromXml(*xml);
        properties.setValue(pluginListKey, xml.get());
        properties.saveIfNeeded();
    }

    const auto failedCount = failedFiles.size();
    statusLabel.setText("Complete — " + juce::String(knownPluginList.getNumTypes())
                        + " plug-ins, " + juce::String(failedCount) + " failed",
                        juce::dontSendNotification);
    blacklistLabel.setText("Blacklist: "
                           + juce::String(knownPluginList.getBlacklistedFiles().size()),
                           juce::dontSendNotification);
    scanButton.setEnabled(true);
    pluginList.setEnabled(true);
    pluginList.updateContent();
    pluginList.repaint();
}

/**
 * @brief
 * @return 
 */
juce::File PluginScannerComponent::getDeadMansPedalFile() const
{
    return properties.getFile().getSiblingFile("PluginAnalyzer.scan-in-progress");
}
