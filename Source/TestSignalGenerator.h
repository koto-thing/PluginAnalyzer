#pragma once

#include <JuceHeader.h>

class TestSignalGenerator
{
public:
    enum class SignalType
    {
        Impulse,
        SineSweep,
        WhiteNoise,
        Sine,
        Ramp,
        AttackRelease
    };

    TestSignalGenerator() = default;

    /**
	 * @brief 準備
	 * @param sampleRate サンプリング周波数
	 * @param blockSize ブロックサイズ
     */
    void prepare(double sampleRate, int blockSize)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = blockSize;
        sweepSampleCount = 0;
    }

    /**
     * @brief バッファを指定された信号で満たす
     * @param buffer オーディオバッファ
     * @param type 信号の種類
     * @param channel チャンネルインデックス
	 */
    void fillBuffer(juce::AudioBuffer<float>& buffer, SignalType type, int channel)
    {
        auto* writePointer = buffer.getWritePointer(channel);
        int numSamples = buffer.getNumSamples();

        switch (type)
        {
        case SignalType::Impulse:
            generateImpulse(writePointer, numSamples);
            break;
        case SignalType::Sine:
            generateSine(writePointer, numSamples);
            break;
        case SignalType::WhiteNoise:
            generateWhiteNoise(writePointer, numSamples);
            break;
        case SignalType::SineSweep:
            generateSineSweep(writePointer, numSamples);
            break;
        case SignalType::Ramp:
            generateRamp(writePointer, numSamples);
            break;
        case SignalType::AttackRelease:
            generateAttackRelease(writePointer, numSamples);
            break;
        default:
            juce::FloatVectorOperations::clear(writePointer, numSamples);
            break;
        }
    }

    /**
	 * @brief ジェネレーターの状態をリセット
     */
    void reset()
    {
        impulseFired = false;
        phase = 0.0;
        sweepSampleCount = 0;
        imdPhase1 = 0.0;
        imdPhase2 = 0.0;
        rampSampleCount = 0;
        attackReleaseSampleCount = 0;
        isInAttackPhase = true;
    }

    void setAmplitude(float newAmplitude) { amplitude = juce::jlimit(0.0f, 1.0f, newAmplitude); }
    float getAmplitude() const { return amplitude; }

    void setFrequency(double newFrequency) { frequency = juce::jlimit(20.0, 20000.0, newFrequency); }
    double getFrequency() const { return frequency; }

    void setIMDFrequencies(double freq1, double freq2)
    {
        imdFreq1 = juce::jlimit(20.0, 20000.0, freq1);
        imdFreq2 = juce::jlimit(20.0, 20000.0, freq2);
    }

    void setSweepParameters(double startFreq, double endFreq, double duration)
    {
        sweepStartFreq = startFreq;
        sweepEndFreq = endFreq;
        sweepDuration = duration;
    }

    void setRampParameters(double duration, float startLevel, float endLevel)
    {
        rampDuration = duration;
        rampStartLevel = startLevel;
        rampEndLevel = endLevel;
    }

    void setAttackReleaseParameters(double attackTime, double releaseTime)
    {
        attackDuration = attackTime;
        releaseDuration = releaseTime;
    }

private:
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

	// インパルス状態
    bool impulseFired = false;

	// サイン波状態
    double phase = 0.0;
    double frequency = 1000.0;
    float amplitude = 0.5f;

	// ホワイトノイズ状態
    juce::Random random;

	// サインスイープ状態
    int sweepSampleCount = 0;
    double sweepStartFreq = 20.0;
    double sweepEndFreq = 20000.0;
    double sweepDuration = 5.0;

	// IMD状態
    double imdPhase1 = 0.0;
    double imdPhase2 = 0.0;
    double imdFreq1 = 250.0;
    double imdFreq2 = 8000.0;

	// ランプ状態
    int rampSampleCount = 0;
    double rampDuration = 2.0;
    float rampStartLevel = -60.0f;  // dB
    float rampEndLevel = 0.0f;      // dB

	// アタック・リリース状態
    int attackReleaseSampleCount = 0;
    double attackDuration = 0.1;
    double releaseDuration = 0.5;
    bool isInAttackPhase = true;

    /**
	 * @brief インパルス信号を生成
	 * @param buffer 出力バッファ
	 * @param numSamples サンプル数
     */
    void generateImpulse(float* buffer, int numSamples)
    {
        juce::FloatVectorOperations::clear(buffer, numSamples);

        if (!impulseFired)
        {
            // Dirac delta at the beginning
            buffer[0] = amplitude;
            impulseFired = true;
        }
    }

    /**
	 * @brief サイン波信号を生成
	 * @param buffer 出力バッファ
	 * @param numSamples サンプル数
     */
    void generateSine(float* buffer, int numSamples)
    {
        auto radsPerSample = juce::MathConstants<double>::twoPi * frequency / currentSampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            buffer[i] = amplitude * (float)std::sin(phase);
            phase += radsPerSample;
            if (phase > juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }
    }

    /**
	 * @brief ホワイトノイズ信号を生成
	 * @param buffer 出力バッファ
	 * @param numSamples サンプル数
     */
    void generateWhiteNoise(float* buffer, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            buffer[i] = amplitude * (random.nextFloat() * 2.0f - 1.0f);
        }
    }

    /**
	 * @brief サインスイープ信号を生成
	 * @param buffer 出力バッファ
	 * @param numSamples サンプル数
     */
    void generateSineSweep(float* buffer, int numSamples)
    {
        double totalSamples = sweepDuration * currentSampleRate;
        
        for (int i = 0; i < numSamples; ++i)
        {
            double progress = sweepSampleCount / totalSamples;
            
            if (progress >= 1.0)
            {
                sweepSampleCount = 0;
                progress = 0.0;
            }
            
            double currentFreq = sweepStartFreq * std::pow(sweepEndFreq / sweepStartFreq, progress);
            
            buffer[i] = amplitude * (float)std::sin(phase);
            
            double radsPerSample = juce::MathConstants<double>::twoPi * currentFreq / currentSampleRate;
            phase += radsPerSample;
            
            while (phase > juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
            
            sweepSampleCount++;
        }
    }

    /**
	 * @brief ランプ信号を生成
	 * @param buffer 出力バッファ
	 * @param numSamples サンプル数
     */
    void generateRamp(float* buffer, int numSamples)
    {
        double totalSamples = rampDuration * currentSampleRate;
        
        for (int i = 0; i < numSamples; ++i)
        {
            double progress = rampSampleCount / totalSamples;
            
            if (progress >= 1.0)
            {
                rampSampleCount = 0;
                progress = 0.0;
            }
            
			// 線形にレベルを変化させる
            float currentDB = rampStartLevel + (rampEndLevel - rampStartLevel) * (float)progress;
            float linearGain = juce::Decibels::decibelsToGain(currentDB);
            
			// 現在の周波数でサイン波を生成
            buffer[i] = linearGain * (float)std::sin(phase);
            
            double radsPerSample = juce::MathConstants<double>::twoPi * frequency / currentSampleRate;
            phase += radsPerSample;
            
            while (phase > juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
            
            rampSampleCount++;
        }
    }

    /**
	 * @brief アタック・リリース信号を生成
	 * @param buffer 出力バッファ
	 * @param numSamples サンプル数
     */
    void generateAttackRelease(float* buffer, int numSamples)
    {
        double attackSamples = attackDuration * currentSampleRate;
        double releaseSamples = releaseDuration * currentSampleRate;
        
        for (int i = 0; i < numSamples; ++i)
        {
            float envelope = 0.0f;
            
            if (isInAttackPhase)
            {
				// Attack phase: 0 -> 1
                double progress = attackReleaseSampleCount / attackSamples;
                
                if (progress >= 1.0)
                {
					// リリースフェーズへ移行
                    isInAttackPhase = false;
                    attackReleaseSampleCount = 0;
                    envelope = 1.0f;
                }
                else
                {
                    envelope = (float)progress;
                }
            }
            else
            {
                // Release phase: 1 -> 0
                double progress = attackReleaseSampleCount / releaseSamples;
                
                if (progress >= 1.0)
                {
					// アタックフェーズへ移行
                    isInAttackPhase = true;
                    attackReleaseSampleCount = 0;
                    envelope = 0.0f;
                }
                else
                {
                    envelope = 1.0f - (float)progress;
                }
            }
            
			// テスト周波数でサイン波を生成
            buffer[i] = amplitude * envelope * (float)std::sin(phase);
            
            double radsPerSample = juce::MathConstants<double>::twoPi * frequency / currentSampleRate;
            phase += radsPerSample;
            
            while (phase > juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
            
            attackReleaseSampleCount++;
        }
    }
};
