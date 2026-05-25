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

    std::optional<WebBrowserComponent::Resource> getResource (const String& url);

    void sendMeterLevelsToWebView();

    CustomAudioProcessor* _audioProcessor;
    RNBO::CoreObject&     _rnboObject;

    WebSliderRelay _gainRelay { "gain" };

    struct SinglePageBrowser : WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const String& newURL) override;
        bool pageLoadHadNetworkError (const String& errorInfo) override;
        void pageFinishedLoading (const String& url) override;

    private:
        bool _devServerFailed = false;
    };

    SinglePageBrowser _webComponent {
        WebBrowserComponent::Options{}
            .withBackend (WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder (File::getSpecialLocation (
                    File::SpecialLocationType::tempDirectory)))
            .withNativeIntegrationEnabled()
            .withOptionsFrom (_gainRelay)
            .withKeepPageLoadedWhenBrowserIsHidden()
            //.withResourceProvider ([this] (const auto& url) { return getResource (url); })
    };

    std::unique_ptr<WebSliderParameterAttachment> _gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebBrowserAudioEditor)
};
