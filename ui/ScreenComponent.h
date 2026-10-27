#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/AuraPad.h"
#include "ui/ChordBubble.h"
#include "ui/LaneView.h"
#include "ui/Readout.h"
#include "ui/StatusFields.h"
#include "ui/TopLine.h"

class MagicDustProcessor;

#include "ui/Boilerplate.h"

namespace md::ui
{

struct Selection
{
    enum class Kind : std::uint8_t
    {
        none,
        chord,
        lane
    };
    Kind kind = Kind::none;
    int index = 0;
};

// Paints the bezel and screen, and owns the top line, bubbles, lanes, aura pad, fields and readout.
class ScreenComponent : public juce::Component
{
  public:
    explicit ScreenComponent(MagicDustProcessor& processor);

    // Pulls everything the screen shows from the processor.
    void refresh();
    void setSelection(Selection selection);
    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    MagicDustProcessor& processor_;
    TopLine topLine_;
    std::array<ChordBubble, 4> bubbles_;
    std::array<LaneView, 3> lanes_;
    AuraPad auraPad_;
    StatusFields fields_;
    Readout readout_;
    Selection selection_;

    MD_COMPONENT_BOILERPLATE(ScreenComponent)
};

} // namespace md::ui
