#pragma once

#include "AnalysisService.h"

#include <optional>

namespace plugin_analyzer::application
{
/**
 * @brief 選択中のタブで表示するコンテンツ種別
 */
enum class ContentView
{
    AnalysisGraph,
    Oscilloscope
};

/**
 * @brief 解析モードごとのコントロール表示状態
 */
struct ModeControls
{
    bool amplitude = false;
    bool frequency = false;
    bool distortion = false;
    bool dynamics = false;
    bool performance = false;
    bool phase = false;
};

/**
 * @brief タブ選択後の表示内容
 */
struct TabSelection
{
    ContentView content = ContentView::AnalysisGraph;
    ModeControls controls;
};

/**
 * @brief タブ選択と解析ユースケースを仲介するセッション
 *
 * タブ番号から解析モードおよび表示コントロールへの変換をUIから分離する。
 */
class AnalysisSession
{
public:
    /**
     * @brief セッションを作成
     * @param serviceToUse 解析処理を委譲するサービス
     */
    explicit AnalysisSession(AnalysisService& serviceToUse)
        : service(serviceToUse)
    {
    }

    /**
     * @brief タブを選択して対応する解析モードを適用
     * @param tabIndex 選択されたタブのインデックス
     * @return 表示するコンテンツとコントロール状態
     */
    [[nodiscard]] TabSelection selectTab(int tabIndex)
    {
        auto selection = selectionForTab(tabIndex);
        if (const auto mode = modeForTab(tabIndex))
            service.setAnalysisMode(*mode);
        return selection;
    }

    /**
     * @brief タブに対応する表示状態を取得
     * @param tabIndex 対象タブのインデックス
     * @return 表示するコンテンツとコントロール状態
     */
    [[nodiscard]] static TabSelection selectionForTab(int tabIndex)
    {
        TabSelection result;
        result.content = tabIndex == 7 ? ContentView::Oscilloscope
                                       : ContentView::AnalysisGraph;
        result.controls.amplitude = tabIndex == 1 || tabIndex == 2
                                 || tabIndex == 3 || tabIndex == 4
                                 || tabIndex == 8;
        result.controls.frequency = tabIndex == 1;
        result.controls.distortion = tabIndex == 1 || tabIndex == 3;
        result.controls.dynamics = tabIndex == 8;
        result.controls.performance = tabIndex == 9;
        result.controls.phase = tabIndex == 0 || tabIndex == 5 || tabIndex == 6;
        return result;
    }

private:
    /**
     * @brief タブに対応する解析モードを取得
     * @param tabIndex 対象タブのインデックス
     * @return 対応する解析モード。解析モードを変更しないタブの場合はnullopt
     */
    [[nodiscard]] static std::optional<domain::AnalysisMode> modeForTab(int tabIndex)
    {
        using domain::AnalysisMode;
        switch (tabIndex)
        {
            case 0: return AnalysisMode::Linear;
            case 1: return AnalysisMode::Harmonic;
            case 2: return AnalysisMode::THDSweep;
            case 3: return AnalysisMode::IMD;
            case 4: return AnalysisMode::Hammerstein;
            case 5: return AnalysisMode::WhiteNoise;
            case 6: return AnalysisMode::SineSweep;
            case 8: return AnalysisMode::Dynamics;
            case 9: return AnalysisMode::Performance;
            default: return std::nullopt;
        }
    }

    AnalysisService& service;
};
}
