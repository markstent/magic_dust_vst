#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "SeedHelper.h"
#include "md/bank.hpp"
#include "md/ranges.hpp"
#include "md/theory.hpp"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "ui/AboutPanel.h"
#include "ui/ChordCell.h"
#include "ui/Drag.h"
#include "ui/Faceplate.h"
#include "ui/Fonts.h"
#include "ui/KeyButton.h"
#include "ui/Knob.h"
#include "ui/LaneView.h"
#include "ui/Readout.h"
#include "ui/ScreenComponent.h"
#include "ui/SenderEditor.h"
#include "ui/Tokens.h"
#include "ui/TopLine.h"

namespace
{

using Rect = juce::Rectangle<int>;
using Catch::Approx;

// Seeds that give exactly this many placed chords with the default style, key and settings.
constexpr float kTolerance = 0.001F;
constexpr int kFourChordSeed = 1;
constexpr int kThreeChordSeed = 6;
constexpr std::int64_t kRandomSeed = 7;

struct Fixture
{
    juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor{md::loadBundledBank(), kRandomSeed};
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

    // The component's bounds in the editor's 1040 x 600 space.
    Rect boundsOf(const char* id)
    {
        auto& c = find(id);
        return editor->getLocalArea(&c, c.getLocalBounds());
    }

    void refresh()
    {
        find<md::ui::TopLine>("topLine").refresh();
        find<md::ui::ScreenComponent>("screen").refresh();
    }
};

juce::MouseEvent mouseAt(juce::Component& c, juce::Point<float> position, juce::Point<float> down,
                         bool dragged, juce::ModifierKeys mods = {})
{
    return {juce::Desktop::getInstance().getMainMouseSource(),
            position,
            mods,
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

TEST_CASE("the editor opens at 1040 x 600", "[editor]")
{
    Fixture f;
    CHECK(f.editor->getWidth() == 1040);
    CHECK(f.editor->getHeight() == 600);
}

TEST_CASE("every component sits where the layout table puts it", "[editor]")
{
    Fixture f;
    CHECK(f.boundsOf("screen") == Rect(54, 86, 672, 306));
    CHECK(f.boundsOf("spellReadout") == Rect(574, 38, 84, 26));
    CHECK(f.boundsOf("styleMenu") == Rect(670, 38, 124, 26));
    CHECK(f.boundsOf("keyMenu") == Rect(806, 38, 84, 26));
    CHECK(f.boundsOf("tempoReadout") == Rect(902, 38, 84, 26));
    for (int i = 0; i < 4; ++i)
    {
        CHECK(f.boundsOf(("cell" + std::to_string(i)).c_str()) ==
              Rect(166 + (i * 137), 98, 137, 48));
    }
    const std::array<int, 3> laneY{154, 222, 290};
    for (std::size_t i = 0; i < 3; ++i)
    {
        CHECK(f.boundsOf(("lane" + std::to_string(i)).c_str()) == Rect(62, laneY.at(i), 656, 64));
        CHECK(f.boundsOf(("roll" + std::to_string(i)).c_str()) ==
              Rect(166, laneY.at(i) + 4, 548, 56));
    }
    CHECK(f.boundsOf("bottomLine") == Rect(66, 360, 648, 20));
}

TEST_CASE("each knob's face sits where the layout table puts it", "[editor]")
{
    Fixture f;
    const auto face = [&f](const char* id)
    {
        auto& knob = f.find<md::ui::Knob>(id);
        return f.editor->getLocalArea(&knob, knob.faceBounds());
    };
    CHECK(face("moodKnob") == Rect(781, 144, 56, 56));
    CHECK(face("energyKnob") == Rect(899, 144, 56, 56));
    CHECK(face("grooveKnob") == Rect(781, 290, 56, 56));
    CHECK(face("flavourKnob") == Rect(899, 290, 56, 56));
}

TEST_CASE("the knobs move the processor's parameters", "[editor]")
{
    Fixture f;
    f.find<md::ui::Knob>("moodKnob").setValue(20.0, juce::sendNotificationSync);
    f.find<md::ui::Knob>("grooveKnob").setValue(2.0, juce::sendNotificationSync);
    CHECK(param(f, "mood") == Approx(20.0F));
    CHECK(param(f, "groove") == 2.0F);
    f.processor.params.getParameter("flavour")->setValueNotifyingHost(1.0F);
    CHECK(f.find<md::ui::Knob>("flavourKnob").getValue() == Approx(2.0));
}

TEST_CASE("the header windows show the spell, style, key and tempo", "[editor]")
{
    Fixture f;
    const auto& idea = f.idea(4);
    f.refresh();
    CHECK(f.find<md::ui::ReadoutWindow>("spellReadout").getText() == "0001");
    CHECK(f.find<md::ui::ReadoutWindow>("tempoReadout").getText() == "124.0");
    CHECK(f.find<md::ui::ReadoutWindow>("styleMenu").getText().isNotEmpty());
    CHECK(f.find<md::ui::ReadoutWindow>("keyMenu").getText() == md::ui::keyText(idea.key));
}

TEST_CASE("clicking the key window opens its menu", "[editor]")
{
    Fixture f;
    click(f.find("keyMenu"), {20.0F, 10.0F});
    CHECK(juce::PopupMenu::dismissAllActiveMenus());
}

TEST_CASE("the top line spells the key as min, maj or the mode name", "[editor]")
{
    CHECK(md::ui::keyText({.root = 9, .mode = md::Mode::aeolian}) == "a min");
    CHECK(md::ui::keyText({.root = 3, .mode = md::Mode::ionian}) == "eb maj");
    CHECK(md::ui::keyText({.root = 9, .mode = md::Mode::dorian}) == "a dorian");
    CHECK(md::ui::keyText({.root = 6, .mode = md::Mode::phrygian}) == "f# phrygian");
    CHECK(md::ui::keyText({.root = 0, .mode = md::Mode::mixolydian}) == "c mixolydian");
}

TEST_CASE("the key window shows the idea's key", "[editor]")
{
    Fixture f;
    const auto& idea = f.idea(4);
    f.refresh();
    CHECK(f.find<md::ui::ReadoutWindow>("keyMenu").getText() == md::ui::keyText(idea.key));
    CHECK(f.find<md::ui::ReadoutWindow>("keyMenu").getText() == "a min");
}

TEST_CASE("an idea with three placed chords shows three cells", "[editor]")
{
    Fixture f(kThreeChordSeed);
    f.idea(3);
    f.refresh();
    CHECK(f.find("cell0").isVisible());
    CHECK(f.find("cell1").isVisible());
    CHECK(f.find("cell2").isVisible());
    CHECK_FALSE(f.find("cell3").isVisible());
}

TEST_CASE("a chord cell's accessible title names the chord and its feel", "[editor]")
{
    Fixture f;
    const auto& idea = f.idea(4);
    f.refresh();
    for (std::size_t i = 0; i < 4; ++i)
    {
        const auto& chord = idea.progression.at(i);
        const auto expected = "chord " + std::to_string(i + 1) + ", " +
                              md::chordName(idea.key, chord.symbol) + ", " + chord.feel;
        CHECK(f.find(("cell" + std::to_string(i)).c_str()).getTitle().toStdString() == expected);
    }
    CHECK(f.find("cell0").getTitle().startsWith("chord 1, "));
    CHECK(f.find("cell0").getTitle().endsWith(", home"));
}

TEST_CASE("a lane maps a note's ticks across the roll", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor{md::loadBundledBank(), kRandomSeed};
    md::ui::LaneView lane(md::LaneId::chords);
    lane.setBounds(62, 154, 656, 64);
    md::Lane data{.id = md::LaneId::chords,
                  .pattern = "organ stabs",
                  .events = {{.pitch = 60, .start = 0, .length = 3840, .velocity = 90}}};
    lane.set(data, 8, std::nullopt, false);

    const auto rects = lane.roll().noteRects();
    REQUIRE(rects.size() == 1);
    CHECK(lane.getX() + lane.roll().getX() + rects.at(0).getX() == 166.0F);
    CHECK(rects.at(0).getWidth() == 548.0F / 8.0F);
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
                  Approx(static_cast<float>(e.start) / ticks * 548.0F).margin(kTolerance));
            CHECK(rects.at(n).getY() >= -kTolerance);
            CHECK(rects.at(n).getBottom() <= 56.0F + kTolerance);
            CHECK(rects.at(n).getHeight() > 5.0F);
        }
    }
}

TEST_CASE("the keys sit where the layout table puts them", "[editor]")
{
    Fixture f;
    CHECK(f.boundsOf("playKey") == Rect{450, 464, 36, 24});
    CHECK(f.boundsOf("stopKey") == Rect{498, 464, 36, 24});
    CHECK(f.boundsOf("sprinkleKey") == Rect{818, 454, 84, 44});
}

TEST_CASE("the level and volume knobs' faces sit where the layout table puts them", "[editor]")
{
    Fixture f;
    const auto face = [&f](const char* id)
    {
        auto& knob = f.find<md::ui::Knob>(id);
        return f.editor->getLocalArea(&knob, knob.faceBounds());
    };
    for (int i = 0; i < 3; ++i)
    {
        const auto id = "level" + std::to_string(i);
        CHECK(face(id.c_str()) == Rect(54 + (i * 124) + 42 - 17, 508 - 17, 34, 34));
    }
    CHECK(face("volumeKnob") == Rect(924, 482, 52, 52));
}

TEST_CASE("the first header legend is IDEA", "[editor]")
{
    CHECK(std::string(md::ui::TopLine::kLegends.at(0)) == "IDEA");
}

TEST_CASE("the mute keys sit in the lane columns beside the level knobs", "[editor]")
{
    Fixture f;
    for (int i = 0; i < 3; ++i)
    {
        const auto id = "mute" + std::to_string(i);
        CHECK(f.boundsOf(id.c_str()) == Rect(54 + (i * 124) + 72, 508 - 12, 36, 24));
    }
}

TEST_CASE("the mute keys toggle their lane's parameter and light their LEDs", "[editor]")
{
    Fixture f;
    const std::array<const char*, 3> ids{"chordsMute", "bassMute", "melodyMute"};
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        auto& key = f.find<md::ui::KeyButton>(("mute" + std::to_string(i)).c_str());
        key.setAnimated(false);
        CHECK_FALSE(key.isLit());
        click(key, {18.0F, 12.0F});
        CHECK(param(f, ids.at(i)) == 1.0F);
        CHECK(key.isLit());
        click(key, {18.0F, 12.0F});
        CHECK(param(f, ids.at(i)) == 0.0F);
        CHECK_FALSE(key.isLit());
        CHECK_FALSE(key.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)));
        CHECK(param(f, ids.at(i)) == 0.0F);
        REQUIRE(key.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));
        CHECK(param(f, ids.at(i)) == 1.0F);
        REQUIRE(key.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));
        CHECK(param(f, ids.at(i)) == 0.0F);
    }
}

TEST_CASE("a mute key and its lane row follow the host, and the level knob stays put", "[editor]")
{
    Fixture f;
    f.idea(4);
    auto& key = f.find<md::ui::KeyButton>("mute1");
    key.setAnimated(false);
    f.processor.params.getParameter("bassMute")->setValueNotifyingHost(1.0F);
    f.refresh();
    CHECK(key.isLit());
    CHECK(key.getToggleState());
    CHECK(key.getTitle() == "bass mute");
    CHECK(f.find<md::ui::LaneView>("lane1").muted());
    CHECK(f.find<md::ui::LaneView>("lane1").getAlpha() == Approx(0.35F).margin(kTolerance));
    CHECK_FALSE(f.find<md::ui::LaneView>("lane0").muted());
    CHECK(f.find<md::ui::Knob>("level1").getValue() == Approx(1.0).margin(kTolerance));
    f.processor.params.getParameter("bassMute")->setValueNotifyingHost(0.0F);
    f.refresh();
    CHECK_FALSE(key.isLit());
    CHECK_FALSE(f.find<md::ui::LaneView>("lane1").muted());
}

TEST_CASE("level knobs bind their lanes", "[editor]")
{
    Fixture f;
    const std::array<const char*, 3> ids{"chordsLevel", "bassLevel", "melodyLevel"};
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        auto& knob = f.find<md::ui::Knob>(("level" + std::to_string(i)).c_str());
        CHECK(knob.getValue() == Approx(1.0).margin(kTolerance));
        f.processor.params.getParameter(ids.at(i))->setValueNotifyingHost(0.4F);
        CHECK(knob.getValue() == Approx(0.4).margin(kTolerance));
        knob.setValue(0.2, juce::sendNotificationSync);
        CHECK(param(f, ids.at(i)) == Approx(0.2F).margin(kTolerance));
        knob.mouseDoubleClick(mouseAt(knob, knob.faceBounds().getCentre().toFloat(),
                                      knob.faceBounds().getCentre().toFloat(), false));
        CHECK(param(f, ids.at(i)) == Approx(1.0F).margin(kTolerance));
    }
    f.processor.params.getParameter("bassLevel")->setValueNotifyingHost(0.5F);
    CHECK(f.find<md::ui::Knob>("level1").getValue() == Approx(0.5).margin(kTolerance));
    CHECK(f.find<md::ui::Knob>("level0").getValue() == Approx(1.0).margin(kTolerance));
}

TEST_CASE("volume knob binds volume", "[editor]")
{
    Fixture f;
    auto& knob = f.find<md::ui::Knob>("volumeKnob");
    knob.setValue(0.3, juce::sendNotificationSync);
    CHECK(param(f, "volume") == Approx(0.3F).margin(kTolerance));
    const auto centre = knob.faceBounds().getCentre().toFloat();
    knob.mouseDoubleClick(mouseAt(knob, centre, centre, false));
    CHECK(param(f, "volume") == Approx(0.8F).margin(kTolerance));
}

TEST_CASE("the lanes carry no slider", "[editor]")
{
    Fixture f;
    const auto hasSlider = [](auto&& self, juce::Component& parent) -> bool
    {
        const auto& children = parent.getChildren();
        return std::ranges::any_of(
            children, [&self](juce::Component* child)
            { return dynamic_cast<juce::Slider*>(child) != nullptr || self(self, *child); });
    };
    for (int i = 0; i < 3; ++i)
    {
        CHECK_FALSE(hasSlider(hasSlider, f.find(("lane" + std::to_string(i)).c_str())));
    }
}

TEST_CASE("clicking play then stop toggles the preview", "[editor]")
{
    Fixture f;
    f.processor.prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    click(f.find("playKey"), {18.0F, 12.0F});
    f.processor.processBlock(buffer, midi);
    CHECK(f.processor.previewPlaying());
    click(f.find("stopKey"), {18.0F, 12.0F});
    f.processor.processBlock(buffer, midi);
    CHECK_FALSE(f.processor.previewPlaying());
}

TEST_CASE("clicking sprinkle writes a new idea and the cells follow", "[editor]")
{
    Fixture f;
    const auto before = f.idea(4).seed;
    const auto titles = [&]
    {
        juce::StringArray all;
        for (int i = 0; i < 4; ++i)
        {
            all.add(f.find(("cell" + std::to_string(i)).c_str()).getTitle());
        }
        return all.joinIntoString("|");
    };
    const auto cellsBefore = titles();

    click(f.find("sprinkleKey"), {40.0F, 20.0F});

    REQUIRE(f.processor.idea() != nullptr);
    CHECK(f.processor.idea()->seed != before);
    CHECK(titles() != cellsBefore);
}

TEST_CASE("a pressed key moves down and jumps when animation is off", "[editor]")
{
    Fixture f;
    auto& key = f.find<md::ui::KeyButton>("playKey");
    key.setAnimated(false);
    const auto at = juce::Point<float>(18.0F, 12.0F);
    CHECK(key.pressedAmount() == Approx(0.0F));
    juce::Component& component = key;
    component.mouseDown(mouseAt(component, at, at, false));
    CHECK(key.pressedAmount() == Approx(1.0F));
    component.mouseUp(mouseAt(component, at, at, false));
    CHECK(key.pressedAmount() == Approx(0.0F));
}

TEST_CASE("clicking a cell selects its chord and a lane click selects the lane", "[editor]")
{
    Fixture f;
    f.idea(4);
    auto& screen = f.find<md::ui::ScreenComponent>("screen");
    click(f.find("cell2"), {40.0F, 40.0F});
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
    int cellDrags = 0;
    int laneDrags = 0;
    auto& cell = f.find<md::ui::ChordCell>("cell1");
    auto& lane = f.find<md::ui::LaneView>("lane2");
    cell.setOnDrag([&] { ++cellDrags; });
    lane.setOnDrag([&] { ++laneDrags; });

    const auto down = juce::Point<float>(40.0F, 40.0F);
    const auto drag = [&](juce::Component& c, float distance)
    {
        c.mouseDown(mouseAt(c, down, down, false));
        const auto to = down.translated(distance, 0.0F);
        c.mouseDrag(mouseAt(c, to, down, true));
        c.mouseUp(mouseAt(c, to, down, true));
    };
    drag(cell, 4.0F);
    drag(lane, 4.0F);
    CHECK(cellDrags == 0);
    CHECK(laneDrags == 0);
    CHECK(screen.selection().kind == md::ui::Selection::Kind::lane);

    screen.setSelection({});
    drag(cell, 5.0F);
    drag(lane, 5.0F);
    CHECK(cellDrags == 1);
    CHECK(laneDrags == 1);
    CHECK(screen.selection().kind == md::ui::Selection::Kind::none);
}

TEST_CASE("tab visits the wordmark, header, cells, lanes, knobs, levels, keys, then volume",
          "[editor]")
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
        "wordmark",   "styleMenu",   "keyMenu", "cell0",   "cell1",       "cell2",
        "cell3",      "lane0",       "lane1",   "lane2",   "moodKnob",    "energyKnob",
        "grooveKnob", "flavourKnob", "level0",  "mute0",   "level1",      "mute1",
        "level2",     "mute2",       "playKey", "stopKey", "sprinkleKey", "volumeKnob"};
    CHECK(order == expected);
}

TEST_CASE("enter activates a key, cell and lane once, and space does not", "[editor]")
{
    Fixture f;
    f.idea(4);
    const auto space = juce::KeyPress(juce::KeyPress::spaceKey);
    const auto enter = juce::KeyPress(juce::KeyPress::returnKey);
    const auto fires = [](juce::Component& c, const juce::KeyPress& key, int& count)
    {
        count = 0;
        c.keyPressed(key);
        return count;
    };

    int count = 0;
    auto& key = f.find<md::ui::KeyButton>("stopKey");
    key.onClick = [&] { ++count; };
    CHECK(fires(key, space, count) == 0);
    CHECK(fires(key, enter, count) == 1);

    auto& cell = f.find<md::ui::ChordCell>("cell0");
    cell.onClick = [&] { ++count; };
    CHECK(fires(cell, space, count) == 0);
    CHECK(fires(cell, enter, count) == 1);

    auto& screen = f.find<md::ui::ScreenComponent>("screen");
    auto& lane = f.find<md::ui::LaneView>("lane1");
    screen.setSelection({});
    CHECK_FALSE(lane.keyPressed(space));
    CHECK(screen.selection().kind == md::ui::Selection::Kind::none);
    REQUIRE(lane.keyPressed(enter));
    CHECK(screen.selection().kind == md::ui::Selection::Kind::lane);
    CHECK(screen.selection().index == 1);
}

namespace
{

struct FakeHead : juce::AudioPlayHead
{
    bool playing = true;

    [[nodiscard]] juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm(120.0);
        info.setIsPlaying(playing);
        info.setPpqPosition(0.0);
        return info;
    }
};

// Offers the key to the component and then each parent, as the host's key press does.
bool press(juce::Component& focused, const juce::KeyPress& key)
{
    for (auto* c = &focused; c != nullptr; c = c->getParentComponent())
    {
        if (c->keyPressed(key))
        {
            return true;
        }
    }
    return false;
}

void block(MagicDustProcessor& p)
{
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    p.processBlock(buffer, midi);
}

constexpr auto kFollow = "FOLLOWING YOUR DAW - USE ITS TRANSPORT";

} // namespace

TEST_CASE("space on the editor or any focused control toggles the preview", "[editor]")
{
    Fixture f;
    f.idea(4);
    f.processor.prepareToPlay(48000.0, 512);
    const auto space = juce::KeyPress(juce::KeyPress::spaceKey);
    REQUIRE(f.editor->getWantsKeyboardFocus());
    for (const char* id : {"moodKnob", "stopKey", "cell0", "lane1", "mute0"})
    {
        CAPTURE(id);
        REQUIRE(press(f.find(id), space));
        block(f.processor);
        CHECK(f.processor.previewPlaying());
        REQUIRE(press(f.find(id), space));
        block(f.processor);
        CHECK_FALSE(f.processor.previewPlaying());
    }
    REQUIRE(f.editor->keyPressed(space));
    block(f.processor);
    CHECK(f.processor.previewPlaying());
    CHECK_FALSE(f.editor->keyPressed(juce::KeyPress('a')));
}

TEST_CASE("while the daw plays, stop and space leave it alone and show the follow message",
          "[editor]")
{
    Fixture f;
    f.idea(4);
    f.processor.prepareToPlay(48000.0, 512);
    FakeHead head;
    f.processor.setPlayHead(&head);
    block(f.processor);
    auto& editor = dynamic_cast<MagicDustEditor&>(*f.editor);
    auto& bottom = f.find<md::ui::Readout>("bottomLine");

    click(f.find("stopKey"), {18.0F, 12.0F});
    CHECK(bottom.getText() == kFollow);
    f.processor.tick(juce::Time::getMillisecondCounter() + 1000);
    editor.refresh();
    CHECK(bottom.getText() == kFollow);
    f.processor.tick(juce::Time::getMillisecondCounter() + 2500);
    editor.refresh();
    CHECK(bottom.getText().startsWith("BAR"));

    REQUIRE(editor.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)));
    CHECK(bottom.getText() == kFollow);
    head.playing = false;
    block(f.processor);
    block(f.processor);
    CHECK_FALSE(f.processor.previewPlaying()); // the stop key never started the internal preview
}

TEST_CASE("a drag is never also a click, and a lost mouse-up does not break the next click",
          "[editor]")
{
    Fixture f;
    f.idea(4);
    auto& screen = f.find<md::ui::ScreenComponent>("screen");
    const auto statuses = std::make_shared<std::vector<std::optional<std::string>>>();
    screen.setOnStatus([statuses](const std::optional<std::string>& s) { statuses->push_back(s); });
    int drags = 0;
    auto& cell = f.find<md::ui::ChordCell>("cell1");
    cell.setOnDrag([&] { ++drags; });
    juce::Component& component = cell;

    const auto down = juce::Point<float>(40.0F, 40.0F);
    const auto far = down.translated(10.0F, 0.0F);
    component.mouseDown(mouseAt(component, down, down, false));
    component.mouseDrag(mouseAt(component, far, down, true));
    component.mouseDrag(mouseAt(component, far.translated(5.0F, 0.0F), down, true));
    component.mouseUp(mouseAt(component, down, down, true));
    CHECK(drags == 1);
    CHECK(screen.selection().kind == md::ui::Selection::Kind::none);
    CHECK(statuses->empty());

    // A drag whose mouse-up never arrives.
    component.mouseDown(mouseAt(component, down, down, false));
    component.mouseDrag(mouseAt(component, far, down, true));
    CHECK(drags == 2);
    click(component, down);
    CHECK(screen.selection().kind == md::ui::Selection::Kind::chord);
    CHECK(screen.selection().index == 1);
    component.mouseDown(mouseAt(component, down, down, false));
    component.mouseDrag(mouseAt(component, far, down, true));
    CHECK(drags == 3);
}

TEST_CASE("a failed drag shows the write error and the next successful action clears it",
          "[editor]")
{
    Fixture f;
    f.idea(4);
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("md-editor-" + juce::Uuid().toString());
    REQUIRE(folder.create().wasOk());
    auto& screen = f.find<md::ui::ScreenComponent>("screen");
    screen.setDragFolder(folder.getChildFile("drag"));
    const auto statuses = std::make_shared<std::vector<std::optional<std::string>>>();
    screen.setOnStatus([statuses](const std::optional<std::string>& s) { statuses->push_back(s); });

    auto& lane = f.find<md::ui::LaneView>("lane0");
    juce::Component& component = lane;
    const auto down = juce::Point<float>(40.0F, 20.0F);
    component.mouseDown(mouseAt(component, down, down, false));
    component.mouseDrag(mouseAt(component, down.translated(10.0F, 0.0F), down, true));
    component.mouseUp(mouseAt(component, down, down, true));
    REQUIRE(statuses->size() == 1);
    CHECK(statuses->back() == "couldn't write the midi file");

    click(f.find("cell0"), {40.0F, 40.0F});
    REQUIRE(statuses->size() == 2);
    CHECK_FALSE(statuses->back().has_value());
    folder.deleteRecursively();
}

TEST_CASE("every control has an accessible title", "[editor]")
{
    Fixture f;
    f.idea(4);
    f.refresh();
    CHECK(f.find("cell0").getTitle().startsWith("chord 1, "));
    CHECK(f.find("lane0").getTitle().startsWith("chords lane, "));
    CHECK(f.find("lane1").getTitle().startsWith("bass lane, "));
    CHECK(f.find("lane2").getTitle().startsWith("melody lane, "));
    CHECK(f.find("level0").getTitle() == "chords level");
    CHECK(f.find("level1").getTitle() == "bass level");
    CHECK(f.find("level2").getTitle() == "melody level");
    CHECK(f.find("moodKnob").getTitle() == "mood");
    CHECK(f.find("energyKnob").getTitle() == "energy");
    CHECK(f.find("grooveKnob").getTitle() == "groove");
    CHECK(f.find("flavourKnob").getTitle() == "flavour");
    CHECK(f.find("volumeKnob").getTitle() == "volume");
    CHECK(f.find("spellReadout").getTitle() == "idea, 0001");
    CHECK(f.find("styleMenu").getTitle().startsWith("style, "));
    CHECK(f.find("keyMenu").getTitle() == "key, a min");
    CHECK(f.find("tempoReadout").getTitle() == "tempo, 124.0");
    CHECK(f.find("wordmark").getTitle() == "about magic dust");
    CHECK(f.find("aboutPanel").getTitle() == juce::String("about magic dust, version ") +
                                                 JucePlugin_VersionString + ", mark stent digital");
    CHECK(f.find("playKey").getTitle() == "play the preview");
    CHECK(f.find("stopKey").getTitle() == "stop the preview");
    CHECK(f.find("sprinkleKey").getTitle() == "sprinkle a new idea");
}

TEST_CASE("the bottom line shows an error, then the readout", "[editor]")
{
    Fixture f;
    f.idea(4);
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("md-editor-" + juce::Uuid().toString());
    REQUIRE(folder.create().wasOk());
    auto& screen = f.find<md::ui::ScreenComponent>("screen");
    screen.setDragFolder(folder.getChildFile("drag"));
    auto& bottom = f.find<md::ui::Readout>("bottomLine");

    auto& lane = f.find<md::ui::LaneView>("lane0");
    juce::Component& component = lane;
    const auto down = juce::Point<float>(40.0F, 20.0F);
    component.mouseDown(mouseAt(component, down, down, false));
    component.mouseDrag(mouseAt(component, down.translated(10.0F, 0.0F), down, true));
    component.mouseUp(mouseAt(component, down, down, true));
    f.refresh();
    CHECK(bottom.getText() == "couldn't write the midi file");

    click(f.find("cell0"), {40.0F, 40.0F});
    f.refresh();
    CHECK(bottom.getText().startsWith("STOPPED"));
    folder.deleteRecursively();
}

TEST_CASE("the bottom line reads bar, selection and pins in capitals", "[editor]")
{
    Fixture f;
    f.idea(4);
    f.refresh();
    CHECK(f.find<md::ui::Readout>("bottomLine").getText() ==
          "STOPPED - SELECTED: NOTHING - NOTHING PINNED");
    click(f.find("cell1"), {40.0F, 40.0F});
    CHECK(f.find<md::ui::Readout>("bottomLine").getText() ==
          "STOPPED - SELECTED: CHORD 2 - NOTHING PINNED");
}

TEST_CASE("a long pattern name ellipsizes inside the label column", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor{md::loadBundledBank(), kRandomSeed};
    md::ui::LaneView lane(md::LaneId::chords);
    lane.setBounds(0, 0, 656, 64);
    md::Lane data{.id = md::LaneId::chords, .pattern = std::string(60, 'w'), .events = {}};
    lane.set(data, 8, std::nullopt, false);
    const auto drawn = lane.drawnPattern();
    CHECK(drawn.endsWith("..."));
    CHECK(drawn.length() < 60);
    CHECK(juce::GlyphArrangement::getStringWidth(
              md::ui::font(md::ui::Face::monoRegular, md::tokens::layout::lanePatternSize),
              drawn) <= static_cast<float>(lane.labelBounds().getWidth()));
    CHECK(lane.labelBounds().getWidth() <= 96);

    // Nothing is drawn between the label column and the roll.
    lane.roll().setVisible(false);
    const auto image = lane.createComponentSnapshot(lane.getLocalBounds(), true, 1.0F);
    for (int x = 100; x < 104; ++x)
    {
        for (int y = 0; y < image.getHeight(); ++y)
        {
            REQUIRE(image.getPixelAt(x, y).getAlpha() == 0);
        }
    }
}

TEST_CASE("a long error ellipsizes inside the bottom line", "[editor]")
{
    Fixture f;
    f.idea(4);
    auto& bottom = f.find<md::ui::Readout>("bottomLine");
    bottom.setError(std::string(200, 'w'));
    const auto drawn = bottom.drawnText();
    CHECK(drawn.endsWith("..."));
    CHECK(juce::GlyphArrangement::getStringWidth(
              md::ui::font(md::ui::Face::monoRegular, md::tokens::layout::bottomLineSize), drawn) <=
          static_cast<float>(bottom.getWidth()));
    bottom.setError(std::nullopt);
    CHECK_FALSE(bottom.drawnText().endsWith("..."));
}

TEST_CASE("the tagline ends 16 px clear of the spell window", "[editor]")
{
    CHECK(md::ui::taglineRight() > 400);
    CHECK(md::ui::taglineRight() + 16 <= 574);
}

TEST_CASE("at 0.8x every child stays inside the editor", "[editor]")
{
    Fixture f;
    f.idea(4);
    f.refresh();
    f.editor->setSize(832, 480);
    const auto outside = [&f](auto&& self, juce::Component& parent) -> bool
    {
        for (auto* child : parent.getChildren())
        {
            const auto bounds = f.editor->getLocalArea(child, child->getLocalBounds());
            if (child->isVisible() && !f.editor->getLocalBounds().contains(bounds))
            {
                return true;
            }
            if (self(self, *child))
            {
                return true;
            }
        }
        return false;
    };
    CHECK_FALSE(outside(outside, *f.editor));
}

TEST_CASE("wordmark opens about", "[editor][about]")
{
    Fixture f;
    auto& panel = f.find<md::ui::AboutPanel>("aboutPanel");
    CHECK_FALSE(panel.isVisible());
    CHECK(f.boundsOf("aboutPanel") == Rect{57, 89, 666, 300});
    click(f.find("wordmark"), {10.0F, 10.0F});
    CHECK(panel.isVisible());
    const auto text = md::ui::AboutPanel::lines().joinIntoString("\n");
    CHECK(text.contains(JucePlugin_VersionString));
    CHECK(text.contains("Mark Stent Digital"));
}

TEST_CASE("escape closes the about panel and focus goes to the wordmark", "[editor][about]")
{
    Fixture f;
    auto& panel = f.find<md::ui::AboutPanel>("aboutPanel");
    auto& wordmark = f.find("wordmark");
    f.editor->addToDesktop(0);
    f.editor->setVisible(true);
    panel.open();
    REQUIRE(panel.hasKeyboardFocus(false));
    CHECK_FALSE(panel.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));
    CHECK(panel.isVisible());
    CHECK(panel.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)));
    CHECK_FALSE(panel.isVisible());
    CHECK(wordmark.hasKeyboardFocus(false));
}

TEST_CASE("a click on the bare faceplate closes the about panel", "[editor][about]")
{
    Fixture f;
    auto& panel = f.find<md::ui::AboutPanel>("aboutPanel");
    panel.open();
    click(*f.editor->getChildComponent(0), {10.0F, 580.0F});
    CHECK_FALSE(panel.isVisible());
}

TEST_CASE("a click on the about panel closes it", "[editor][about]")
{
    Fixture f;
    auto& panel = f.find<md::ui::AboutPanel>("aboutPanel");
    panel.open();
    click(panel, {5.0F, 5.0F});
    CHECK_FALSE(panel.isVisible());
}

TEST_CASE("the about link opens the project page", "[editor][about]")
{
    Fixture f;
    auto& link = f.find<juce::HyperlinkButton>("aboutLink");
    CHECK(link.getURL() == juce::URL("https://github.com/markstent/magic_dust_vst"));
    CHECK(link.getButtonText() == "github.com/markstent/magic_dust_vst");
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

// A second instance while a main instance is alive becomes a sender and opens the sender screen.
struct SenderFixture
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<MagicDustProcessor> main = std::make_unique<MagicDustProcessor>();
    MagicDustProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};

    SenderFixture() { REQUIRE(processor.mode() == InstanceMode::sender); }

    md::ui::KeyButton& key(const char* id) const
    {
        auto* found = dynamic_cast<md::ui::KeyButton*>(Fixture::search(*editor, id));
        REQUIRE(found != nullptr);
        return *found;
    }

    md::ui::SenderBody& body() const
    {
        for (auto* child : editor->getChildren())
        {
            if (auto* sender = dynamic_cast<md::ui::SenderBody*>(child))
            {
                return *sender;
            }
        }
        FAIL("no sender screen");
        throw std::logic_error("unreachable");
    }
};

TEST_CASE("a sender opens the sender screen at 520 x 240", "[editor][sender]")
{
    SenderFixture f;
    CHECK(f.editor->getWidth() == 520);
    CHECK(f.editor->getHeight() == 240);
    const auto bounds = [&f](const char* id)
    {
        auto& c = f.key(id);
        return f.editor->getLocalArea(&c, c.getLocalBounds());
    };
    CHECK(bounds("senderKey-chords") == Rect{50, 104, 72, 24});
    CHECK(bounds("senderKey-bass") == Rect{134, 104, 72, 24});
    CHECK(bounds("senderKey-melody") == Rect{218, 104, 72, 24});
    CHECK(bounds("senderMainKey") == Rect{322, 104, 120, 24});
}

TEST_CASE("clicking a lane key picks the lane and lights only its LED", "[editor][sender]")
{
    SenderFixture f;
    f.key("senderKey-melody").setAnimated(false);
    click(f.key("senderKey-melody"), {30.0F, 12.0F});
    CHECK(f.processor.senderLane() == md::LaneId::melody);
    CHECK(f.key("senderKey-melody").isLit());
    CHECK_FALSE(f.key("senderKey-chords").isLit());
    CHECK_FALSE(f.key("senderKey-bass").isLit());
    CHECK(f.body().status() == "sending melody from magic dust");
}

TEST_CASE("the longest sender status fits inside its window whole", "[editor][sender]")
{
    SenderFixture f;
    f.main = nullptr;
    f.body().refresh();
    const auto drawn = f.body().drawnStatus();
    CHECK(drawn == f.body().status().toUpperCase());
    const auto status = md::ui::font(md::ui::Face::monoRegular, 10.0F);
    CHECK(juce::GlyphArrangement::getStringWidth(status, drawn) <= 420.0F - 20.0F);
}

TEST_CASE("the sender lane keys carry their lane colours and the main key no LED",
          "[editor][sender]")
{
    SenderFixture f;
    f.key("senderKey-chords").setAnimated(false);
    f.processor.setSenderLane(md::LaneId::chords);
    f.body().refresh();
    CHECK(f.key("senderKey-chords").isLit());
    CHECK_FALSE(f.key("senderMainKey").isLit());
}

TEST_CASE("the sender status follows whether a main instance exists", "[editor][sender]")
{
    SenderFixture f;
    CHECK(MagicDustProcessor::senderLinked());
    CHECK(f.body().status().startsWith("sending "));
    f.main = nullptr;
    CHECK_FALSE(MagicDustProcessor::senderLinked());
    f.body().refresh();
    CHECK(f.body().status() == "no magic dust found - load magic dust on another track first");
}

TEST_CASE("full magic dust turns the instance main and the editor becomes the main editor",
          "[editor][sender]")
{
    SenderFixture f;
    click(f.key("senderMainKey"), {40.0F, 12.0F});
    CHECK(f.processor.mode() == InstanceMode::main);
    dynamic_cast<MagicDustEditor&>(*f.editor).sync();
    CHECK(f.editor->getWidth() == 1040);
    CHECK(f.editor->getHeight() == 600);
    CHECK(Fixture::search(*f.editor, "playKey") != nullptr);
    CHECK(Fixture::search(*f.editor, "senderMainKey") == nullptr);
}

// The editor's window size, resize limits and aspect ratio for a view of this logical size.
void checkWindow(juce::AudioProcessorEditor& editor, int width, int height)
{
    const auto* limits = editor.getConstrainer();
    CHECK(editor.getWidth() == width);
    CHECK(editor.getHeight() == height);
    CHECK(limits->getMinimumWidth() == static_cast<int>(static_cast<float>(width) * 0.8F));
    CHECK(limits->getMinimumHeight() == static_cast<int>(static_cast<float>(height) * 0.8F));
    CHECK(limits->getMaximumWidth() == static_cast<int>(static_cast<float>(width) * 1.5F));
    CHECK(limits->getMaximumHeight() == static_cast<int>(static_cast<float>(height) * 1.5F));
    CHECK(limits->getFixedAspectRatio() == Approx(static_cast<double>(width) / height));
}

juce::MemoryBlock stateOf(MagicDustProcessor& p)
{
    juce::MemoryBlock state;
    p.getStateInformation(state);
    return state;
}

TEST_CASE("the editor swaps views, size and limits as the mode changes", "[editor][sender]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor main;
    const auto senderState = [&]
    {
        MagicDustProcessor other;
        REQUIRE(other.mode() == InstanceMode::sender);
        return stateOf(other);
    }();
    std::unique_ptr<juce::AudioProcessorEditor> editor{main.createEditor()};
    auto& mine = dynamic_cast<MagicDustEditor&>(*editor);
    checkWindow(*editor, 1040, 600);

    main.setStateInformation(senderState.getData(), static_cast<int>(senderState.getSize()));
    REQUIRE(main.mode() == InstanceMode::sender);
    mine.sync();
    checkWindow(*editor, 520, 240);
    CHECK(Fixture::search(*editor, "senderMainKey") != nullptr);

    main.becomeMain();
    mine.sync();
    checkWindow(*editor, 1040, 600);
    CHECK(Fixture::search(*editor, "playKey") != nullptr);
}

TEST_CASE("a processor already in sender mode opens straight in the sender view",
          "[editor][sender]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor main;
    MagicDustProcessor other;
    REQUIRE(other.mode() == InstanceMode::sender);
    const auto state = stateOf(other);
    MagicDustProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(restored.mode() == InstanceMode::sender);
    std::unique_ptr<juce::AudioProcessorEditor> editor{restored.createEditor()};
    checkWindow(*editor, 520, 240);
    CHECK(Fixture::search(*editor, "senderMainKey") != nullptr);
    CHECK(Fixture::search(*editor, "playKey") == nullptr);
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
    for (const auto& [id, level] : {std::pair{"chordsLevel", 1.0F}, std::pair{"bassLevel", 0.5F},
                                    std::pair{"melodyLevel", 0.2F}})
    {
        f.processor.params.getParameter(id)->setValueNotifyingHost(level);
    }
    f.find<md::ui::KeyButton>("mute1").setAnimated(false);
    f.processor.params.getParameter("bassMute")->setValueNotifyingHost(1.0F);
    f.refresh();
    const juce::File out(dir);
    out.createDirectory();
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    const auto write = [&](const char* name, int width, float scale)
    {
        f.editor->setSize(width, width * 600 / 1040);
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
    f.find<md::ui::AboutPanel>("aboutPanel").setVisible(true);
    write("about-100.png", 1040, 1.0F);
}

// Renders the sender screen to PNGs in $MD_RENDER_DIR; does nothing when the variable is not set.
TEST_CASE("render the sender editor", "[.render]")
{
    const char* dir = std::getenv("MD_RENDER_DIR");
    if (dir == nullptr)
    {
        return;
    }
    SenderFixture f;
    f.key("senderKey-bass").setAnimated(false);
    f.processor.setSenderLane(md::LaneId::bass);
    f.body().refresh();
    const juce::File out(dir);
    out.createDirectory();
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    const auto write = [&](const char* name, float scale)
    {
        const auto image =
            f.editor->createComponentSnapshot(f.editor->getLocalBounds(), true, scale);
        juce::FileOutputStream stream(out.getChildFile(name));
        REQUIRE(stream.openedOk());
        stream.setPosition(0);
        stream.truncate();
        REQUIRE(juce::PNGImageFormat().writeImageToStream(image, stream));
    };
    write("sender-100.png", 1.0F);
    write("sender-200.png", 2.0F);
}

namespace
{

struct KnobFixture
{
    juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor{md::loadBundledBank(), kRandomSeed};

    std::unique_ptr<md::ui::Knob> make(const char* id, std::optional<double> reset = std::nullopt)
    {
        auto knob = std::make_unique<md::ui::Knob>(processor.params, id,
                                                   md::ui::KnobLook{.legend = "TEST",
                                                                    .rangeWords = {},
                                                                    .stepWords = {},
                                                                    .cap = juce::Colours::grey,
                                                                    .diameter = 56.0F,
                                                                    .resetValue = reset});
        knob->setBounds(0, 0, 120, 140);
        return knob;
    }

    float raw(const char* id) const { return processor.params.getRawParameterValue(id)->load(); }

    void set(const char* id, float normalised) const
    {
        processor.params.getParameter(id)->setValueNotifyingHost(normalised);
    }

    // Presses at the knob's centre and drags up by dy px, then releases.
    static void dragUp(md::ui::Knob& knob, float dy, juce::ModifierKeys mods = {})
    {
        const auto down = knob.faceBounds().getCentre().toFloat();
        knob.mouseDown(mouseAt(knob, down, down, false, mods));
        knob.mouseDrag(mouseAt(knob, down.translated(0.0F, -dy), down, true, mods));
        knob.mouseUp(mouseAt(knob, down.translated(0.0F, -dy), down, true, mods));
    }
};

} // namespace

TEST_CASE("Knob follows its parameter", "[knob]")
{
    KnobFixture f;
    auto knob = f.make("mood");
    f.set("mood", 0.75F);
    CHECK(knob->getValue() == Approx(75).margin(0.5));
}

TEST_CASE("Knob writes its parameter", "[knob]")
{
    KnobFixture f;
    auto knob = f.make("mood");
    knob->setValue(20, juce::sendNotificationSync);
    CHECK(f.raw("mood") == Approx(20));
}

TEST_CASE("Stepped knob steps once per 16 px", "[knob]")
{
    KnobFixture f;
    auto knob = f.make("groove");
    KnobFixture::dragUp(*knob, 16.0F);
    CHECK(f.raw("groove") == Approx(1));
    KnobFixture::dragUp(*knob, 32.0F);
    CHECK(f.raw("groove") == Approx(3));
    KnobFixture::dragUp(*knob, 100.0F);
    CHECK(f.raw("groove") == Approx(3));
}

TEST_CASE("Continuous knob covers its range in 160 px", "[knob]")
{
    KnobFixture f;
    auto knob = f.make("mood");
    knob->setValue(0, juce::sendNotificationSync);
    KnobFixture::dragUp(*knob, 80.0F);
    CHECK(knob->getValue() == Approx(50).margin(1));
    knob->setValue(0, juce::sendNotificationSync);
    KnobFixture::dragUp(*knob, 80.0F, juce::ModifierKeys(juce::ModifierKeys::shiftModifier));
    CHECK(knob->getValue() == Approx(12.5).margin(1));
}

TEST_CASE("Pressing shift mid-drag does not make the value jump", "[knob]")
{
    KnobFixture f;
    auto knob = f.make("mood");
    knob->setValue(0, juce::sendNotificationSync);
    const auto down = knob->faceBounds().getCentre().toFloat();
    const auto shift = juce::ModifierKeys(juce::ModifierKeys::shiftModifier);
    const auto at = [&](float dy, juce::ModifierKeys mods, bool dragged = true)
    { return mouseAt(*knob, down.translated(0.0F, -dy), down, dragged, mods); };
    knob->mouseDown(at(0.0F, {}, false));
    knob->mouseDrag(at(40.0F, {}));
    CHECK(knob->getValue() == Approx(25).margin(0.5));
    knob->mouseDrag(at(40.0F, shift));
    CHECK(knob->getValue() == Approx(25).margin(0.5));
    knob->mouseDrag(at(80.0F, shift));
    CHECK(knob->getValue() == Approx(31.25).margin(0.5));
    knob->mouseDrag(at(80.0F, {}));
    CHECK(knob->getValue() == Approx(31.25).margin(0.5));
    knob->mouseUp(at(80.0F, {}));
}

TEST_CASE("Arrow keys step a knob", "[knob]")
{
    KnobFixture f;
    auto knob = f.make("flavour");
    CHECK(f.raw("flavour") == Approx(1));
    knob->keyPressed(juce::KeyPress(juce::KeyPress::upKey));
    CHECK(f.raw("flavour") == Approx(2));
    knob->keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    knob->keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    CHECK(f.raw("flavour") == Approx(0));
}

TEST_CASE("Small smooth wheel deltas add up to one step per notch", "[knob]")
{
    KnobFixture f;
    auto knob = f.make("flavour");
    const auto at = knob->faceBounds().getCentre().toFloat();
    const auto swipe = [&](float delta)
    {
        knob->mouseWheelMove(mouseAt(*knob, at, at, false), {.deltaX = 0.0F,
                                                             .deltaY = delta,
                                                             .isReversed = false,
                                                             .isSmooth = true,
                                                             .isInertial = false});
    };
    for (int i = 0; i < 5; ++i)
    {
        swipe(0.0125F);
    }
    CHECK(f.raw("flavour") == Approx(1));
    for (int i = 0; i < 5; ++i)
    {
        swipe(0.0125F);
    }
    CHECK(f.raw("flavour") == Approx(2));
}

TEST_CASE("Double-click resets", "[knob]")
{
    KnobFixture f;
    auto knob = f.make("chordsLevel", 1.0);
    knob->setValue(0.3, juce::sendNotificationSync);
    const auto at = knob->faceBounds().getCentre().toFloat();
    knob->mouseDoubleClick(mouseAt(*knob, at, at, false));
    CHECK(knob->getValue() == Approx(1.0));
}

TEST_CASE("Knob title and value text", "[knob]")
{
    KnobFixture f;
    auto mood = f.make("mood");
    CHECK(mood->getTitle() == "test");
    CHECK(mood->getTextFromValue(mood->getValue()) == "30");
    auto groove = f.make("groove");
    CHECK(groove->getTextFromValue(groove->getValue()) == "straight");
    auto chords = f.make("chordsLevel");
    CHECK(chords->getTextFromValue(chords->getValue()) == "100%");
}

TEST_CASE("Serif face loads", "[knob]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    CHECK(md::ui::font(md::ui::Face::serifBold, 20.0F).getTypefaceName() == "Libre Baskerville");
}

TEST_CASE("the melody lane label shows PLUCK for a pluck idea", "[editor]")
{
    Fixture f;
    const auto& idea = f.idea(4);
    const auto& melody = idea.lanes.at(2);
    REQUIRE(melody.pattern == "pluck");
    f.refresh();
    CHECK(f.find<md::ui::LaneView>("lane2").drawnPattern() == "PLUCK");
}
