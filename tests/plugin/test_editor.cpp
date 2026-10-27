#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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
#include "ui/AuraPad.h"
#include "ui/ChordBubble.h"
#include "ui/Drag.h"
#include "ui/Fonts.h"
#include "ui/KeyButton.h"
#include "ui/Knob.h"
#include "ui/LaneView.h"
#include "ui/ScreenComponent.h"
#include "ui/SenderEditor.h"
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

    // The component's bounds in the editor's 1040 x 740 space.
    Rect boundsOf(const char* id)
    {
        auto& c = find(id);
        return editor->getLocalArea(&c, c.getLocalBounds());
    }

    void refresh() { find<md::ui::ScreenComponent>("screen").refresh(); }
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
    MagicDustProcessor processor{md::loadBundledBank(), kRandomSeed};
    md::ui::LaneView lane(md::LaneId::chords, processor.params);
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
        "keyMenu", "styleMenu", "bubble0", "bubble1", "bubble2", "bubble3",
        "lane0",   "level0",    "lane1",   "level1",  "lane2",   "level2",
        "auraPad", "groove",    "flavour", "playKey", "stopKey", "sprinkleKey"};
    CHECK(order == expected);
}

TEST_CASE("space and enter each activate a key, bubble, lane and field row once", "[editor]")
{
    Fixture f;
    f.idea(4);
    const auto space = juce::KeyPress(juce::KeyPress::spaceKey);
    const auto enter = juce::KeyPress(juce::KeyPress::returnKey);
    const auto fires = [](juce::Component& c, const juce::KeyPress& key, int& count)
    {
        count = 0;
        REQUIRE(c.keyPressed(key));
        return count;
    };

    int count = 0;
    auto& key = f.find<md::ui::KeyButton>("stopKey");
    key.onClick = [&] { ++count; };
    CHECK(fires(key, space, count) == 1);
    CHECK(fires(key, enter, count) == 1);

    auto& bubble = f.find<md::ui::ChordBubble>("bubble0");
    bubble.onClick = [&] { ++count; };
    CHECK(fires(bubble, space, count) == 1);
    CHECK(fires(bubble, enter, count) == 1);

    auto& screen = f.find<md::ui::ScreenComponent>("screen");
    auto& lane = f.find<md::ui::LaneView>("lane1");
    for (const auto& pressed : {space, enter})
    {
        screen.setSelection({});
        REQUIRE(lane.keyPressed(pressed));
        CHECK(screen.selection().kind == md::ui::Selection::Kind::lane);
        CHECK(screen.selection().index == 1);
    }

    auto& field = f.find("flavour");
    for (const auto& pressed : {space, enter})
    {
        const auto before = param(f, "flavour");
        REQUIRE(field.keyPressed(pressed));
        CHECK(param(f, "flavour") != before);
    }
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
    auto& bubble = f.find<md::ui::ChordBubble>("bubble1");
    bubble.setOnDrag([&] { ++drags; });
    juce::Component& component = bubble;

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

    click(f.find("bubble0"), {40.0F, 40.0F});
    REQUIRE(statuses->size() == 2);
    CHECK_FALSE(statuses->back().has_value());
    folder.deleteRecursively();
}

TEST_CASE("every control has an accessible title", "[editor]")
{
    Fixture f;
    f.idea(4);
    f.refresh();
    CHECK(f.find("bubble0").getTitle().startsWith("chord 1, "));
    CHECK(f.find("lane0").getTitle().startsWith("chords lane, "));
    CHECK(f.find("lane1").getTitle().startsWith("bass lane, "));
    CHECK(f.find("lane2").getTitle().startsWith("melody lane, "));
    CHECK(f.find("level0").getTitle() == "chords level");
    CHECK(f.find("level1").getTitle() == "bass level");
    CHECK(f.find("level2").getTitle() == "melody level");
    CHECK(f.find("auraPad").getTitle().startsWith("aura pad, mood "));
    CHECK(f.find("groove").getTitle().startsWith("groove, "));
    CHECK(f.find("flavour").getTitle().startsWith("flavour, "));
    CHECK(f.find("playKey").getTitle() == "play the preview");
    CHECK(f.find("stopKey").getTitle() == "stop the preview");
    CHECK(f.find("sprinkleKey").getTitle() == "sprinkle a new idea");
}

TEST_CASE("each lane has a level slider attached to its parameter", "[editor]")
{
    Fixture f;
    f.idea(4);
    const std::array<const char*, 3> ids{"chordsLevel", "bassLevel", "melodyLevel"};
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        auto& slider = f.find<juce::Slider>(("level" + std::to_string(i)).c_str());
        CHECK(slider.getValue() == Approx(1.0).margin(kTolerance));
        auto* parameter = f.processor.params.getParameter(ids.at(i));
        parameter->setValueNotifyingHost(0.4F);
        CHECK(slider.getValue() == Approx(0.4).margin(kTolerance));
        CHECK(slider.getTextFromValue(0.8) == "80%");
        slider.setValue(0.2, juce::sendNotificationSync);
        CHECK(param(f, ids.at(i)) == Approx(0.2F).margin(kTolerance));
        slider.mouseDoubleClick(mouseAt(slider, {10.0F, 6.0F}, {10.0F, 6.0F}, false));
        CHECK(param(f, ids.at(i)) == Approx(1.0F).margin(kTolerance));
    }
}

TEST_CASE("a level slider sits under the label, ends before the roll and does not select the lane",
          "[editor]")
{
    Fixture f;
    f.idea(4);
    namespace lay = md::tokens::layout;
    auto& lane = f.find("lane1");
    auto& slider = f.find("level1");
    CHECK(slider.getBounds() == Rect(lay::laneLabelPad, lay::laneHeight - 12 - 6,
                                     lay::rollX - lay::laneX - lay::laneLabelPad - 6, 12));
    CHECK(slider.getParentComponent() == &lane);

    // A press on the slider goes to the slider, never to the lane.
    auto& screen = f.find<md::ui::ScreenComponent>("screen");
    const auto centre = slider.getLocalBounds().getCentre().toFloat();
    auto* const hit = lane.getComponentAt(lane.getLocalPoint(&slider, centre.toInt()));
    CHECK(hit == &slider);
    CHECK(screen.selection().kind == md::ui::Selection::Kind::none);
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

    md::ui::LabelKey& key(const char* id) const
    {
        auto* found = dynamic_cast<md::ui::LabelKey*>(Fixture::search(*editor, id));
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
    CHECK(bounds("senderKey-chords") == Rect{27, 100, 82, 82});
    CHECK(bounds("senderKey-bass") == Rect{121, 100, 82, 82});
    CHECK(bounds("senderKey-melody") == Rect{215, 100, 82, 82});
    CHECK(bounds("senderMainKey") == Rect{321, 100, 120, 82});
}

TEST_CASE("clicking a lane key picks the lane and lights only its LED", "[editor][sender]")
{
    SenderFixture f;
    f.key("senderKey-melody").setAnimated(false);
    click(f.key("senderKey-melody"), {40.0F, 40.0F});
    CHECK(f.processor.senderLane() == md::LaneId::melody);
    CHECK(f.key("senderKey-melody").isLit());
    CHECK_FALSE(f.key("senderKey-chords").isLit());
    CHECK_FALSE(f.key("senderKey-bass").isLit());
    CHECK(f.body().status() == "sending melody from magic dust");
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
    click(f.key("senderMainKey"), {40.0F, 40.0F});
    CHECK(f.processor.mode() == InstanceMode::main);
    dynamic_cast<MagicDustEditor&>(*f.editor).sync();
    CHECK(f.editor->getWidth() == 1040);
    CHECK(f.editor->getHeight() == 740);
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
    checkWindow(*editor, 1040, 740);

    main.setStateInformation(senderState.getData(), static_cast<int>(senderState.getSize()));
    REQUIRE(main.mode() == InstanceMode::sender);
    mine.sync();
    checkWindow(*editor, 520, 240);
    CHECK(Fixture::search(*editor, "senderMainKey") != nullptr);

    main.becomeMain();
    mine.sync();
    checkWindow(*editor, 1040, 740);
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
