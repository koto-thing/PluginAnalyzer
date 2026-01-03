#pragma once

#include <JuceHeader.h>
#include "TestSignalGenerator.h"

class AnalyzerEngine : public juce::ChangeBroadcaster
{
public:
    AnalyzerEngine();
    ~AnalyzerEngine();

    // Setup
    void prepare(double sampleRate, int blockSize);
    void setBlockSize(int newBlockSize);
    
    // Configuration
    void setFFTOrder(int newFftOrder);
    int getFFTOrder() const { return fftOrder; }
    int getFFTSize() const { return fftSize; }
    
    // Plugin Management
    bool loadPlugin(const juce::File& file);
    void unloadPlugin();
    juce::AudioPluginInstance* getPluginInstance() { return pluginInstance.get(); }
    juce::String getPluginName() const { return pluginInstance ? pluginInstance->getName() : "No Plugin Loaded"; }

    enum class AnalysisMode
    {
		Linear,         // インパルス応答
		Harmonic,       // harmonic分析
		Hammerstein,    // Hammerstein-Wienerモデル
		WhiteNoise,     // ホワイトノイズ
        SineSweep,      // Sinスイープ
		THDSweep,       // THDスイープ
		IMD,            // IMD分析
		Dynamics,       // ダイナミクス分析
		Performance     // パフォーマンス分析
    };

    void setAnalysisMode(AnalysisMode mode);
    AnalysisMode getAnalysisMode() const { return currentMode; }

    // THD
    void setInputAmplitude(float amplitude);
    float getInputAmplitude() const;
    void setTestFrequency(double frequency);
    double getTestFrequency() const;
    
    float getTHD() const { return currentTHD; }
    float getTHDPlusN() const { return currentTHDPlusN; }
    float getIMD() const { return currentIMD; }
    
    const std::vector<float>& getHarmonicLevels() const { return harmonicLevels; }

    // Dynamics
    struct DynamicsData
    {
        std::vector<float> inputLevels;
        std::vector<float> outputLevels;
        float compressionRatio = 1.0f;
        float threshold = 0.0f;
    };
    
    const DynamicsData& getDynamicsData() const { return dynamicsData; }
    
    struct EnvelopeData
    {
        std::vector<float> timePoints;
        std::vector<float> envelopeValues;
        float attackTime = 0.0f;
        float releaseTime = 0.0f;
    };
    
    const EnvelopeData& getEnvelopeData() const { return envelopeData; }

    // Performance
    struct PerformanceData
    {
        float averageProcessingTime = 0.0f;
        float peakProcessingTime = 0.0f;
        float cpuUsagePercent = 0.0f;
        int bufferSize = 0;
        double sampleRate = 0.0;
        std::vector<float> processingTimeHistory;
    };
    
    const PerformanceData& getPerformanceData() const { return performanceData; }

    // Analysis
    void processAudio(juce::AudioBuffer<float>& buffer);
    void triggerImpulseAnalysis();
    
    // データアクセス用
    const std::vector<float>& getMagnitudeSpectrumL() const { return magnitudeSpectrumL; }
    const std::vector<float>& getMagnitudeSpectrumR() const { return magnitudeSpectrumR; }
    const std::vector<float>& getPhaseSpectrumL() const { return phaseSpectrumL; }
    const std::vector<float>& getPhaseSpectrumR() const { return phaseSpectrumR; }
    
    const std::vector<float>& getMagnitudeSpectrum() const { return magnitudeSpectrumL; }

    // Oscilloscope
    void addToScopeFifo(const float* data, int numSamples);
    int readFromScopeFifo(float* dest, int numSamples);
    
    enum
    {
        scopeFifoSize = 32768
    };
    juce::AbstractFifo scopeFifo { scopeFifoSize };
    std::vector<float> scopeData;

private:
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    juce::AudioPluginFormatManager formatManager;

    TestSignalGenerator signalGenerator;
    AnalysisMode currentMode = AnalysisMode::Linear;
    
    // FFT
    int fftOrder = 11;
    int fftSize = 1 << 11;
    
    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    
    // ステレオ
    std::vector<float> fftDataL;
    std::vector<float> fftDataR;
    
    std::vector<float> complexDataL;
    std::vector<float> complexDataR;
    
    std::vector<float> magnitudeSpectrumL;
    std::vector<float> magnitudeSpectrumR;
    std::vector<float> phaseSpectrumL;
    std::vector<float> phaseSpectrumR;
    
    std::vector<float> accumulationBufferL;
    std::vector<float> accumulationBufferR;
    int accumulationIndex = 0;

    bool isAnalyzing = false;
    
    // THD/IMD
    float currentTHD = 0.0f;
    float currentTHDPlusN = 0.0f;
    float currentIMD = 0.0f;
    std::vector<float> harmonicLevels;
    
    void calculateTHD();
    void calculateIMD();
    
    // Dynamics
    DynamicsData dynamicsData;
    EnvelopeData envelopeData;
    
    void analyzeDynamics(const juce::AudioBuffer<float>& inputBuffer, const juce::AudioBuffer<float>& outputBuffer);
    void analyzeEnvelope(const juce::AudioBuffer<float>& buffer);
    
    // Performance
    PerformanceData performanceData;
    double lastSampleRate = 44100.0;
    int lastBufferSize = 512;
    
    void updatePerformanceMetrics(double processingTimeMs);
    void resizeFFTBuffers();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerEngine)
};