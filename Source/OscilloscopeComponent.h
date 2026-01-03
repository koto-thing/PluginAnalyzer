#pragma once

#include <JuceHeader.h>
#include "AnalyzerEngine.h"

class OscilloscopeComponent : public juce::Component,
                              public juce::Timer
{
public:
    OscilloscopeComponent(AnalyzerEngine& engine) : analyzer(engine)
    {
        startTimerHz(60);
        plotBuffer.resize(2048, 0.0f);
    }

    ~OscilloscopeComponent() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        // 背景
        g.fillAll(juce::Colour(0xff202020));

        // グリッド
        g.setColour(juce::Colour(0xff555555));
        g.drawRect(getLocalBounds(), 1);
        g.drawLine(0.0f, getHeight() / 2.0f, (float)getWidth(), getHeight() / 2.0f);

        juce::Path p;
        auto w = getWidth();
        auto h = getHeight();
        auto halfH = h / 2.0f;
        
        bool started = false;
        
        float xRatio = (float)w / (float)plotBuffer.size();
        
        for (int i = 0; i < (int)plotBuffer.size(); ++i)
        {
            float x = i * xRatio;
            float sample = plotBuffer[i];
            
            // スケーリング
            float y = halfH - (sample * halfH * 0.9f);
            
            if (!started)
            {
                p.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                p.lineTo(x, y);
            }
        }
        
        g.setColour(juce::Colour(0xff00ffcc));
        g.strokePath(p, juce::PathStrokeType(2.0f));
    }

    void timerCallback() override
    {

        int numToRead = analyzer.readFromScopeFifo(tempBuffer.data(), 1024);
        
        if (numToRead > 0)
        {
            int samplesToKeep = (int)plotBuffer.size() - numToRead;
            if (samplesToKeep > 0)
            {
                std::vector<float> newBuffer(plotBuffer.size());
                std::copy(plotBuffer.begin() + numToRead, plotBuffer.end(), newBuffer.begin());
                std::copy(tempBuffer.begin(), tempBuffer.begin() + numToRead, newBuffer.begin() + samplesToKeep);
                plotBuffer = newBuffer;
            }
            else
            {
                // 置き換え
                 std::copy(tempBuffer.begin(), tempBuffer.begin() + plotBuffer.size(), plotBuffer.begin());
            }
            
            repaint();
        }
    }

private:
    AnalyzerEngine& analyzer;
    std::vector<float> plotBuffer;
    std::array<float, 2048> tempBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilloscopeComponent)
};