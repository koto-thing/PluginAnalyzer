#include "MainComponent.h"
#include "OscilloscopeComponent.h"
#include "AnalysisGraphComponent.h"
#include "PluginScannerComponent.h"

MainComponent::MainComponent()
{
	// ルックアンドフィール設定
    setLookAndFeel(&sslLookAndFeel);
    
	// ビューコンポーネント作成
    graphComponent = std::make_unique<AnalysisGraphComponent>(engine);
    scopeComponent = std::make_unique<OscilloscopeComponent>(engine);
    engine.addChangeListener(graphComponent.get()); 
    
    addAndMakeVisible(tabs);
    tabs.addTab("LinearAnalysis", juce::Colours::darkgrey, 0);
    tabs.addTab("HarmonicAnalysis", juce::Colours::darkgrey, 1);
    tabs.addTab("THD Sweep", juce::Colours::darkgrey, 2);
    tabs.addTab("IMD", juce::Colours::darkgrey, 3);
    tabs.addTab("Hammerstein", juce::Colours::darkgrey, 4);
    tabs.addTab("WhiteNoise", juce::Colours::darkgrey, 5);
    tabs.addTab("SineSweep", juce::Colours::darkgrey, 6);
    tabs.addTab("Oscilloscope", juce::Colours::darkgrey, 7);
    tabs.addTab("Dynamics", juce::Colours::darkgrey, 8);
    tabs.addTab("Performance", juce::Colours::darkgrey, 9);
    tabs.addChangeListener(this);
    tabs.setCurrentTabIndex(0);

    addAndMakeVisible(loadButton);
    loadButton.onClick = [this] { loadPluginClicked(); };
    loadButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff444444));
    loadButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    
    addAndMakeVisible(showPhaseButton);
    showPhaseButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::cyan);
    showPhaseButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    showPhaseButton.onClick = [this] {
        if (graphComponent)
            graphComponent->setShowPhase(showPhaseButton.getToggleState());
    };
    
    addAndMakeVisible(pluginNameLabel);
    pluginNameLabel.setText("No Plugin Loaded", juce::dontSendNotification);
    pluginNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    pluginNameLabel.setJustificationType(juce::Justification::centredRight);
    
    // Settings
    addAndMakeVisible(settingsButton);
    settingsButton.onClick = [this] { showSettingsDialog(); };
    settingsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d2d2d));
    settingsButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    
    // Browser
    addAndMakeVisible(browserButton);
    browserButton.onClick = [this] { showPluginBrowser(); };
    browserButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d2d2d));
    browserButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    
    // 現在の設定を初期化
    currentSettings.bufferSize = 512;
    currentSettings.sampleRate = 48000.0;
    currentSettings.fftOrder = 11;
    
    // THD
    addAndMakeVisible(amplitudeSlider);
    amplitudeSlider.setRange(0.0, 1.0, 0.01);
    amplitudeSlider.setValue(0.5);
    amplitudeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    amplitudeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    amplitudeSlider.onValueChange = [this] {
        engine.setInputAmplitude((float)amplitudeSlider.getValue());
    };
    
    addAndMakeVisible(amplitudeLabel);
    amplitudeLabel.setText("Amplitude:", juce::dontSendNotification);
    amplitudeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    amplitudeLabel.attachToComponent(&amplitudeSlider, true);
    
    addAndMakeVisible(frequencySlider);
    frequencySlider.setRange(20.0, 20000.0, 1.0);
    frequencySlider.setValue(1000.0);
    frequencySlider.setSkewFactorFromMidPoint(1000.0);
    frequencySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    frequencySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    frequencySlider.onValueChange = [this] {
        engine.setTestFrequency(frequencySlider.getValue());
    };
    
    addAndMakeVisible(frequencyLabel);
    frequencyLabel.setText("Frequency (Hz):", juce::dontSendNotification);
    frequencyLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    frequencyLabel.attachToComponent(&frequencySlider, true);
    
    addAndMakeVisible(thdLabel);
    thdLabel.setText("THD:", juce::dontSendNotification);
    thdLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    
    addAndMakeVisible(thdValueLabel);
    thdValueLabel.setText("0.00%", juce::dontSendNotification);
    thdValueLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    thdValueLabel.setJustificationType(juce::Justification::centredLeft);
    
    // Dynamics
    addAndMakeVisible(dynamicsLabel);
    dynamicsLabel.setText("Dynamics:", juce::dontSendNotification);
    dynamicsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    
    addAndMakeVisible(compressionRatioLabel);
    compressionRatioLabel.setText("Ratio: 1:1", juce::dontSendNotification);
    compressionRatioLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    
    addAndMakeVisible(envelopeLabel);
    envelopeLabel.setText("Envelope:", juce::dontSendNotification);
    envelopeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    
    addAndMakeVisible(attackTimeLabel);
    attackTimeLabel.setText("Attack: 0ms", juce::dontSendNotification);
    attackTimeLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    
    // Performance
    addAndMakeVisible(performanceLabel);
    performanceLabel.setText("Performance:", juce::dontSendNotification);
    performanceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    performanceLabel.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    
    addAndMakeVisible(avgProcessingTimeLabel);
    avgProcessingTimeLabel.setText("Avg: 0.00 ms", juce::dontSendNotification);
    avgProcessingTimeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    avgProcessingTimeLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    
    addAndMakeVisible(peakProcessingTimeLabel);
    peakProcessingTimeLabel.setText("Peak: 0.00 ms", juce::dontSendNotification);
    peakProcessingTimeLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    peakProcessingTimeLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    
    addAndMakeVisible(cpuUsageLabel);
    cpuUsageLabel.setText("CPU: 0.0%", juce::dontSendNotification);
    cpuUsageLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
    cpuUsageLabel.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    
    addAndMakeVisible(graphComponent.get());
    currentContentComp = graphComponent.get();

    setSize(800, 600);
    setAudioChannels(2, 2);
    
    startTimer(100);
}

MainComponent::~MainComponent()
{
    setLookAndFeel(nullptr);
    shutdownAudio();
}

/**
 * @brief オーディオ準備
 * @param samplesPerBlockExpected 期待されるサンプルブロックサイズ
 * @param sampleRate サンプルレート
 */
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
	// 現在の設定に基づいてエンジンを準備
    int actualBufferSize = (currentSettings.bufferSize > 0) ? currentSettings.bufferSize : samplesPerBlockExpected;
    double actualSampleRate = (currentSettings.sampleRate > 0) ? currentSettings.sampleRate : sampleRate;
    
    engine.prepare(actualSampleRate, actualBufferSize);
}

/**
 * @brief 次のオーディオブロックを取得
 * @param bufferToFill オーディオバッファ情報
 */
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
	// 現在のバッファ領域をクリア
    bufferToFill.clearActiveBufferRegion();
    
	// 一時バッファを作成
    juce::AudioBuffer<float> tempBuffer(bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples);
    tempBuffer.clear(); // Start silent
    
	// エンジンでオーディオ処理
    engine.processAudio(tempBuffer);
}

void MainComponent::releaseResources()
{
    
}

/**
 * @brief コンポーネントの描画
 * @param g グラフィックスコンテキスト
 */
void MainComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // メインの背景
    g.fillAll(juce::Colour(0xff1a1a1a));
    
	// ヘッダーのグラデーション
    auto headerBounds = bounds.removeFromTop(40);
    auto headerGradient = juce::ColourGradient::vertical(
        juce::Colour(0xff2d2d2d), headerBounds.getY(),
        juce::Colour(0xff1a1a1a), headerBounds.getBottom()
    );
    g.setGradientFill(headerGradient);
    g.fillRect(headerBounds);
    
    // ヘッダのセパレータ
    g.setColour(juce::Colour(0xff00a0ff).withAlpha(0.5f));
    g.fillRect(0, 40, getWidth(), 2);
    
	// セクションセパレータ
    g.setColour(juce::Colour(0xff404040).withAlpha(0.3f));
    g.drawLine(0, 70, getWidth(), 70, 1.0f);
    g.drawLine(0, 150, getWidth(), 150, 1.0f);
    
    juce::Path cornerPath;
    cornerPath.addTriangle(0, 0, 30, 0, 0, 30);
    g.setColour(juce::Colour(0xff00a0ff).withAlpha(0.2f));
    g.fillPath(cornerPath);
    
    g.setColour(juce::Colour(0xff00a0ff));
    g.fillRect(0, 0, getWidth(), 1);
}

/**
 * @brief コンポーネントのリサイズ処理
 */
void MainComponent::resized()
{
    auto area = getLocalBounds();
    
    // ヘッダー
    auto header = area.removeFromTop(40);
    loadButton.setBounds(header.removeFromLeft(120).reduced(5));
    browserButton.setBounds(header.removeFromLeft(90).reduced(5));
    settingsButton.setBounds(header.removeFromLeft(100).reduced(5));
    showPhaseButton.setBounds(header.removeFromLeft(100).reduced(5));
    pluginNameLabel.setBounds(header.removeFromRight(300).reduced(5));
    
    // タブ
    auto tabBar = area.removeFromTop(30);
    tabs.setBounds(tabBar);
    
    // THD
    auto controlArea = area.removeFromTop(80);
    auto row1 = controlArea.removeFromTop(30);
    amplitudeSlider.setBounds(row1.removeFromLeft(300).reduced(5));
    
    auto row2 = controlArea.removeFromTop(30);
    frequencySlider.setBounds(row2.removeFromLeft(400).reduced(5));
    
    thdLabel.setBounds(row2.removeFromLeft(60).reduced(5));
    thdValueLabel.setBounds(row2.removeFromLeft(150).reduced(5));
    
    // Dynamics
    dynamicsLabel.setBounds(row2.removeFromLeft(80).reduced(5));
    compressionRatioLabel.setBounds(row2.removeFromLeft(100).reduced(5));
    
    auto row3 = controlArea.removeFromTop(20);
    envelopeLabel.setBounds(row3.removeFromLeft(80).reduced(5));
    attackTimeLabel.setBounds(row3.removeFromLeft(120).reduced(5));
    
    // Performance
    performanceLabel.setBounds(row3.removeFromLeft(100).reduced(5));
    avgProcessingTimeLabel.setBounds(row3.removeFromLeft(120).reduced(5));
    peakProcessingTimeLabel.setBounds(row3.removeFromLeft(120).reduced(5));
    cpuUsageLabel.setBounds(row3.removeFromLeft(100).reduced(5));
    
    // Content
    if (currentContentComp)
    {
        currentContentComp->setBounds(area);
    }
}

void MainComponent::loadPluginClicked()
{
	// 対応するプラグイン形式のファイルパターンを構築
    juce::String filePatterns;
    
   #if JUCE_PLUGINHOST_VST3
    filePatterns += "*.vst3;";
   #endif
    
   #if JUCE_MAC
    #if JUCE_PLUGINHOST_AU
        filePatterns += "*.component;*.appex;";
    #endif
   #endif
    
   #if JUCE_LINUX
    #if JUCE_PLUGINHOST_LADSPA
        filePatterns += "*.so;";
    #endif
    
    #if JUCE_PLUGINHOST_LV2
        filePatterns += "*.lv2;";
    #endif
   #endif
    
    // セミコロンで終わっていたら削除
    if (filePatterns.endsWithChar(';'))
        filePatterns = filePatterns.dropLastCharacters(1);
    
    fileChooser = std::make_unique<juce::FileChooser>("Select a Plugin",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        filePatterns.isEmpty() ? "*.*" : filePatterns);

    auto folderChooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.exists())
        {
            if (engine.loadPlugin(file))
            {
                pluginNameLabel.setText(engine.getPluginName(), juce::dontSendNotification);
            }
        }
    });
}

/**
 * @brief タイマコールバック
 */
void MainComponent::timerCallback()
{
	// THD/IMD表示の更新
    float thd = engine.getTHD();
    float thdPlusN = engine.getTHDPlusN();
    
    juce::String thdText = juce::String(thd, 3) + "% (THD+N: " + juce::String(thdPlusN, 3) + "%)";
    thdValueLabel.setText(thdText, juce::dontSendNotification);
    
	// Dynamics表示の更新
    auto& dynamicsData = engine.getDynamicsData();
    if (dynamicsData.compressionRatio > 0.0f)
    {
        juce::String ratioText = "Ratio: " + juce::String(dynamicsData.compressionRatio, 2) + ":1";
        compressionRatioLabel.setText(ratioText, juce::dontSendNotification);
    }
    
    // Envelope表示の更新
    auto& envelopeData = engine.getEnvelopeData();
    if (envelopeData.attackTime > 0.0f)
    {
        juce::String attackText = "Attack: " + juce::String(envelopeData.attackTime * 1000.0f, 1) + "ms";
        attackTimeLabel.setText(attackText, juce::dontSendNotification);
    }
    
	// Performance表示の更新
    auto& perfData = engine.getPerformanceData();
    
	// 平均処理時間
    avgProcessingTimeLabel.setText("Avg: " + juce::String(perfData.averageProcessingTime, 3) + " ms", 
                                   juce::dontSendNotification);
    
	// ピーク処理時間
    peakProcessingTimeLabel.setText("Peak: " + juce::String(perfData.peakProcessingTime, 3) + " ms", 
                                    juce::dontSendNotification);
    
	// CPU使用率
    cpuUsageLabel.setText("CPU: " + juce::String(perfData.cpuUsagePercent, 1) + "%", 
                          juce::dontSendNotification);
    
	// CPU使用率に応じた色変更
    if (perfData.cpuUsagePercent < 50.0f)
        cpuUsageLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    else if (perfData.cpuUsagePercent < 80.0f)
        cpuUsageLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
    else
        cpuUsageLabel.setColour(juce::Label::textColourId, juce::Colours::red);
}

/**
 * @brief チェンジリスナコールバック
 * @param source チェンジブロードキャスタ
 */
void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &tabs)
    {
        currentTabChanged(tabs.getCurrentTabIndex(), tabs.getCurrentTabName());
    }
}

/**
 * @brief 現在のタブが変更されたときの処理
 * @param newCurrentTabIndex 新しいタブのインデックス
 * @param newCurrentTabName 新しいタブの名前
 */
void MainComponent::currentTabChanged(int newCurrentTabIndex, const juce::String& /*newCurrentTabName*/)
{
    juce::Component* newContent = nullptr;
    
    // デフォ
    newContent = graphComponent.get(); 
    
    switch (newCurrentTabIndex)
    {
        case 0: // LinearAnalysis
            engine.setAnalysisMode(AnalyzerEngine::AnalysisMode::Linear);
            break;
        case 1: // HarmonicAnalysis
            engine.setAnalysisMode(AnalyzerEngine::AnalysisMode::Harmonic);
            break;
        case 2: // THD Sweep
            engine.setAnalysisMode(AnalyzerEngine::AnalysisMode::THDSweep);
            break;
        case 3: // IMD
            engine.setAnalysisMode(AnalyzerEngine::AnalysisMode::IMD);
            break;
        case 4: // Hammerstein
            engine.setAnalysisMode(AnalyzerEngine::AnalysisMode::Hammerstein);
            break;
        case 5: // WhiteNoise
            engine.setAnalysisMode(AnalyzerEngine::AnalysisMode::WhiteNoise);
            break;
        case 6: // SineSweep
            engine.setAnalysisMode(AnalyzerEngine::AnalysisMode::SineSweep);
            break;
        case 7: // Oscilloscope
            newContent = scopeComponent.get();
            break;
        case 8: // Dynamics
            engine.setAnalysisMode(AnalyzerEngine::AnalysisMode::Dynamics);
            break;
        case 9: // Performance
            engine.setAnalysisMode(AnalyzerEngine::AnalysisMode::Performance);
            break;
    }

    if (currentContentComp != newContent)
    {
        currentContentComp->setVisible(false);
        currentContentComp = newContent;
        addAndMakeVisible(currentContentComp);
        resized();
    }
}

/**
 * @brief 設定ダイアログを表示
 */
void MainComponent::showSettingsDialog()
{
    auto* settingsComp = new SettingsComponent(currentSettings);
    settingsComp->onSettingsChanged = [this](const SettingsComponent::Settings& newSettings) {
        applySettings(newSettings);
    };
    
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(settingsComp);
    options.dialogTitle = "Analyzer Settings";
    options.dialogBackgroundColour = juce::Colour(0xff1a1a1a);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;
    
    auto* dialog = options.launchAsync();
    if (dialog != nullptr)
        dialog->centreWithSize(500, 450);
}

/**
 * @brief 新しい設定を適用
 * @param newSettings 新しい設定
 */
void MainComponent::applySettings(const SettingsComponent::Settings& newSettings)
{
	// オーディオをシャットダウン
    shutdownAudio();
    
	// 新しい設定を保存
    currentSettings = newSettings;
    
	// エンジンに新しい設定を適用
    engine.setFFTOrder(newSettings.fftOrder);
    
	// オーディオチャネルを再設定
    setAudioChannels(newSettings.numInputChannels, newSettings.numOutputChannels, nullptr);
    
	// 新しいサンプルレートとバッファサイズでエンジンを準備
    engine.prepare(newSettings.sampleRate, newSettings.bufferSize);
    
    DBG("Settings applied: BufferSize=" << newSettings.bufferSize 
        << ", SampleRate=" << newSettings.sampleRate 
        << ", FFTOrder=" << newSettings.fftOrder);
}

/**
 * @brief プラグインブラウザを表示
 */
void MainComponent::showPluginBrowser()
{
    auto* browserComp = new PluginScannerComponent(currentSettings.pluginScanPaths);
    browserComp->onPluginSelected = [this](const juce::PluginDescription& desc) {
		// プラグインをロード
        juce::File pluginFile(desc.fileOrIdentifier);
        
        if (pluginFile.existsAsFile() && engine.loadPlugin(pluginFile))
        {
            pluginNameLabel.setText(engine.getPluginName(), juce::dontSendNotification);
            
			// ダイアログを閉じる
            if (auto* dialog = juce::TopLevelWindow::getActiveTopLevelWindow())
            {
                if (auto* dialogWindow = dynamic_cast<juce::DialogWindow*>(dialog))
                    dialogWindow->exitModalState(1);
            }
        }
    };
    
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(browserComp);
    options.dialogTitle = "Plugin Browser";
    options.dialogBackgroundColour = juce::Colour(0xff1a1a1a);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = true;
    
    auto* dialog = options.launchAsync();
    if (dialog != nullptr)
        dialog->centreWithSize(600, 500);
}
