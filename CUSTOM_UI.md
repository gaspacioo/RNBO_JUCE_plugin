# Custom UI (RNBO + JUCE)

This project ships a minimal custom UI for the **SimpleMeter** RNBO export: one `gain` parameter and RMS level outports. Use it as a skeleton for your own patch and interface.

For setup and build instructions, see [README.md](./README.md). For a web-based UI you also need [Node.js](https://nodejs.org/).

## Choosing an editor mode

In `CMakeLists.txt`:

```cmake
set(RNBO_EDITOR_MODE "DEFAULT" CACHE STRING "...")
```

| Value | Behaviour |
| - | - |
| `DEFAULT` | Generic RNBO parameter sliders (`RNBO::JuceAudioProcessor::createEditor`) |
| `NATIVE` | C++ editor in `src/CustomAudioEditor.*` |
| `WEBVIEW` | HTML/CSS/JS in `src/webui/` via `WebBrowserComponent` |

Configure at build time:

```sh
cd build
cmake .. -DRNBO_EDITOR_MODE=WEBVIEW -G Ninja
cmake --build .
```

`CustomAudioProcessor::createEditor()` selects the implementation:

```cpp
#if defined(RNBO_EDITOR_NATIVE)
    return new CustomAudioEditor (this, this->_rnboObject);
#elif defined(RNBO_EDITOR_WEBVIEW)
    return new WebBrowserAudioEditor (this, this->_rnboObject);
#else
    return RNBO::JuceAudioProcessor::createEditor();
#endif
```

## RNBO export (parameters and outports)

After exporting your patch into `export/`, check `export/description.json`. For SimpleMeter it defines:

- **Parameter:** `gain` (−60 … +12 dB)
- **Outports:** `in_rms_L`, `in_rms_R`, `out_rms_L`, `out_rms_R` (RMS levels in dB)

The default export source file is `export/rnbo_source.cpp` (`RNBO_CLASS_FILE_NAME` in CMake). If you rename the export file, pass it to CMake:

```sh
cmake .. -DRNBO_CLASS_FILE_NAME=my_patch.cpp
```

Store your Max patcher under `patches/` for reference only; the build uses the C++ export in `export/`.

---

## Web-based UI (`WEBVIEW`)

### Layout

Files in `src/webui/`:

| File | Role |
| - | - |
| `index.html` | Structure: input meters, gain slider, output meters |
| `style.css` | Styling |
| `main.js` | Bindings to RNBO via JUCE frontend helpers |

Production assets are bundled into `src/webui/dist/` and embedded with `juce_add_binary_data(RNBOUIData)` when `RNBO_EDITOR_MODE=WEBVIEW`.

### Hot reload (development)

With `npm run dev` in `src/webui/`, the editor loads `http://localhost:3000/` first; if the server is down, it falls back to embedded `dist/` files.

```sh
cd src/webui
npm install    # once
npm run dev    # dev server on port 3000
```

Edit HTML/CSS/JS and reload the plugin window to see changes. For release builds:

```sh
npm run build
cd ../../build && cmake --build .
```

### Binding a parameter (`gain`)

**C++** — declare a relay whose name matches the RNBO parameter, register it on the browser, and attach it to the processor parameter:

```cpp
WebSliderRelay _gainRelay { "gain" };

SinglePageBrowser _webComponent {
    WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        .withOptionsFrom (_gainRelay)
        ...
};

_gainAttachment = std::make_unique<WebSliderParameterAttachment> (
    findParameter (p, "gain"), _gainRelay, nullptr);
```

**JavaScript** — use the same id with `getSliderState`:

```js
import { getSliderState } from 'juce-framework-frontend';

const state = getSliderState('gain');
state.valueChangedEvent.addListener(() => { /* host → UI */ });
slider.addEventListener('input', () => state.setNormalisedValue(parseFloat(slider.value)));
```

To add another parameter: export it from RNBO, add a `WebSliderRelay`, attachment, HTML control, and `bindSliderParam('yourParam', ...)`.

### Binding outports (RMS meters)

Outports are **messages**, not parameters. They are handled in three layers:

1. **`CustomAudioProcessor::handleMessageEvent`** — reads `MessageEvent`s tagged `in_rms_L`, etc., and stores dB values in `meterLevels` (atomics).
2. **`WebBrowserAudioEditor`** — timer at 30 Hz calls `emitEventIfBrowserIsVisible("meterLevels", { inL, inR, outL, outR })`.
3. **`main.js`** — `backend.addEventListener('meterLevels', ...)` updates the meter bars.

To add a new outport: define it in Max, re-export, add a `TAG("your_tag")` branch in `handleMessageEvent`, extend the JSON payload and the JS listener.

---

## Native UI (`NATIVE`)

`CustomAudioEditor` is a minimal example: one horizontal slider for `gain`. Copy the pattern (slider + `SliderParameterAttachment` + `findParameter`) for each new RNBO parameter.

Outport-driven UI (meters) is not implemented in the native editor; use `WEBVIEW` or extend `CustomAudioProcessor` / `CustomAudioEditor` with a `Timer` and custom widgets.

```sh
cmake .. -DRNBO_EDITOR_MODE=NATIVE -G Ninja
cmake --build .
```

---

## Checklist for a new patch

1. Export C++ into `export/` (keep `description.json` in sync).
2. Match parameter **names** in relays / attachments / `getSliderState`.
3. Match outport **tags** in `handleMessageEvent` and in your UI bridge.
4. Rebuild web assets (`npm run build`) if using `WEBVIEW`.
5. Reconfigure/rebuild CMake after changing `RNBO_CLASS_FILE_NAME` or editor mode.
