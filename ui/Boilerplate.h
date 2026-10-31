#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Ends a component class: fixed in place (no copy or move), a defaulted destructor, and JUCE's
// leak detector.
// NOLINTBEGIN(cppcoreguidelines-macro-usage,bugprone-macro-parentheses)
#define MD_COMPONENT_BOILERPLATE(Class)                                                            \
  public:                                                                                          \
    Class(const Class&) = delete;                                                                  \
    Class& operator=(const Class&) = delete;                                                       \
    Class(Class&&) = delete;                                                                       \
    Class& operator=(Class&&) = delete;                                                            \
    ~Class() override = default;                                                                   \
    JUCE_LEAK_DETECTOR(Class)
// NOLINTEND(cppcoreguidelines-macro-usage,bugprone-macro-parentheses)
