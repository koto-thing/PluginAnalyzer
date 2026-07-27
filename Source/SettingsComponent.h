#pragma once

#include <JuceHeader.h>

class SettingsComponent final : public juce::Component,
                                public juce::ListBoxModel
{
public:
    struct Settings
    {
        int bufferSize = 512;
        double sampleRate = 48000.0;
        int fftOrder = 11;
        juce::String audioDeviceName;
        int numInputChannels = 2;
        int numOutputChannels = 2;
        juce::StringArray pluginScanPaths;
    };

    SettingsComponent(const Settings& settings, juce::AudioDeviceManager& deviceManager)
        : editedSettings(settings),
          audioSelector(deviceManager, 0, 2, 1, 2, false, false, true, false)
    {
        addAndMakeVisible(audioSelector);

        addAndMakeVisible(fftOrderLabel);
        fftOrderLabel.setText("FFT Size", juce::dontSendNotification);
        fftOrderLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(fftOrderCombo);
        for (int order = 9; order <= 14; ++order)
            fftOrderCombo.addItem(juce::String(1 << order) + " (2^" + juce::String(order) + ")",
                                  order - 8);
        fftOrderCombo.setSelectedId(juce::jlimit(1, 6, settings.fftOrder - 8),
                                    juce::dontSendNotification);

        addAndMakeVisible(pluginPathsLabel);
        pluginPathsLabel.setText("Plugin Scan Paths", juce::dontSendNotification);
        pluginPathsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(pathListBox);
        pathListBox.setModel(this);
        pathListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0d0d0d));

        addAndMakeVisible(addPathButton);
        addPathButton.onClick = [this] { addPluginPath(); };
        addAndMakeVisible(removePathButton);
        removePathButton.onClick = [this] { removeSelectedPath(); };

        addAndMakeVisible(applyButton);
        applyButton.onClick = [this, &deviceManager]
        {
            editedSettings.fftOrder = fftOrderCombo.getSelectedId() + 8;
            const auto setup = deviceManager.getAudioDeviceSetup();
            editedSettings.sampleRate = setup.sampleRate;
            editedSettings.bufferSize = setup.bufferSize;
            editedSettings.audioDeviceName = setup.outputDeviceName;
            if (onSettingsChanged)
                onSettingsChanged(editedSettings);
            if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
                parent->exitModalState(1);
        };

        addAndMakeVisible(cancelButton);
        cancelButton.onClick = [this]
        {
            if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
                parent->exitModalState(0);
        };
        setSize(620, 690);
    }

    ~SettingsComponent() override
    {
        pathListBox.setModel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a1a));
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
        g.drawText("Analyzer Settings", 12, 8, getWidth() - 24, 30,
                   juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(16);
        area.removeFromTop(36);
        audioSelector.setBounds(area.removeFromTop(390));
        auto fftRow = area.removeFromTop(36);
        fftOrderLabel.setBounds(fftRow.removeFromLeft(100));
        fftOrderCombo.setBounds(fftRow.reduced(3));
        pluginPathsLabel.setBounds(area.removeFromTop(26));
        auto pathButtons = area.removeFromTop(32);
        addPathButton.setBounds(pathButtons.removeFromLeft(130).reduced(2));
        removePathButton.setBounds(pathButtons.removeFromLeft(130).reduced(2));
        pathListBox.setBounds(area.removeFromTop(105));
        auto buttons = area.removeFromBottom(38);
        cancelButton.setBounds(buttons.removeFromLeft(buttons.getWidth() / 2).reduced(4));
        applyButton.setBounds(buttons.reduced(4));
    }

    std::function<void(const Settings&)> onSettingsChanged;
    int getNumRows() override { return editedSettings.pluginScanPaths.size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height,
                          bool selected) override
    {
        if (selected)
            g.fillAll(juce::Colour(0xff00a0ff).withAlpha(0.3f));
        if (juce::isPositiveAndBelow(row, editedSettings.pluginScanPaths.size()))
        {
            g.setColour(juce::Colours::white);
            g.drawText(editedSettings.pluginScanPaths[row], 5, 0, width - 10, height,
                       juce::Justification::centredLeft, true);
        }
    }

private:
    void addPluginPath()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select Plugin Folder", juce::File::getSpecialLocation(juce::File::userHomeDirectory));
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode
                                     | juce::FileBrowserComponent::canSelectDirectories,
                                 [this](const juce::FileChooser& chooser)
                                 {
                                     const auto folder = chooser.getResult();
                                     if (folder.isDirectory()
                                         && !editedSettings.pluginScanPaths.contains(
                                             folder.getFullPathName()))
                                     {
                                         editedSettings.pluginScanPaths.add(
                                             folder.getFullPathName());
                                         pathListBox.updateContent();
                                     }
                                 });
    }

    void removeSelectedPath()
    {
        const auto row = pathListBox.getSelectedRow();
        if (juce::isPositiveAndBelow(row, editedSettings.pluginScanPaths.size()))
        {
            editedSettings.pluginScanPaths.remove(row);
            pathListBox.updateContent();
        }
    }

    Settings editedSettings;
    juce::AudioDeviceSelectorComponent audioSelector;
    juce::Label fftOrderLabel;
    juce::ComboBox fftOrderCombo;
    juce::Label pluginPathsLabel;
    juce::ListBox pathListBox;
    juce::TextButton addPathButton { "Add Path" };
    juce::TextButton removePathButton { "Remove" };
    juce::TextButton applyButton { "Apply" };
    juce::TextButton cancelButton { "Cancel" };
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};
