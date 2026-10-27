#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "md/ranges.hpp"
#include "md/theory.hpp"
#include "plugin/PluginProcessor.h"
#include "ui/AuraPad.h"
#include "ui/ChordBubble.h"
#include "ui/KeyButton.h"
#include "ui/LaneView.h"
#include "ui/ScreenComponent.h"
#include "ui/StatusFields.h"
#include "ui/TopLine.h"

namespace
{

using Rect = juce::Rectangle<int>;
using Catch::Approx;

// Seeds that give exactly this many placed chords with the default style, key and settings.
constexpr float kTolerance = 0.001F;
constexpr int kFourChordSeed = 1;
constexpr int kThreeChordSeed = 6;

// Puts the processor on a known idea: the state's seed is restored and the lane seeds derived.
void setSeed(MagicDustProcessor& p, int seed)
{
    juce::MemoryBlock state;
    p.getStateInformation(state);
    auto xml =
        juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    xml->setAttribute("seed", seed);
    xml->removeAttribute("chordsSeed");
    juce::MemoryBlock edited;
    juce::AudioProcessor::copyXmlToBinary(*xml, edited);
    p.setStateInformation(edited.getData(), static_cast<int>(edited.getSize()));
}

struct Fixture
{
    juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    explicit Fixture(int seed = kFourChordSeed)
    {
        setSeed(processor, seed);
        editor.reset(processor.createEditor());
    }

    // The fixed idea, which every test that reads it first checks has the chords it expects.
    const md::Idea& idea(std::size_t chords) const
    {
        REQUIRE(processor.idea() != nullptr);
        REQUIRE(processor.idea()->progression.size() == chords);
        return *processor.idea();
    }

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
    const auto& idea = f.idea(4);
    f.refresh();
    CHECK(f.find<md::ui::MenuText>("keyMenu").getText() == md::ui::keyText(idea.key));
    CHECK(f.find<md::ui::MenuText>("keyMenu").getText() == "a min");
}

TEST_CASE("an idea with three placed chords shows three bubbles", "[editor]")
{
    Fixture f(kThreeChordSeed);
    f.idea(3);
    f.refresh();
    CHECK(f.find("bubble0").isVisible());
    CHECK(f.find("bubble1").isVisible());
    CHECK(f.find("bubble2").isVisible());
    CHECK_FALSE(f.find("bubble3").isVisible());
}

TEST_CASE("a bubble's accessible title names the chord and its feel", "[editor]")
{
    Fixture f;
    const auto& idea = f.idea(4);
    f.refresh();
    for (std::size_t i = 0; i < 4; ++i)
    {
        const auto& chord = idea.progression.at(i);
        const auto expected = "chord " + std::to_string(i + 1) + ", " +
                              md::chordName(idea.key, chord.symbol) + ", " + chord.feel;
        CHECK(f.find(("bubble" + std::to_string(i)).c_str()).getTitle().toStdString() == expected);
    }
    CHECK(f.find("bubble0").getTitle().startsWith("chord 1, "));
    CHECK(f.find("bubble0").getTitle().endsWith(", home"));
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
    const auto& idea = f.idea(4);
    f.refresh();
    for (std::size_t i = 0; i < 3; ++i)
    {
        auto& roll = f.find<md::ui::PianoRoll>(("roll" + std::to_string(i)).c_str());
        const auto rects = roll.noteRects();
        REQUIRE(rects.size() == idea.lanes.at(i).events.size());
        for (std::size_t n = 0; n < rects.size(); ++n)
        {
            const auto& e = idea.lanes.at(i).events.at(n);
            const auto ticks = static_cast<float>(idea.settings.bars * md::kTicksPerBar);
            CHECK(rects.at(n).getX() ==
                  Approx(static_cast<float>(e.start) / ticks * 414.0F).margin(kTolerance));
            CHECK(rects.at(n).getY() >= -kTolerance);
            CHECK(rects.at(n).getBottom() <= 56.0F + kTolerance);
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

TEST_CASE("the keys sit where the layout table puts them", "[editor]")
{
    Fixture f;
    CHECK(f.boundsOf("playKey") == Rect{27, 575, 82, 82});
    CHECK(f.boundsOf("stopKey") == Rect{121, 575, 82, 82});
    CHECK(f.boundsOf("sprinkleKey") == Rect{227, 575, 176, 82});
}

TEST_CASE("clicking play then stop toggles the preview", "[editor]")
{
    Fixture f;
    f.processor.prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    click(f.find("playKey"), {40.0F, 40.0F});
    f.processor.processBlock(buffer, midi);
    CHECK(f.processor.previewPlaying());
    click(f.find("stopKey"), {40.0F, 40.0F});
    f.processor.processBlock(buffer, midi);
    CHECK_FALSE(f.processor.previewPlaying());
}

TEST_CASE("clicking sprinkle writes a new idea and the bubbles follow", "[editor]")
{
    Fixture f;
    juce::Random::getSystemRandom().setSeed(7);
    const auto before = f.idea(4).seed;
    const auto titles = [&]
    {
        juce::StringArray all;
        for (int i = 0; i < 4; ++i)
        {
            all.add(f.find(("bubble" + std::to_string(i)).c_str()).getTitle());
        }
        return all.joinIntoString("|");
    };
    const auto bubblesBefore = titles();

    click(f.find("sprinkleKey"), {40.0F, 40.0F});

    REQUIRE(f.processor.idea() != nullptr);
    CHECK(f.processor.idea()->seed != before);
    CHECK(titles() != bubblesBefore);
}

TEST_CASE("a pressed key moves down and jumps when animation is off", "[editor]")
{
    Fixture f;
    auto& key = f.find<md::ui::KeyButton>("playKey");
    key.setAnimated(false);
    const auto at = juce::Point<float>(40.0F, 40.0F);
    CHECK(key.pressedAmount() == Approx(0.0F));
    juce::Component& component = key;
    component.mouseDown(mouseAt(component, at, at, false));
    CHECK(key.pressedAmount() == Approx(1.0F));
    component.mouseUp(mouseAt(component, at, at, false));
    CHECK(key.pressedAmount() == Approx(0.0F));
}

TEST_CASE("clicking a bubble selects its chord and a lane click selects the lane", "[editor]")
{
    Fixture f;
    f.idea(4);
    auto& screen = f.find<md::ui::ScreenComponent>("screen");
    click(f.find("bubble2"), {40.0F, 40.0F});
    CHECK(screen.selection().kind == md::ui::Selection::Kind::chord);
    CHECK(screen.selection().index == 2);
    click(f.find("lane1"), {20.0F, 20.0F});
    CHECK(screen.selection().kind == md::ui::Selection::Kind::lane);
    CHECK(screen.selection().index == 1);
}

TEST_CASE("moving 4 px is a click and moving 5 px is a drag", "[editor]")
{
    Fixture f;
    f.idea(4);
    auto& screen = f.find<md::ui::ScreenComponent>("screen");
    int bubbleDrags = 0;
    int laneDrags = 0;
    auto& bubble = f.find<md::ui::ChordBubble>("bubble1");
    auto& lane = f.find<md::ui::LaneView>("lane2");
    bubble.setOnDrag([&] { ++bubbleDrags; });
    lane.setOnDrag([&] { ++laneDrags; });

    const auto down = juce::Point<float>(40.0F, 40.0F);
    const auto drag = [&](juce::Component& c, float distance)
    {
        c.mouseDown(mouseAt(c, down, down, false));
        const auto to = down.translated(distance, 0.0F);
        c.mouseDrag(mouseAt(c, to, down, true));
        c.mouseUp(mouseAt(c, to, down, true));
    };
    drag(bubble, 4.0F);
    drag(lane, 4.0F);
    CHECK(bubbleDrags == 0);
    CHECK(laneDrags == 0);
    CHECK(screen.selection().kind == md::ui::Selection::Kind::lane);

    screen.setSelection({});
    drag(bubble, 5.0F);
    drag(lane, 5.0F);
    CHECK(bubbleDrags == 1);
    CHECK(laneDrags == 1);
    CHECK(screen.selection().kind == md::ui::Selection::Kind::none);
}

TEST_CASE("tab visits top line, bubbles, lanes, aura pad, fields, then keys", "[editor]")
{
    Fixture f;
    f.idea(4);
    juce::KeyboardFocusTraverser traverser;
    std::vector<std::string> order;
    auto* current = traverser.getDefaultComponent(f.editor.get());
    while (current != nullptr && order.size() < 30)
    {
        order.push_back(current->getComponentID().toStdString());
        current = traverser.getNextComponent(current);
    }
    const std::vector<std::string> expected{
        "keyMenu", "styleMenu", "bubble0", "bubble1", "bubble2", "bubble3", "lane0",      "lane1",
        "lane2",   "auraPad",   "groove",  "flavour", "playKey", "stopKey", "sprinkleKey"};
    CHECK(order == expected);
}

TEST_CASE("every key and bubble has an accessible title", "[editor]")
{
    Fixture f;
    CHECK(f.find("playKey").getTitle() == "play the preview");
    CHECK(f.find("stopKey").getTitle() == "stop the preview");
    CHECK(f.find("sprinkleKey").getTitle() == "sprinkle a new idea");
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
