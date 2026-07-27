#include "MainComponent.h"
#include "OscilloscopeComponent.h"
#include "AnalysisGraphComponent.h"
#include "PluginScannerComponent.h"

/**
 * @brief メイン画面を初期化
 */
MainComponent::MainComponent()
{
	// ルックアンドフィール設定
    setLookAndFeel(&sslLookAndFeel);
    
	// ビューコンポーネント作成
    graphComponent = std::make_unique<AnalysisGraphComponent>(analysisService);
    scopeComponent = std::make_unique<OscilloscopeComponent>(analysisService);
    
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
    
    juce::PropertiesFile::Options propertyOptions;
    propertyOptions.applicationName = "PluginAnalyzer";
    propertyOptions.filenameSuffix = "settings";
    propertyOptions.folderName = "PluginAnalyzer";
    propertyOptions.osxLibrarySubFolder = "Application Support";
    properties = std::make_unique<juce::PropertiesFile>(propertyOptions);
    loadPersistentSettings();
    
    // THD
    addAndMakeVisible(amplitudeSlider);
    amplitudeSlider.setRange(0.0, 1.0, 0.01);
    amplitudeSlider.setValue(0.5);
    amplitudeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    amplitudeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    amplitudeSlider.onValueChange = [this] {
        analysisService.setInputAmplitude((float)amplitudeSlider.getValue());
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
        analysisService.setTestFrequency(frequencySlider.getValue());
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
    auto savedAudioState = properties->getXmlValue("audioDeviceState");
    setAudioChannels(currentSettings.numInputChannels,
                     currentSettings.numOutputChannels,
                     savedAudioState.get());
    engine.setFFTOrder(currentSettings.fftOrder);
    currentTabChanged(0, tabs.getCurrentTabName());
    
    startTimer(100);
}

/**
 * @brief UI、オーディオデバイス、永続設定を終了処理
 */
MainComponent::~MainComponent()
{
    tabs.removeChangeListener(this);
    stopTimer();
    savePersistentSettings();
    shutdownAudio();
    setLookAndFeel(nullptr);
}

/**
 * @brief オーディオ準備
 * @param samplesPerBlockExpected 期待されるサンプルブロックサイズ
 * @param sampleRate サンプルレート
 */
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    const auto numOutputChannels = juce::jmax(1, currentSettings.numOutputChannels);
    audioWorkBuffer.setSize(numOutputChannels, samplesPerBlockExpected, false, true, false);
    preparedAudioBlockSize = samplesPerBlockExpected;
    preparedOutputChannels = numOutputChannels;

    // Always prepare from values reported by the active device, rather than the
    // requested settings (which the driver may have rejected or adjusted).
    engine.prepare(sampleRate, samplesPerBlockExpected);
}

/**
 * @brief 次のオーディオブロックを取得
 * @param bufferToFill オーディオバッファ情報
 */
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    if (bufferToFill.buffer == nullptr
        || bufferToFill.numSamples > preparedAudioBlockSize
        || bufferToFill.buffer->getNumChannels() > preparedOutputChannels)
        return;

    // This only changes the logical view into storage allocated in prepareToPlay.
    audioWorkBuffer.setSize(bufferToFill.buffer->getNumChannels(),
                            bufferToFill.numSamples,
                            false, false, true);
    audioWorkBuffer.clear();
    engine.processAudio(audioWorkBuffer);

    for (int channel = 0; channel < audioWorkBuffer.getNumChannels(); ++channel)
    {
        bufferToFill.buffer->copyFrom(channel,
                                      bufferToFill.startSample,
                                      audioWorkBuffer,
                                      channel,
                                      0,
                                      bufferToFill.numSamples);
    }
}

/**
 * @brief オーディオ処理用リソースを解放
 */
void MainComponent::releaseResources()
{
    engine.releaseResources();
    audioWorkBuffer.clear();
    preparedAudioBlockSize = 0;
    preparedOutputChannels = 0;
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
        juce::Colour(0xff2d2d2d), static_cast<float>(headerBounds.getY()),
        juce::Colour(0xff1a1a1a), static_cast<float>(headerBounds.getBottom())
    );
    g.setGradientFill(headerGradient);
    g.fillRect(headerBounds);
    
    // ヘッダのセパレータ
    g.setColour(juce::Colour(0xff00a0ff).withAlpha(0.5f));
    g.fillRect(0, 40, getWidth(), 2);
    
	// セクションセパレータ
    g.setColour(juce::Colour(0xff404040).withAlpha(0.3f));
    g.drawLine(0.0f, 70.0f, static_cast<float>(getWidth()), 70.0f, 1.0f);
    g.drawLine(0.0f, 150.0f, static_cast<float>(getWidth()), 150.0f, 1.0f);
    
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
    peakProcessingTimeLabel.setBounds(row3.removeFromLeft(260).reduced(5));
    cpuUsageLabel.setBounds(row3.removeFromLeft(100).reduced(5));
    
    // Content
    if (currentContentComp)
    {
        currentContentComp->setBounds(area);
    }
}

/**
 * @brief ファイル選択ダイアログからプラグインをロード
 */
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

    // VST3 bundles are files on some platforms and directories on others.
    auto folderChooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::canSelectDirectories;

    fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.exists())
        {
            if (engine.loadPlugin(file))
            {
                pluginNameLabel.setText(engine.getPluginName(), juce::dontSendNotification);
            }
            else
            {
                showPluginLoadError();
            }
        }
    });
}

/**
 * @brief 直近のプラグイン読み込みエラーを表示
 */
void MainComponent::showPluginLoadError()
{
    auto message = engine.getLastPluginError();
    if (message.isEmpty())
        message = "The selected plug-in could not be loaded.";

    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                           "Plug-in Load Failed",
                                           message);
}

/**
 * @brief タイマコールバック
 */
void MainComponent::timerCallback()
{
    const auto snapshot = analysisService.getAnalysisSnapshot();
    if (graphComponent != nullptr)
        graphComponent->repaint();

	// THD/IMD表示の更新
    juce::String thdText;
    if (analysisService.getAnalysisMode() == plugin_analyzer::domain::AnalysisMode::IMD)
        thdText = "IMD: " + juce::String(snapshot->imd, 3) + "%";
    else
        thdText = juce::String(snapshot->thd, 3) + "% (THD+N: "
                + juce::String(snapshot->thdPlusN, 3) + "%)";
    thdValueLabel.setText(thdText, juce::dontSendNotification);
    
	// Dynamics表示の更新
    const auto& dynamicsData = snapshot->dynamics;
    if (dynamicsData.compressionRatio > 0.0f)
    {
        juce::String ratioText = "Ratio: " + juce::String(dynamicsData.compressionRatio, 2) + ":1";
        compressionRatioLabel.setText(ratioText, juce::dontSendNotification);
    }
    
    // Envelope表示の更新
    const auto& envelopeData = snapshot->envelope;
    if (envelopeData.attackTime > 0.0f)
    {
        juce::String attackText = "Attack: " + juce::String(envelopeData.attackTime * 1000.0f, 1) + "ms";
        attackTimeLabel.setText(attackText, juce::dontSendNotification);
    }
    
	// Performance表示の更新
    const auto& perfData = snapshot->performance;
    
	// 平均処理時間
    avgProcessingTimeLabel.setText("Avg: " + juce::String(perfData.averageProcessingTime, 3) + " ms", 
                                   juce::dontSendNotification);
    
	// ピーク処理時間
    peakProcessingTimeLabel.setText(
        "Peak: " + juce::String(perfData.peakProcessingTime, 3)
        + " ms  p95/p99: " + juce::String(perfData.p95ProcessingTime, 3)
        + "/" + juce::String(perfData.p99ProcessingTime, 3) + " ms",
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
    const auto selection = analysisSession.selectTab(newCurrentTabIndex);
    auto* newContent = selection.content
                         == plugin_analyzer::application::ContentView::Oscilloscope
                     ? static_cast<juce::Component*>(scopeComponent.get())
                     : static_cast<juce::Component*>(graphComponent.get());

    if (currentContentComp != newContent)
    {
        currentContentComp->setVisible(false);
        currentContentComp = newContent;
        addAndMakeVisible(currentContentComp);
        resized();
    }
    updateModeControls(selection.controls);
}

/**
 * @brief 設定ダイアログを表示
 */
void MainComponent::showSettingsDialog()
{
    auto* settingsComp = new SettingsComponent(currentSettings, deviceManager);
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
        dialog->centreWithSize(620, 690);
}

/**
 * @brief 新しい設定を適用
 * @param newSettings 新しい設定
 */
void MainComponent::applySettings(const SettingsComponent::Settings& newSettings)
{
    currentSettings = newSettings;
    engine.setFFTOrder(newSettings.fftOrder);
    savePersistentSettings();
}

/**
 * @brief プラグインブラウザを表示
 */
void MainComponent::showPluginBrowser()
{
    auto* browserComp = new PluginScannerComponent(currentSettings.pluginScanPaths, *properties);
    browserComp->onPluginSelected = [this](const juce::PluginDescription& desc) {
        if (engine.loadPlugin(desc))
        {
            pluginNameLabel.setText(engine.getPluginName(), juce::dontSendNotification);
            
			// ダイアログを閉じる
            if (auto* dialog = juce::TopLevelWindow::getActiveTopLevelWindow())
            {
                if (auto* dialogWindow = dynamic_cast<juce::DialogWindow*>(dialog))
                    dialogWindow->exitModalState(1);
            }
        }
        else
        {
            showPluginLoadError();
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

/**
 * @brief 永続ストレージからアプリケーション設定を読み込む
 */
void MainComponent::loadPersistentSettings()
{
    currentSettings.bufferSize = properties->getIntValue("bufferSize", 512);
    currentSettings.sampleRate = properties->getDoubleValue("sampleRate", 48000.0);
    currentSettings.fftOrder = properties->getIntValue("fftOrder", 11);
    currentSettings.numInputChannels = properties->getIntValue("inputChannels", 2);
    currentSettings.numOutputChannels = properties->getIntValue("outputChannels", 2);
    currentSettings.audioDeviceName = properties->getValue("audioDeviceName");
    currentSettings.pluginScanPaths.addTokens(properties->getValue("pluginScanPaths"),
                                              "\n", {});
    currentSettings.pluginScanPaths.removeEmptyStrings();
}

/**
 * @brief 現在のアプリケーション設定を永続ストレージへ保存
 */
void MainComponent::savePersistentSettings()
{
    if (properties == nullptr)
        return;

    if (auto state = deviceManager.createStateXml())
        properties->setValue("audioDeviceState", state.get());
    const auto setup = deviceManager.getAudioDeviceSetup();
    currentSettings.sampleRate = setup.sampleRate;
    currentSettings.bufferSize = setup.bufferSize;
    currentSettings.audioDeviceName = setup.outputDeviceName;
    properties->setValue("sampleRate", currentSettings.sampleRate);
    properties->setValue("bufferSize", currentSettings.bufferSize);
    properties->setValue("audioDeviceName", currentSettings.audioDeviceName);
    properties->setValue("fftOrder", currentSettings.fftOrder);
    properties->setValue("inputChannels", currentSettings.numInputChannels);
    properties->setValue("outputChannels", currentSettings.numOutputChannels);
    properties->setValue("pluginScanPaths", currentSettings.pluginScanPaths.joinIntoString("\n"));
    properties->saveIfNeeded();
}

/**
 * @brief 解析モードに応じて操作コントロールの表示状態を更新
 * @param controls 各コントロールの表示設定
 */
void MainComponent::updateModeControls(
    const plugin_analyzer::application::ModeControls& controls)
{
    amplitudeSlider.setVisible(controls.amplitude);
    amplitudeLabel.setVisible(controls.amplitude);
    frequencySlider.setVisible(controls.frequency);
    frequencyLabel.setVisible(controls.frequency);
    thdLabel.setVisible(controls.distortion);
    thdValueLabel.setVisible(controls.distortion);
    dynamicsLabel.setVisible(controls.dynamics);
    compressionRatioLabel.setVisible(controls.dynamics);
    envelopeLabel.setVisible(controls.dynamics);
    attackTimeLabel.setVisible(controls.dynamics);
    performanceLabel.setVisible(controls.performance);
    avgProcessingTimeLabel.setVisible(controls.performance);
    peakProcessingTimeLabel.setVisible(controls.performance);
    cpuUsageLabel.setVisible(controls.performance);
    showPhaseButton.setVisible(controls.phase);
}
