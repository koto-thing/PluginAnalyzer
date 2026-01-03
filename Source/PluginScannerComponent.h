#pragma once

#include <JuceHeader.h>

class PluginScannerComponent : public juce::Component,
                               public juce::ListBoxModel
{
public:
    PluginScannerComponent(const juce::StringArray& scanPaths)
        : pathsToScan(scanPaths)
    {
        addAndMakeVisible(pluginList);
        pluginList.setModel(this);
        pluginList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0d0d0d));
        
        addAndMakeVisible(scanButton);
        scanButton.setButtonText("Scan Plugins");
        scanButton.onClick = [this] { startScan(); };
        
        #if JUCE_PLUGINHOST_VST3
            formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
        #endif
        #if JUCE_PLUGINHOST_AU && JUCE_MAC
            formatManager.addFormat(std::make_unique<juce::AudioUnitPluginFormat>());
        #endif
        #if JUCE_PLUGINHOST_VST
            formatManager.addFormat(std::make_unique<juce::VSTPluginFormat>());
        #endif
        
        setSize(600, 400);
    }
    
    ~PluginScannerComponent() override
    {
        pluginList.setModel(nullptr);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a1a));
    }
    
    void resized() override
    {
        auto area = getLocalBounds();
        scanButton.setBounds(area.removeFromTop(40).reduced(5));
        pluginList.setBounds(area.reduced(5));
    }
    
    std::function<void(const juce::PluginDescription&)> onPluginSelected;
    
    int getNumRows() override { return knownPluginList.getNumTypes(); }
    
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll(juce::Colour(0xff00a0ff).withAlpha(0.3f));
            
        g.setColour(juce::Colours::white);
        if (auto* type = knownPluginList.getType(rowNumber))
        {
             g.drawText(type->name + " (" + type->pluginFormatName + ")", 5, 0, width, height, juce::Justification::centredLeft);
        }
    }
    
    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        if (onPluginSelected)
        {
            if (auto* type = knownPluginList.getType(row))
                onPluginSelected(*type);
        }
    }
    
private:
    juce::StringArray pathsToScan;
    juce::ListBox pluginList;
    juce::TextButton scanButton;
    
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    
    void startScan()
    {
        knownPluginList.clear();
        
        for (auto& path : pathsToScan)
        {
            juce::File dir(path);
            if (dir.isDirectory())
            {
                // 再帰的に
                juce::DirectoryIterator iter(dir, true, "*", juce::File::findFiles);
                while (iter.next())
                {
                    auto file = iter.getFile();
                    for (auto format : formatManager.getFormats())
                    {
                        juce::OwnedArray<juce::PluginDescription> found;
                        format->findAllTypesForFile(found, file.getFullPathName());
                        for (auto* desc : found)
                        {
                            knownPluginList.addType(*desc);
                        }
                    }
                }
            }
        }
        
        pluginList.updateContent();
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginScannerComponent)
};