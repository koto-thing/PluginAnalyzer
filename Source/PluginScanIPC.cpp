#include "PluginScanIPC.h"

#include <condition_variable>

namespace
{
constexpr auto scannerProcessId = "plugin-analyzer-scanner";

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

class ScannerCoordinator final : private juce::ChildProcessCoordinator
{
public:
    ScannerCoordinator()
    {
        launchWorkerProcess(juce::File::getSpecialLocation(juce::File::currentExecutableFile),
                            scannerProcessId, 0, 0);
    }

    /**
     * @brief
     * @param format
     * @param identifier
     * @param result
     * @param shouldExit
     * @return
     */
    bool scan(juce::AudioPluginFormat& format,
              const juce::String& identifier,
              juce::OwnedArray<juce::PluginDescription>& result,
              const std::function<bool()>& shouldExit)
    {
        juce::MemoryBlock request;
        juce::MemoryOutputStream stream(request, true);
        stream.writeString(format.getName());
        stream.writeString(identifier);

        if (!sendMessageToWorker(request))
            return false;

        std::unique_lock<std::mutex> lock(mutex);
        while (!receivedResult && !connectionLost)
        {
            condition.wait_for(lock, std::chrono::milliseconds(50));
            if (shouldExit())
                return true;
        }

        if (connectionLost)
            return false;

        if (response != nullptr)
        {
            for (const auto* item : response->getChildIterator())
            {
                auto description = std::make_unique<juce::PluginDescription>();
                if (description->loadFromXml(*item))
                    result.add(std::move(description));
            }
        }

        receivedResult = false;
        response.reset();
        return true;
    }

private:
    /**
     * @brief
     * @param message
     */
    void handleMessageFromWorker(const juce::MemoryBlock& message) override
    {
        const std::lock_guard<std::mutex> lock(mutex);
        response = juce::parseXML(message.toString());
        receivedResult = true;
        condition.notify_one();
    }

    /**
     * @brief
     */
    void handleConnectionLost() override
    {
        const std::lock_guard<std::mutex> lock(mutex);
        connectionLost = true;
        condition.notify_one();
    }

    std::mutex mutex;
    std::condition_variable condition;
    std::unique_ptr<juce::XmlElement> response;
    bool receivedResult = false;
    bool connectionLost = false;
};

class IsolatedScanner final : public juce::KnownPluginList::CustomScanner
{
public:
    /**
     * @brief
     * @param format
     * @param result
     * @param identifier
     * @return
     */
    bool findPluginTypesFor(juce::AudioPluginFormat& format,
                            juce::OwnedArray<juce::PluginDescription>& result,
                            const juce::String& identifier) override
    {
        if (coordinator == nullptr)
            coordinator = std::make_unique<ScannerCoordinator>();

        if (coordinator->scan(format, identifier, result, [this] { return shouldExit(); }))
            return true;

        // A crash or disconnect blacklists this identifier. The next scan gets
        // a fresh worker, so one bad plug-in does not prevent later discoveries.
        coordinator.reset();
        return false;
    }

    /**
     * @brief
     */
    void scanFinished() override
    {
        coordinator.reset();
    }

private:
    std::unique_ptr<ScannerCoordinator> coordinator;
};
}

/**
 * @brief 
 */
PluginScanWorker::PluginScanWorker()
{
    addPluginFormats(formatManager);
}

/**
 * @brief
 * @param commandLine
 * @return
 */
bool PluginScanWorker::initialise(const juce::String& commandLine)
{
    return initialiseFromCommandLine(commandLine, scannerProcessId);
}

/**
 * @brief
 * @param message
 */
void PluginScanWorker::handleMessageFromCoordinator(const juce::MemoryBlock& message)
{
    if (message.isEmpty())
        return;

    const std::lock_guard<std::mutex> lock(mutex);
    pendingMessages.push(message);
    triggerAsyncUpdate();
}

/**
 * @brief
 */
void PluginScanWorker::handleConnectionLost()
{
    juce::JUCEApplicationBase::quit();
}

/**
 * @brief
 */
void PluginScanWorker::handleAsyncUpdate()
{
    for (;;)
    {
        juce::MemoryBlock message;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            if (pendingMessages.empty())
                return;
            message = std::move(pendingMessages.front());
            pendingMessages.pop();
        }
        sendResults(scan(message));
    }
}

/**
 * @brief
 * @param message
 * @return
 */
juce::OwnedArray<juce::PluginDescription> PluginScanWorker::scan(const juce::MemoryBlock& message)
{
    juce::MemoryInputStream stream(message, false);
    const auto formatName = stream.readString();
    const auto identifier = stream.readString();

    juce::OwnedArray<juce::PluginDescription> result;
    for (auto* format : formatManager.getFormats())
    {
        if (format->getName() == formatName)
        {
            format->findAllTypesForFile(result, identifier);
            break;
        }
    }
    return result;
}

/**
 * @brief
 * @param results
 */
void PluginScanWorker::sendResults(const juce::OwnedArray<juce::PluginDescription>& results)
{
    juce::XmlElement xml("PLUGIN_LIST");
    for (const auto* description : results)
        xml.addChildElement(description->createXml().release());

    const auto text = xml.toString();
    sendMessageToCoordinator({ text.toRawUTF8(), text.getNumBytesAsUTF8() });
}

/**
 * @brief
 * @return
 */
std::unique_ptr<juce::KnownPluginList::CustomScanner> createIsolatedPluginScanner()
{
    return std::make_unique<IsolatedScanner>();
}
