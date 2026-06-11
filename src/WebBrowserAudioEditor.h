#pragma once

#include "JuceHeader.h"
#include "RNBO.h"
#include "RNBO_JuceAudioProcessor.h"
#include "CustomAudioProcessor.h"
#include <memory>

class WebBrowserAudioEditor : public AudioProcessorEditor,
                              private Timer
{
public:
    WebBrowserAudioEditor (CustomAudioProcessor* const p, RNBO::CoreObject& rnboObject);
    ~WebBrowserAudioEditor() override;

    void paint (Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    std::vector<std::byte> getWebViewFileAsBytes (const juce::String& filepath);

    std::optional<WebBrowserComponent::Resource> getResource (const String& url);

    void sendMeterLevelsToWebView();

    CustomAudioProcessor* _audioProcessor;
    RNBO::CoreObject&     _rnboObject;

    int _specSendTick = 0;   // invia lo spettro ogni 4 tick del timer (~15 Hz)

    WebSliderRelay _gainRelay { "gain" };
    WebSliderRelay _tempRelay { "temperature" };
    WebSliderRelay _distRelay { "distance" };
    WebToggleButtonRelay _phaseInvRelay { "phase_inv" };
    WebSliderRelay _midGainRelay { "mid_gain" };
    WebSliderRelay _sideGainRelay { "side_gain" };
    WebToggleButtonRelay _midMuteRelay  { "mid_mute" };
    WebToggleButtonRelay _sideMuteRelay { "side_mute" };
    WebSliderRelay       _lGainRelay    { "l_gain" };
    WebSliderRelay       _rGainRelay    { "r_gain" };
    WebToggleButtonRelay _lMuteRelay    { "l_mute" };
    WebToggleButtonRelay _rMuteRelay    { "r_mute" };

    struct SinglePageBrowser : WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const String& newURL) override;
        bool pageLoadHadNetworkError (const String& errorInfo) override;
        void pageFinishedLoading (const String& url) override;
        bool isPageReady() const { return _pageReady; }

    private:
        bool _devServerFailed = false;
        bool _pageReady = false;
    };

    SinglePageBrowser _webComponent {
        WebBrowserComponent::Options{}
            .withBackend (WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder (juce::File::getSpecialLocation (
                    juce::File::SpecialLocationType::tempDirectory)))
            .withNativeIntegrationEnabled()
            .withNativeFunction("resizeWindow", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
                if (args.size() == 2) {
                    int newWidth  = args[0];
                    int newHeight = args[1];

                    juce::MessageManager::callAsync ([this, newWidth, newHeight]() {
                        setSize (newWidth, newHeight);
                    });
                }
                completion (juce::var());
            })
            .withOptionsFrom (_gainRelay)
            .withOptionsFrom (_tempRelay)
            .withOptionsFrom (_distRelay)
            .withOptionsFrom (_phaseInvRelay)
            .withOptionsFrom (_midGainRelay)
            .withOptionsFrom (_sideGainRelay)
            .withOptionsFrom (_midMuteRelay)
            .withOptionsFrom (_sideMuteRelay)
            .withOptionsFrom (_lGainRelay)
            .withOptionsFrom (_rGainRelay)
            .withOptionsFrom (_lMuteRelay)
            .withOptionsFrom (_rMuteRelay)
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([this] (const auto& url) { return getResource (url); })
    };

    std::unique_ptr<WebSliderParameterAttachment> _gainAttachment;
    std::unique_ptr<WebSliderParameterAttachment> _tempAttachment;
    std::unique_ptr<WebSliderParameterAttachment> _distAttachment;
    std::unique_ptr<WebToggleButtonParameterAttachment> _phaseInvAttachment;
    std::unique_ptr<WebSliderParameterAttachment> _midGainAttachment;
    std::unique_ptr<WebSliderParameterAttachment> _sideGainAttachment;
    std::unique_ptr<WebToggleButtonParameterAttachment> _midMuteAttachment;
    std::unique_ptr<WebToggleButtonParameterAttachment> _sideMuteAttachment;
    std::unique_ptr<WebSliderParameterAttachment>       _lGainAttachment;
    std::unique_ptr<WebSliderParameterAttachment>       _rGainAttachment;
    std::unique_ptr<WebToggleButtonParameterAttachment> _lMuteAttachment;
    std::unique_ptr<WebToggleButtonParameterAttachment> _rMuteAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebBrowserAudioEditor)
};
