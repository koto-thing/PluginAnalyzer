#pragma once

#include <JuceHeader.h>
#include "Application/AnalysisService.h"
#include "TestSignalGenerator.h"
#include <array>
#include <atomic>
#include <memory>

class AnalyzerEngine : public plugin_analyzer::application::AnalysisService,
                       private juce::Thread
{
public:
    using AnalysisMode = plugin_analyzer::domain::AnalysisMode;
    using DynamicsData = plugin_analyzer::domain::DynamicsData;
    using EnvelopeData = plugin_analyzer::domain::EnvelopeData;
    using PerformanceData = plugin_analyzer::domain::PerformanceData;
    using AnalysisSnapshot = plugin_analyzer::domain::AnalysisSnapshot;

    AnalyzerEngine();
    ~AnalyzerEngine() override;

    void prepare(double sampleRate, int blockSize);
    void releaseResources();
    void setBlockSize(int newBlockSize);

    void setFFTOrder(int newFftOrder);
    int getFFTOrder() const { return requestedFFTOrder.load(std::memory_order_relaxed); }
    int getFFTSize() const { return 1 << getFFTOrder(); }

    bool loadPlugin(const juce::File& file);
    bool loadPlugin(const juce::PluginDescription& description);
    // Also used by the Phase 6 calibration suite to host deterministic
    // in-process processors without depending on an installed plug-in format.
    bool loadProcessor(std::unique_ptr<juce::AudioProcessor> processor);
    void unloadPlugin();
    juce::String getPluginName() const;
    std::string getPluginDisplayName() const override;
    juce::String getLastPluginError() const;

    void setAnalysisMode(AnalysisMode mode) override;
    AnalysisMode getAnalysisMode() const override
    {
        return requestedMode.load(std::memory_order_relaxed);
    }

    void setInputAmplitude(float amplitude) override;
    float getInputAmplitude() const { return requestedAmplitude.load(std::memory_order_relaxed); }
    void setTestFrequency(double frequency) override;
    double getTestFrequency() const { return requestedFrequency.load(std::memory_order_relaxed); }

    std::shared_ptr<const AnalysisSnapshot> getAnalysisSnapshot() const override;

    void processAudio(juce::AudioBuffer<float>& buffer);
    void triggerImpulseAnalysis();

    void addToScopeFifo(const float* data, int numSamples);
    int readFromScopeFifo(float* dest, int numSamples) override;

    enum { scopeFifoSize = 32768 };

private:
    struct AnalysisSample
    {
        float input = 0.0f;
        float outputL = 0.0f;
        float outputR = 0.0f;
        AnalysisMode mode = AnalysisMode::Linear;
        uint32_t generation = 0;
        float measurementFrequency = 1000.0f;
    };

    struct PerformanceRecord
    {
        float processingTimeMs = 0.0f;
        int blockSize = 0;
    };

    static constexpr int analysisFifoSize = 1 << 17;
    static constexpr int performanceFifoSize = 512;
    static constexpr int performanceHistorySize = 100;

    void run() override;
    void drainAnalysisFifo();
    void drainPerformanceFifo();
    void processAnalysisSamples(const AnalysisSample* samples, int count);
    void processCompletedFFT(AnalysisMode mode);
    void configureWorkerFFT(int order);
    void resetWorkerAnalysis(uint32_t generation);
    void publishSnapshot();
    void calculateTHD(AnalysisSnapshot& result);
    void calculateIMD(AnalysisSnapshot& result);
    void analyzeDynamicsSample(float input, float output);
    void analyzeEnvelopeSample(float output);
    void updateTHDSweep(float frequency, float thd);
    void updatePerformanceMetrics(const PerformanceRecord& record);
    void resizeAudioBuffers(int blockSize);

    std::unique_ptr<juce::AudioProcessor> pluginInstance;
    juce::AudioPluginFormatManager formatManager;
    mutable juce::CriticalSection pluginLock;
    juce::AudioBuffer<float> pluginProcessingBuffer;
    juce::String lastPluginError;
    bool pluginIsPrepared = false;
    int pluginInputChannels = 0;
    int pluginOutputChannels = 0;
    std::atomic<int> pluginLatencySamples { 0 };

    TestSignalGenerator signalGenerator;
    std::atomic<AnalysisMode> requestedMode { AnalysisMode::Linear };
    std::atomic<float> requestedAmplitude { 0.5f };
    std::atomic<double> requestedFrequency { 1000.0 };
    std::atomic<int> requestedFFTOrder { 11 };
    std::atomic<uint32_t> requestedGeneration { 1 };
    std::atomic<uint32_t> completedLinearGeneration { 0 };
    uint32_t audioGeneration = 0;
    bool audioIsAnalyzing = false;

    juce::AbstractFifo analysisFifo { analysisFifoSize };
    std::vector<AnalysisSample> analysisQueue;
    juce::AbstractFifo performanceFifo { performanceFifoSize };
    std::array<PerformanceRecord, performanceFifoSize> performanceQueue {};

    juce::AbstractFifo scopeFifo { scopeFifoSize };
    std::vector<float> scopeData;

    int workerFFTOrder = 11;
    int workerFFTSize = 1 << 11;
    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    std::vector<float> complexDataL, complexDataR;
    std::vector<float> complexInput;
    std::vector<float> accumulationBufferInput, accumulationBufferL, accumulationBufferR;
    std::vector<double> averagedInputPower, averagedOutputPowerL, averagedOutputPowerR;
    int spectralAverageCount = 0;
    int accumulationIndex = 0;
    float completedFrameFrequency = 1000.0f;
    uint32_t workerGeneration = 0;
    int dynamicsDecimationCounter = 0;
    int envelopeDecimationCounter = 0;
    double dynamicsInputSquared = 0.0;
    double dynamicsOutputSquared = 0.0;
    int dynamicsWindowSamples = 0;
    double envelopeTimeSeconds = 0.0;
    float envelopePrevious = 0.0f;

    int thdSweepSamplesAtFrequency = 0;
    int thdSweepBin = 0;
    static constexpr int thdSweepSteps = 30;

    AnalysisSnapshot workerResult;
    std::shared_ptr<const AnalysisSnapshot> publishedSnapshot;
    std::array<float, performanceHistorySize> performanceHistory {};
    int performanceHistoryWrite = 0;
    int performanceHistoryCount = 0;
    std::atomic<uint64_t> droppedAnalysisSamples { 0 };
    std::atomic<uint64_t> droppedScopeSamples { 0 };
    std::atomic<uint64_t> droppedPerformanceRecords { 0 };

    std::atomic<double> activeSampleRate { 44100.0 };
    std::atomic<int> activeBlockSize { 512 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerEngine)
};
