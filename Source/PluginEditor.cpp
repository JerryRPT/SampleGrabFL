#include "PluginProcessor.h"
#include "PluginEditor.h"

SampleGrabAudioProcessorEditor::SampleGrabAudioProcessorEditor (SampleGrabAudioProcessor& p)
    : AudioProcessorEditor (&p), juce::Thread("PythonBackendThread"), audioProcessor (p)
{
    setSize (480, 440);
    setLookAndFeel(&customLookAndFeel);

    titleLabel.setText("SampleGrab", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(32.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffffff));
    addAndMakeVisible(titleLabel);

    urlInput.setTextToShowWhenEmpty(" Paste YouTube URL here...", juce::Colour(0xff777777));
    urlInput.setMultiLine(false);
    urlInput.setReturnKeyStartsNewLine(false);
    urlInput.setFont(juce::FontOptions(16.0f));
    addAndMakeVisible(urlInput);
    
    downloadBtn.onClick = [this]() {
        currentUrl = urlInput.getText();
        if (currentUrl.isNotEmpty()) {
            statusLabel.setText("Status: Downloading and analyzing...", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff9a00));
            // Specifically NOT clearing bpmLabel or keyLabel here so they persist between scans!
            dragZone.setFile("");
            startThread(); 
        }
    };
    addAndMakeVisible(downloadBtn);
    
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setText("Status: Waiting for URL", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    statusLabel.setFont(juce::FontOptions(14.0f));
    addAndMakeVisible(statusLabel);
    
    addAndMakeVisible(statsBox);

    bpmTitleLabel.setJustificationType(juce::Justification::centred);
    bpmTitleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    bpmTitleLabel.setText("TEMPO", juce::dontSendNotification);
    bpmTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff777777));
    addAndMakeVisible(bpmTitleLabel);

    bpmLabel.setJustificationType(juce::Justification::centred);
    bpmLabel.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    bpmLabel.setText("--", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00e5ff)); // bright cyan
    addAndMakeVisible(bpmLabel);
    
    keyTitleLabel.setJustificationType(juce::Justification::centred);
    keyTitleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    keyTitleLabel.setText("MUSICAL KEY", juce::dontSendNotification);
    keyTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff777777));
    addAndMakeVisible(keyTitleLabel);

    keyLabel.setJustificationType(juce::Justification::centred);
    keyLabel.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    keyLabel.setText("--", juce::dontSendNotification);
    keyLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff007f)); // bright magenta
    addAndMakeVisible(keyLabel);
    
    addAndMakeVisible(dragZone);
}

SampleGrabAudioProcessorEditor::~SampleGrabAudioProcessorEditor()
{
    stopThread(2000);
    setLookAndFeel(nullptr);
}

void SampleGrabAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Premium dark gradient background
    juce::ColourGradient bgGrad(juce::Colour(0xff181818), 0, 0, juce::Colour(0xff080808), 0, (float)getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillAll();
}

void SampleGrabAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(24);
    
    titleLabel.setBounds(area.removeFromTop(45));
    area.removeFromTop(10);
    
    auto topRow = area.removeFromTop(40);
    urlInput.setBounds(topRow.removeFromLeft(topRow.getWidth() - 120));
    topRow.removeFromLeft(10);
    downloadBtn.setBounds(topRow);
    
    area.removeFromTop(10);
    statusLabel.setBounds(area.removeFromTop(20));
    
    area.removeFromTop(15);
    
    auto statsArea = area.removeFromTop(80);
    statsBox.setBounds(statsArea); 
    
    auto halfWidth = statsArea.getWidth() / 2;
    auto leftStats = statsArea.removeFromLeft(halfWidth);
    
    bpmTitleLabel.setBounds(leftStats.removeFromTop(30).withTrimmedTop(10));
    bpmLabel.setBounds(leftStats);
    
    keyTitleLabel.setBounds(statsArea.removeFromTop(30).withTrimmedTop(10));
    keyLabel.setBounds(statsArea);
    
    area.removeFromTop(25);
    dragZone.setBounds(area);
}

void SampleGrabAudioProcessorEditor::run()
{
    juce::File programData = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);
    juce::File scriptFile = programData.getChildFile("jerryrpt").getChildFile("SampleGrab").getChildFile("backend.py");
    
    juce::File userMusic = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    juce::File outDir = userMusic.getChildFile("SampleGrab");
    
    if (!outDir.exists())
        outDir.createDirectory();

    juce::StringArray args;
    args.add("python");
    args.add(scriptFile.getFullPathName());
    args.add(currentUrl);
    args.add(outDir.getFullPathName());

    juce::ChildProcess process;
    if (process.start(args))
    {
        juce::String output = process.readAllProcessOutput();
        
        juce::MessageManager::callAsync([this, output]() {
            int start = output.indexOfChar('{');
            int end = output.lastIndexOfChar('}');
            
            if (start >= 0 && end > start) {
                juce::String jsonStr = output.substring(start, end + 1);
                juce::var parsedResult;
                juce::Result res = juce::JSON::parse(jsonStr, parsedResult);
                
                if (res.wasOk() && parsedResult.isObject())
                {
                    auto* obj = parsedResult.getDynamicObject();
                    if (obj->hasProperty("error"))
                    {
                        statusLabel.setText("Error: " + obj->getProperty("error").toString(), juce::dontSendNotification);
                        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff5555));
                    }
                    else if (obj->hasProperty("file") && obj->hasProperty("bpm") && obj->hasProperty("key"))
                    {
                        juce::String file = obj->getProperty("file").toString();
                        juce::String bpm = obj->getProperty("bpm").toString();
                        juce::String key = obj->getProperty("key").toString();
                        
                        statusLabel.setText("Status: Analysis Complete!", juce::dontSendNotification);
                        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff55ff55));
                        bpmLabel.setText(bpm, juce::dontSendNotification);
                        keyLabel.setText(key, juce::dontSendNotification);
                        dragZone.setFile(file);
                    }
                    else
                    {
                        statusLabel.setText("Status: Unknown response format.", juce::dontSendNotification);
                        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff5555));
                    }
                }
                else
                {
                    statusLabel.setText("Status: Failed to parse output.", juce::dontSendNotification);
                    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff5555));
                }
            } else {
                statusLabel.setText("Status: No valid JSON received from Python.", juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff5555));
            }
        });
    }
    else
    {
        juce::MessageManager::callAsync([this]() {
            statusLabel.setText("Status: Failed to start Python process.", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff5555));
        });
    }
}
