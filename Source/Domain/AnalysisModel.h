#pragma once

#include <cstdint>
#include <vector>

namespace plugin_analyzer::domain
{
/**
 * @brief アナライザーが実行する解析種別
 */
enum class AnalysisMode
{
    Linear,
    Harmonic,
    Hammerstein,
    WhiteNoise,
    SineSweep,
    THDSweep,
    IMD,
    Dynamics,
    Performance
};

/**
 * @brief 入出力レベルから算出したダイナミクス解析結果
 */
struct DynamicsData
{
    std::vector<float> inputLevels;
    std::vector<float> outputLevels;
    float compressionRatio = 1.0f;
    float threshold = 0.0f;
};

/**
 * @brief 時間軸上のエンベロープ解析結果
 */
struct EnvelopeData
{
    std::vector<float> timePoints;
    std::vector<float> envelopeValues;
    float attackTime = 0.0f;
    float releaseTime = 0.0f;
};

/**
 * @brief オーディオ処理時間とドロップ数をまとめた性能解析結果
 */
struct PerformanceData
{
    float averageProcessingTime = 0.0f;
    float peakProcessingTime = 0.0f;
    float p95ProcessingTime = 0.0f;
    float p99ProcessingTime = 0.0f;
    float cpuUsagePercent = 0.0f;
    int bufferSize = 0;
    double sampleRate = 0.0;
    std::uint64_t droppedAnalysisSamples = 0;
    std::uint64_t droppedScopeSamples = 0;
    std::uint64_t droppedPerformanceRecords = 0;
    std::vector<float> processingTimeHistory;
};

/**
 * @brief UIへ公開する読み取り専用の解析結果
 *
 * 公開後のインスタンスは変更しない。利用側は描画や表示更新が完了するまで
 * 同じ共有ポインタを保持する。
 */
struct AnalysisSnapshot
{
    std::vector<float> magnitudeSpectrumL;
    std::vector<float> magnitudeSpectrumR;
    std::vector<float> phaseSpectrumL;
    std::vector<float> phaseSpectrumR;
    std::vector<float> harmonicLevels;
    std::vector<float> thdSweepFrequencies;
    std::vector<float> thdSweepValues;
    DynamicsData dynamics;
    EnvelopeData envelope;
    PerformanceData performance;
    float thd = 0.0f;
    float thdPlusN = 0.0f;
    float imd = 0.0f;
    int latencySamples = 0;
    double sampleRate = 44100.0;
};
}
