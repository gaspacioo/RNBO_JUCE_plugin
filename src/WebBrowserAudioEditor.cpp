#include "WebBrowserAudioEditor.h"
#include "BinaryData.h"

// The dev server address. When a server is listening here, the browser loads from it
// instead of the built-in resource provider, so you can iterate on src/webui/ without
// recompiling. Use a server that disables caching and (optionally) auto-reloads:
//
//   # No-cache static server (manual page reload required after edits):
//   python3 -c "
//   import http.server
//   class H(http.server.SimpleHTTPRequestHandler):
//       def end_headers(self):
//           self.send_header('Cache-Control', 'no-store')
//           super().end_headers()
//   http.server.test(HandlerClass=H, port=3000)
//   "
//
//   # Hot-reload (auto-reloads browser on file change, requires Node):
//   npx live-server --port=3000
//
// pageLoadHadNetworkError falls back to the resource provider when no server is running.
#if JUCE_ANDROID
static const juce::String kDevServerAddress = "http://10.0.2.2:3000/";
#else
static const juce::String kDevServerAddress = "http://localhost:3000/";
#endif

//==============================================================================
bool WebBrowserAudioEditor::SinglePageBrowser::pageAboutToLoad (const String& newURL)
{
    _pageReady = false;

    // Allow the dev server and the JUCE resource provider root; block everything else
    // so the single-page UI can't accidentally navigate away.
    return newURL.startsWith (kDevServerAddress)
        || newURL == getResourceProviderRoot();
}

bool WebBrowserAudioEditor::SinglePageBrowser::pageLoadHadNetworkError (const String& /*errorInfo*/)
{
    // Dev server is not reachable — fall back to serving the file via the resource provider.
    // The flag prevents an infinite loop if the resource provider also somehow fails.
    if (! _devServerFailed)
    {
        _devServerFailed = true;
        goToURL (getResourceProviderRoot());
        return false;   // we handled it; don't show the browser's error page
    }

    return true;    // let the browser show its error page
}

void WebBrowserAudioEditor::SinglePageBrowser::pageFinishedLoading (const String& /*url*/)
{
    _pageReady = true;
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
    try
    {
        _gainAttachment = std::make_unique<WebSliderParameterAttachment> (
            findParameter (p, "gain"), _gainRelay, nullptr);
        _tempAttachment = std::make_unique<WebSliderParameterAttachment> (
            findParameter (p, "temperature"), _tempRelay, nullptr);
        _distAttachment = std::make_unique<WebSliderParameterAttachment> (
            findParameter (p, "distance"), _distRelay, nullptr);
        _phaseInvAttachment = std::make_unique<WebToggleButtonParameterAttachment> (
            findParameter (p, "phase_inv"), _phaseInvRelay, nullptr);
        _midGainAttachment = std::make_unique<WebSliderParameterAttachment> (
            findParameter (p, "mid_gain"), _midGainRelay, nullptr);
        _sideGainAttachment = std::make_unique<WebSliderParameterAttachment> (
            findParameter (p, "side_gain"), _sideGainRelay, nullptr);
        _midMuteAttachment = std::make_unique<WebToggleButtonParameterAttachment> (
            findParameter (p, "mid_mute"), _midMuteRelay, nullptr);
        _sideMuteAttachment = std::make_unique<WebToggleButtonParameterAttachment> (
            findParameter (p, "side_mute"), _sideMuteRelay, nullptr);
    }
    catch (const std::exception& e)
    {
        DBG ("WebBrowserAudioEditor: parameter attach failed: " + String (e.what()));
    }

    addAndMakeVisible (_webComponent);

    // Try the dev server first. If nothing is listening on that port,
    // pageLoadHadNetworkError fires quickly and redirects to getResourceProviderRoot().
    _webComponent.goToURL (kDevServerAddress);
    //_webComponent.goToURL(WebBrowserComponent::getResourceProviderRoot());

    setResizable (true, true);
    setResizeLimits (260, 300, 720, 900);
    setSize (560, 400);

    startTimerHz (60);
}

WebBrowserAudioEditor::~WebBrowserAudioEditor()
{
    stopTimer();
    //_audioProcessor->AudioProcessor::removeListener (this);
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
    if (! _webComponent.isPageReady())
        return;

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
    payload->setProperty ("delayTime", levels.delayTime.load (std::memory_order_relaxed));
    payload->setProperty ("sampleRate", _audioProcessor->getSampleRate());

    _webComponent.emitEventIfBrowserIsVisible ("meterLevels", var (payload.get()));
}

std::vector<std::byte> streamToVector (juce::InputStream& stream) {

    const auto sizeInBytes = static_cast<size_t> (stream.getTotalLength());
    std::vector<std::byte> result (sizeInBytes);
    stream.setPosition (0);
    stream.read (result.data(), result.size());
    return result;
}

std::vector<std::byte> WebBrowserAudioEditor::getWebViewFileAsBytes(const juce::String& filepath) {

    juce::MemoryInputStream zipStream(RNBOUIData::web_ui_zip, RNBOUIData::web_ui_zipSize, false);
    juce::ZipFile zipFile(zipStream);

    auto* zipEntry = zipFile.getEntry(filepath);
    if(zipEntry == nullptr) zipEntry = zipFile.getEntry("dist/" + filepath);

    if(zipEntry != nullptr) {
        const std::unique_ptr<juce::InputStream> entryStream(zipFile.createStreamForEntry(*zipEntry));
        if(entryStream != nullptr) {
            return streamToVector(*entryStream);
        }
    }
    return {};
}

std::optional<WebBrowserComponent::Resource>
WebBrowserAudioEditor::getResource (const String& url) {
    
    // Map "/" or "" to index.html; strip any leading slash for other paths.
    const auto filename = (url == "/" || url.isEmpty())
                            ? String ("index.html")
                            : url.fromFirstOccurrenceOf ("/", false, false);

    const auto resource = getWebViewFileAsBytes(filename);

    if(!resource.empty()) {
        const auto extension  = filename.fromLastOccurrenceOf (".", false, false).toLowerCase();
        const auto mime = getMimeForExtension (extension);

        return juce::WebBrowserComponent::Resource {
            std::move(resource), mime 
        };
    }
    return std::nullopt;
}
