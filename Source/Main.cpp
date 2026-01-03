#include <JuceHeader.h>
#include "MainComponent.h"

class PluginAnalyzerApplication  : public juce::JUCEApplication
{
public:
    PluginAnalyzerApplication() {}

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    /**
	 * @brief アプリケーション初期化
     */
    void initialise(const juce::String& /*commandLine*/) override
    {
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

	/**
	 * @brief アプリケーション終了
     */
    void shutdown() override
    {
        mainWindow = nullptr;
    }

    /**
	 * @brief システムからアプリケーション終了が要求されたとき
     */
    void systemRequestedQuit() override
    {
        quit();
    }

    /**
	 * @brief 別のインスタンスが起動されたとき
     */
    void anotherInstanceStarted(const juce::String& /*commandLine*/) override
    {
    }

    /**
	 * @brief メインウィンドウクラス
     */
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                         .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            
			// メインコンポーネントをセット
            setContentOwned(new MainComponent(), true);

            #if JUCE_IOS || JUCE_ANDROID
             setFullScreen(true);
            #else
             setResizable(true, true);
             centreWithSize(getWidth(), getHeight());
            #endif

            setVisible(true);
        }

        /**
		 * @brief 閉じるボタンが押されたときの処理
         */
        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

// エントリーポイント
START_JUCE_APPLICATION(PluginAnalyzerApplication)
