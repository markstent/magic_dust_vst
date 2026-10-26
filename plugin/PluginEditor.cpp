#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "ui/Tokens.h"

MagicDustEditor::MagicDustEditor(MagicDustProcessor& owner) : AudioProcessorEditor(owner)
{
    setSize(1040, 740);
}

void MagicDustEditor::paint(juce::Graphics& g) { g.fillAll(md::tokens::body); }
