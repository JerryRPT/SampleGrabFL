#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "BinaryData.h"

// ─── Liquid Glass LookAndFeel ────────────────────────────────────────
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        setDefaultSansSerifTypefaceName ("Karla");
        // Frosted glass button
        setColour (juce::TextButton::buttonColourId, juce::Colour (0x40ffffff));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xff1a3a5c));
        // Frosted glass text editor
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x38ffffff));
        setColour (juce::TextEditor::textColourId, juce::Colour (0xff1a3a5c));
        setColour (juce::TextEditor::outlineColourId, juce::Colour (0x00000000));
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0x667ba7cc));
        setColour (juce::Label::textColourId, juce::Colour (0xff3a6a8a));
    }
    
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto cornerSize = 10.0f;
        
        juce::Colour baseColour = backgroundColour;
        if (shouldDrawButtonAsDown)
            baseColour = juce::Colour (0x55ffffff);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = juce::Colour (0x50ffffff);
            
        // Frosted glass fill
        g.setColour(baseColour);
        g.fillRoundedRectangle (bounds, cornerSize);
        
        // Subtle glass border (top-highlight for depth)
        g.setColour (juce::Colour(0x55ffffff));
        g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, 1.2f);
        
        // Bottom shadow line for 3D glass effect
        g.setColour (juce::Colour(0x18000000));
        g.drawLine(bounds.getX() + cornerSize, bounds.getBottom() - 0.5f,
                   bounds.getRight() - cornerSize, bounds.getBottom() - 0.5f, 0.8f);
    }

    void drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    {
        auto cornerSize = 10.0f;
        g.setColour(juce::Colour(0x44ffffff));
        g.drawRoundedRectangle (0.5f, 0.5f, width - 1.0f, height - 1.0f, cornerSize, 1.2f);
        
        if (textEditor.hasKeyboardFocus(true) && !textEditor.isReadOnly())
        {
            g.setColour (findColour (juce::TextEditor::focusedOutlineColourId));
            g.drawRoundedRectangle (0.5f, 0.5f, width - 1.0f, height - 1.0f, cornerSize, 2.0f);
        }
    }

    void fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    {
        g.setColour (findColour (juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle(0.0f, 0.0f, (float)width, (float)height, 10.0f);
    }
};

// ─── Waveform Drag Zone ──────────────────────────────────────────────
class WaveformDragZone : public juce::Component, public juce::ChangeListener
{
public:
    WaveformDragZone(SampleGrabAudioProcessor& p) 
        : processor(p), thumbnailCache(1), thumbnail(512, p.formatManager, thumbnailCache)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        thumbnail.addChangeListener(this);
    }
    
    ~WaveformDragZone() override
    {
        thumbnail.removeChangeListener(this);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto cornerSize = 14.0f;

        if (filePath.isEmpty() || thumbnail.getNumChannels() == 0) {
            // Empty state — frosted glass panel
            g.setColour(juce::Colour(0x38ffffff));
            g.fillRoundedRectangle(bounds, cornerSize);
            
            g.setColour(juce::Colour(0x55ffffff));
            g.drawRoundedRectangle(bounds.reduced(1.0f), cornerSize, 1.5f);
            
            g.setColour(juce::Colour(0xff6a9ab8));
            g.setFont(juce::FontOptions(18.0f, juce::Font::italic));
            g.drawText("Waiting for audio...", getLocalBounds(), juce::Justification::centred);
        } else {
            // Waveform state — frosted glass with waveform
            g.setColour(juce::Colour(0x30ffffff));
            g.fillRoundedRectangle(bounds, cornerSize);
            
            auto waveBounds = bounds.reduced(10.0f, 10.0f).withTrimmedBottom(20.0f).toNearestInt();
            
            // Waveform bars in deep blue
            int numBars = waveBounds.getWidth() / 4;
            double totalLen = thumbnail.getTotalLength();
            if (totalLen > 0.0 && numBars > 0) {
                double timePerBar = totalLen / numBars;
                for (int i = 0; i < numBars; ++i) {
                    float minValue = 0.0f; float maxValue = 0.0f;
                    thumbnail.getApproximateMinMax(i * timePerBar, (i + 1) * timePerBar, 0, minValue, maxValue);
                    
                    float magnitude = std::max(std::abs(minValue), std::abs(maxValue));
                    float barHeight = std::max(2.0f, magnitude * waveBounds.getHeight());
                    
                    // Gradient feel: bars fade from deep blue to lighter blue
                    float alpha = 0.55f + 0.45f * magnitude;
                    g.setColour(juce::Colour(0xff2e6da4).withAlpha(alpha));
                    g.fillRoundedRectangle(waveBounds.getX() + i * 4, waveBounds.getCentreY() - barHeight / 2.0f, 2.0f, barHeight, 1.0f);
                }
            }
            
            // Glass border
            g.setColour(juce::Colour(0x55ffffff));
            g.drawRoundedRectangle(bounds.reduced(1.0f), cornerSize, 1.5f);
            
            // Play button — frosted circle-ish container
            auto playBtnBounds = bounds.removeFromLeft(40.0f).withTrimmedLeft(5.0f).withTrimmedRight(5.0f).reduced(5.0f);
            g.setColour(juce::Colour(0x44ffffff));
            g.fillRoundedRectangle(playBtnBounds, 6.0f);
            g.setColour(juce::Colour(0x55ffffff));
            g.drawRoundedRectangle(playBtnBounds, 6.0f, 1.0f);
            
            if (processor.isPreviewPlaying()) {
                g.setColour(juce::Colour(0xff1a3a5c));
                auto iconBounds = playBtnBounds.withSizeKeepingCentre(12, 12);
                g.fillRect(iconBounds.getX(), iconBounds.getY(), 4.0f, 12.0f);
                g.fillRect(iconBounds.getRight() - 4.0f, iconBounds.getY(), 4.0f, 12.0f);
            } else {
                g.setColour(juce::Colour(0xff1a3a5c));
                juce::Path playTriangle;
                auto iconBounds = playBtnBounds.withSizeKeepingCentre(12, 14);
                playTriangle.addTriangle(iconBounds.getX(), iconBounds.getY(), 
                                         iconBounds.getRight(), iconBounds.getCentreY(), 
                                         iconBounds.getX(), iconBounds.getBottom());
                g.fillPath(playTriangle);
            }
            
            // Instructional text
            g.setColour(juce::Colour(0xff1a3a5c));
            g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
            g.drawText("DRAG WAVEFORM TO EXPORT", bounds.reduced(10).toNearestInt(), juce::Justification::bottomRight);
        }
    }
    
    void changeListenerCallback(juce::ChangeBroadcaster* source) override
    {
        if (source == &thumbnail)
            repaint();
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (filePath.isNotEmpty() && !isDragging)
        {
            auto playBtnBounds = getLocalBounds().toFloat().removeFromLeft(40.0f);
            if (!playBtnBounds.contains(e.getPosition().toFloat())) {
                isDragging = true;
                juce::StringArray files;
                files.add(filePath);
                juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false, this, [this]() { isDragging = false; repaint(); });
                repaint();
            }
        }
    }
    
    void mouseUp(const juce::MouseEvent& e) override
    {
        isDragging = false;
        if (filePath.isNotEmpty() && !e.mouseWasDraggedSinceMouseDown())
        {
            auto playBtnBounds = getLocalBounds().toFloat().removeFromLeft(40.0f);
            if (playBtnBounds.contains(e.getPosition().toFloat())) {
                if (processor.isPreviewPlaying()) {
                    processor.stopPreview();
                } else {
                    processor.playPreview();
                }
            }
        }
        repaint();
    }
    
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    
    void setFile(const juce::String& path)
    {
        filePath = path;
        
        if (path.isNotEmpty()) {
            thumbnail.setSource(new juce::FileInputSource(juce::File(path)));
        } else {
            thumbnail.clear();
        }
        
        repaint();
    }

private:
    SampleGrabAudioProcessor& processor;
    juce::String filePath;
    bool isDragging = false;
    
    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;
};

// ─── Stats Box (Frosted Glass Panel) ─────────────────────────────────
class StatsBox : public juce::Component
{
public:
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        
        // Frosted glass panel
        g.setColour(juce::Colour(0x38ffffff));
        g.fillRoundedRectangle(bounds, 12.0f);
        
        // Glass highlight border
        g.setColour(juce::Colour(0x55ffffff));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.5f);
        
        // Separator line (subtle blue-ish)
        g.setColour(juce::Colour(0x33a0c4e0));
        g.drawLine(bounds.getWidth() / 2.0f, 15.0f, bounds.getWidth() / 2.0f, bounds.getHeight() - 15.0f, 1.5f);
    }
};

// ─── Folder Button ───────────────────────────────────────────────────
class FolderButton : public juce::Button
{
public:
    FolderButton() : juce::Button("FolderBtn") { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        
        juce::Colour colour = juce::Colour(0xff7aadcc);
        if (shouldDrawButtonAsHighlighted) colour = juce::Colour(0xff1a3a5c);
        if (shouldDrawButtonAsDown) {
            colour = juce::Colour(0xff2e6da4);
            bounds.translate(0.0f, 1.0f);
        }
        
        g.setColour(colour);
        
        juce::Path p;
        p.startNewSubPath(0.0f, bounds.getHeight() * 0.2f);
        p.lineTo(bounds.getWidth() * 0.4f, bounds.getHeight() * 0.2f);
        p.lineTo(bounds.getWidth() * 0.5f, bounds.getHeight() * 0.35f);
        p.lineTo(bounds.getWidth(), bounds.getHeight() * 0.35f);
        p.lineTo(bounds.getWidth(), bounds.getHeight());
        p.lineTo(0.0f, bounds.getHeight());
        p.closeSubPath();
        
        g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    }
};

// ─── Main Editor ─────────────────────────────────────────────────────
class SampleGrabAudioProcessorEditor : public juce::AudioProcessorEditor, 
                                       private juce::Thread, 
                                       private juce::Timer,
                                       public juce::FileDragAndDropTarget,
                                       public juce::ListBoxModel
{
public:
    SampleGrabAudioProcessorEditor (SampleGrabAudioProcessor&);
    ~SampleGrabAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    void run() override; 
    void timerCallback() override; 
    
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override; 

private:
    SampleGrabAudioProcessor& audioProcessor;
    CustomLookAndFeel customLookAndFeel;

    // Background + Logo images (loaded from BinaryData)
    juce::Image backgroundImage;
    juce::Image logoImage;

    juce::Label titleLabel;
    juce::TextEditor urlInput;
    juce::TextButton downloadBtn{"DOWNLOAD"};
    juce::TextButton historyBtn{"HISTORY"};
    FolderButton openFolderBtn;
    juce::Label statusLabel;
    
    double downloadProgress = -1.0;
    bool isDownloading = false;
    
    StatsBox statsBox;
    juce::Label bpmTitleLabel;
    juce::Label bpmLabel;
    juce::Label keyTitleLabel;
    juce::Label keyLabel;
    juce::Label keyDetailLabel;
    
    WaveformDragZone dragZone;
    
    juce::String currentUrl;
    
    // History components
    struct HistoryItem {
        juce::String file;
        juce::String bpm;
        juce::String primaryKey;
        juce::String alternateKey;
        juce::String tuningDisplay;
        juce::String displayKey;
    };
    juce::Array<HistoryItem> historyItems;
    juce::ListBox historyList;
    bool showHistory = false;
    void loadHistory();
    void applyAnalysisResult(const juce::String& file,
                             const juce::String& bpm,
                             const juce::String& primaryKey,
                             const juce::String& alternateKey,
                             const juce::String& displayKey,
                             const juce::String& tuningDisplay);
    static juce::String buildKeyDetailText(const juce::String& alternateKey, const juce::String& tuningDisplay);
    
    // ListBoxModel overrides
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleGrabAudioProcessorEditor)
};
