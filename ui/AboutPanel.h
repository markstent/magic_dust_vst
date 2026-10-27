#pragma once

#include <functional>
#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Boilerplate.h"

namespace md::ui
{

// The about panel: cream, over the display, with the name, version, owner, licence and a link to
// the source. Escape or a click anywhere closes it and calls onClose; so does focus moving
// elsewhere.
class AboutPanel : public juce::Component
{
  public:
    AboutPanel();

    // Shows the panel and takes the keyboard focus.
    void open();
    // Hides the panel and calls onClose; does nothing when it is already hidden.
    void dismiss();
    // The panel's lines in order, the link last.
    [[nodiscard]] static juce::StringArray lines();

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void focusOfChildComponentChanged(FocusChangeType cause) override;

    // Called when Escape or a click dismisses the panel, so focus can go back to the wordmark.
    void setOnClose(std::function<void()> onClose) { onClose_ = std::move(onClose); }

  private:
    std::function<void()> onClose_;
    juce::HyperlinkButton link_;

    MD_COMPONENT_BOILERPLATE(AboutPanel)
};

} // namespace md::ui
