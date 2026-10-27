#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include "md/types.hpp"
#include "plugin/DragFiles.h"
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
    [[nodiscard]] Selection selection() const { return selection_; }
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Told the status line text a click or drag produced: an error, or nothing once it succeeds.
    void setOnStatus(std::function<void(std::optional<std::string>)> callback)
    {
        onStatus_ = std::move(callback);
    }

    // Where dragged MIDI files are written; tests point it elsewhere.
    void setDragFolder(juce::File folder) { dragFolder_ = std::move(folder); }

  private:
    void startDrag(std::optional<md::LaneId> lane, std::optional<std::size_t> slot);
    void report(std::optional<std::string> status);

    MagicDustProcessor& processor_;
    TopLine topLine_;
    std::array<ChordBubble, 4> bubbles_;
    std::array<LaneView, 3> lanes_;
    AuraPad auraPad_;
    StatusFields fields_;
    Readout readout_;
    Selection selection_;
    std::function<void(std::optional<std::string>)> onStatus_;
    juce::File dragFolder_ = md::defaultDragFolder();

    MD_COMPONENT_BOILERPLATE(ScreenComponent)
};

} // namespace md::ui
