#pragma once

#include <algorithm>
#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>

namespace md
{

// One built-in sound. Notes take effect at the next render call. noteOn, noteOff, render and
// setLevel never allocate, lock or block after prepare.
class Instrument
{
  public:
    Instrument() = default;
    Instrument(const Instrument&) = delete;
    Instrument& operator=(const Instrument&) = delete;
    Instrument(Instrument&&) = delete;
    Instrument& operator=(Instrument&&) = delete;
    virtual ~Instrument() = default;

    virtual void prepare(double sampleRate) = 0;
    // A velocity of 0 is a note-off.
    virtual void noteOn(int note, int velocity) = 0;
    virtual void noteOff(int note) = 0;
    virtual void allNotesOff() = 0;
    // Adds into the buffer; start and num must lie within it.
    virtual void render(juce::AudioBuffer<float>& buffer, int start, int num) = 0;
    // The level is the volume times a lane level, each up to 2.0 (+6 dB).
    void setLevel(float level) { level_.store(std::clamp(level, 0.0F, 4.0F)); }

  protected:
    float level() const { return level_.load(); }

  private:
    std::atomic<float> level_{0.8F};
};

} // namespace md
