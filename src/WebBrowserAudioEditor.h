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

    WebSliderRelay _gainRelay { "gain" };
    WebSliderRelay _tempRelay { "temperature" };
    WebSliderRelay _distRelay { "distance" };

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
            .withOptionsFrom (_gainRelay)
            .withOptionsFrom (_tempRelay)
            .withOptionsFrom (_distRelay)
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([this] (const auto& url) { return getResource (url); })
    };

    std::unique_ptr<WebSliderParameterAttachment> _gainAttachment;
    std::unique_ptr<WebSliderParameterAttachment> _tempAttachment;
    std::unique_ptr<WebSliderParameterAttachment> _distAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebBrowserAudioEditor)
};
