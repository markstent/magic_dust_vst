#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include <juce_audio_processors/juce_audio_processors.h>

#include "md/ranges.hpp"
#include "md/theory.hpp"
#include "plugin/PluginProcessor.h"
#include "ui/AuraPad.h"
#include "ui/ChordBubble.h"
#include "ui/LaneView.h"
#include "ui/ScreenComponent.h"
#include "ui/StatusFields.h"
#include "ui/TopLine.h"

namespace
{

using Rect = juce::Rectangle<int>;

struct Fixture
{
    juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};

    static juce::Component* search(juce::Component& parent, const juce::String& id)
    {
        for (auto* child : parent.getChildren())
        {
            if (child->getComponentID() == id)
            {
                return child;
            }
            if (auto* found = search(*child, id))
            {
                return found;
            }
        }
        return nullptr;
    }

    template <typename T = juce::Component> T& find(const char* id)
    {
        auto* found = dynamic_cast<T*>(search(*editor, id));
        REQUIRE(found != nullptr);
        return *found;
    }

    // The component's bounds in the editor's 1040 x 740 space.
    Rect boundsOf(const char* id)
    {
        auto& c = find(id);
        return editor->getLocalArea(&c, c.getLocalBounds());
    }

    void refresh() { find<md::ui::ScreenComponent>("screen").refresh(); }

    // Sprinkles until the idea has exactly `count` placed chords.
    void ideaWithChords(std::size_t count)
    {
        for (int tries = 0; tries < 500; ++tries)
        {
            if (processor.idea() != nullptr && processor.idea()->progression.size() == count)
            {
                refresh();
                return;
            }
            processor.sprinkle();
        }
        FAIL("no idea with " << count << " chords");
    }
};

juce::MouseEvent mouseAt(juce::Component& c, juce::Point<float> position, juce::Point<float> down,
                         bool dragged)
{
    return {juce::Desktop::getInstance().getMainMouseSource(),
            position,
            juce::ModifierKeys(),
            1.0F,
            juce::MouseInputSource::defaultOrientation,
            juce::MouseInputSource::defaultRotation,
            juce::MouseInputSource::defaultTiltX,
            juce::MouseInputSource::defaultTiltY,
            &c,
            &c,
            juce::Time::getCurrentTime(),
            down,
            juce::Time::getCurrentTime(),
            1,
            dragged};
}

void click(juce::Component& c, juce::Point<float> at)
{
    c.mouseDown(mouseAt(c, at, at, false));
    c.mouseUp(mouseAt(c, at, at, false));
}

float param(Fixture& f, const char* id)
{
    return f.processor.params.getRawParameterValue(id)->load();
}

} // namespace

TEST_CASE("the editor opens at 1040 x 740", "[editor]")
{
    Fixture f;
    CHECK(f.editor->getWidth() == 1040);
    CHECK(f.editor->getHeight() == 740);
}

TEST_CASE("every component sits where the layout table puts it", "[editor]")
{
    Fixture f;
    CHECK(f.boundsOf("screen") == Rect(245, 27, 546, 498));
    CHECK(f.boundsOf("topLine") == Rect(272, 50, 492, 20));
    const std::array<int, 4> bubbleX{278, 407, 537, 666};
    for (std::size_t i = 0; i < 4; ++i)
    {
        CHECK(f.boundsOf(("bubble" + std::to_string(i)).c_str()) ==
              Rect(bubbleX.at(i), 82, 92, 92));
    }
    const std::array<int, 3> laneY{198, 264, 330};
    for (std::size_t i = 0; i < 3; ++i)
    {
        CHECK(f.boundsOf(("lane" + std::to_string(i)).c_str()) == Rect(272, laneY.at(i), 492, 62));
        CHECK(f.boundsOf(("roll" + std::to_string(i)).c_str()) ==
              Rect(344, laneY.at(i) + 3, 414, 56));
    }
    CHECK(f.boundsOf("auraPad") == Rect(272, 432, 70, 70));
    CHECK(f.boundsOf("statusFields").getX() == 358);
    CHECK(f.boundsOf("readout").getRight() == 764);
}

TEST_CASE("the top line spells the key as min, maj or the mode name", "[editor]")
{
    CHECK(md::ui::keyText({.root = 9, .mode = md::Mode::aeolian}) == "a min");
    CHECK(md::ui::keyText({.root = 3, .mode = md::Mode::ionian}) == "eb maj");
    CHECK(md::ui::keyText({.root = 9, .mode = md::Mode::dorian}) == "a dorian");
    CHECK(md::ui::keyText({.root = 6, .mode = md::Mode::phrygian}) == "f# phrygian");
    CHECK(md::ui::keyText({.root = 0, .mode = md::Mode::mixolydian}) == "c mixolydian");
}

TEST_CASE("the key button shows the idea's key", "[editor]")
{
    Fixture f;
    f.refresh();
    CHECK(f.find<md::ui::MenuText>("keyMenu").getText() ==
          md::ui::keyText(f.processor.idea()->key));
}

TEST_CASE("an idea with three placed chords shows three bubbles", "[editor]")
{
    Fixture f;
    f.ideaWithChords(3);
    CHECK(f.find("bubble0").isVisible());
    CHECK(f.find("bubble1").isVisible());
    CHECK(f.find("bubble2").isVisible());
    CHECK_FALSE(f.find("bubble3").isVisible());
}

TEST_CASE("a bubble's accessible title names the chord and its feel", "[editor]")
{
    Fixture f;
    f.ideaWithChords(4);
    const auto& idea = *f.processor.idea();
    for (std::size_t i = 0; i < 4; ++i)
    {
        const auto& chord = idea.progression.at(i);
        const auto expected = "chord " + std::to_string(i + 1) + ", " +
                              md::chordName(idea.key, chord.symbol) + ", " + chord.feel;
        CHECK(f.find(("bubble" + std::to_string(i)).c_str()).getTitle().toStdString() == expected);
    }
    CHECK(f.find("bubble0").getTitle().startsWith("chord 1, "));
}

TEST_CASE("a lane maps a note's ticks across the roll", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    md::ui::LaneView lane(md::LaneId::chords);
    lane.setBounds(272, 198, 492, 62);
    md::Lane data{.id = md::LaneId::chords,
                  .pattern = "organ stabs",
                  .events = {{.pitch = 60, .start = 0, .length = 3840, .velocity = 90}}};
    lane.set(data, 8, std::nullopt, false);

    const auto rects = lane.roll().noteRects();
    REQUIRE(rects.size() == 1);
    CHECK(lane.getX() + lane.roll().getX() + rects.at(0).getX() == 344.0F);
    CHECK(rects.at(0).getWidth() == 414.0F / 8.0F);
}

TEST_CASE("the real notes map the same way and stay inside the roll", "[editor]")
{
    Fixture f;
    f.refresh();
    const auto& idea = *f.processor.idea();
    for (std::size_t i = 0; i < 3; ++i)
    {
        auto& roll = f.find<md::ui::PianoRoll>(("roll" + std::to_string(i)).c_str());
        const auto rects = roll.noteRects();
        REQUIRE(rects.size() == idea.lanes.at(i).events.size());
        for (std::size_t n = 0; n < rects.size(); ++n)
        {
            const auto& e = idea.lanes.at(i).events.at(n);
            const auto ticks = static_cast<float>(idea.settings.bars * md::kTicksPerBar);
            CHECK(rects.at(n).getX() == static_cast<float>(e.start) / ticks * 414.0F);
            CHECK(rects.at(n).getY() >= 0.0F);
            CHECK(rects.at(n).getBottom() <= 56.0F);
            CHECK(rects.at(n).getHeight() > 5.0F);
        }
    }
}

TEST_CASE("clicking the flavour field cycles simple, soulful, jazzy, simple", "[editor]")
{
    Fixture f;
    f.processor.params.getParameter("flavour")->setValueNotifyingHost(0.0F);
    auto& field = f.find("flavour");
    const std::array<float, 3> expected{1.0F, 2.0F, 0.0F};
    for (const float flavour : expected)
    {
        click(field, {10.0F, 9.0F});
        CHECK(param(f, "flavour") == flavour);
    }
}

TEST_CASE("the groove field steps with the scroll wheel and wraps", "[editor]")
{
    Fixture f;
    auto& field = f.find("groove");
    juce::MouseWheelDetails wheel{};
    wheel.deltaY = -1.0F;
    field.mouseWheelMove(mouseAt(field, {10.0F, 9.0F}, {10.0F, 9.0F}, false), wheel);
    CHECK(param(f, "groove") == 1.0F);
    wheel.deltaY = 1.0F;
    field.mouseWheelMove(mouseAt(field, {10.0F, 9.0F}, {10.0F, 9.0F}, false), wheel);
    field.mouseWheelMove(mouseAt(field, {10.0F, 9.0F}, {10.0F, 9.0F}, false), wheel);
    CHECK(param(f, "groove") == 3.0F);
}

TEST_CASE("dragging on the aura pad sets mood and energy within the 8% inset", "[editor]")
{
    Fixture f;
    auto& pad = f.find<md::ui::AuraPad>("auraPad");
    const juce::Point<float> centre{35.0F, 35.0F};
    pad.mouseDown(mouseAt(pad, centre, centre, false));
    CHECK(param(f, "mood") == 50.0F);
    CHECK(param(f, "energy") == 50.0F);
    pad.mouseDrag(mouseAt(pad, {70.0F, 0.0F}, centre, true));
    pad.mouseUp(mouseAt(pad, {70.0F, 0.0F}, centre, true));
    CHECK(param(f, "mood") == 100.0F);
    CHECK(param(f, "energy") == 100.0F);

    pad.mouseDown(mouseAt(pad, {0.0F, 70.0F}, {0.0F, 70.0F}, false));
    pad.mouseUp(mouseAt(pad, {0.0F, 70.0F}, {0.0F, 70.0F}, false));
    CHECK(param(f, "mood") == 0.0F);
    CHECK(param(f, "energy") == 0.0F);
}

TEST_CASE("the aura field names the quadrant at mood and energy 50", "[editor]")
{
    Fixture f;
    auto& fields = f.find<md::ui::StatusFields>("statusFields");
    const auto set = [&f, &fields](float mood, float energy)
    {
        f.processor.params.getParameter("mood")->setValueNotifyingHost(mood / 100.0F);
        f.processor.params.getParameter("energy")->setValueNotifyingHost(energy / 100.0F);
        fields.refresh();
        return fields.getTitle().toStdString();
    };
    CHECK(set(49, 50) == "aura, midnight rave");
    CHECK(set(50, 50) == "aura, starburst");
    CHECK(set(49, 49) == "aura, moonlight");
    CHECK(set(50, 49) == "aura, daydream");
}

TEST_CASE("opening and closing the editor leaks nothing", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor;
    for (int i = 0; i < 20; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
        juce::Component::SafePointer<juce::AudioProcessorEditor> watch(editor.get());
        editor.reset();
        CHECK(watch == nullptr);
    }
    CHECK(processor.getActiveEditor() == nullptr);
}

// Renders the editor to PNGs in $MD_RENDER_DIR for the side-by-side review; does nothing when
// the variable is not set.
TEST_CASE("render the editor", "[.render]")
{
    const char* dir = std::getenv("MD_RENDER_DIR");
    if (dir == nullptr)
    {
        return;
    }
    Fixture f;
    f.refresh();
    const juce::File out(dir);
    out.createDirectory();
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    const auto write = [&](const char* name, int width, float scale)
    {
        f.editor->setSize(width, width * 740 / 1040);
        const auto image =
            f.editor->createComponentSnapshot(f.editor->getLocalBounds(), true, scale);
        juce::FileOutputStream stream(out.getChildFile(name));
        REQUIRE(stream.openedOk());
        stream.setPosition(0);
        stream.truncate();
        REQUIRE(juce::PNGImageFormat().writeImageToStream(image, stream));
    };
    write("editor-100.png", 1040, 1.0F);
    write("editor-150.png", 1560, 1.0F);
    write("editor-200.png", 1040, 2.0F);
}
