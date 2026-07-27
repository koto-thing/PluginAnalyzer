#pragma once

#include "../Domain/AnalysisModel.h"

#include <memory>
#include <string>

namespace plugin_analyzer::application
{
/**
 * @brief プレゼンテーション層から解析機能を利用するための境界
 *
 * JUCEの型を公開せず、アプリケーション層とUIフレームワークを分離する。
 */
class AnalysisService
{
public:
    virtual ~AnalysisService() = default;

    /**
     * @brief 実行する解析モードを設定
     * @param mode 新しい解析モード
     */
    virtual void setAnalysisMode(domain::AnalysisMode mode) = 0;

    /**
     * @brief 現在の解析モードを取得
     * @return 現在の解析モード
     */
    [[nodiscard]] virtual domain::AnalysisMode getAnalysisMode() const = 0;

    /**
     * @brief テスト信号の振幅を設定
     * @param amplitude 振幅
     */
    virtual void setInputAmplitude(float amplitude) = 0;

    /**
     * @brief テスト信号の周波数を設定
     * @param frequency 周波数（Hz）
     */
    virtual void setTestFrequency(double frequency) = 0;

    /**
     * @brief 最新の解析結果を取得
     * @return 読み取り専用の解析結果
     */
    [[nodiscard]] virtual std::shared_ptr<const domain::AnalysisSnapshot>
        getAnalysisSnapshot() const = 0;

    /**
     * @brief 表示用のプラグイン名を取得
     * @return ロード中のプラグイン名
     */
    [[nodiscard]] virtual std::string getPluginDisplayName() const = 0;

    /**
     * @brief オシロスコープ用FIFOからサンプルを読み出す
     * @param destination 読み出し先
     * @param sampleCount 最大読み出しサンプル数
     * @return 実際に読み出したサンプル数
     */
    virtual int readFromScopeFifo(float* destination, int sampleCount) = 0;
};
}
