#include "WebBrowserAudioEditor.h"
#include "BinaryData.h"

#if JUCE_ANDROID
static const juce::String kDevServerAddress = "http://10.0.2.2:3000/";
#else
static const juce::String kDevServerAddress = "http://localhost:3000/";
#endif

//==============================================================================
bool WebBrowserAudioEditor::SinglePageBrowser::pageAboutToLoad (const String& newURL)
{
    setVisible (false);

    return newURL.startsWith (kDevServerAddress)
        || newURL == getResourceProviderRoot();
}

bool WebBrowserAudioEditor::SinglePageBrowser::pageLoadHadNetworkError (const String& /*errorInfo*/)
{
    if (! _devServerFailed)
    {
        _devServerFailed = true;
        goToURL (getResourceProviderRoot());
        return false;
    }

    return true;
}

void WebBrowserAudioEditor::SinglePageBrowser::pageFinishedLoading (const String& /*url*/)
{
    setVisible (true);
}

//==============================================================================
namespace
{
    const char* getMimeForExtension (const String& ext)
    {
        if (ext == "html" || ext == "htm") return "text/html";
        if (ext == "css")                  return "text/css";
        if (ext == "js")                   return "text/javascript";
        if (ext == "json")                 return "application/json";
        if (ext == "svg")                  return "image/svg+xml";
        if (ext == "png")                  return "image/png";
        if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
        if (ext == "ico")                  return "image/vnd.microsoft.icon";
        if (ext == "woff2")                return "font/woff2";
        return "application/octet-stream";
    }

    juce::RangedAudioParameter& findParameter (RNBO::JuceAudioProcessor* p, const juce::String& name)
    {
        for (auto* param : p->getParameters())
        {
            if (param->getName (128) == name)
                return static_cast<juce::RangedAudioParameter&> (*param);
        }

        throw std::runtime_error ("Parameter not found: " + name.toStdString());
    }
}

//==============================================================================
WebBrowserAudioEditor::WebBrowserAudioEditor (CustomAudioProcessor* const p,
                                              RNBO::CoreObject& rnboObject)
    : AudioProcessorEditor (p)
    , _audioProcessor (p)
    , _rnboObject (rnboObject)
{
    addChildComponent (_webComponent);
    _webComponent.goToURL (kDevServerAddress);

    setResizable (true, true);
    setResizeLimits (260, 300, 720, 900);
    setSize (360, 420);

    try
    {
        _gainAttachment = std::make_unique<WebSliderParameterAttachment> (
            findParameter (p, "gain"), _gainRelay, nullptr);
    }
    catch (const std::exception& e)
    {
        DBG ("WebBrowserAudioEditor: parameter attach failed: " + String (e.what()));
    }

    startTimerHz (60);
}

WebBrowserAudioEditor::~WebBrowserAudioEditor()
{
    stopTimer();
}

void WebBrowserAudioEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff16161e));
}

void WebBrowserAudioEditor::resized()
{
    _webComponent.setBounds (getLocalBounds());
}

void WebBrowserAudioEditor::timerCallback()
{
    sendMeterLevelsToWebView();
}

void WebBrowserAudioEditor::sendMeterLevelsToWebView()
{
    const auto& levels = _audioProcessor->meterLevels;

    DynamicObject::Ptr payload = new DynamicObject();
    payload->setProperty ("inL",       levels.inL.load (std::memory_order_relaxed));
    payload->setProperty ("inR",       levels.inR.load (std::memory_order_relaxed));
    payload->setProperty ("outL",      levels.outL.load (std::memory_order_relaxed));
    payload->setProperty ("outR",      levels.outR.load (std::memory_order_relaxed));
    payload->setProperty ("inPeakL",   levels.inPeakL.load (std::memory_order_relaxed));
    payload->setProperty ("inPeakR",   levels.inPeakR.load (std::memory_order_relaxed));
    payload->setProperty ("outPeakL",  levels.outPeakL.load (std::memory_order_relaxed));
    payload->setProperty ("outPeakR",  levels.outPeakR.load (std::memory_order_relaxed));

    _webComponent.emitEventIfBrowserIsVisible ("meterLevels", var (payload.get()));
}

std::optional<WebBrowserComponent::Resource>
WebBrowserAudioEditor::getResource (const String& url)
{
    const auto filename = (url == "/" || url.isEmpty())
                            ? String ("index.html")
                            : url.fromFirstOccurrenceOf ("/", false, false);

    const auto ext  = filename.fromLastOccurrenceOf (".", false, false).toLowerCase();
    const auto mime = getMimeForExtension (ext);

    String resourceName;
    for (auto c : filename)
        resourceName += (CharacterFunctions::isLetterOrDigit (c) || c == '_') ? c : juce_wchar ('_');

    int dataSize = 0;
    const char* data = RNBOUIData::getNamedResource (resourceName.toRawUTF8(), dataSize);

    if (data != nullptr)
    {
        const auto* begin = reinterpret_cast<const std::byte*> (data);
        return WebBrowserComponent::Resource { std::vector<std::byte> (begin, begin + dataSize), mime };
    }

    return std::nullopt;
}
