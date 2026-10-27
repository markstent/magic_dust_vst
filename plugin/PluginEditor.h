#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ui/MdLookAndFeel.h"
#include "ui/SenderEditor.h"

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
    // Space toggles the preview, whichever control has the focus.
    bool keyPressed(const juce::KeyPress& key) override;

    // Swaps the main view and the sender view when the instance's mode has changed.
    void sync();
    // The 30 Hz timer's work, public so tests can run it.
    void refresh();

  private:
    class Body;

    void timerCallback() override;
    void showView();
    void updateScale();

    MagicDustProcessor& processor_;

    md::ui::MdLookAndFeel lookAndFeel_;
    std::unique_ptr<Body> body_;
    std::unique_ptr<md::ui::SenderBody> sender_;

    JUCE_LEAK_DETECTOR(MagicDustEditor)
};
