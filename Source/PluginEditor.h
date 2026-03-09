#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1a1a1a));
        setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff151515));
        setColour (juce::TextEditor::textColourId, juce::Colours::white);
        setColour (juce::TextEditor::outlineColourId, juce::Colour (0x00000000));
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0x55ff6f00)); // Subtle orange glow
        setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    }
    
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto cornerSize = 8.0f;
        
        juce::Colour baseColour = backgroundColour;
        if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter (0.1f);
            
        juce::ColourGradient grad(baseColour.brighter(0.05f), 0, 0, baseColour.darker(0.1f), 0, bounds.getHeight(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle (bounds, cornerSize);
        
        g.setColour (juce::Colour(0x66ff6f00)); // Soft Orange border 
        if (button.isMouseOver())
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, 1.5f);
        else
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, 1.0f);
    }

    void drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    {
        auto cornerSize = 8.0f;
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRoundedRectangle (0.5f, 0.5f, width - 1.0f, height - 1.0f, cornerSize, 1.0f);
        
        if (textEditor.hasKeyboardFocus(true) && !textEditor.isReadOnly())
        {
            g.setColour (findColour (juce::TextEditor::focusedOutlineColourId));
            g.drawRoundedRectangle (0.5f, 0.5f, width - 1.0f, height - 1.0f, cornerSize, 2.0f);
        }
    }

    void fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    {
        g.setColour (findColour (juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle(0.0f, 0.0f, (float)width, (float)height, 8.0f);
    }
};

class DragDropSourceComponent : public juce::Component
{
public:
    DragDropSourceComponent() 
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto cornerSize = 12.0f; // Soft round

        if (filePath.isEmpty()) {
            g.setColour(juce::Colour(0xff181818));
            g.fillRoundedRectangle(bounds, cornerSize);
            
            // Subtle dotted or dashed border vibe using thin outline
            g.setColour(juce::Colour(0xff333333));
            g.drawRoundedRectangle(bounds.reduced(1.0f), cornerSize, 2.0f);
            
            g.setColour(juce::Colour(0xff555555));
            g.setFont(juce::FontOptions(18.0f, juce::Font::italic));
            g.drawText("Waiting for audio...", getLocalBounds(), juce::Justification::centred);
        } else {
            // Premium glowing gradient
            juce::ColourGradient gradient(juce::Colour(0xffff5a00), 0, 0, juce::Colour(0xffff9a00), bounds.getWidth(), bounds.getHeight(), false);
            g.setGradientFill(gradient);
            g.fillRoundedRectangle(bounds, cornerSize);
            
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
            g.drawText("DRAG TO PLAYLIST", getLocalBounds(), juce::Justification::centred);
        }
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (filePath.isNotEmpty() && !isDragging)
        {
            isDragging = true;
            juce::StringArray files;
            files.add(filePath);
            juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false, this, [this]() { isDragging = false; });
        }
    }
    
    void mouseUp(const juce::MouseEvent&) override
    {
        isDragging = false;
    }
    
    void setFile(const juce::String& path)
    {
        filePath = path;
        repaint();
    }

private:
    juce::String filePath;
    bool isDragging = false;
};

class StatsBox : public juce::Component
{
public:
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        
        // Inner Glassy container
        juce::ColourGradient grad(juce::Colour(0xff111111), 0, 0, juce::Colour(0xff181818), 0, bounds.getHeight(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, 10.0f);
        
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.5f);
        
        // Separator line
        g.setColour(juce::Colour(0xff333333));
        g.drawLine(bounds.getWidth() / 2.0f, 15.0f, bounds.getWidth() / 2.0f, bounds.getHeight() - 15.0f, 1.5f);
    }
};

class SampleGrabAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Thread
{
public:
    SampleGrabAudioProcessorEditor (SampleGrabAudioProcessor&);
    ~SampleGrabAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    void run() override; 

private:
    SampleGrabAudioProcessor& audioProcessor;
    CustomLookAndFeel customLookAndFeel;

    juce::Label titleLabel;
    juce::TextEditor urlInput;
    juce::TextButton downloadBtn{"DOWNLOAD"};
    juce::Label statusLabel;
    
    StatsBox statsBox;
    juce::Label bpmTitleLabel;
    juce::Label bpmLabel;
    juce::Label keyTitleLabel;
    juce::Label keyLabel;
    
    DragDropSourceComponent dragZone;
    
    juce::String currentUrl;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleGrabAudioProcessorEditor)
};
