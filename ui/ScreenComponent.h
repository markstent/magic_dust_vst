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
#include "ui/ChordCell.h"
#include "ui/LaneView.h"
#include "ui/Readout.h"

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

// The display: paints the bezel and cream window, and owns the chord cells, lanes and bottom line.
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

    // Told the text a click or drag produced: an error, or nothing once it succeeds.
    void setOnStatus(std::function<void(std::optional<std::string>)> callback)
    {
        onStatus_ = std::move(callback);
    }

    // Shows an error on the bottom line in place of the readout, or the readout again for none.
    void setError(std::optional<std::string> error) { bottomLine_.setError(std::move(error)); }

    // Where dragged MIDI files are written; tests point it elsewhere.
    void setDragFolder(juce::File folder) { dragFolder_ = std::move(folder); }

  private:
    void startDrag(std::optional<md::LaneId> lane, std::optional<std::size_t> slot);
    void report(std::optional<std::string> status);

    MagicDustProcessor& processor_;
    std::array<ChordCell, 4> cells_;
    std::array<LaneView, 3> lanes_;
    Readout bottomLine_;
    Selection selection_;
    std::function<void(std::optional<std::string>)> onStatus_;
    juce::File dragFolder_ = md::defaultDragFolder();

    MD_COMPONENT_BOILERPLATE(ScreenComponent)
};

} // namespace md::ui
