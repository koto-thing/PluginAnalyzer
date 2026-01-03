#pragma once

#include <JuceHeader.h>
#include "AnalyzerEngine.h"

class AnalysisGraphComponent : public juce::Component, public juce::ChangeListener
{
public:
    AnalysisGraphComponent(AnalyzerEngine& engine);
    ~AnalysisGraphComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void setShowPhase(bool shouldShowPhase) { showPhase = shouldShowPhase; repaint(); }

private:
    AnalyzerEngine& analyzer;
    bool showPhase = false;

    // ヘルパ
    float getXForFrequency(float freq, float width) const;
    
    // 色
    const juce::Colour backgroundColour = juce::Colour(0xff0d0d0d);
    const juce::Colour gridColour = juce::Colour(0xff2d2d2d);
    const juce::Colour curveColour = juce::Colour(0xff00a0ff);
    const juce::Colour curveGlowColour = juce::Colour(0x4400a0ff);

    void drawGrid(juce::Graphics& g);
    void drawResponse(juce::Graphics& g);
    void drawCurve(juce::Graphics& g, const std::vector<float>& data, juce::Colour colour, float minVal, float maxVal);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalysisGraphComponent)
};
