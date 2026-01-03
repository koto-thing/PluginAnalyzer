#include "AnalyzerEngine.h"

AnalyzerEngine::AnalyzerEngine()
{
    // Register all available plugin formats
#if JUCE_PLUGINHOST_VST3
    formatManager.addFormat(new juce::VST3PluginFormat());
#endif

#if JUCE_MAC
 #if JUCE_PLUGINHOST_AU
    formatManager.addFormat(new juce::AudioUnitPluginFormat());
 #endif
#endif

#if JUCE_LINUX
 #if JUCE_PLUGINHOST_LADSPA
    formatManager.addFormat(new juce::LADSPAPluginFormat());
 #endif

 #if JUCE_PLUGINHOST_LV2
    formatManager.addFormat(new juce::LV2PluginFormat());
 #endif
#endif
    
    // Initialize FFT and window
    forwardFFT = std::make_unique<juce::dsp::FFT>(fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::hann);
    
    // Initialize with default FFT size
    resizeFFTBuffers();
    
    scopeData.resize(scopeFifoSize, 0.0f);
    
    harmonicLevels.resize(10, 0.0f); // Store up to 10 harmonics
    
    DBG("Registered plugin formats: " << formatManager.getNumFormats());
}

AnalyzerEngine::~AnalyzerEngine()
{
}

void AnalyzerEngine::resizeFFTBuffers()
{
    // Size for Real-Only transform is 2 * fftSize because output is complex
    fftDataL.resize(fftSize * 2, 0.0f);
    fftDataR.resize(fftSize * 2, 0.0f);
    
    complexDataL.resize(fftSize * 2, 0.0f);
    complexDataR.resize(fftSize * 2, 0.0f);
    
    magnitudeSpectrumL.resize(fftSize / 2, -100.0f);
    magnitudeSpectrumR.resize(fftSize / 2, -100.0f);
    
    phaseSpectrumL.resize(fftSize / 2, 0.0f);
    phaseSpectrumR.resize(fftSize / 2, 0.0f);
    
    accumulationBufferL.resize(fftSize, 0.0f);
    accumulationBufferR.resize(fftSize, 0.0f);
}

void AnalyzerEngine::setFFTOrder(int newFftOrder)
{
    if (newFftOrder < 8 || newFftOrder > 15)
        return; // Clamp to reasonable range
    
    fftOrder = newFftOrder;
    fftSize = 1 << fftOrder;
    
    // Recreate FFT and window
    forwardFFT = std::make_unique<juce::dsp::FFT>(fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::hann);
    
    // Resize all buffers
    resizeFFTBuffers();
    
    // Reset analysis state
    accumulationIndex = 0;
}

void AnalyzerEngine::prepare(double sampleRate, int blockSize)
{
    signalGenerator.prepare(sampleRate, blockSize);
    if (pluginInstance)
        pluginInstance->prepareToPlay(sampleRate, blockSize);
    
    lastSampleRate = sampleRate;
    lastBufferSize = blockSize;
    performanceData.sampleRate = sampleRate;
    performanceData.bufferSize = blockSize;
}

void AnalyzerEngine::setBlockSize(int /*newBlockSize*/)
{
    // Re-prepare if needed
    // In this simple context, we just wait for the next prepare call from MainComponent usually
}

bool AnalyzerEngine::loadPlugin(const juce::File& file)
{
    if (!file.existsAsFile()) return false;

    juce::OwnedArray<juce::PluginDescription> foundPlugins;
    // Iterate formats
    for (auto format : formatManager.getFormats())
    {
        format->findAllTypesForFile(foundPlugins, file.getFullPathName());
    }

    if (foundPlugins.size() > 0)
    {
        juce::String error;
        // Verify formatManager is AudioPluginFormatManager
        pluginInstance = formatManager.createPluginInstance(*foundPlugins[0], 44100.0, 512, error);
        
        if (pluginInstance)
        {
            DBG("Plugin Loaded: " << pluginInstance->getName());
            pluginInstance->prepareToPlay(44100.0, 512);
            triggerImpulseAnalysis(); // Auto start analysis
            sendChangeMessage(); // Notify UI
            return true;
        }
        else
        {
            DBG("Failed to create plugin instance: " << error);
        }
    }
    else
    {
        DBG("No plugin types found in file.");
    }
    
    return false;
}

void AnalyzerEngine::unloadPlugin()
{
    pluginInstance = nullptr;
    sendChangeMessage();
}

void AnalyzerEngine::triggerImpulseAnalysis()
{
    signalGenerator.reset();
    isAnalyzing = true;
    accumulationIndex = 0;
    std::fill(accumulationBufferL.begin(), accumulationBufferL.end(), 0.0f);
    std::fill(accumulationBufferR.begin(), accumulationBufferR.end(), 0.0f);
    std::fill(fftDataL.begin(), fftDataL.end(), 0.0f);
    std::fill(fftDataR.begin(), fftDataR.end(), 0.0f);
}

void AnalyzerEngine::setAnalysisMode(AnalysisMode mode)
{
    if (currentMode != mode)
    {
        currentMode = mode;
        triggerImpulseAnalysis(); // Reset state
    }
}

void AnalyzerEngine::setInputAmplitude(float amplitude)
{
    signalGenerator.setAmplitude(amplitude);
}

float AnalyzerEngine::getInputAmplitude() const
{
    return signalGenerator.getAmplitude();
}

void AnalyzerEngine::setTestFrequency(double frequency)
{
    signalGenerator.setFrequency(frequency);
}

double AnalyzerEngine::getTestFrequency() const
{
    return signalGenerator.getFrequency();
}

void AnalyzerEngine::calculateTHD()
{
    // Calculate THD from magnitude spectrum (Left channel)
    double testFreq = signalGenerator.getFrequency();
    double binWidth = 44100.0 / fftSize; // Sample rate / FFT size
    
    int fundamentalBin = (int)(testFreq / binWidth + 0.5);
    
    if (fundamentalBin < 1 || fundamentalBin >= fftSize / 2)
    {
        currentTHD = 0.0f;
        currentTHDPlusN = 0.0f;
        return;
    }
    
    // Get fundamental magnitude (in linear scale)
    float fundamentalMag = juce::Decibels::decibelsToGain(magnitudeSpectrumL[fundamentalBin]);
    
    // Calculate harmonic magnitudes
    double sumHarmonicsSquared = 0.0;
    double sumAllNoiseSquared = 0.0;
    
    for (int h = 2; h <= 10; ++h) // Up to 10th harmonic
    {
        int harmonicBin = fundamentalBin * h;
        if (harmonicBin >= fftSize / 2) break;
        
        float harmonicMag = juce::Decibels::decibelsToGain(magnitudeSpectrumL[harmonicBin]);
        sumHarmonicsSquared += harmonicMag * harmonicMag;
        
        if (h - 2 < harmonicLevels.size())
            harmonicLevels[h - 2] = magnitudeSpectrumL[harmonicBin];
    }
    
    // Calculate total noise (excluding fundamental and harmonics)
    for (int i = 1; i < fftSize / 2; ++i)
    {
        bool isHarmonic = false;
        for (int h = 1; h <= 10; ++h)
        {
            if (i == fundamentalBin * h)
            {
                isHarmonic = true;
                break;
            }
        }
        
        if (!isHarmonic)
        {
            float mag = juce::Decibels::decibelsToGain(magnitudeSpectrumL[i]);
            sumAllNoiseSquared += mag * mag;
        }
    }
    
    // THD = sqrt(sum of harmonics squared) / fundamental
    currentTHD = (float)(std::sqrt(sumHarmonicsSquared) / fundamentalMag) * 100.0f;
    
    // THD+N = sqrt(sum of harmonics + noise squared) / fundamental
    currentTHDPlusN = (float)(std::sqrt(sumHarmonicsSquared + sumAllNoiseSquared) / fundamentalMag) * 100.0f;
}

void AnalyzerEngine::calculateIMD()
{
    // Simplified IMD calculation (SMPTE method)
    // Measure intermodulation products at f2 +/- f1, f2 +/- 2*f1, etc.
    
    // double binWidth = 44100.0 / fftSize; // Unused variable warning fix
    
    // For now, just set to 0 - full IMD implementation would require dual-tone setup
    currentIMD = 0.0f;
}

void AnalyzerEngine::analyzeDynamics(const juce::AudioBuffer<float>& inputBuffer, const juce::AudioBuffer<float>& outputBuffer)
{
    // Analyze compression/expansion characteristics
    int numSamples = inputBuffer.getNumSamples();
    
    // Calculate RMS for input and output
    auto calculateRMS = [](const float* data, int samples) -> float {
        float sumSquares = 0.0f;
        for (int i = 0; i < samples; ++i)
            sumSquares += data[i] * data[i];
        return std::sqrt(sumSquares / samples);
    };
    
    float inputRMS = calculateRMS(inputBuffer.getReadPointer(0), numSamples);
    float outputRMS = calculateRMS(outputBuffer.getReadPointer(0), numSamples);
    
    float inputDB = juce::Decibels::gainToDecibels(inputRMS, -100.0f);
    float outputDB = juce::Decibels::gainToDecibels(outputRMS, -100.0f);
    
    // Store data point
    dynamicsData.inputLevels.push_back(inputDB);
    dynamicsData.outputLevels.push_back(outputDB);
    
    // Keep only recent data (for performance)
    if (dynamicsData.inputLevels.size() > 1000)
    {
        dynamicsData.inputLevels.erase(dynamicsData.inputLevels.begin());
        dynamicsData.outputLevels.erase(dynamicsData.outputLevels.begin());
    }
    
    // Estimate compression ratio from recent data
    if (dynamicsData.inputLevels.size() > 10)
    {
        int n = (int)dynamicsData.inputLevels.size();
        float inputChange = dynamicsData.inputLevels[n-1] - dynamicsData.inputLevels[n-10];
        float outputChange = dynamicsData.outputLevels[n-1] - dynamicsData.outputLevels[n-10];
        
        if (std::abs(inputChange) > 1.0f)
        {
            dynamicsData.compressionRatio = inputChange / outputChange;
        }
    }
}

void AnalyzerEngine::analyzeEnvelope(const juce::AudioBuffer<float>& buffer)
{
    // Analyze attack and release envelope characteristics
    int numSamples = buffer.getNumSamples();
    const float* data = buffer.getReadPointer(0);
    
    // Calculate envelope (peak detection with smoothing)
    for (int i = 0; i < numSamples; ++i)
    {
        float absValue = std::abs(data[i]);
        
        // Simple time point (in seconds)
        float timePoint = (float)envelopeData.envelopeValues.size() / 44100.0f;
        
        envelopeData.timePoints.push_back(timePoint);
        envelopeData.envelopeValues.push_back(absValue);
    }
    
    // Keep only recent envelope data
    if (envelopeData.envelopeValues.size() > 44100 * 10) // Max 10 seconds
    {
        int excess = (int)envelopeData.envelopeValues.size() - 44100 * 10;
        envelopeData.timePoints.erase(envelopeData.timePoints.begin(), envelopeData.timePoints.begin() + excess);
        envelopeData.envelopeValues.erase(envelopeData.envelopeValues.begin(), envelopeData.envelopeValues.begin() + excess);
    }
    
    // Estimate attack and release times from envelope data
    if (envelopeData.envelopeValues.size() > 100)
    {
        // Find attack time (10% to 90% rise time)
        float maxVal = *std::max_element(envelopeData.envelopeValues.end() - 100, envelopeData.envelopeValues.end());
        float threshold10 = maxVal * 0.1f;
        float threshold90 = maxVal * 0.9f;
        
        int idx10 = -1, idx90 = -1;
        for (int i = (int)envelopeData.envelopeValues.size() - 100; i < envelopeData.envelopeValues.size(); ++i)
        {
            if (idx10 < 0 && envelopeData.envelopeValues[i] >= threshold10)
                idx10 = i;
            if (idx90 < 0 && envelopeData.envelopeValues[i] >= threshold90)
                idx90 = i;
        }
        
        if (idx10 >= 0 && idx90 >= 0 && idx90 > idx10)
        {
            envelopeData.attackTime = envelopeData.timePoints[idx90] - envelopeData.timePoints[idx10];
        }
    }
}

void AnalyzerEngine::updatePerformanceMetrics(double processingTimeMs)
{
    // Add to history
    performanceData.processingTimeHistory.push_back((float)processingTimeMs);
    
    // Keep only recent 100 measurements
    if (performanceData.processingTimeHistory.size() > 100)
    {
        performanceData.processingTimeHistory.erase(performanceData.processingTimeHistory.begin());
    }
    
    // Calculate average
    float sum = 0.0f;
    float peak = 0.0f;
    for (float time : performanceData.processingTimeHistory)
    {
        sum += time;
        if (time > peak)
            peak = time;
    }
    
    performanceData.averageProcessingTime = sum / performanceData.processingTimeHistory.size();
    performanceData.peakProcessingTime = peak;
    
    // Calculate CPU usage percentage
    // Available time = buffer size / sample rate (in milliseconds)
    double availableTimeMs = (lastBufferSize / lastSampleRate) * 1000.0;
    performanceData.cpuUsagePercent = (float)((processingTimeMs / availableTimeMs) * 100.0);
}

void AnalyzerEngine::processAudio(juce::AudioBuffer<float>& buffer)
{
    // In Harmonic/WhiteNoise/SineSweep/THDSweep/Performance mode, we always run. In Linear, we wait for trigger.
    if (currentMode == AnalysisMode::Linear && !isAnalyzing) return;
    if (currentMode == AnalysisMode::Harmonic || 
        currentMode == AnalysisMode::WhiteNoise || 
        currentMode == AnalysisMode::SineSweep ||
        currentMode == AnalysisMode::THDSweep ||
        currentMode == AnalysisMode::IMD ||
        currentMode == AnalysisMode::Dynamics ||
        currentMode == AnalysisMode::Performance) 
        isAnalyzing = true; 

    int numSamples = buffer.getNumSamples();
    
    // 1. Generate Signal
    TestSignalGenerator::SignalType sigType;
    
    switch (currentMode)
    {
        case AnalysisMode::Linear:
            sigType = TestSignalGenerator::SignalType::Impulse;
            break;
        case AnalysisMode::Harmonic:
        case AnalysisMode::THDSweep:
        case AnalysisMode::IMD:
            sigType = TestSignalGenerator::SignalType::Sine;
            break;
        case AnalysisMode::WhiteNoise:
            sigType = TestSignalGenerator::SignalType::WhiteNoise;
            break;
        case AnalysisMode::SineSweep:
            sigType = TestSignalGenerator::SignalType::SineSweep;
            break;
        case AnalysisMode::Dynamics:
            sigType = TestSignalGenerator::SignalType::Ramp;
            break;
        case AnalysisMode::Hammerstein:
            sigType = TestSignalGenerator::SignalType::AttackRelease;
            break;
        case AnalysisMode::Performance:
            sigType = TestSignalGenerator::SignalType::Sine;  // Use sine wave for performance testing
            break;
        default:
            sigType = TestSignalGenerator::SignalType::Impulse;
            break;
    }
    
    signalGenerator.fillBuffer(buffer, sigType, 0);
    
    if (buffer.getNumChannels() > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);

    // Store input buffer for dynamics analysis
    juce::AudioBuffer<float> inputBuffer(buffer.getNumChannels(), numSamples);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        inputBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // 2. Process Plugin with performance measurement
    if (pluginInstance)
    {
        // Start timing
        auto startTime = juce::Time::getMillisecondCounterHiRes();
        
        juce::MidiBuffer midi;
        pluginInstance->processBlock(buffer, midi);
        
        // End timing
        auto endTime = juce::Time::getMillisecondCounterHiRes();
        double processingTimeMs = endTime - startTime;
        
        // Update performance metrics
        if (currentMode == AnalysisMode::Performance)
        {
            updatePerformanceMetrics(processingTimeMs);
        }
    }

    // 3. Dynamics Analysis (for Dynamics and Hammerstein modes)
    if (currentMode == AnalysisMode::Dynamics)
    {
        analyzeDynamics(inputBuffer, buffer);
    }
    else if (currentMode == AnalysisMode::Hammerstein)
    {
        analyzeEnvelope(buffer);
    }

    // 4. FFT Analysis
    if (accumulationIndex < fftSize)
    {
        auto* channelDataL = buffer.getReadPointer(0);
        auto* channelDataR = (buffer.getNumChannels() > 1) ? buffer.getReadPointer(1) : channelDataL;
        
        int samplesToCopy = std::min(numSamples, fftSize - accumulationIndex);
        
        for (int i = 0; i < samplesToCopy; ++i)
        {
            accumulationBufferL[accumulationIndex + i] = channelDataL[i];
            accumulationBufferR[accumulationIndex + i] = channelDataR[i];
        }
        accumulationIndex += samplesToCopy;

        // If block filled
        if (accumulationIndex >= fftSize)
        {
             // Process Left (Real-Only -> Complex)
             std::fill(complexDataL.begin(), complexDataL.end(), 0.0f);
             std::copy(accumulationBufferL.begin(), accumulationBufferL.end(), complexDataL.begin());
             
             // Apply windowing for continuous signals
             if (currentMode == AnalysisMode::Harmonic || 
                 currentMode == AnalysisMode::WhiteNoise || 
                 currentMode == AnalysisMode::SineSweep ||
                 currentMode == AnalysisMode::THDSweep ||
                 currentMode == AnalysisMode::IMD ||
                 currentMode == AnalysisMode::Dynamics ||
                 currentMode == AnalysisMode::Hammerstein)
             {
                 window->multiplyWithWindowingTable(complexDataL.data(), fftSize);
             }
             
             forwardFFT->performRealOnlyForwardTransform(complexDataL.data());
             
             // Process Right
             std::fill(complexDataR.begin(), complexDataR.end(), 0.0f);
             std::copy(accumulationBufferR.begin(), accumulationBufferR.end(), complexDataR.begin());
             
             if (currentMode == AnalysisMode::Harmonic || 
                 currentMode == AnalysisMode::WhiteNoise || 
                 currentMode == AnalysisMode::SineSweep ||
                 currentMode == AnalysisMode::THDSweep ||
                 currentMode == AnalysisMode::IMD ||
                 currentMode == AnalysisMode::Dynamics ||
                 currentMode == AnalysisMode::Hammerstein)
             {
                 window->multiplyWithWindowingTable(complexDataR.data(), fftSize);
             }
             
             forwardFFT->performRealOnlyForwardTransform(complexDataR.data());
             
             // Extract Mag/Phase
             auto extract = [&](const std::vector<float>& complexData, std::vector<float>& magData, std::vector<float>& phaseData) {
                 for (int i = 0; i < fftSize / 2; ++i)
                 {
                     float re = 0.0f;
                     float im = 0.0f;
                     
                     if (i == 0) {
                         re = complexData[0];
                         im = 0.0f;
                     } else if (i == fftSize / 2 - 1) {
                         // Nyquist case
                     }
                     
                     if (i > 0 && i < fftSize / 2) {
                         re = complexData[2 * i];
                         im = complexData[2 * i + 1];
                     }
                     
                     float mag = std::sqrt(re * re + im * im);
                     if (i == 0) mag = std::abs(complexData[0]);
                     
                     if (std::isnan(mag) || std::isinf(mag)) mag = 0.0f;
                     magData[i] = juce::Decibels::gainToDecibels(mag, -120.0f);
                     phaseData[i] = std::atan2(im, re);
                 }
             };

             extract(complexDataL, magnitudeSpectrumL, phaseSpectrumL);
             extract(complexDataR, magnitudeSpectrumR, phaseSpectrumR);
             
             // Calculate THD/IMD if in appropriate mode
             if (currentMode == AnalysisMode::Harmonic || 
                 currentMode == AnalysisMode::THDSweep)
             {
                 calculateTHD();
             }
             else if (currentMode == AnalysisMode::IMD)
             {
                 calculateIMD();
             }
             
             // Reset for next block
             accumulationIndex = 0;
             if (currentMode == AnalysisMode::Linear) 
             {
                 isAnalyzing = false;
                 DBG("Linear Analysis Finished.");
             }
             
             std::fill(accumulationBufferL.begin(), accumulationBufferL.end(), 0.0f);
             std::fill(accumulationBufferR.begin(), accumulationBufferR.end(), 0.0f);
             sendChangeMessage(); 
        }
    }
    
    // 5. Oscilloscope Update
    addToScopeFifo(buffer.getReadPointer(0), buffer.getNumSamples());
}

void AnalyzerEngine::addToScopeFifo(const float* data, int numSamples)
{
    int start1, size1, start2, size2;
    scopeFifo.prepareToWrite(numSamples, start1, size1, start2, size2);
    
    if (size1 > 0) std::copy(data, data + size1, scopeData.begin() + start1);
    if (size2 > 0) std::copy(data + size1, data + size1 + size2, scopeData.begin() + start2);
    
    scopeFifo.finishedWrite(size1 + size2);
}

int AnalyzerEngine::readFromScopeFifo(float* dest, int numSamples)
{
    int start1, size1, start2, size2;
    scopeFifo.prepareToRead(numSamples, start1, size1, start2, size2);
    
    if (size1 > 0) std::copy(scopeData.begin() + start1, scopeData.begin() + start1 + size1, dest);
    if (size2 > 0) std::copy(scopeData.begin() + start2, scopeData.begin() + start2 + size2, dest + size1);
    
    scopeFifo.finishedRead(size1 + size2);
    return size1 + size2;
}