#include <JuceHeader.h>
#include "../Source/AnalyzerEngine.h"
#include "../Source/Application/AnalysisSession.h"
#include "../Source/TestSignalGenerator.h"
#include <atomic>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace
{
constexpr double testSampleRate = 48000.0;
constexpr int testBlockSize = 256;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(double actual, double expected, double tolerance, const char* message)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(std::string(message) + " (actual "
                                 + std::to_string(actual) + ", expected "
                                 + std::to_string(expected) + ")");
}

double toneAmplitude(const float* samples, int count, double frequency)
{
    double real = 0.0;
    double imaginary = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const auto phase = juce::MathConstants<double>::twoPi * frequency * i / testSampleRate;
        real += samples[i] * std::cos(phase);
        imaginary -= samples[i] * std::sin(phase);
    }
    return 2.0 * std::sqrt(real * real + imaginary * imaginary) / count;
}

struct ProcessorStats
{
    std::atomic<int> prepareCalls { 0 };
    std::atomic<int> releaseCalls { 0 };
    std::atomic<int> processCalls { 0 };
};

class FakeProcessor final : public juce::AudioProcessor
{
public:
    enum class Kind { Gain, Delay, Clipper, Waveshaper, Compressor };

    FakeProcessor(Kind processorKind, float valueToUse,
                  std::shared_ptr<ProcessorStats> sharedStats = {})
        : AudioProcessor(BusesProperties()
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          kind(processorKind), value(valueToUse), stats(std::move(sharedStats))
    {
        if (kind == Kind::Delay)
            setLatencySamples(juce::jmax(0, juce::roundToInt(value)));
    }

    const juce::String getName() const override
    {
        static const char* names[] = { "Fake Gain", "Fake Delay", "Fake Clipper",
                                      "Fake Waveshaper", "Fake Compressor" };
        return names[static_cast<size_t>(kind)];
    }

    void prepareToPlay(double, int maximumExpectedSamplesPerBlock) override
    {
        if (stats)
            ++stats->prepareCalls;
        const auto delay = kind == Kind::Delay ? juce::jmax(1, juce::roundToInt(value)) : 1;
        delayLines.assign(2, std::vector<float>(
            static_cast<size_t>(delay + maximumExpectedSamplesPerBlock), 0.0f));
        delayWrite = 0;
    }

    void releaseResources() override
    {
        if (stats)
            ++stats->releaseCalls;
        delayLines.clear();
    }

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        if (stats)
            ++stats->processCalls;

        if (kind == Kind::Delay)
        {
            const auto delay = juce::jmax(1, juce::roundToInt(value));
            const auto lineSize = static_cast<int>(delayLines[0].size());
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto read = (delayWrite - delay + lineSize) % lineSize;
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                {
                    auto& line = delayLines[static_cast<size_t>(channel)];
                    const auto input = buffer.getSample(channel, sample);
                    buffer.setSample(channel, sample, line[static_cast<size_t>(read)]);
                    line[static_cast<size_t>(delayWrite)] = input;
                }
                delayWrite = (delayWrite + 1) % lineSize;
            }
            return;
        }

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* samples = buffer.getWritePointer(channel);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto x = samples[i];
                switch (kind)
                {
                    case Kind::Gain: samples[i] = x * value; break;
                    case Kind::Clipper:
                        samples[i] = juce::jlimit(-value, value, x);
                        break;
                    case Kind::Waveshaper:
                        samples[i] = x + value * x * x;
                        break;
                    case Kind::Compressor:
                    {
                        const auto threshold = value;
                        const auto magnitude = std::abs(x);
                        samples[i] = magnitude <= threshold ? x
                            : std::copysign(threshold
                                + (magnitude - threshold) * 0.25f, x);
                        break;
                    }
                    case Kind::Delay: break;
                }
            }
        }
    }

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }

private:
    Kind kind;
    float value;
    std::shared_ptr<ProcessorStats> stats;
    std::vector<std::vector<float>> delayLines;
    int delayWrite = 0;
};

void processBlocks(AnalyzerEngine& engine, int count,
                   int samplesPerBlock = testBlockSize)
{
    juce::AudioBuffer<float> buffer(2, samplesPerBlock);
    for (int block = 0; block < count; ++block)
    {
        buffer.clear();
        engine.processAudio(buffer);
        if ((block & 7) == 0)
            std::this_thread::yield();
    }
}

template <typename Predicate>
bool waitFor(Predicate predicate, int timeoutMs = 3000)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;
    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (predicate())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
}

void testSignalGeneration()
{
    TestSignalGenerator generator;
    generator.prepare(testSampleRate, 4096);
    generator.setAmplitude(0.4f);
    generator.setFrequency(1500.0);
    juce::AudioBuffer<float> signal(1, 4096);
    generator.fillBuffer(signal, TestSignalGenerator::SignalType::Sine, 0);
    requireNear(toneAmplitude(signal.getReadPointer(0), signal.getNumSamples(), 1500.0),
                0.4, 0.002, "Sine amplitude is not calibrated");

    generator.reset();
    generator.fillBuffer(signal, TestSignalGenerator::SignalType::Impulse, 0);
    requireNear(signal.getSample(0, 0), 0.4, 1.0e-6, "Impulse amplitude is incorrect");
    requireNear(signal.getMagnitude(0, 1, signal.getNumSamples() - 1), 0.0, 1.0e-7,
                "Impulse contains trailing samples");

    generator.reset();
    generator.setAmplitude(0.6f);
    generator.setIMDFrequencies(750.0, 6000.0);
    generator.setIMDAmplitudeRatio(1.0);
    generator.fillBuffer(signal, TestSignalGenerator::SignalType::IMDDualTone, 0);
    requireNear(toneAmplitude(signal.getReadPointer(0), signal.getNumSamples(), 750.0),
                0.3, 0.002, "IMD low tone amplitude is incorrect");
    requireNear(toneAmplitude(signal.getReadPointer(0), signal.getNumSamples(), 6000.0),
                0.3, 0.002, "IMD high tone amplitude is incorrect");
}

void testFakeProcessors()
{
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer(2, 8);
    buffer.clear();
    buffer.setSample(0, 0, 0.8f);
    buffer.setSample(1, 0, -0.8f);

    FakeProcessor clipper(FakeProcessor::Kind::Clipper, 0.25f);
    clipper.prepareToPlay(testSampleRate, 8);
    clipper.processBlock(buffer, midi);
    requireNear(buffer.getSample(0, 0), 0.25, 1.0e-6, "Clipper positive limit failed");
    requireNear(buffer.getSample(1, 0), -0.25, 1.0e-6, "Clipper negative limit failed");

    FakeProcessor compressor(FakeProcessor::Kind::Compressor, 0.2f);
    compressor.prepareToPlay(testSampleRate, 8);
    buffer.setSample(0, 0, 1.0f);
    compressor.processBlock(buffer, midi);
    requireNear(buffer.getSample(0, 0), 0.4, 1.0e-6, "Compressor ratio failed");
}

void testLinearFFTAndLifecycle()
{
    AnalyzerEngine engine;
    engine.prepare(testSampleRate, testBlockSize);
    engine.setFFTOrder(11);
    auto stats = std::make_shared<ProcessorStats>();
    require(engine.loadProcessor(std::make_unique<FakeProcessor>(
                FakeProcessor::Kind::Gain, 2.0f, stats)),
            "Fake gain could not be loaded");
    require(engine.getPluginName() == "Fake Gain", "Loaded processor name is incorrect");
    processBlocks(engine, 16);
    require(waitFor([&]
    {
        const auto snapshot = engine.getAnalysisSnapshot();
        const auto bin = juce::roundToInt(1000.0 * 2048.0 / testSampleRate);
        return snapshot && snapshot->magnitudeSpectrumL.size() > static_cast<size_t>(bin)
            && snapshot->magnitudeSpectrumL[static_cast<size_t>(bin)] > 5.5f;
    }), "Linear FFT did not publish a gain result");
    const auto gainSnapshot = engine.getAnalysisSnapshot();
    const auto bin = juce::roundToInt(1000.0 * 2048.0 / testSampleRate);
    requireNear(gainSnapshot->magnitudeSpectrumL[static_cast<size_t>(bin)],
                juce::Decibels::gainToDecibels(2.0f), 0.15,
                "Linear transfer magnitude is inaccurate");

    engine.prepare(44100.0, 128);
    processBlocks(engine, 24, 128);
    const auto alternateBin = juce::roundToInt(1000.0 * 2048.0 / 44100.0);
    require(waitFor([&]
    {
        const auto snapshot = engine.getAnalysisSnapshot();
        return snapshot->sampleRate == 44100.0
            && snapshot->magnitudeSpectrumL[static_cast<size_t>(alternateBin)] > 5.5f;
    }), "Linear FFT did not re-calibrate at 44.1 kHz / 128 samples");
    requireNear(engine.getAnalysisSnapshot()
                    ->magnitudeSpectrumL[static_cast<size_t>(alternateBin)],
                juce::Decibels::gainToDecibels(2.0f), 0.15,
                "Alternate device transfer magnitude is inaccurate");
    engine.unloadPlugin();
    require(stats->prepareCalls == 2, "Processor was not prepared on load and device change");
    require(stats->releaseCalls == 2, "Processor lifecycle did not release exactly once per prepare");

    engine.prepare(testSampleRate, testBlockSize);
    require(engine.loadProcessor(std::make_unique<FakeProcessor>(
                FakeProcessor::Kind::Delay, 37.0f)),
            "Fake delay could not be loaded");
    processBlocks(engine, 16);
    require(waitFor([&] { return engine.getAnalysisSnapshot()->latencySamples == 37; }),
            "Reported delay latency was not measured");
    const auto delaySnapshot = engine.getAnalysisSnapshot();
    requireNear(delaySnapshot->phaseSpectrumL[static_cast<size_t>(bin)], 0.0, 0.08,
                "Delay phase was not latency-corrected");
}

void testDistortionMeasurements()
{
    AnalyzerEngine engine;
    engine.prepare(testSampleRate, testBlockSize);
    engine.setFFTOrder(11);
    engine.setAnalysisMode(AnalyzerEngine::AnalysisMode::Harmonic);
    engine.setTestFrequency(1500.0);
    engine.setInputAmplitude(0.5f);
    require(engine.loadProcessor(std::make_unique<FakeProcessor>(
                FakeProcessor::Kind::Waveshaper, 0.5f)),
            "Fake waveshaper could not be loaded");
    processBlocks(engine, 48);
    require(waitFor([&] { return engine.getAnalysisSnapshot()->thd > 10.0f; }),
            "THD measurement did not detect the second harmonic");
    const auto harmonic = engine.getAnalysisSnapshot();
    requireNear(harmonic->thd, 12.5, 1.0, "THD calibration is outside tolerance");
    requireNear(harmonic->harmonicLevels[0], -18.06, 1.0,
                "Second harmonic level is outside tolerance");

    engine.unloadPlugin();
    require(engine.loadProcessor(std::make_unique<FakeProcessor>(
                FakeProcessor::Kind::Clipper, 0.2f)),
            "Fake clipper could not be loaded");
    processBlocks(engine, 32);
    require(waitFor([&] { return engine.getAnalysisSnapshot()->thd > 20.0f; }),
            "THD measurement did not detect clipping");

    engine.setAnalysisMode(AnalyzerEngine::AnalysisMode::IMD);
    engine.unloadPlugin();
    require(engine.loadProcessor(std::make_unique<FakeProcessor>(
                FakeProcessor::Kind::Waveshaper, 1.0f)),
            "Fake IMD waveshaper could not be loaded");
    processBlocks(engine, 48);
    require(waitFor([&] { return engine.getAnalysisSnapshot()->imd > 1.0f; }),
            "IMD measurement did not detect intermodulation products");
}

void testFifoAndSmoke()
{
    AnalyzerEngine engine;
    engine.prepare(testSampleRate, testBlockSize);
    std::array<float, 512> source {};
    std::array<float, 512> destination {};
    for (size_t i = 0; i < source.size(); ++i)
        source[i] = static_cast<float>(i);
    engine.addToScopeFifo(source.data(), static_cast<int>(source.size()));
    require(engine.readFromScopeFifo(destination.data(), 512) == 512,
            "Scope FIFO returned the wrong sample count");
    require(std::equal(source.begin(), source.end(), destination.begin()),
            "Scope FIFO changed sample order");

    for (int mode = static_cast<int>(AnalyzerEngine::AnalysisMode::Linear);
         mode <= static_cast<int>(AnalyzerEngine::AnalysisMode::Performance); ++mode)
    {
        engine.setAnalysisMode(static_cast<AnalyzerEngine::AnalysisMode>(mode));
        processBlocks(engine, 10);
        const auto snapshot = engine.getAnalysisSnapshot();
        require(snapshot != nullptr && std::isfinite(snapshot->sampleRate),
                "Mode smoke test produced an invalid snapshot");
    }
    engine.releaseResources();
}

void testAnalysisSessionPresentationPolicy()
{
    using plugin_analyzer::application::AnalysisSession;
    using plugin_analyzer::application::AnalysisService;
    using plugin_analyzer::application::ContentView;
    using plugin_analyzer::domain::AnalysisMode;
    using plugin_analyzer::domain::AnalysisSnapshot;

    class RecordingAnalysisService final : public AnalysisService
    {
    public:
        void setAnalysisMode(AnalysisMode newMode) override { mode = newMode; }
        AnalysisMode getAnalysisMode() const override { return mode; }
        void setInputAmplitude(float) override {}
        void setTestFrequency(double) override {}
        std::shared_ptr<const AnalysisSnapshot> getAnalysisSnapshot() const override
        {
            return std::make_shared<const AnalysisSnapshot>();
        }
        std::string getPluginDisplayName() const override { return {}; }
        int readFromScopeFifo(float*, int) override { return 0; }

        AnalysisMode mode = AnalysisMode::Linear;
    };

    const auto linear = AnalysisSession::selectionForTab(0);
    require(linear.content == ContentView::AnalysisGraph,
            "Linear analysis should use the graph view");
    require(linear.controls.phase && !linear.controls.amplitude,
            "Linear analysis controls are incorrect");

    const auto harmonic = AnalysisSession::selectionForTab(1);
    require(harmonic.controls.amplitude && harmonic.controls.frequency
                && harmonic.controls.distortion,
            "Harmonic analysis controls are incorrect");

    const auto scope = AnalysisSession::selectionForTab(7);
    require(scope.content == ContentView::Oscilloscope,
            "Oscilloscope tab should use the scope view");

    const auto performance = AnalysisSession::selectionForTab(9);
    require(performance.controls.performance && !performance.controls.phase,
            "Performance analysis controls are incorrect");

    RecordingAnalysisService service;
    AnalysisSession session(service);
    (void) session.selectTab(3);
    require(service.mode == AnalysisMode::IMD,
            "Analysis session did not invoke the selected use case");
    (void) session.selectTab(7);
    require(service.mode == AnalysisMode::IMD,
            "Oscilloscope should not change the active analysis mode");
}

void runStressTest()
{
    AnalyzerEngine engine;
    engine.prepare(testSampleRate, testBlockSize);
    engine.setAnalysisMode(AnalyzerEngine::AnalysisMode::Performance);
    auto stats = std::make_shared<ProcessorStats>();
    require(engine.loadProcessor(std::make_unique<FakeProcessor>(
                FakeProcessor::Kind::Gain, 1.0f, stats)),
            "Stress processor could not be loaded");
    const auto end = juce::Time::getMillisecondCounterHiRes() + 5000.0;
    while (juce::Time::getMillisecondCounterHiRes() < end)
    {
        processBlocks(engine, 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(stats->processCalls > 1000, "Stress test processed too few blocks");
    require(engine.getAnalysisSnapshot()->performance.peakProcessingTime < 20.0f,
            "Stress processor exceeded the dropout guard");
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    try
    {
        if (argc > 1 && juce::String(argv[1]) == "--stress")
        {
            runStressTest();
            std::cout << "PluginAnalyzer stress test passed\n";
            return 0;
        }

        testSignalGeneration();
        testFakeProcessors();
        testLinearFFTAndLifecycle();
        testDistortionMeasurements();
        testFifoAndSmoke();
        testAnalysisSessionPresentationPolicy();
        std::cout << "PluginAnalyzer Phase 6 tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
