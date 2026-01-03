/**
 * @author Goto Kenta
 * @brief グラフ描画コンポーネント実装
 */

#include "AnalysisGraphComponent.h"

/**
 * @brief コンストラクタ
 * @param e AnalyzerEngine参照
 */
AnalysisGraphComponent::AnalysisGraphComponent(AnalyzerEngine& e) : analyzer(e) {
    analyzer.addChangeListener(this);
}

/**
 * @brief デストラクタ
 */
AnalysisGraphComponent::~AnalysisGraphComponent() {
    analyzer.removeChangeListener(this);
}

/**
 * @brief リスナーコールバック
 */
void AnalysisGraphComponent::changeListenerCallback(juce::ChangeBroadcaster* /*source*/) {
    repaint();
}

/**
 * @brief リサイズハンドラ
 */
void AnalysisGraphComponent::resized() {
    repaint();
}

/**
 * @brief グラフ描画
 * @param g グラフィックスコンテキスト
 */
void AnalysisGraphComponent::paint(juce::Graphics& g) {
	// 背景
    auto bounds = getLocalBounds().toFloat();
    auto bgGradient = juce::ColourGradient::vertical(
        juce::Colour(0xff0d0d0d), bounds.getY(),
        juce::Colour(0xff1a1a1a), bounds.getBottom()
    );
    g.setGradientFill(bgGradient);
    g.fillRect(bounds);

    // グリッドを描画
    drawGrid(g);
    
	// 位相/振幅スペクトルを描画
    if (showPhase) {
        const auto& spectrumR = analyzer.getPhaseSpectrumR();
        if (!spectrumR.empty())
            drawCurve(g, spectrumR, juce::Colour(0xffff6b35), -juce::MathConstants<float>::pi, juce::MathConstants<float>::pi);

        const auto& spectrumL = analyzer.getPhaseSpectrumL();
        if (!spectrumL.empty())
            drawCurve(g, spectrumL, curveColour, -juce::MathConstants<float>::pi, juce::MathConstants<float>::pi);
    }
    else {
        const auto& spectrumR = analyzer.getMagnitudeSpectrumR();
        if (!spectrumR.empty())
            drawCurve(g, spectrumR, juce::Colour(0xffff6b35), -100.0f, 20.0f);

        const auto& spectrumL = analyzer.getMagnitudeSpectrumL();
        if (!spectrumL.empty())
            drawCurve(g, spectrumL, curveColour, -100.0f, 20.0f);
    }
    
	// プラグイン未ロード時のメッセージ表示
    if (analyzer.getPluginName() == "No Plugin Loaded") {
        g.setColour(juce::Colour(0xff808080));
        g.setFont(juce::Font(juce::FontOptions("Arial", 24.0f, juce::Font::bold)));
        g.drawText("Load a Plugin to Analyze", getLocalBounds(), juce::Justification::centred, true);
    }
    
	// Border
    g.setColour(juce::Colour(0xff404040));
    g.drawRect(getLocalBounds(), 2);
    
	// アクセント
    g.setColour(juce::Colour(0xff00a0ff).withAlpha(0.3f));
    g.fillRect(0, 0, 4, 4);
    g.fillRect(getWidth() - 4, 0, 4, 4);
}

/**
 * @brief グリッド描画
 * @param g グラフィックスコンテキスト
 */
void AnalysisGraphComponent::drawGrid(juce::Graphics& g) {
    auto w = (float)getWidth();
    auto h = (float)getHeight();

	// 横軸（周波数、対数スケール）
    std::vector<float> freqs = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (auto f : freqs) {
        float x = getXForFrequency(f, w);
        if (x >= 0 && x <= w) {
            // グリッド
            bool isMajor = (f == 1000.0f || f == 10000.0f);
            g.setColour(isMajor ? juce::Colour(0xff3d3d3d) : gridColour);
            g.drawVerticalLine((int)x, 0.0f, h);
            
            // ラベル
            g.setColour(juce::Colour(0xff808080));
            g.setFont(juce::Font(juce::FontOptions("Arial", 11.0f, juce::Font::bold)));
            juce::String label = (f >= 1000) ? juce::String(f / 1000.0f, 1) + "k" : juce::String((int)f);
            g.drawText(label, (int)x - 15, (int)h - 20, 30, 15, juce::Justification::centred);
        }
    }

	// 縦軸（位相/振幅）
    if (showPhase) {
        std::vector<int> degrees = { 180, 90, 0, -90, -180 };
        for (auto deg : degrees)
        {
            float rad = juce::degreesToRadians((float)deg);
            float y = juce::jmap(rad, -juce::MathConstants<float>::pi, juce::MathConstants<float>::pi, h, 0.0f);
            
            bool isZero = (deg == 0);
            g.setColour(isZero ? juce::Colour(0xff00a0ff).withAlpha(0.3f) : gridColour);
            g.drawHorizontalLine((int)y, 0.0f, w);
            
            g.setColour(juce::Colour(0xff808080));
            g.setFont(juce::Font(juce::FontOptions("Arial", 11.0f, juce::Font::bold)));
            g.drawText(juce::String(deg) + juce::String::fromUTF8(u8"\u00B0"), 5, (int)y - 8, 40, 15, juce::Justification::left);
        }
    }
    else {
        for (float db = 20.0f; db >= -100.0f; db -= 10.0f) {
            float y = juce::jmap(db, -100.0f, 20.0f, h, 0.0f);
            
            bool isZero = (std::abs(db) < 0.1f);
            bool isMajor = ((int)db % 20 == 0);
            
            if (isZero)
                g.setColour(juce::Colour(0xff00a0ff).withAlpha(0.3f));
            else if (isMajor)
                g.setColour(juce::Colour(0xff3d3d3d));
            else
                g.setColour(gridColour);
                
            g.drawHorizontalLine((int)y, 0.0f, w);
            
            if (isMajor) {
                 g.setColour(juce::Colour(0xff808080));
                 g.setFont(juce::Font(juce::FontOptions("Arial", 11.0f, juce::Font::bold)));
                 g.drawText(juce::String((int)db) + "dB", 5, (int)y - 8, 40, 15, juce::Justification::left);
            }
        }
    }
}

/**
 * @brief スペクトル曲線描画
 * @param g グラフィックスコンテキスト
 * @param data スペクトルデータ配列
 * @param colour 曲線色
 * @param minVal Y軸最小値
 * @param maxVal Y軸最大値
 */
void AnalysisGraphComponent::drawCurve(juce::Graphics& g, const std::vector<float>& data, juce::Colour colour, float minVal, float maxVal) {
    if (data.empty())
        return;

    // 平滑化
    std::vector<float> smoothed = data;
    int smoothSize = 8;
    
    if (!showPhase) {
        for (size_t i = 0; i < smoothed.size(); ++i) {
            float sum = 0.0f;
            int count = 0;
            for (int k = -smoothSize; k <= smoothSize; ++k) {
                int idx = (int)i + k;
                if (idx >= 0 && idx < (int)data.size()) {
                    sum += data[idx];
                    count++;
                }
            }
            if (count > 0)
                smoothed[i] = sum / count;
        }
    }

    juce::Path p;
    auto w = (float)getWidth();
    auto h = (float)getHeight();
    float nyquist = 22050.0f;
    int numBins = (int)smoothed.size();
    bool started = false;

    for (int i = 1; i < numBins; ++i) {
        float freq = (float)i / (float)numBins * nyquist;
        float x = getXForFrequency(freq, w);
        float val = smoothed[i];
        float y = juce::jmap(val, minVal, maxVal, h, 0.0f);

        if (y > h) 
            y = h;

        if (y < 0)
            y = 0;

        if (!started) {
            p.startNewSubPath(x, y);
            started = true;
        }
        else {
            if (showPhase && i > 0 && std::abs(val - smoothed[i-1]) > juce::MathConstants<float>::pi) {
                 p.startNewSubPath(x, y);
            }
            else {
                p.lineTo(x, y);
            }
        }
    }

    // グロー
    g.setColour(colour.withAlpha(0.15f));
    g.strokePath(p, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    
    // 中間グロー
    g.setColour(colour.withAlpha(0.3f));
    g.strokePath(p, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    
    // メインの線
    g.setColour(colour);
    g.strokePath(p, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    
    // ハイライト
    g.setColour(colour.brighter(0.5f).withAlpha(0.8f));
    g.strokePath(p, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

/**
 * @brief 周波数からX座標を取得（対数スケール）
 * @param freq 周波数
 * @param width コンポーネント幅
 * @return X座標
 */
float AnalysisGraphComponent::getXForFrequency(float freq, float width) const {
	// 対数スケール変換
    float minF = 20.0f;
    float maxF = 20000.0f;
    
    if (freq < minF) 
        return -1.0f;
    if (freq > maxF) 
        return width + 1.0f;
    
    float norm = (std::log10(freq) - std::log10(minF)) / (std::log10(maxF) - std::log10(minF));
    return norm * width;
}
