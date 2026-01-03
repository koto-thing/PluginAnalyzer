#pragma once

#include <JuceHeader.h>

class SettingsComponent : public juce::Component,
                          public juce::ListBoxModel
{
public:
    struct Settings
    {
        int bufferSize = 512;
        double sampleRate = 48000.0;
        int fftOrder = 11;  // 2^11 = 2048
        
        // オーディオデバイス設定
        juce::String audioDeviceName;
        int numInputChannels = 2;
        int numOutputChannels = 2;
        
        // スキャンするプラグインフォルダのパス
        juce::StringArray pluginScanPaths;
    };
    
    SettingsComponent(Settings& settings)
        : currentSettings(settings)
    {
        // バッファサイズ
        addAndMakeVisible(bufferSizeLabel);
        bufferSizeLabel.setText("Buffer Size:", juce::dontSendNotification);
        bufferSizeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        
        addAndMakeVisible(bufferSizeCombo);
        bufferSizeCombo.addItem("64", 1);
        bufferSizeCombo.addItem("128", 2);
        bufferSizeCombo.addItem("256", 3);
        bufferSizeCombo.addItem("512", 4);
        bufferSizeCombo.addItem("1024", 5);
        bufferSizeCombo.addItem("2048", 6);
        bufferSizeCombo.addItem("4096", 7);
        
        // 現在の値をセット
        int bufferIndex = getBufferSizeIndex(settings.bufferSize);
        bufferSizeCombo.setSelectedId(bufferIndex, juce::dontSendNotification);
        
        // サンプルレート
        addAndMakeVisible(sampleRateLabel);
        sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
        sampleRateLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        
        addAndMakeVisible(sampleRateCombo);
        sampleRateCombo.addItem("44100 Hz", 1);
        sampleRateCombo.addItem("48000 Hz", 2);
        sampleRateCombo.addItem("88200 Hz", 3);
        sampleRateCombo.addItem("96000 Hz", 4);
        sampleRateCombo.addItem("192000 Hz", 5);
        
        // 現在の値を設定
        int rateIndex = getSampleRateIndex(settings.sampleRate);
        sampleRateCombo.setSelectedId(rateIndex, juce::dontSendNotification);
        
        // FFTのオーダー
        addAndMakeVisible(fftOrderLabel);
        fftOrderLabel.setText("FFT Size:", juce::dontSendNotification);
        fftOrderLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        
        addAndMakeVisible(fftOrderCombo);
        fftOrderCombo.addItem("512 (2^9)", 1);
        fftOrderCombo.addItem("1024 (2^10)", 2);
        fftOrderCombo.addItem("2048 (2^11)", 3);
        fftOrderCombo.addItem("4096 (2^12)", 4);
        fftOrderCombo.addItem("8192 (2^13)", 5);
        fftOrderCombo.addItem("16384 (2^14)", 6);
        
        // 現在の値を設定
        int fftIndex = getFftOrderIndex(settings.fftOrder);
        fftOrderCombo.setSelectedId(fftIndex, juce::dontSendNotification);
        
        // プラグインのパスを設定
        addAndMakeVisible(pluginPathsLabel);
        pluginPathsLabel.setText("Plugin Scan Paths:", juce::dontSendNotification);
        pluginPathsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        
        addAndMakeVisible(pathListBox);
        pathListBox.setModel(this);
        pathListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0d0d0d));
        pathListBox.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff404040));
        
        addAndMakeVisible(addPathButton);
        addPathButton.setButtonText("Add Path");
        addPathButton.onClick = [this] { addPluginPath(); };
        
        addAndMakeVisible(removePathButton);
        removePathButton.setButtonText("Remove");
        removePathButton.onClick = [this] { removeSelectedPath(); };
        
        // ボタン
        addAndMakeVisible(applyButton);
        applyButton.setButtonText("Apply");
        applyButton.onClick = [this] { applySettings(); };
        
        addAndMakeVisible(cancelButton);
        cancelButton.setButtonText("Cancel");
        cancelButton.onClick = [this] { 
            if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
                parent->exitModalState(0);
        };
        
        // Infoラベル
        addAndMakeVisible(infoLabel);
        infoLabel.setText("Note: Changing these settings will restart audio processing", juce::dontSendNotification);
        infoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff6b35));
        infoLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::italic)));
        
        setSize(500, 450);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a1a));
        
        g.setColour(juce::Colour(0xff00a0ff));
        g.drawRect(getLocalBounds(), 2);
        
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
        g.drawText("Analyzer Settings", 10, 10, getWidth() - 20, 30, juce::Justification::centred);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(20);
        bounds.removeFromTop(50);
        
        auto bufferRow = bounds.removeFromTop(40);
        bufferSizeLabel.setBounds(bufferRow.removeFromLeft(150));
        bufferSizeCombo.setBounds(bufferRow.reduced(5));
        
        bounds.removeFromTop(10);
        
        auto rateRow = bounds.removeFromTop(40);
        sampleRateLabel.setBounds(rateRow.removeFromLeft(150));
        sampleRateCombo.setBounds(rateRow.reduced(5));
        
        bounds.removeFromTop(10);
        
        auto fftRow = bounds.removeFromTop(40);
        fftOrderLabel.setBounds(fftRow.removeFromLeft(150));
        fftOrderCombo.setBounds(fftRow.reduced(5));
        
        bounds.removeFromTop(10);
        
        pluginPathsLabel.setBounds(bounds.removeFromTop(25));
        
        auto pathButtonRow = bounds.removeFromTop(30);
        addPathButton.setBounds(pathButtonRow.removeFromLeft(pathButtonRow.getWidth() / 2).reduced(2));
        removePathButton.setBounds(pathButtonRow.reduced(2));
        
        pathListBox.setBounds(bounds.removeFromTop(120));
        
        bounds.removeFromTop(10);
        
        infoLabel.setBounds(bounds.removeFromTop(30));
        
        bounds.removeFromTop(10);
        
        auto buttonRow = bounds.removeFromTop(40);
        cancelButton.setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 2).reduced(5));
        applyButton.setBounds(buttonRow.reduced(5));
    }
    
    std::function<void(const Settings&)> onSettingsChanged;
    
    int getNumRows() override
    {
        return currentSettings.pluginScanPaths.size();
    }
    
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll(juce::Colour(0xff00a0ff).withAlpha(0.3f));
        
        if (rowNumber >= 0 && rowNumber < currentSettings.pluginScanPaths.size())
        {
            g.setColour(juce::Colours::white);
            g.setFont(height * 0.6f);
            g.drawText(currentSettings.pluginScanPaths[rowNumber], 5, 0, width - 10, height, 
                      juce::Justification::centredLeft, true);
        }
    }
    
private:
    Settings& currentSettings;
    
    juce::Label bufferSizeLabel;
    juce::ComboBox bufferSizeCombo;
    
    juce::Label sampleRateLabel;
    juce::ComboBox sampleRateCombo;
    
    juce::Label fftOrderLabel;
    juce::ComboBox fftOrderCombo;
    
    juce::Label pluginPathsLabel;
    juce::ListBox pathListBox;
    juce::TextButton addPathButton;
    juce::TextButton removePathButton;
    
    juce::Label infoLabel;
    
    juce::TextButton applyButton;
    juce::TextButton cancelButton;
    
    int getBufferSizeIndex(int bufferSize)
    {
        switch (bufferSize)
        {
            case 64: return 1;
            case 128: return 2;
            case 256: return 3;
            case 512: return 4;
            case 1024: return 5;
            case 2048: return 6;
            case 4096: return 7;
            default: return 4;
        }
    }
    
    int getBufferSizeFromIndex(int index)
    {
        switch (index)
        {
            case 1: return 64;
            case 2: return 128;
            case 3: return 256;
            case 4: return 512;
            case 5: return 1024;
            case 6: return 2048;
            case 7: return 4096;
            default: return 512;
        }
    }
    
    int getSampleRateIndex(double sampleRate)
    {
        if (sampleRate == 44100.0) return 1;
        if (sampleRate == 48000.0) return 2;
        if (sampleRate == 88200.0) return 3;
        if (sampleRate == 96000.0) return 4;
        if (sampleRate == 192000.0) return 5;
        return 2;
    }
    
    double getSampleRateFromIndex(int index)
    {
        switch (index)
        {
            case 1: return 44100.0;
            case 2: return 48000.0;
            case 3: return 88200.0;
            case 4: return 96000.0;
            case 5: return 192000.0;
            default: return 48000.0;
        }
    }
    
    int getFftOrderIndex(int fftOrder)
    {
        switch (fftOrder)
        {
            case 9: return 1;
            case 10: return 2;
            case 11: return 3;
            case 12: return 4;
            case 13: return 5;
            case 14: return 6;
            default: return 3;
        }
    }
    
    int getFftOrderFromIndex(int index)
    {
        switch (index)
        {
            case 1: return 9;
            case 2: return 10;
            case 3: return 11;
            case 4: return 12;
            case 5: return 13;
            case 6: return 14;
            default: return 11;
        }
    }
    
    void applySettings()
    {
        Settings newSettings;
        newSettings.bufferSize = getBufferSizeFromIndex(bufferSizeCombo.getSelectedId());
        newSettings.sampleRate = getSampleRateFromIndex(sampleRateCombo.getSelectedId());
        newSettings.fftOrder = getFftOrderFromIndex(fftOrderCombo.getSelectedId());
        newSettings.pluginScanPaths = currentSettings.pluginScanPaths;
        
        currentSettings = newSettings;
        
        if (onSettingsChanged)
            onSettingsChanged(newSettings);
        
        if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
            parent->exitModalState(1);
    }
    
    void addPluginPath()
    {
        fileChooser = std::make_unique<juce::FileChooser>("Select Plugin Folder",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory));
        
        auto browserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
        
        fileChooser->launchAsync(browserFlags, [this](const juce::FileChooser& fc) {
            auto folder = fc.getResult();
            if (folder.exists() && folder.isDirectory())
            {
                juce::String path = folder.getFullPathName();
                if (!currentSettings.pluginScanPaths.contains(path))
                {
                    currentSettings.pluginScanPaths.add(path);
                    pathListBox.updateContent();
                }
            }
        });
    }
    
    void removeSelectedPath()
    {
        int selectedRow = pathListBox.getSelectedRow();
        if (selectedRow >= 0 && selectedRow < currentSettings.pluginScanPaths.size())
        {
            currentSettings.pluginScanPaths.remove(selectedRow);
            pathListBox.updateContent();
        }
    }
    
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};