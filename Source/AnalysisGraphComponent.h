#pragma once

#include <JuceHeader.h>
#include "Application/AnalysisService.h"

class AnalysisGraphComponent : public juce::Component
{
public:
    /**
     * @brief グラフコンポーネントを作成
     * @param service 解析結果の取得に使用するサービス
     */
    explicit AnalysisGraphComponent(plugin_analyzer::application::AnalysisService& service);

    void paint(juce::Graphics& g) override;
    void resized() override;

    /**
     * @brief 位相グラフの表示状態を設定
     * @param shouldShowPhase trueの場合は位相、falseの場合は振幅を表示
     */
    void setShowPhase(bool shouldShowPhase) { showPhase = shouldShowPhase; repaint(); }

private:
    plugin_analyzer::application::AnalysisService& analysisService;
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
    void drawCurve(juce::Graphics& g, const std::vector<float>& data, juce::Colour colour,
                   float minVal, float maxVal, double sampleRate);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalysisGraphComponent)
};
