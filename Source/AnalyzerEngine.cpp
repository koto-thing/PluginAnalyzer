#include "AnalyzerEngine.h"

namespace
{
bool modeRunsContinuously(AnalyzerEngine::AnalysisMode mode)
{
    return mode != AnalyzerEngine::AnalysisMode::Linear;
}

bool modeUsesWindow(AnalyzerEngine::AnalysisMode mode)
{
    return mode != AnalyzerEngine::AnalysisMode::Linear
        && mode != AnalyzerEngine::AnalysisMode::Performance;
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

void AnalyzerEngine::configureWorkerFFT(int order)
{
    workerFFTOrder = juce::jlimit(8, 15, order);
    workerFFTSize = 1 << workerFFTOrder;
    forwardFFT = std::make_unique<juce::dsp::FFT>(workerFFTOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(
        workerFFTSize, juce::dsp::WindowingFunction<float>::hann);
    complexDataL.assign(static_cast<size_t>(workerFFTSize * 2), 0.0f);
    complexDataR.assign(static_cast<size_t>(workerFFTSize * 2), 0.0f);
    accumulationBufferL.assign(static_cast<size_t>(workerFFTSize), 0.0f);
    accumulationBufferR.assign(static_cast<size_t>(workerFFTSize), 0.0f);
    workerResult.magnitudeSpectrumL.assign(static_cast<size_t>(workerFFTSize / 2), -120.0f);
    workerResult.magnitudeSpectrumR.assign(static_cast<size_t>(workerFFTSize / 2), -120.0f);
    workerResult.phaseSpectrumL.assign(static_cast<size_t>(workerFFTSize / 2), 0.0f);
    workerResult.phaseSpectrumR.assign(static_cast<size_t>(workerFFTSize / 2), 0.0f);
    accumulationIndex = 0;
}

void AnalyzerEngine::resizeAudioBuffers(int blockSize)
{
    pluginProcessingBuffer.setSize(juce::jmax(pluginInputChannels, pluginOutputChannels),
                                   blockSize, false, true, false);
}

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
        pluginIsPrepared = true;
    }
    triggerImpulseAnalysis();
    notify();
}

void AnalyzerEngine::releaseResources()
{
    const juce::ScopedLock lock(pluginLock);
    if (pluginInstance && pluginIsPrepared)
    {
        pluginInstance->releaseResources();
        pluginIsPrepared = false;
    }
}

void AnalyzerEngine::setBlockSize(int newBlockSize)
{
    if (newBlockSize > 0)
        activeBlockSize.store(newBlockSize, std::memory_order_release);
}

void AnalyzerEngine::setFFTOrder(int order)
{
    if (order < 8 || order > 15)
        return;
    requestedFFTOrder.store(order, std::memory_order_release);
    requestedGeneration.fetch_add(1, std::memory_order_acq_rel);
    notify();
}

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

    juce::String error;
    const auto sampleRate = activeSampleRate.load(std::memory_order_acquire);
    const auto blockSize = activeBlockSize.load(std::memory_order_acquire);
    auto candidate = formatManager.createPluginInstance(*found[0], sampleRate, blockSize, error);
    if (!candidate)
    {
        const juce::ScopedLock lock(pluginLock);
        lastPluginError = error.isNotEmpty() ? error : "The plug-in instance could not be created.";
        return false;
    }

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
        pluginIsPrepared = true;
        lastPluginError.clear();
    }
    triggerImpulseAnalysis();
    sendChangeMessage();
    return true;
}

void AnalyzerEngine::unloadPlugin()
{
    {
        const juce::ScopedLock lock(pluginLock);
        if (pluginInstance && pluginIsPrepared)
            pluginInstance->releaseResources();
        pluginInstance.reset();
        pluginProcessingBuffer.setSize(0, 0);
        pluginInputChannels = pluginOutputChannels = 0;
        pluginIsPrepared = false;
        lastPluginError.clear();
    }
    sendChangeMessage();
}

juce::String AnalyzerEngine::getPluginName() const
{
    const juce::ScopedLock lock(pluginLock);
    return pluginInstance ? pluginInstance->getName() : "No Plugin Loaded";
}

juce::String AnalyzerEngine::getLastPluginError() const
{
    const juce::ScopedLock lock(pluginLock);
    return lastPluginError;
}

void AnalyzerEngine::triggerImpulseAnalysis()
{
    requestedGeneration.fetch_add(1, std::memory_order_acq_rel);
}

void AnalyzerEngine::setAnalysisMode(AnalysisMode mode)
{
    if (requestedMode.exchange(mode, std::memory_order_acq_rel) != mode)
        triggerImpulseAnalysis();
}

void AnalyzerEngine::setInputAmplitude(float amplitude)
{
    requestedAmplitude.store(juce::jlimit(0.0f, 1.0f, amplitude), std::memory_order_release);
}

void AnalyzerEngine::setTestFrequency(double frequency)
{
    requestedFrequency.store(juce::jlimit(20.0, 20000.0, frequency), std::memory_order_release);
}

std::shared_ptr<const AnalyzerEngine::AnalysisSnapshot> AnalyzerEngine::getAnalysisSnapshot() const
{
    return std::atomic_load_explicit(&publishedSnapshot, std::memory_order_acquire);
}

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
    }
    if (modeRunsContinuously(mode))
        audioIsAnalyzing = true;
    else if (completedLinearGeneration.load(std::memory_order_acquire) == generation)
        audioIsAnalyzing = false;
    if (!audioIsAnalyzing)
        return;

    signalGenerator.setAmplitude(requestedAmplitude.load(std::memory_order_relaxed));
    signalGenerator.setFrequency(requestedFrequency.load(std::memory_order_relaxed));

    TestSignalGenerator::SignalType signalType = TestSignalGenerator::SignalType::Impulse;
    switch (mode)
    {
        case AnalysisMode::Harmonic:
        case AnalysisMode::THDSweep:
        case AnalysisMode::IMD:
        case AnalysisMode::Performance: signalType = TestSignalGenerator::SignalType::Sine; break;
        case AnalysisMode::WhiteNoise: signalType = TestSignalGenerator::SignalType::WhiteNoise; break;
        case AnalysisMode::SineSweep: signalType = TestSignalGenerator::SignalType::SineSweep; break;
        case AnalysisMode::Dynamics: signalType = TestSignalGenerator::SignalType::Ramp; break;
        case AnalysisMode::Hammerstein: signalType = TestSignalGenerator::SignalType::AttackRelease; break;
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
    }

    addToScopeFifo(buffer.getReadPointer(0), numSamples);
    notify();
}

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

        if (sample.mode == AnalysisMode::Dynamics)
            analyzeDynamicsSample(sample.input, sample.outputL);
        else if (sample.mode == AnalysisMode::Hammerstein)
            analyzeEnvelopeSample(sample.outputL);

        accumulationBufferL[static_cast<size_t>(accumulationIndex)] = sample.outputL;
        accumulationBufferR[static_cast<size_t>(accumulationIndex)] = sample.outputR;
        if (++accumulationIndex == workerFFTSize)
        {
            processCompletedFFT(sample.mode);
            accumulationIndex = 0;
        }
    }
}

void AnalyzerEngine::resetWorkerAnalysis(uint32_t generation)
{
    workerGeneration = generation;
    accumulationIndex = 0;
    std::fill(accumulationBufferL.begin(), accumulationBufferL.end(), 0.0f);
    std::fill(accumulationBufferR.begin(), accumulationBufferR.end(), 0.0f);
    workerResult.dynamics = {};
    workerResult.envelope = {};
    dynamicsDecimationCounter = 0;
    envelopeDecimationCounter = 0;
}

void AnalyzerEngine::processCompletedFFT(AnalysisMode mode)
{
    std::fill(complexDataL.begin(), complexDataL.end(), 0.0f);
    std::fill(complexDataR.begin(), complexDataR.end(), 0.0f);
    std::copy(accumulationBufferL.begin(), accumulationBufferL.end(), complexDataL.begin());
    std::copy(accumulationBufferR.begin(), accumulationBufferR.end(), complexDataR.begin());
    if (modeUsesWindow(mode))
    {
        window->multiplyWithWindowingTable(complexDataL.data(), workerFFTSize);
        window->multiplyWithWindowingTable(complexDataR.data(), workerFFTSize);
    }
    forwardFFT->performRealOnlyForwardTransform(complexDataL.data());
    forwardFFT->performRealOnlyForwardTransform(complexDataR.data());

    auto extract = [this](const std::vector<float>& complex,
                          std::vector<float>& magnitude,
                          std::vector<float>& phase)
    {
        for (int bin = 0; bin < workerFFTSize / 2; ++bin)
        {
            const auto re = bin == 0 ? complex[0] : complex[static_cast<size_t>(2 * bin)];
            const auto im = bin == 0 ? 0.0f : complex[static_cast<size_t>(2 * bin + 1)];
            const auto gain = std::hypot(re, im);
            magnitude[static_cast<size_t>(bin)] =
                juce::Decibels::gainToDecibels(std::isfinite(gain) ? gain : 0.0f, -120.0f);
            phase[static_cast<size_t>(bin)] = std::atan2(im, re);
        }
    };
    extract(complexDataL, workerResult.magnitudeSpectrumL, workerResult.phaseSpectrumL);
    extract(complexDataR, workerResult.magnitudeSpectrumR, workerResult.phaseSpectrumR);

    workerResult.sampleRate = activeSampleRate.load(std::memory_order_acquire);
    if (mode == AnalysisMode::Harmonic || mode == AnalysisMode::THDSweep)
        calculateTHD(workerResult);
    else if (mode == AnalysisMode::IMD)
        calculateIMD(workerResult);
    publishSnapshot();
    if (mode == AnalysisMode::Linear)
        completedLinearGeneration.store(workerGeneration, std::memory_order_release);
}

void AnalyzerEngine::calculateTHD(AnalysisSnapshot& result)
{
    const auto binWidth = result.sampleRate / workerFFTSize;
    const auto fundamental = juce::roundToInt(
        requestedFrequency.load(std::memory_order_relaxed) / binWidth);
    if (fundamental < 1 || fundamental >= workerFFTSize / 2)
    {
        result.thd = result.thdPlusN = 0.0f;
        return;
    }

    const auto fundamentalGain = juce::Decibels::decibelsToGain(
        result.magnitudeSpectrumL[static_cast<size_t>(fundamental)]);
    if (fundamentalGain <= 1.0e-12f)
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
        const auto db = result.magnitudeSpectrumL[static_cast<size_t>(bin)];
        result.harmonicLevels[static_cast<size_t>(harmonic - 2)] = db;
        const auto gain = juce::Decibels::decibelsToGain(db);
        harmonicsSquared += gain * gain;
    }
    for (int bin = 1; bin < workerFFTSize / 2; ++bin)
    {
        bool excluded = false;
        for (int harmonic = 1; harmonic <= 10; ++harmonic)
            excluded = excluded || bin == fundamental * harmonic;
        if (!excluded)
        {
            const auto gain = juce::Decibels::decibelsToGain(
                result.magnitudeSpectrumL[static_cast<size_t>(bin)]);
            noiseSquared += gain * gain;
        }
    }
    result.thd = static_cast<float>(std::sqrt(harmonicsSquared) / fundamentalGain * 100.0);
    result.thdPlusN = static_cast<float>(
        std::sqrt(harmonicsSquared + noiseSquared) / fundamentalGain * 100.0);
}

void AnalyzerEngine::calculateIMD(AnalysisSnapshot& result)
{
    result.imd = 0.0f; // Numerical calibration belongs to Phase 4.
}

void AnalyzerEngine::analyzeDynamicsSample(float input, float output)
{
    // Decimate to keep the worker-side snapshot compact and bounded.
    static constexpr int decimation = 256;
    if (++dynamicsDecimationCounter < decimation)
        return;
    dynamicsDecimationCounter = 0;
    auto& dynamics = workerResult.dynamics;
    if (dynamics.inputLevels.size() == 1000)
    {
        dynamics.inputLevels.erase(dynamics.inputLevels.begin());
        dynamics.outputLevels.erase(dynamics.outputLevels.begin());
    }
    dynamics.inputLevels.push_back(juce::Decibels::gainToDecibels(std::abs(input), -100.0f));
    dynamics.outputLevels.push_back(juce::Decibels::gainToDecibels(std::abs(output), -100.0f));
    if (dynamics.inputLevels.size() > 10)
    {
        const auto n = dynamics.inputLevels.size();
        const auto inputChange = dynamics.inputLevels[n - 1] - dynamics.inputLevels[n - 10];
        const auto outputChange = dynamics.outputLevels[n - 1] - dynamics.outputLevels[n - 10];
        if (std::abs(inputChange) > 1.0f && std::abs(outputChange) > 0.01f)
            dynamics.compressionRatio = inputChange / outputChange;
    }
}

void AnalyzerEngine::analyzeEnvelopeSample(float output)
{
    static constexpr int decimation = 32;
    if (++envelopeDecimationCounter < decimation)
        return;
    envelopeDecimationCounter = 0;
    auto& envelope = workerResult.envelope;
    if (envelope.envelopeValues.size() == 4096)
    {
        envelope.envelopeValues.erase(envelope.envelopeValues.begin());
        envelope.timePoints.erase(envelope.timePoints.begin());
    }
    envelope.envelopeValues.push_back(std::abs(output));
    envelope.timePoints.push_back(static_cast<float>(
        envelope.timePoints.size() * decimation / activeSampleRate.load(std::memory_order_relaxed)));
}

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
    performance.bufferSize = record.blockSize;
    performance.sampleRate = activeSampleRate.load(std::memory_order_acquire);
    const auto availableMs = record.blockSize / performance.sampleRate * 1000.0;
    performance.cpuUsagePercent = static_cast<float>(record.processingTimeMs / availableMs * 100.0);
    publishSnapshot();
}

void AnalyzerEngine::publishSnapshot()
{
    auto snapshot = std::make_shared<const AnalysisSnapshot>(workerResult);
    std::atomic_store_explicit(&publishedSnapshot, std::move(snapshot), std::memory_order_release);
    sendChangeMessage();
}

void AnalyzerEngine::addToScopeFifo(const float* data, int numSamples)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    scopeFifo.prepareToWrite(numSamples, start1, size1, start2, size2);
    if (size1 > 0)
        std::copy(data, data + size1, scopeData.begin() + start1);
    if (size2 > 0)
        std::copy(data + size1, data + size1 + size2, scopeData.begin() + start2);
    scopeFifo.finishedWrite(size1 + size2);
}

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
