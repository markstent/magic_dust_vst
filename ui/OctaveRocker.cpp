#include "ui/OctaveRocker.h"

#include <algorithm>
#include <cmath>

#include "md/ranges.hpp"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

juce::String octaveText(int octave)
{
    return octave > 0 ? "+" + juce::String(octave) : juce::String(octave);
}

} // namespace

OctaveRocker::OctaveRocker(juce::AudioProcessorValueTreeState& params, const char* paramId,
                           const juce::String& lane, int index)
    : params_(params), paramId_(paramId), down_("", std::nullopt, KeyIcon::arrowDown),
      window_("octave" + juce::String(index) + "Window", lane + " octave", false,
              t::layout::octaveTextSize),
      up_("", std::nullopt, KeyIcon::arrowUp)
{
    setComponentID("octave" + juce::String(index));
    const auto place = [](juce::Component& c, int x, int width)
    { c.setBounds(x, 0, width, t::layout::octaveHeight); };
    place(down_, 0, t::layout::octaveKeyWidth);
    place(window_, t::layout::octaveKeyWidth + t::layout::octaveGap, t::layout::octaveWindowWidth);
    place(up_, t::layout::octaveWidth - t::layout::octaveKeyWidth, t::layout::octaveKeyWidth);
    down_.setComponentID("octave" + juce::String(index) + "Down");
    up_.setComponentID("octave" + juce::String(index) + "Up");
    down_.setTitle(lane + " octave down");
    up_.setTitle(lane + " octave up");
    down_.setExplicitFocusOrder(1);
    up_.setExplicitFocusOrder(2);
    down_.onClick = [this] { step(-1); };
    up_.onClick = [this] { step(1); };
    addAndMakeVisible(down_);
    addAndMakeVisible(window_);
    addAndMakeVisible(up_);
    setSize(t::layout::octaveWidth, t::layout::octaveHeight + t::layout::octaveShadowRoom);
    refresh();
}

int OctaveRocker::current() const
{
    const auto raw = params_.getRawParameterValue(paramId_)->load();
    return std::clamp(static_cast<int>(std::lround(raw)), md::kOctaveMin, md::kOctaveMax);
}

void OctaveRocker::refresh()
{
    const int octave = current();
    window_.setText(octaveText(octave));
    down_.setEnabled(octave > md::kOctaveMin);
    up_.setEnabled(octave < md::kOctaveMax);
}

void OctaveRocker::step(int delta)
{
    auto* param = params_.getParameter(paramId_);
    const int next = std::clamp(current() + delta, md::kOctaveMin, md::kOctaveMax);
    param->beginChangeGesture();
    param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(next)));
    param->endChangeGesture();
    refresh();
}

} // namespace md::ui
