#include "AnalyzerEngine.h"
#include <algorithm>
#include <complex>

namespace
{
constexpr double imdLowFrequency = 250.0;
constexpr double imdHighFrequency = 8000.0;

/**
 * @brief 
 * @param mode
 * @return 
 */
bool modeRunsContinuously(AnalyzerEngine::AnalysisMode mode)
{
    return mode != AnalyzerEngine::AnalysisMode::Linear;
}

/**
 * @brief
 * @param mode
 * @return
 */
bool modeUsesWindow(AnalyzerEngine::AnalysisMode mode)
{
    return mode != AnalyzerEngine::AnalysisMode::Linear
        && mode != AnalyzerEngine::AnalysisMode::Performance;
}

/**
 * @brief 
 * @param frequency
 * @param sampleRate
 * @param fftSize
 * @return 
 */
double quantiseToFFTBin(double frequency, double sampleRate, int fftSize)
{
    const auto bin = juce::jlimit(1, fftSize / 2 - 1,
                                  juce::roundToInt(frequency * fftSize / sampleRate));
    return bin * sampleRate / fftSize;
}

/**
 * @brief 
 * @param sortedValues
 * @param proportion
 * @return 
 */
float percentile(const std::vector<float>& sortedValues, double proportion)
{
    if (sortedValues.empty())
        return 0.0f;
    const auto position = proportion * static_cast<double>(sortedValues.size() - 1);
    const auto lower = static_cast<size_t>(std::floor(position));
    const auto upper = juce::jmin(lower + 1, sortedValues.size() - 1);
    const auto fraction = static_cast<float>(position - static_cast<double>(lower));
    return sortedValues[lower] + fraction * (sortedValues[upper] - sortedValues[lower]);
}
}

AnalyzerEngine::AnalyzerEngine()
    : juce::Thread("PluginAnalyzer analysis")
{
#if JUCE_PLUGINHOST_VST3
    formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
#endif
#if JUCE_MAC && JUCE_PLUGINHOST_AU
    formatManager.addFormat(std::make_unique<juce::AudioUnitPluginFormat>());
#endif
#if JUCE_LINUX && JUCE_PLUGINHOST_LADSPA
    formatManager.addFormat(std::make_unique<juce::LADSPAPluginFormat>());
#endif
#if JUCE_LINUX && JUCE_PLUGINHOST_LV2
    formatManager.addFormat(std::make_unique<juce::LV2PluginFormat>());
#endif

    analysisQueue.resize(analysisFifoSize);
    scopeData.resize(scopeFifoSize, 0.0f);
    workerResult.harmonicLevels.resize(10, 0.0f);
    configureWorkerFFT(workerFFTOrder);
    publishSnapshot();
    startThread(juce::Thread::Priority::normal);
}

AnalyzerEngine::~AnalyzerEngine()
{
    signalThreadShouldExit();
    notify();
    stopThread(3000);
    unloadPlugin();
}

/**
 * @brief
 * @param order
 */
void AnalyzerEngine::configureWorkerFFT(int order)
{
    workerFFTOrder = juce::jlimit(8, 15, order);
    workerFFTSize = 1 << workerFFTOrder;
    forwardFFT = std::make_unique<juce::dsp::FFT>(workerFFTOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(
        workerFFTSize, juce::dsp::WindowingFunction<float>::hann);
    complexDataL.assign(static_cast<size_t>(workerFFTSize * 2), 0.0f);
    complexDataR.assign(static_cast<size_t>(workerFFTSize * 2), 0.0f);
    complexInput.assign(static_cast<size_t>(workerFFTSize * 2), 0.0f);
    accumulationBufferInput.assign(static_cast<size_t>(workerFFTSize), 0.0f);
    accumulationBufferL.assign(static_cast<size_t>(workerFFTSize), 0.0f);
    accumulationBufferR.assign(static_cast<size_t>(workerFFTSize), 0.0f);
    averagedInputPower.assign(static_cast<size_t>(workerFFTSize / 2), 0.0);
    averagedOutputPowerL.assign(static_cast<size_t>(workerFFTSize / 2), 0.0);
    averagedOutputPowerR.assign(static_cast<size_t>(workerFFTSize / 2), 0.0);
    workerResult.magnitudeSpectrumL.assign(static_cast<size_t>(workerFFTSize / 2), -120.0f);
    workerResult.magnitudeSpectrumR.assign(static_cast<size_t>(workerFFTSize / 2), -120.0f);
    workerResult.phaseSpectrumL.assign(static_cast<size_t>(workerFFTSize / 2), 0.0f);
    workerResult.phaseSpectrumR.assign(static_cast<size_t>(workerFFTSize / 2), 0.0f);
    accumulationIndex = 0;
    spectralAverageCount = 0;
}

/**
 * @brief
 * @param blockSize
 */
void AnalyzerEngine::resizeAudioBuffers(int blockSize)
{
    pluginProcessingBuffer.setSize(juce::jmax(pluginInputChannels, pluginOutputChannels),
                                   blockSize, false, true, false);
}

/**
 * @brief
 * @param sampleRate
 * @param blockSize
 */
void AnalyzerEngine::prepare(double sampleRate, int blockSize)
{
    jassert(sampleRate > 0.0 && blockSize > 0);
    const juce::ScopedLock lock(pluginLock);
    activeSampleRate.store(sampleRate, std::memory_order_release);
    activeBlockSize.store(blockSize, std::memory_order_release);
    signalGenerator.prepare(sampleRate, blockSize);
    resizeAudioBuffers(blockSize);

    if (pluginInstance)
    {
        if (pluginIsPrepared)
            pluginInstance->releaseResources();
        pluginInstance->setRateAndBufferSizeDetails(sampleRate, blockSize);
        pluginInstance->prepareToPlay(sampleRate, blockSize);
        pluginLatencySamples.store(pluginInstance->getLatencySamples(),
                                   std::memory_order_release);
        pluginIsPrepared = true;
    }
    triggerImpulseAnalysis();
    notify();
}

/**
 * @brief  
 */
void AnalyzerEngine::releaseResources()
{
    const juce::ScopedLock lock(pluginLock);
    if (pluginInstance && pluginIsPrepared)
    {
        pluginInstance->releaseResources();
        pluginIsPrepared = false;
    }
}

/**
 * @brief
 * @param newBlockSize
 */
void AnalyzerEngine::setBlockSize(int newBlockSize)
{
    if (newBlockSize > 0)
        activeBlockSize.store(newBlockSize, std::memory_order_release);
}

/**
 * @brief
 * @param order
 */
void AnalyzerEngine::setFFTOrder(int order)
{
    if (order < 8 || order > 15)
        return;
    requestedFFTOrder.store(order, std::memory_order_release);
    requestedGeneration.fetch_add(1, std::memory_order_acq_rel);
    notify();
}

/**
 * @brief 
 * @param file
 * @return
 */
bool AnalyzerEngine::loadPlugin(const juce::File& file)
{
    if (!file.exists())
    {
        const juce::ScopedLock lock(pluginLock);
        lastPluginError = "The selected plug-in does not exist:\n" + file.getFullPathName();
        return false;
    }

    juce::OwnedArray<juce::PluginDescription> found;
    for (auto* format : formatManager.getFormats())
        format->findAllTypesForFile(found, file.getFullPathName());

    if (found.isEmpty())
    {
        const juce::ScopedLock lock(pluginLock);
        lastPluginError = "No supported plug-in type was found at:\n" + file.getFullPathName();
        return false;
    }

    return loadPlugin(*found[0]);
}

/**
 * @brief 
 * @param description
 * @return
 */
bool AnalyzerEngine::loadPlugin(const juce::PluginDescription& description)
{
    juce::String error;
    const auto sampleRate = activeSampleRate.load(std::memory_order_acquire);
    const auto blockSize = activeBlockSize.load(std::memory_order_acquire);
    auto candidate = formatManager.createPluginInstance(description, sampleRate, blockSize, error);
    if (!candidate)
    {
        const juce::ScopedLock lock(pluginLock);
        lastPluginError = error.isNotEmpty() ? error : "The plug-in instance could not be created.";
        return false;
    }

    return loadProcessor(std::move(candidate));
}

/**
 * @brief
 * @param candidate
 * @return
 */
bool AnalyzerEngine::loadProcessor(std::unique_ptr<juce::AudioProcessor> candidate)
{
    if (!candidate)
    {
        const juce::ScopedLock lock(pluginLock);
        lastPluginError = "The processor instance is null.";
        return false;
    }

    const auto sampleRate = activeSampleRate.load(std::memory_order_acquire);
    const auto blockSize = activeBlockSize.load(std::memory_order_acquire);
    const auto inputs = candidate->getTotalNumInputChannels();
    const auto outputs = candidate->getTotalNumOutputChannels();
    if (inputs < 1 || inputs > 2 || outputs < 1 || outputs > 2)
    {
        const juce::ScopedLock lock(pluginLock);
        lastPluginError = "Unsupported bus layout for \"" + candidate->getName()
            + "\". PluginAnalyzer supports mono or stereo effects.\n\nDetected: "
            + juce::String(inputs) + " input(s), " + juce::String(outputs) + " output(s).";
        return false;
    }

    candidate->setNonRealtime(false);
    candidate->setRateAndBufferSizeDetails(sampleRate, blockSize);
    candidate->prepareToPlay(sampleRate, blockSize);
    juce::AudioBuffer<float> newBuffer(juce::jmax(inputs, outputs), blockSize);
    newBuffer.clear();

    {
        const juce::ScopedLock lock(pluginLock);
        if (pluginInstance && pluginIsPrepared)
            pluginInstance->releaseResources();
        pluginInstance = std::move(candidate);
        pluginProcessingBuffer = std::move(newBuffer);
        pluginInputChannels = inputs;
        pluginOutputChannels = outputs;
        pluginLatencySamples.store(pluginInstance->getLatencySamples(),
                                   std::memory_order_release);
        pluginIsPrepared = true;
        lastPluginError.clear();
    }
    triggerImpulseAnalysis();
    return true;
}

/**
 * @brief 
 */
void AnalyzerEngine::unloadPlugin()
{
    {
        const juce::ScopedLock lock(pluginLock);
        if (pluginInstance && pluginIsPrepared)
            pluginInstance->releaseResources();
        pluginInstance.reset();
        pluginProcessingBuffer.setSize(0, 0);
        pluginInputChannels = pluginOutputChannels = 0;
        pluginLatencySamples.store(0, std::memory_order_release);
        pluginIsPrepared = false;
        lastPluginError.clear();
    }
}

/**
 * @brief
 * @return
 */
juce::String AnalyzerEngine::getPluginName() const
{
    const juce::ScopedLock lock(pluginLock);
    return pluginInstance ? pluginInstance->getName() : "No Plugin Loaded";
}

/**
 * @brief UI表示用のプラグイン名を標準文字列で取得
 * @return ロード中のプラグイン名
 */
std::string AnalyzerEngine::getPluginDisplayName() const
{
    return getPluginName().toStdString();
}

/**
 * @brief
 * @return
 */
juce::String AnalyzerEngine::getLastPluginError() const
{
    const juce::ScopedLock lock(pluginLock);
    return lastPluginError;
}

/**
 * @brief 
 */
void AnalyzerEngine::triggerImpulseAnalysis()
{
    requestedGeneration.fetch_add(1, std::memory_order_acq_rel);
}

/**
 * @brief
 * @param mode
 */
void AnalyzerEngine::setAnalysisMode(AnalysisMode mode)
{
    if (requestedMode.exchange(mode, std::memory_order_acq_rel) != mode)
        triggerImpulseAnalysis();
}

/**
 * @brief 
 * @param amplitude
 */
void AnalyzerEngine::setInputAmplitude(float amplitude)
{
    requestedAmplitude.store(juce::jlimit(0.0f, 1.0f, amplitude), std::memory_order_release);
}

/**
 * @brief 
 * @param frequency
 */
void AnalyzerEngine::setTestFrequency(double frequency)
{
    const auto clamped = juce::jlimit(20.0, 20000.0, frequency);
    if (requestedFrequency.exchange(clamped, std::memory_order_acq_rel) != clamped)
        triggerImpulseAnalysis();
}

/**
 * @brief
 * @return
 */
std::shared_ptr<const AnalyzerEngine::AnalysisSnapshot> AnalyzerEngine::getAnalysisSnapshot() const
{
    return std::atomic_load_explicit(&publishedSnapshot, std::memory_order_acquire);
}

/**
 * @brief
 * @param buffer
 */
void AnalyzerEngine::processAudio(juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
        return;

    const auto generation = requestedGeneration.load(std::memory_order_acquire);
    const auto mode = requestedMode.load(std::memory_order_acquire);
    if (generation != audioGeneration)
    {
        audioGeneration = generation;
        audioIsAnalyzing = true;
        signalGenerator.reset();
        thdSweepSamplesAtFrequency = 0;
        thdSweepBin = 0;
    }
    if (modeRunsContinuously(mode))
        audioIsAnalyzing = true;
    else if (completedLinearGeneration.load(std::memory_order_acquire) == generation)
        audioIsAnalyzing = false;
    if (!audioIsAnalyzing)
        return;

    signalGenerator.setAmplitude(requestedAmplitude.load(std::memory_order_relaxed));
    const auto sampleRate = activeSampleRate.load(std::memory_order_relaxed);
    const auto fftSize = 1 << requestedFFTOrder.load(std::memory_order_relaxed);
    auto measurementFrequency = requestedFrequency.load(std::memory_order_relaxed);

    if (mode == AnalysisMode::Harmonic)
        measurementFrequency = quantiseToFFTBin(measurementFrequency, sampleRate, fftSize);
    else if (mode == AnalysisMode::THDSweep)
    {
        const auto progress = static_cast<double>(thdSweepBin)
                            / static_cast<double>(thdSweepSteps - 1);
        measurementFrequency = 20.0 * std::pow(1000.0, progress);
        measurementFrequency = quantiseToFFTBin(measurementFrequency, sampleRate, fftSize);
    }
    signalGenerator.setFrequency(measurementFrequency);

    TestSignalGenerator::SignalType signalType = TestSignalGenerator::SignalType::Impulse;
    switch (mode)
    {
        case AnalysisMode::Harmonic:
        case AnalysisMode::THDSweep:
        case AnalysisMode::Performance: signalType = TestSignalGenerator::SignalType::Sine; break;
        case AnalysisMode::IMD:
            signalGenerator.setIMDFrequencies(
                quantiseToFFTBin(imdLowFrequency, sampleRate, fftSize),
                quantiseToFFTBin(imdHighFrequency, sampleRate, fftSize));
            signalGenerator.setIMDAmplitudeRatio(1.0);
            signalType = TestSignalGenerator::SignalType::IMDDualTone;
            break;
        case AnalysisMode::WhiteNoise: signalType = TestSignalGenerator::SignalType::WhiteNoise; break;
        case AnalysisMode::SineSweep: signalType = TestSignalGenerator::SignalType::SineSweep; break;
        case AnalysisMode::Dynamics: signalType = TestSignalGenerator::SignalType::Ramp; break;
        case AnalysisMode::Hammerstein: signalType = TestSignalGenerator::SignalType::SineSweep; break;
        case AnalysisMode::Linear: break;
    }

    signalGenerator.fillBuffer(buffer, signalType, 0);
    for (int channel = 1; channel < numChannels; ++channel)
        buffer.copyFrom(channel, 0, buffer, 0, 0, numSamples);

    // Reserve the FIFO space before processing so the original input can be
    // copied directly into its final, preallocated location.
    int write1 = 0, size1 = 0, write2 = 0, size2 = 0;
    analysisFifo.prepareToWrite(numSamples, write1, size1, write2, size2);
    auto captureInput = [&](int start, int count, int sourceOffset)
    {
        const auto* input = buffer.getReadPointer(0) + sourceOffset;
        for (int i = 0; i < count; ++i)
        {
            auto& sample = analysisQueue[static_cast<size_t>(start + i)];
            sample.input = input[i];
            sample.mode = mode;
            sample.generation = generation;
            sample.measurementFrequency = static_cast<float>(measurementFrequency);
        }
    };
    captureInput(write1, size1, 0);
    captureInput(write2, size2, size1);

    double processingTimeMs = 0.0;
    bool processedPlugin = false;
    if (pluginLock.tryEnter())
    {
        if (pluginInstance && pluginIsPrepared
            && numSamples <= activeBlockSize.load(std::memory_order_relaxed))
        {
            const auto channels = juce::jmax(pluginInputChannels, pluginOutputChannels);
            pluginProcessingBuffer.setSize(channels, numSamples, false, false, true);
            pluginProcessingBuffer.clear();
            for (int channel = 0; channel < pluginInputChannels; ++channel)
                pluginProcessingBuffer.copyFrom(channel, 0, buffer,
                    juce::jmin(channel, numChannels - 1), 0, numSamples);

            const auto start = juce::Time::getMillisecondCounterHiRes();
            juce::MidiBuffer midi;
            pluginInstance->processBlock(pluginProcessingBuffer, midi);
            processingTimeMs = juce::Time::getMillisecondCounterHiRes() - start;
            processedPlugin = true;

            buffer.clear();
            for (int channel = 0; channel < numChannels; ++channel)
                buffer.copyFrom(channel, 0, pluginProcessingBuffer,
                    juce::jmin(channel, pluginOutputChannels - 1), 0, numSamples);
        }
        pluginLock.exit();
    }

    auto captureOutput = [&](int start, int count, int sourceOffset)
    {
        const auto* left = buffer.getReadPointer(0) + sourceOffset;
        const auto* right = buffer.getReadPointer(juce::jmin(1, numChannels - 1)) + sourceOffset;
        for (int i = 0; i < count; ++i)
        {
            auto& sample = analysisQueue[static_cast<size_t>(start + i)];
            sample.outputL = left[i];
            sample.outputR = right[i];
        }
    };
    captureOutput(write1, size1, 0);
    captureOutput(write2, size2, size1);
    analysisFifo.finishedWrite(size1 + size2);
    if (size1 + size2 < numSamples)
        droppedAnalysisSamples.fetch_add(static_cast<uint64_t>(numSamples - size1 - size2),
                                         std::memory_order_relaxed);

    if (mode == AnalysisMode::Performance && processedPlugin)
    {
        int p1 = 0, n1 = 0, p2 = 0, n2 = 0;
        performanceFifo.prepareToWrite(1, p1, n1, p2, n2);
        if (n1 == 1)
        {
            performanceQueue[static_cast<size_t>(p1)] =
                { static_cast<float>(processingTimeMs), numSamples };
            performanceFifo.finishedWrite(1);
        }
        else
        {
            droppedPerformanceRecords.fetch_add(1, std::memory_order_relaxed);
        }
    }

    if (mode == AnalysisMode::THDSweep)
    {
        thdSweepSamplesAtFrequency += numSamples;
        if (thdSweepSamplesAtFrequency >= fftSize)
        {
            thdSweepSamplesAtFrequency -= fftSize;
            thdSweepBin = (thdSweepBin + 1) % thdSweepSteps;
        }
    }

    addToScopeFifo(buffer.getReadPointer(0), numSamples);
    notify();
}

/**
 * @brief
 */
void AnalyzerEngine::run()
{
    while (!threadShouldExit())
    {
        drainAnalysisFifo();
        drainPerformanceFifo();
        wait(20);
    }
    drainAnalysisFifo();
    drainPerformanceFifo();
}

/**
 * @brief
 */
void AnalyzerEngine::drainAnalysisFifo()
{
    for (;;)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        analysisFifo.prepareToRead(4096, start1, size1, start2, size2);
        if (size1 + size2 == 0)
            break;
        processAnalysisSamples(analysisQueue.data() + start1, size1);
        processAnalysisSamples(analysisQueue.data() + start2, size2);
        analysisFifo.finishedRead(size1 + size2);
    }
}

/**
 * @brief
 * @param samples
 * @param count
 */
void AnalyzerEngine::processAnalysisSamples(const AnalysisSample* samples, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const auto& sample = samples[i];
        const auto desiredOrder = requestedFFTOrder.load(std::memory_order_acquire);
        if (sample.generation != workerGeneration || desiredOrder != workerFFTOrder)
        {
            if (desiredOrder != workerFFTOrder)
                configureWorkerFFT(desiredOrder);
            resetWorkerAnalysis(sample.generation);
        }

        // A producer may enqueue more than one FFT frame before the worker
        // publishes completion. Only the first frame contains the impulse;
        // later frames are silence and must not overwrite the valid transfer.
        if (sample.mode == AnalysisMode::Linear
            && completedLinearGeneration.load(std::memory_order_acquire) == sample.generation)
            continue;

        if (sample.mode == AnalysisMode::Dynamics)
            analyzeDynamicsSample(sample.input, sample.outputL);
        else if (sample.mode == AnalysisMode::Hammerstein)
            analyzeEnvelopeSample(sample.outputL);

        accumulationBufferInput[static_cast<size_t>(accumulationIndex)] = sample.input;
        accumulationBufferL[static_cast<size_t>(accumulationIndex)] = sample.outputL;
        accumulationBufferR[static_cast<size_t>(accumulationIndex)] = sample.outputR;
        completedFrameFrequency = sample.measurementFrequency;
        if (++accumulationIndex == workerFFTSize)
        {
            processCompletedFFT(sample.mode);
            accumulationIndex = 0;
        }
    }
}

/**
 * @brief
 * @param generation
 */
void AnalyzerEngine::resetWorkerAnalysis(uint32_t generation)
{
    workerGeneration = generation;
    accumulationIndex = 0;
    std::fill(accumulationBufferL.begin(), accumulationBufferL.end(), 0.0f);
    std::fill(accumulationBufferR.begin(), accumulationBufferR.end(), 0.0f);
    std::fill(accumulationBufferInput.begin(), accumulationBufferInput.end(), 0.0f);
    std::fill(averagedInputPower.begin(), averagedInputPower.end(), 0.0);
    std::fill(averagedOutputPowerL.begin(), averagedOutputPowerL.end(), 0.0);
    std::fill(averagedOutputPowerR.begin(), averagedOutputPowerR.end(), 0.0);
    spectralAverageCount = 0;
    workerResult.dynamics = {};
    workerResult.envelope = {};
    workerResult.thdSweepFrequencies.clear();
    workerResult.thdSweepValues.clear();
    workerResult.thd = 0.0f;
    workerResult.thdPlusN = 0.0f;
    workerResult.imd = 0.0f;
    dynamicsDecimationCounter = 0;
    envelopeDecimationCounter = 0;
    dynamicsInputSquared = dynamicsOutputSquared = 0.0;
    dynamicsWindowSamples = 0;
    envelopeTimeSeconds = 0.0;
    envelopePrevious = 0.0f;
}

/**
 * @brief
 * @param mode
 */
void AnalyzerEngine::processCompletedFFT(AnalysisMode mode)
{
    std::fill(complexInput.begin(), complexInput.end(), 0.0f);
    std::fill(complexDataL.begin(), complexDataL.end(), 0.0f);
    std::fill(complexDataR.begin(), complexDataR.end(), 0.0f);
    std::copy(accumulationBufferInput.begin(), accumulationBufferInput.end(), complexInput.begin());
    std::copy(accumulationBufferL.begin(), accumulationBufferL.end(), complexDataL.begin());
    std::copy(accumulationBufferR.begin(), accumulationBufferR.end(), complexDataR.begin());
    if (modeUsesWindow(mode))
    {
        window->multiplyWithWindowingTable(complexInput.data(), workerFFTSize);
        window->multiplyWithWindowingTable(complexDataL.data(), workerFFTSize);
        window->multiplyWithWindowingTable(complexDataR.data(), workerFFTSize);
    }
    forwardFFT->performRealOnlyForwardTransform(complexInput.data());
    forwardFFT->performRealOnlyForwardTransform(complexDataL.data());
    forwardFFT->performRealOnlyForwardTransform(complexDataR.data());

    const auto sampleRate = activeSampleRate.load(std::memory_order_acquire);
    workerResult.sampleRate = sampleRate;
    const auto coherentGain = modeUsesWindow(mode) ? 0.5 : 1.0;
    const auto amplitudeScale = 2.0 / (static_cast<double>(workerFFTSize) * coherentGain);
    auto component = [](const std::vector<float>& data, int bin)
    {
        return std::complex<double>(
            bin == 0 ? data[0] : data[static_cast<size_t>(2 * bin)],
            bin == 0 ? 0.0 : data[static_cast<size_t>(2 * bin + 1)]);
    };

    int latency = 0;
    if (mode == AnalysisMode::Linear)
    {
        const auto inputPeak = static_cast<int>(std::distance(
            accumulationBufferInput.begin(),
            std::max_element(accumulationBufferInput.begin(), accumulationBufferInput.end(),
                [](float a, float b) { return std::abs(a) < std::abs(b); })));
        const auto outputPeak = static_cast<int>(std::distance(
            accumulationBufferL.begin(),
            std::max_element(accumulationBufferL.begin(), accumulationBufferL.end(),
                [](float a, float b) { return std::abs(a) < std::abs(b); })));
        const auto detectedLatency = juce::jlimit(0, workerFFTSize - 1,
                                                  outputPeak - inputPeak);
        const auto reportedLatency = juce::jlimit(
            0, workerFFTSize - 1, pluginLatencySamples.load(std::memory_order_acquire));
        latency = reportedLatency > 0 ? reportedLatency : detectedLatency;
    }
    workerResult.latencySamples = latency;

    const auto isTransferMeasurement = mode == AnalysisMode::Linear
                                    || mode == AnalysisMode::WhiteNoise
                                    || mode == AnalysisMode::SineSweep
                                    || mode == AnalysisMode::Hammerstein;
    if (mode == AnalysisMode::WhiteNoise)
        ++spectralAverageCount;

    for (int bin = 0; bin < workerFFTSize / 2; ++bin)
    {
        const auto input = component(complexInput, bin);
        const auto left = component(complexDataL, bin);
        const auto right = component(complexDataR, bin);
        std::complex<double> displayLeft = left * amplitudeScale;
        std::complex<double> displayRight = right * amplitudeScale;

        if (isTransferMeasurement && std::norm(input) > 1.0e-20)
        {
            if (mode == AnalysisMode::WhiteNoise)
            {
                constexpr double averaging = 0.9;
                const auto index = static_cast<size_t>(bin);
                const auto blend = spectralAverageCount == 1 ? 0.0 : averaging;
                averagedInputPower[index] =
                    blend * averagedInputPower[index] + (1.0 - blend) * std::norm(input);
                averagedOutputPowerL[index] =
                    blend * averagedOutputPowerL[index] + (1.0 - blend) * std::norm(left);
                averagedOutputPowerR[index] =
                    blend * averagedOutputPowerR[index] + (1.0 - blend) * std::norm(right);
                const auto inputPower = juce::jmax(averagedInputPower[index], 1.0e-20);
                displayLeft = std::polar(std::sqrt(averagedOutputPowerL[index] / inputPower),
                                         std::arg(left / input));
                displayRight = std::polar(std::sqrt(averagedOutputPowerR[index] / inputPower),
                                          std::arg(right / input));
            }
            else
            {
                displayLeft = left / input;
                displayRight = right / input;
            }

            if (latency > 0)
            {
                const auto correction = juce::MathConstants<double>::twoPi
                                      * static_cast<double>(bin * latency)
                                      / static_cast<double>(workerFFTSize);
                displayLeft *= std::polar(1.0, correction);
                displayRight *= std::polar(1.0, correction);
            }
        }

        auto store = [bin](const std::complex<double>& value,
                           std::vector<float>& magnitudes,
                           std::vector<float>& phases)
        {
            const auto magnitude = std::isfinite(std::abs(value)) ? std::abs(value) : 0.0;
            magnitudes[static_cast<size_t>(bin)] =
                juce::Decibels::gainToDecibels(static_cast<float>(magnitude), -160.0f);
            phases[static_cast<size_t>(bin)] = static_cast<float>(std::arg(value));
        };
        store(displayLeft, workerResult.magnitudeSpectrumL, workerResult.phaseSpectrumL);
        store(displayRight, workerResult.magnitudeSpectrumR, workerResult.phaseSpectrumR);
    }

    if (mode == AnalysisMode::Hammerstein)
    {
        int fundamentalBin = 1;
        double inputPeakPower = 0.0;
        for (int bin = 1; bin < workerFFTSize / 2; ++bin)
        {
            const auto power = std::norm(component(complexInput, bin));
            if (power > inputPeakPower)
            {
                inputPeakPower = power;
                fundamentalBin = bin;
            }
        }

        const auto outputPowerAt = [this, &component](int centre)
        {
            double power = 0.0;
            for (int bin = juce::jmax(1, centre - 1);
                 bin <= juce::jmin(workerFFTSize / 2 - 1, centre + 1); ++bin)
                power += std::norm(component(complexDataL, bin));
            return power;
        };
        const auto fundamentalPower = outputPowerAt(fundamentalBin);
        std::fill(workerResult.harmonicLevels.begin(),
                  workerResult.harmonicLevels.end(), -160.0f);
        if (fundamentalPower > 1.0e-20)
        {
            for (int harmonic = 2; harmonic <= 10; ++harmonic)
            {
                const auto bin = fundamentalBin * harmonic;
                if (bin >= workerFFTSize / 2)
                    break;
                workerResult.harmonicLevels[static_cast<size_t>(harmonic - 2)] =
                    juce::Decibels::gainToDecibels(static_cast<float>(
                        std::sqrt(outputPowerAt(bin) / fundamentalPower)), -160.0f);
            }
        }
    }

    if (mode == AnalysisMode::Harmonic || mode == AnalysisMode::THDSweep)
    {
        calculateTHD(workerResult);
        if (mode == AnalysisMode::THDSweep)
            updateTHDSweep(completedFrameFrequency, workerResult.thd);
    }
    else if (mode == AnalysisMode::IMD)
        calculateIMD(workerResult);
    publishSnapshot();
    if (mode == AnalysisMode::Linear)
        completedLinearGeneration.store(workerGeneration, std::memory_order_release);
}

/**
 * @brief
 * @param result
 */
void AnalyzerEngine::calculateTHD(AnalysisSnapshot& result)
{
    const auto binWidth = result.sampleRate / workerFFTSize;
    const auto fundamental = juce::roundToInt(completedFrameFrequency / binWidth);
    if (fundamental < 1 || fundamental >= workerFFTSize / 2)
    {
        result.thd = result.thdPlusN = 0.0f;
        return;
    }

    auto bandPower = [&result, this](int centre, int radius)
    {
        double power = 0.0;
        for (int bin = juce::jmax(1, centre - radius);
             bin <= juce::jmin(workerFFTSize / 2 - 1, centre + radius); ++bin)
        {
            const auto gain = juce::Decibels::decibelsToGain(
                result.magnitudeSpectrumL[static_cast<size_t>(bin)]);
            power += static_cast<double>(gain) * gain;
        }
        return power;
    };

    const auto fundamentalPower = bandPower(fundamental, 1);
    if (fundamentalPower <= 1.0e-20)
    {
        result.thd = result.thdPlusN = 0.0f;
        return;
    }

    double harmonicsSquared = 0.0, noiseSquared = 0.0;
    std::fill(result.harmonicLevels.begin(), result.harmonicLevels.end(), -120.0f);
    for (int harmonic = 2; harmonic <= 10; ++harmonic)
    {
        const auto bin = fundamental * harmonic;
        if (bin >= workerFFTSize / 2)
            break;
        const auto power = bandPower(bin, 1);
        harmonicsSquared += power;
        result.harmonicLevels[static_cast<size_t>(harmonic - 2)] =
            juce::Decibels::gainToDecibels(
                static_cast<float>(std::sqrt(power / fundamentalPower)), -160.0f);
    }

    const auto firstMeasurementBin = juce::jmax(1, juce::roundToInt(20.0 / binWidth));
    const auto lastMeasurementBin = juce::jmin(workerFFTSize / 2 - 1,
                                               juce::roundToInt(20000.0 / binWidth));
    for (int bin = firstMeasurementBin; bin <= lastMeasurementBin; ++bin)
    {
        if (std::abs(bin - fundamental) > 1)
        {
            const auto gain = juce::Decibels::decibelsToGain(
                result.magnitudeSpectrumL[static_cast<size_t>(bin)]);
            noiseSquared += gain * gain;
        }
    }
    // A Hann window has an equivalent noise bandwidth of 1.5 bins.
    noiseSquared /= 1.5;
    result.thd = static_cast<float>(std::sqrt(harmonicsSquared / fundamentalPower) * 100.0);
    result.thdPlusN = static_cast<float>(
        std::sqrt(noiseSquared / fundamentalPower) * 100.0);
}

/**
 * @brief
 * @param result
 */
void AnalyzerEngine::calculateIMD(AnalysisSnapshot& result)
{
    const auto binWidth = result.sampleRate / workerFFTSize;
    const auto lowBin = juce::roundToInt(
        quantiseToFFTBin(imdLowFrequency, result.sampleRate, workerFFTSize) / binWidth);
    const auto highBin = juce::roundToInt(
        quantiseToFFTBin(imdHighFrequency, result.sampleRate, workerFFTSize) / binWidth);
    auto powerAt = [&result, this](int centre)
    {
        double power = 0.0;
        for (int bin = juce::jmax(1, centre - 1);
             bin <= juce::jmin(workerFFTSize / 2 - 1, centre + 1); ++bin)
        {
            const auto gain = juce::Decibels::decibelsToGain(
                result.magnitudeSpectrumL[static_cast<size_t>(bin)]);
            power += static_cast<double>(gain) * gain;
        }
        return power;
    };
    const auto carrierPower = powerAt(highBin);
    if (carrierPower <= 1.0e-20)
    {
        result.imd = 0.0f;
        return;
    }

    double productsPower = 0.0;
    for (int order = 1; order <= 3; ++order)
    {
        const auto offset = order * lowBin;
        if (highBin - offset > 0)
            productsPower += powerAt(highBin - offset);
        if (highBin + offset < workerFFTSize / 2)
            productsPower += powerAt(highBin + offset);
    }
    result.imd = static_cast<float>(std::sqrt(productsPower / carrierPower) * 100.0);
}

/**
 * @brief
 * @param input
 * @param output
 */
void AnalyzerEngine::analyzeDynamicsSample(float input, float output)
{
    // RMS windows reject the test tone phase and produce a stable static curve.
    static constexpr int rmsWindow = 2048;
    dynamicsInputSquared += static_cast<double>(input) * input;
    dynamicsOutputSquared += static_cast<double>(output) * output;
    if (++dynamicsWindowSamples < rmsWindow)
        return;

    auto& dynamics = workerResult.dynamics;
    if (dynamics.inputLevels.size() == 1000)
    {
        dynamics.inputLevels.erase(dynamics.inputLevels.begin());
        dynamics.outputLevels.erase(dynamics.outputLevels.begin());
    }
    const auto inputRms = std::sqrt(dynamicsInputSquared / dynamicsWindowSamples);
    const auto outputRms = std::sqrt(dynamicsOutputSquared / dynamicsWindowSamples);
    const auto inputLevel =
        juce::Decibels::gainToDecibels(static_cast<float>(inputRms), -100.0f);
    const auto outputLevel =
        juce::Decibels::gainToDecibels(static_cast<float>(outputRms), -100.0f);
    if (!dynamics.inputLevels.empty()
        && inputLevel < dynamics.inputLevels.back() - 6.0f)
    {
        dynamics.inputLevels.clear();
        dynamics.outputLevels.clear();
    }
    dynamics.inputLevels.push_back(inputLevel);
    dynamics.outputLevels.push_back(outputLevel);
    dynamicsInputSquared = dynamicsOutputSquared = 0.0;
    dynamicsWindowSamples = 0;

    const auto count = dynamics.inputLevels.size();
    if (count >= 12)
    {
        const auto fit = [&dynamics](size_t first, size_t last)
        {
            double sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumXY = 0.0;
            const auto n = static_cast<double>(last - first);
            for (auto i = first; i < last; ++i)
            {
                const auto x = dynamics.inputLevels[i];
                const auto y = dynamics.outputLevels[i];
                sumX += x;
                sumY += y;
                sumXX += x * x;
                sumXY += x * y;
            }
            const auto denominator = n * sumXX - sumX * sumX;
            const auto slope = std::abs(denominator) > 1.0e-9
                             ? (n * sumXY - sumX * sumY) / denominator : 1.0;
            const auto intercept = (sumY - slope * sumX) / n;
            return std::pair<double, double>(slope, intercept);
        };

        const auto segment = juce::jmax<size_t>(4, count / 3);
        const auto low = fit(0, segment);
        const auto high = fit(count - segment, count);
        if (high.first > 0.01 && high.first < 1.25)
            dynamics.compressionRatio = static_cast<float>(1.0 / high.first);
        const auto slopeDifference = low.first - high.first;
        if (std::abs(slopeDifference) > 0.02)
            dynamics.threshold = static_cast<float>((high.second - low.second) / slopeDifference);
    }
}

/**
 * @brief
 * @param output
 */
void AnalyzerEngine::analyzeEnvelopeSample(float output)
{
    static constexpr int decimation = 32;
    envelopePrevious = juce::jmax(envelopePrevious, std::abs(output));
    envelopeTimeSeconds += 1.0 / activeSampleRate.load(std::memory_order_relaxed);
    if (++envelopeDecimationCounter < decimation)
        return;
    envelopeDecimationCounter = 0;
    auto& envelope = workerResult.envelope;
    if (envelope.envelopeValues.size() == 4096)
    {
        envelope.envelopeValues.erase(envelope.envelopeValues.begin());
        envelope.timePoints.erase(envelope.timePoints.begin());
    }
    envelope.envelopeValues.push_back(envelopePrevious);
    envelope.timePoints.push_back(static_cast<float>(envelopeTimeSeconds));
    envelopePrevious = 0.0f;

    if (envelope.envelopeValues.size() >= 2)
    {
        const auto peak = *std::max_element(envelope.envelopeValues.begin(),
                                            envelope.envelopeValues.end());
        if (peak > 1.0e-6f)
        {
            auto crossing = [&envelope](float level, bool rising)
            {
                if (rising)
                {
                    for (size_t i = 0; i < envelope.envelopeValues.size(); ++i)
                        if (envelope.envelopeValues[i] >= level)
                            return envelope.timePoints[i];
                }
                else
                {
                    const auto peakIt = std::max_element(envelope.envelopeValues.begin(),
                                                         envelope.envelopeValues.end());
                    for (auto i = static_cast<size_t>(
                             std::distance(envelope.envelopeValues.begin(), peakIt));
                         i < envelope.envelopeValues.size(); ++i)
                        if (envelope.envelopeValues[i] <= level)
                            return envelope.timePoints[i];
                }
                return -1.0f;
            };
            const auto attack10 = crossing(peak * 0.1f, true);
            const auto attack90 = crossing(peak * 0.9f, true);
            const auto release90 = crossing(peak * 0.9f, false);
            const auto release10 = crossing(peak * 0.1f, false);
            if (attack10 >= 0.0f && attack90 >= attack10)
                envelope.attackTime = attack90 - attack10;
            if (release90 >= 0.0f && release10 >= release90)
                envelope.releaseTime = release10 - release90;
        }
    }
}

/**
 * @brief
 * @param frequency
 * @param thd
 */
void AnalyzerEngine::updateTHDSweep(float frequency, float thd)
{
    auto& frequencies = workerResult.thdSweepFrequencies;
    auto& values = workerResult.thdSweepValues;
    const auto existing = std::find_if(frequencies.begin(), frequencies.end(),
        [frequency](float value)
        {
            return std::abs(value - frequency) <= juce::jmax(1.0f, frequency * 0.001f);
        });
    if (existing == frequencies.end())
    {
        frequencies.push_back(frequency);
        values.push_back(thd);
    }
    else
    {
        values[static_cast<size_t>(std::distance(frequencies.begin(), existing))] = thd;
    }
}

/**
 * @brief
 */
void AnalyzerEngine::drainPerformanceFifo()
{
    for (;;)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        performanceFifo.prepareToRead(performanceHistorySize, start1, size1, start2, size2);
        if (size1 + size2 == 0)
            break;
        for (int i = 0; i < size1; ++i)
            updatePerformanceMetrics(performanceQueue[static_cast<size_t>(start1 + i)]);
        for (int i = 0; i < size2; ++i)
            updatePerformanceMetrics(performanceQueue[static_cast<size_t>(start2 + i)]);
        performanceFifo.finishedRead(size1 + size2);
    }
}

/**
 * @brief
 * @param record
 */
void AnalyzerEngine::updatePerformanceMetrics(const PerformanceRecord& record)
{
    performanceHistory[static_cast<size_t>(performanceHistoryWrite)] = record.processingTimeMs;
    performanceHistoryWrite = (performanceHistoryWrite + 1) % performanceHistorySize;
    performanceHistoryCount = juce::jmin(performanceHistoryCount + 1, performanceHistorySize);

    auto& performance = workerResult.performance;
    performance.processingTimeHistory.resize(static_cast<size_t>(performanceHistoryCount));
    float sum = 0.0f, peak = 0.0f;
    for (int i = 0; i < performanceHistoryCount; ++i)
    {
        const auto index = (performanceHistoryWrite - performanceHistoryCount + i
                            + performanceHistorySize) % performanceHistorySize;
        const auto value = performanceHistory[static_cast<size_t>(index)];
        performance.processingTimeHistory[static_cast<size_t>(i)] = value;
        sum += value;
        peak = juce::jmax(peak, value);
    }
    performance.averageProcessingTime = sum / static_cast<float>(performanceHistoryCount);
    performance.peakProcessingTime = peak;
    auto sorted = performance.processingTimeHistory;
    std::sort(sorted.begin(), sorted.end());
    performance.p95ProcessingTime = percentile(sorted, 0.95);
    performance.p99ProcessingTime = percentile(sorted, 0.99);
    performance.bufferSize = record.blockSize;
    performance.sampleRate = activeSampleRate.load(std::memory_order_acquire);
    const auto availableMs = record.blockSize / performance.sampleRate * 1000.0;
    performance.cpuUsagePercent = static_cast<float>(
        performance.averageProcessingTime / availableMs * 100.0);
    performance.droppedAnalysisSamples =
        droppedAnalysisSamples.load(std::memory_order_relaxed);
    performance.droppedScopeSamples =
        droppedScopeSamples.load(std::memory_order_relaxed);
    performance.droppedPerformanceRecords =
        droppedPerformanceRecords.load(std::memory_order_relaxed);
    publishSnapshot();
}

/**
 * @brief
 */
void AnalyzerEngine::publishSnapshot()
{
    workerResult.performance.droppedAnalysisSamples =
        droppedAnalysisSamples.load(std::memory_order_relaxed);
    workerResult.performance.droppedScopeSamples =
        droppedScopeSamples.load(std::memory_order_relaxed);
    workerResult.performance.droppedPerformanceRecords =
        droppedPerformanceRecords.load(std::memory_order_relaxed);
    auto snapshot = std::make_shared<const AnalysisSnapshot>(workerResult);
    std::atomic_store_explicit(&publishedSnapshot, std::move(snapshot), std::memory_order_release);
}

/**
 * @brief
 * @param data
 * @param numSamples
 */
void AnalyzerEngine::addToScopeFifo(const float* data, int numSamples)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    scopeFifo.prepareToWrite(numSamples, start1, size1, start2, size2);
    if (size1 > 0)
        std::copy(data, data + size1, scopeData.begin() + start1);
    if (size2 > 0)
        std::copy(data + size1, data + size1 + size2, scopeData.begin() + start2);
    scopeFifo.finishedWrite(size1 + size2);
    if (size1 + size2 < numSamples)
        droppedScopeSamples.fetch_add(static_cast<uint64_t>(numSamples - size1 - size2),
                                      std::memory_order_relaxed);
}

/**
 * @brief
 * @param dest
 * @param numSamples
 * @return
 */
int AnalyzerEngine::readFromScopeFifo(float* dest, int numSamples)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    scopeFifo.prepareToRead(numSamples, start1, size1, start2, size2);
    if (size1 > 0)
        std::copy(scopeData.begin() + start1, scopeData.begin() + start1 + size1, dest);
    if (size2 > 0)
        std::copy(scopeData.begin() + start2, scopeData.begin() + start2 + size2, dest + size1);
    scopeFifo.finishedRead(size1 + size2);
    return size1 + size2;
}
