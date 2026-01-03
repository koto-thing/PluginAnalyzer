#include "AnalyzerEngine.h"

AnalyzerEngine::AnalyzerEngine()
{
    // すべての使用可能なプラグインフォーマット
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
    
    // FFTと窓を初期化
    forwardFFT = std::make_unique<juce::dsp::FFT>(fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::hann);
    
    // FFTのデフォのサイズを初期化
    resizeFFTBuffers();
    
    scopeData.resize(scopeFifoSize, 0.0f);
    
    harmonicLevels.resize(10, 0.0f);
    
    DBG("Registered plugin formats: " << formatManager.getNumFormats());
}

AnalyzerEngine::~AnalyzerEngine()
{
}

/**
 * @brief FFT関連のバッファをリサイズする
 */
void AnalyzerEngine::resizeFFTBuffers()
{
	// 実数専用変換のサイズは2 * fftSize(出力が複素数だから)
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

/**
 * @brief FFTオーダーを設定する
 * @param newFftOrder FFTオーダー (8 〜 15)
 */
void AnalyzerEngine::setFFTOrder(int newFftOrder)
{
    if (newFftOrder < 8 || newFftOrder > 15)
        return; // クランプ
    
    fftOrder = newFftOrder;
    fftSize = 1 << fftOrder;
    
	// FFTと窓を再初期化
    forwardFFT = std::make_unique<juce::dsp::FFT>(fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::hann);
    
	// すべてのバッファを再初期化
    resizeFFTBuffers();
    
	// 解析状態をリセット
    accumulationIndex = 0;
}

/**
 * @brief サンプルレートとブロックサイズを設定する
 * @param sampleRate サンプルレート
 * @param blockSize ブロックサイズ
 */
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

/**
 * @brief ブロックサイズを設定する
 */
void AnalyzerEngine::setBlockSize(int /*newBlockSize*/)
{
    
}

/**
 * @brief プラグインをロードする
 * @param file プラグインファイルのパス
 * @return ロード成功ならtrue、失敗ならfalse
 */
bool AnalyzerEngine::loadPlugin(const juce::File& file)
{
    if (!file.existsAsFile()) 
        return false;

    juce::OwnedArray<juce::PluginDescription> foundPlugins;
	// フォーマットマネージャーを使ってプラグインタイプを検索
    for (auto format : formatManager.getFormats())
    {
        format->findAllTypesForFile(foundPlugins, file.getFullPathName());
    }

    if (foundPlugins.size() > 0)
    {
        juce::String error;
		// 使えるプラグインタイプが見つかったらインスタンスを作成
        pluginInstance = formatManager.createPluginInstance(*foundPlugins[0], 44100.0, 512, error);
        
        if (pluginInstance)
        {
            DBG("Plugin Loaded: " << pluginInstance->getName());
            pluginInstance->prepareToPlay(44100.0, 512);
            triggerImpulseAnalysis();
            sendChangeMessage();
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

/**
 * @brief プラグインをアンロードする
 */
void AnalyzerEngine::unloadPlugin()
{
    pluginInstance = nullptr;
    sendChangeMessage();
}

/**
 * @brief インパルス応答解析をトリガーする
 */
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

/**
 * @brief 解析モードを設定する
 * @param mode 解析モード
 */
void AnalyzerEngine::setAnalysisMode(AnalysisMode mode)
{
    if (currentMode != mode)
    {
        currentMode = mode;
        triggerImpulseAnalysis();
    }
}

/**
 * @brief テスト信号の振幅を設定する
 * @param amplitude 振幅 (0.0f 〜 1.0f)
 */
void AnalyzerEngine::setInputAmplitude(float amplitude)
{
    signalGenerator.setAmplitude(amplitude);
}

/**
 * @brief テスト信号の振幅を取得する
 */
float AnalyzerEngine::getInputAmplitude() const
{
    return signalGenerator.getAmplitude();
}

/**
 * @brief テスト信号の周波数を設定する
 * @param frequency 周波数 (Hz)
 */
void AnalyzerEngine::setTestFrequency(double frequency)
{
    signalGenerator.setFrequency(frequency);
}

/**
 * @brief テスト信号の周波数を取得する
 */
double AnalyzerEngine::getTestFrequency() const
{
    return signalGenerator.getFrequency();
}

/**
 * @brief THDとTHD+Nを計算する
 */
void AnalyzerEngine::calculateTHD()
{
	// THDとTHD+Nの計算
    double testFreq = signalGenerator.getFrequency();
    double binWidth = 44100.0 / fftSize;
    
    int fundamentalBin = (int)(testFreq / binWidth + 0.5);
    
    if (fundamentalBin < 1 || fundamentalBin >= fftSize / 2)
    {
        currentTHD = 0.0f;
        currentTHDPlusN = 0.0f;
        return;
    }
    
	// 基音の大きさを取得
    float fundamentalMag = juce::Decibels::decibelsToGain(magnitudeSpectrumL[fundamentalBin]);
    
	// 高調波の合計を計算
    double sumHarmonicsSquared = 0.0;
    double sumAllNoiseSquared = 0.0;
    
	for (int h = 2; h <= 10; ++h) // 2次から10次までの高調波
    {
        int harmonicBin = fundamentalBin * h;
        if (harmonicBin >= fftSize / 2) break;
        
        float harmonicMag = juce::Decibels::decibelsToGain(magnitudeSpectrumL[harmonicBin]);
        sumHarmonicsSquared += harmonicMag * harmonicMag;
        
        if (h - 2 < harmonicLevels.size())
            harmonicLevels[h - 2] = magnitudeSpectrumL[harmonicBin];
    }
    
	// ノイズ成分の合計を計算（高調波以外のすべての成分）
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

/**
 * @brief IMDを計算する
 */
void AnalyzerEngine::calculateIMD()
{
	// IMDの計算
    currentIMD = 0.0f;
}

/**
 * @brief ダイナミクスを解析する
 * @param inputBuffer 入力オーディオバッファ
 * @param outputBuffer 出力オーディオバッファ
 */
void AnalyzerEngine::analyzeDynamics(const juce::AudioBuffer<float>& inputBuffer, const juce::AudioBuffer<float>& outputBuffer)
{
	// 圧縮率の推定
    int numSamples = inputBuffer.getNumSamples();
    
	// RMSレベルを計算
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
    
	// データを保存
    dynamicsData.inputLevels.push_back(inputDB);
    dynamicsData.outputLevels.push_back(outputDB);
    
	// 古いデータを削除
    if (dynamicsData.inputLevels.size() > 1000)
    {
        dynamicsData.inputLevels.erase(dynamicsData.inputLevels.begin());
        dynamicsData.outputLevels.erase(dynamicsData.outputLevels.begin());
    }
    
	// 圧縮率を推定
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

/**
 * @brief エンベロープを解析する
 * @param buffer 入力オーディオバッファ
 */
void AnalyzerEngine::analyzeEnvelope(const juce::AudioBuffer<float>& buffer)
{
	// エンベロープの計算
    int numSamples = buffer.getNumSamples();
    const float* data = buffer.getReadPointer(0);
    
	// エンベロープデータを更新
    for (int i = 0; i < numSamples; ++i)
    {
        float absValue = std::abs(data[i]);
        
		// サンプリングポイントを時間に変換
        float timePoint = (float)envelopeData.envelopeValues.size() / 44100.0f;
        
        envelopeData.timePoints.push_back(timePoint);
        envelopeData.envelopeValues.push_back(absValue);
    }
    
	// 古いデータを削除
	if (envelopeData.envelopeValues.size() > 44100 * 10) // 最大10秒分
    {
        int excess = (int)envelopeData.envelopeValues.size() - 44100 * 10;
        envelopeData.timePoints.erase(envelopeData.timePoints.begin(), envelopeData.timePoints.begin() + excess);
        envelopeData.envelopeValues.erase(envelopeData.envelopeValues.begin(), envelopeData.envelopeValues.begin() + excess);
    }
    
	// アタックタイムの計算
    if (envelopeData.envelopeValues.size() > 100)
    {
		// 最後の100サンプルでの最大値を取得
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

/**
 * @brief パフォーマンスメトリクスを更新する
 * @param processingTimeMs 処理時間（ミリ秒）
 */
void AnalyzerEngine::updatePerformanceMetrics(double processingTimeMs)
{
	// 処理時間履歴を更新
    performanceData.processingTimeHistory.push_back((float)processingTimeMs);
    
	// 履歴が100を超えたら古いデータを削除
    if (performanceData.processingTimeHistory.size() > 100)
    {
        performanceData.processingTimeHistory.erase(performanceData.processingTimeHistory.begin());
    }
    
	// 平均とピーク処理時間を計算
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
    
	// CPU使用率を計算
    double availableTimeMs = (lastBufferSize / lastSampleRate) * 1000.0;
    performanceData.cpuUsagePercent = (float)((processingTimeMs / availableTimeMs) * 100.0);
}

/**
 * @brief オーディオバッファを処理し、解析を行う
 * @param buffer 入力および出力のオーディオバッファ
 */
void AnalyzerEngine::processAudio(juce::AudioBuffer<float>& buffer)
{
	// 解析モードに基づいて信号を生成し、プラグインを処理し、FFT解析を行う
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
    
	// 信号生成
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
            sigType = TestSignalGenerator::SignalType::Sine;
            break;
        default:
            sigType = TestSignalGenerator::SignalType::Impulse;
            break;
    }
    
    signalGenerator.fillBuffer(buffer, sigType, 0);
    
    if (buffer.getNumChannels() > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);

	// 保存用に入力バッファをコピー
    juce::AudioBuffer<float> inputBuffer(buffer.getNumChannels(), numSamples);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        inputBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

	// プラグイン処理
    if (pluginInstance)
    {
		// 開始タイミング
        auto startTime = juce::Time::getMillisecondCounterHiRes();
        
        juce::MidiBuffer midi;
        pluginInstance->processBlock(buffer, midi);
        
		// 終了タイミング
        auto endTime = juce::Time::getMillisecondCounterHiRes();
        double processingTimeMs = endTime - startTime;
        
		// パフォーマンス解析の更新
        if (currentMode == AnalysisMode::Performance)
        {
            updatePerformanceMetrics(processingTimeMs);
        }
    }

	// DynamicsとHammersteinの解析
    if (currentMode == AnalysisMode::Dynamics)
    {
        analyzeDynamics(inputBuffer, buffer);
    }
    else if (currentMode == AnalysisMode::Hammerstein)
    {
        analyzeEnvelope(buffer);
    }

	// FFT解析
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

		// FFTサイズに達したら処理
        if (accumulationIndex >= fftSize)
        {
			 // Process Left
             std::fill(complexDataL.begin(), complexDataL.end(), 0.0f);
             std::copy(accumulationBufferL.begin(), accumulationBufferL.end(), complexDataL.begin());
             
			 // 窓関数の適用
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
             
			 // MagnitudeとPhaseの抽出
             auto extract = [&](const std::vector<float>& complexData, std::vector<float>& magData, std::vector<float>& phaseData) {
                 for (int i = 0; i < fftSize / 2; ++i)
                 {
                     float re = 0.0f;
                     float im = 0.0f;
                     
                     if (i == 0) {
                         re = complexData[0];
                         im = 0.0f;
                     } else if (i == fftSize / 2 - 1) {
						 // Nyquist周波数成分
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
             
			 // THDまたはIMDの計算
             if (currentMode == AnalysisMode::Harmonic || 
                 currentMode == AnalysisMode::THDSweep)
             {
                 calculateTHD();
             }
             else if (currentMode == AnalysisMode::IMD)
             {
                 calculateIMD();
             }
             
			 // 解析完了後のリセット
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
    
	// スコープ用FIFOにデータを追加
    addToScopeFifo(buffer.getReadPointer(0), buffer.getNumSamples());
}

/**
 * @brief スコープFIFOにデータを追加する
 * @param data 追加するデータ配列
 * @param numSamples 追加するサンプル数
 */
void AnalyzerEngine::addToScopeFifo(const float* data, int numSamples)
{
    int start1, size1, start2, size2;
    scopeFifo.prepareToWrite(numSamples, start1, size1, start2, size2);
    
    if (size1 > 0)
        std::copy(data, data + size1, scopeData.begin() + start1);
    
    if (size2 > 0) 
        std::copy(data + size1, data + size1 + size2, scopeData.begin() + start2);
    
    scopeFifo.finishedWrite(size1 + size2);
}

/**
 * @brief スコープFIFOからデータを読み取る
 * @param dest データの格納先配列
 * @param numSamples 読み取るサンプル数
 */
int AnalyzerEngine::readFromScopeFifo(float* dest, int numSamples)
{
    int start1, size1, start2, size2;
    scopeFifo.prepareToRead(numSamples, start1, size1, start2, size2);
    
    if (size1 > 0) 
        std::copy(scopeData.begin() + start1, scopeData.begin() + start1 + size1, dest);
    
    if (size2 > 0) 
        std::copy(scopeData.begin() + start2, scopeData.begin() + start2 + size2, dest + size1);
    
    scopeFifo.finishedRead(size1 + size2);
    return size1 + size2;
}