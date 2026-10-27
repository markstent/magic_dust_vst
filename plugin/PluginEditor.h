#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ui/MdLookAndFeel.h"

class MagicDustProcessor;

class MagicDustEditor : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit MagicDustEditor(MagicDustProcessor& owner);
    MagicDustEditor(const MagicDustEditor&) = delete;
    MagicDustEditor& operator=(const MagicDustEditor&) = delete;
    MagicDustEditor(MagicDustEditor&&) = delete;
    MagicDustEditor& operator=(MagicDustEditor&&) = delete;
    ~MagicDustEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    class Body;

    void timerCallback() override;

    md::ui::MdLookAndFeel lookAndFeel_;
    std::unique_ptr<Body> body_;

    JUCE_LEAK_DETECTOR(MagicDustEditor)
};
