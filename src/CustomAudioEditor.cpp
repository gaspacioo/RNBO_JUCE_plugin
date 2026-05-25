#include "CustomAudioEditor.h"

namespace
{
    juce::RangedAudioParameter* findParameter (RNBO::JuceAudioProcessor* p, const juce::String& name)
    {
        for (auto* param : p->getParameters())
        {
            if (param->getName (128) == name)
                return static_cast<juce::RangedAudioParameter*> (param);
        }

        return nullptr;
    }
}

CustomAudioEditor::CustomAudioEditor (RNBO::JuceAudioProcessor* const p,
                                      RNBO::CoreObject& rnboObject)
    : AudioProcessorEditor (p)
    , _audioProcessor (p)
    , _rnboObject (rnboObject)
{
    _gainSlider.setSliderStyle (Slider::LinearHorizontal);
    _gainSlider.setTextBoxStyle (Slider::TextBoxRight, false, 60, 20);
    addAndMakeVisible (_gainSlider);

    _gainLabel.setText ("Gain", dontSendNotification);
    _gainLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (_gainLabel);

    if (auto* gainParam = findParameter (p, "gain"))
        _gainAttachment = std::make_unique<SliderParameterAttachment> (*gainParam, _gainSlider);
    else
        DBG ("CustomAudioEditor: parameter 'gain' not found — add a matching param in your RNBO patch.");

    setSize (400, 80);
}

CustomAudioEditor::~CustomAudioEditor() = default;

void CustomAudioEditor::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void CustomAudioEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    auto row  = area.removeFromTop (28);
    _gainLabel.setBounds (row.removeFromLeft (50));
    _gainSlider.setBounds (row);
}
