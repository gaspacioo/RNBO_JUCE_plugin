#pragma once

#include "JuceHeader.h"
#include "RNBO.h"
#include "RNBO_JuceAudioProcessor.h"
#include <memory>

/** Native JUCE editor skeleton — extend with controls for your RNBO parameters. */
class CustomAudioEditor : public AudioProcessorEditor
{
public:
    CustomAudioEditor (RNBO::JuceAudioProcessor* const p, RNBO::CoreObject& rnboObject);
    ~CustomAudioEditor() override;

    void paint (Graphics& g) override;
    void resized() override;

private:
    RNBO::JuceAudioProcessor* _audioProcessor;
    RNBO::CoreObject&         _rnboObject;

    Slider _gainSlider;
    Label  _gainLabel;

    std::unique_ptr<SliderParameterAttachment> _gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomAudioEditor)
};
