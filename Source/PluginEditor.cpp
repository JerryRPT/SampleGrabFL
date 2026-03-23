#include "PluginProcessor.h"
#include "PluginEditor.h"

SampleGrabAudioProcessorEditor::SampleGrabAudioProcessorEditor (SampleGrabAudioProcessor& p)
    : AudioProcessorEditor (&p), juce::Thread("PythonBackendThread"), audioProcessor (p), dragZone(p)
{
    setSize (780, 520);
    setLookAndFeel(&customLookAndFeel);

    // Load embedded images
    backgroundImage = juce::ImageCache::getFromMemory(BinaryData::Rectangle_11_png, BinaryData::Rectangle_11_pngSize);
    logoImage = juce::ImageCache::getFromMemory(BinaryData::SampleGrab_png, BinaryData::SampleGrab_pngSize);

    // Title label (hidden if logo image is used, but kept as fallback)
    titleLabel.setText("SampleGrab", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(32.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffffff));
    titleLabel.setVisible(false); // We use the logo image instead
    addChildComponent(titleLabel); // Hidden — logo image is used instead

    urlInput.setTextToShowWhenEmpty(" Paste YouTube URL or Drop Local File...", juce::Colour(0xff8ab4d0));
    urlInput.setMultiLine(false);
    urlInput.setReturnKeyStartsNewLine(false);
    urlInput.setFont(juce::FontOptions(16.0f));
    urlInput.setIndents(8, 10); // Center text vertically in the 40px row
    addAndMakeVisible(urlInput);
    
    downloadBtn.onClick = [this]() {
        if (isThreadRunning()) return;
        currentUrl = urlInput.getText();
        if (currentUrl.isNotEmpty()) {
            statusLabel.setText("Status: Starting...", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff2e6da4));
            // Specifically NOT clearing bpmLabel or keyLabel here so they persist between scans!
            dragZone.setFile("");
            startThread(); 
        }
    };
    addAndMakeVisible(downloadBtn);
    
    openFolderBtn.onClick = []() {
        juce::File userMusic = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
        juce::File outDir = userMusic.getChildFile("SampleGrab");
        if (outDir.exists()) outDir.revealToUser();
    };
    addAndMakeVisible(openFolderBtn);
    
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setText("Status: Waiting for URL", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6a9ab8));
    statusLabel.setFont(juce::FontOptions(14.0f));
    addAndMakeVisible(statusLabel);
    
    addAndMakeVisible(statsBox);

    bpmTitleLabel.setJustificationType(juce::Justification::centred);
    bpmTitleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    bpmTitleLabel.setText("TEMPO", juce::dontSendNotification);
    bpmTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7aadcc));
    addAndMakeVisible(bpmTitleLabel);

    bpmLabel.setJustificationType(juce::Justification::centred);
    bpmLabel.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    bpmLabel.setText("--", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colour(0xff2e6da4)); // deep blue
    addAndMakeVisible(bpmLabel);
    
    keyTitleLabel.setJustificationType(juce::Justification::centred);
    keyTitleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    keyTitleLabel.setText("MUSICAL KEY", juce::dontSendNotification);
    keyTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7aadcc));
    addAndMakeVisible(keyTitleLabel);

    keyLabel.setJustificationType(juce::Justification::centred);
    keyLabel.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    keyLabel.setText("--", juce::dontSendNotification);
    keyLabel.setColour(juce::Label::textColourId, juce::Colour(0xff4a9ec5)); // sky blue
    addAndMakeVisible(keyLabel);

    keyDetailLabel.setJustificationType(juce::Justification::centred);
    keyDetailLabel.setFont(juce::FontOptions(12.5f, juce::Font::plain));
    keyDetailLabel.setText("", juce::dontSendNotification);
    keyDetailLabel.setColour(juce::Label::textColourId, juce::Colour(0xff5a8aa8));
    addAndMakeVisible(keyDetailLabel);
    
    // Restore last state
    if (audioProcessor.lastLoadedFile.isNotEmpty()) {
        bpmLabel.setText(audioProcessor.lastBpm, juce::dontSendNotification);
        keyLabel.setText(audioProcessor.lastKey, juce::dontSendNotification);
        keyDetailLabel.setText(buildKeyDetailText(audioProcessor.lastAlternateKey, audioProcessor.lastTuningDisplay), juce::dontSendNotification);
        dragZone.setFile(audioProcessor.lastLoadedFile);
        statusLabel.setText("Status: Ready", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff3a8a5c));
    }
    
    historyBtn.onClick = [this]() {
        showHistory = !showHistory;
        if (showHistory) {
            loadHistory();
            historyBtn.setButtonText("CLOSE HISTORY");
            historyList.setVisible(true);
            dragZone.setVisible(false);
        } else {
            historyBtn.setButtonText("HISTORY");
            historyList.setVisible(false);
            dragZone.setVisible(true);
        }
        resized();
    };
    addAndMakeVisible(historyBtn);
    
    historyList.setModel(this);
    historyList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0x28ffffff));
    historyList.setRowHeight(48);
    historyList.setVisible(false);
    addChildComponent(historyList);
    
    addAndMakeVisible(dragZone);
    
    startTimerHz(20);
}

SampleGrabAudioProcessorEditor::~SampleGrabAudioProcessorEditor()
{
    stopTimer();
    stopThread(2000);
    audioProcessor.stopPreview();
    setLookAndFeel(nullptr);
}

void SampleGrabAudioProcessorEditor::timerCallback()
{
    dragZone.repaint();
}

void SampleGrabAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Draw the light blue gradient background image
    if (backgroundImage.isValid())
    {
        g.drawImage(backgroundImage, getLocalBounds().toFloat(),
                    juce::RectanglePlacement::stretchToFit);
    }
    else
    {
        // Fallback: programmatic gradient
        juce::ColourGradient bg(juce::Colour(0xffd4e8f7), getWidth() * 0.4f, getHeight() * 0.3f,
                                juce::Colour(0xffb0cfe8), 0.0f, (float)getHeight(), true);
        g.setGradientFill(bg);
        g.fillAll();
    }
    
    // Draw the logo image in the title area
    if (logoImage.isValid())
    {
        auto logoArea = getLocalBounds().reduced(24).removeFromTop(45);
        // Scale logo to fit height while maintaining aspect ratio
        float logoAspect = (float)logoImage.getWidth() / (float)logoImage.getHeight();
        float logoHeight = (float)logoArea.getHeight();
        float logoWidth = logoHeight * logoAspect;
        auto logoBounds = juce::Rectangle<float>(
            logoArea.getCentreX() - logoWidth / 2.0f,
            logoArea.getY() + (logoArea.getHeight() - logoHeight) / 2.0f,
            logoWidth, logoHeight);
        g.drawImage(logoImage, logoBounds,
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }
    
    // Progress bar (liquid glass style)
    if (isDownloading)
    {
        auto bounds = statusLabel.getBounds().toFloat();
        bounds.translate(0, bounds.getHeight() + 2.0f);
        bounds.setHeight(4.0f);
        
        // Track background
        g.setColour(juce::Colour(0x30ffffff));
        g.fillRoundedRectangle(bounds, 2.0f);
        
        if (downloadProgress >= 0.0)
        {
            auto fillBounds = bounds;
            fillBounds.setWidth(bounds.getWidth() * (float)downloadProgress);
            // Blue gradient progress fill
            juce::ColourGradient progressGrad(juce::Colour(0xff4a9ec5), fillBounds.getX(), 0,
                                               juce::Colour(0xff2e6da4), fillBounds.getRight(), 0, false);
            g.setGradientFill(progressGrad);
            g.fillRoundedRectangle(fillBounds, 2.0f);
        }
    }
}

void SampleGrabAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(24);
    
    auto titleArea = area.removeFromTop(45);
    titleLabel.setBounds(titleArea);
    openFolderBtn.setBounds(titleArea.removeFromRight(30).withSizeKeepingCentre(26, 22));
    
    area.removeFromTop(10);
    
    auto topRow = area.removeFromTop(40);
    urlInput.setBounds(topRow.removeFromLeft(topRow.getWidth() - 120));
    topRow.removeFromLeft(10);
    downloadBtn.setBounds(topRow);
    
    area.removeFromTop(10);
    statusLabel.setBounds(area.removeFromTop(20));
    
    area.removeFromTop(15);
    
    auto statsArea = area.removeFromTop(92);
    statsBox.setBounds(statsArea); 
    
    auto halfWidth = statsArea.getWidth() / 2;
    auto leftStats = statsArea.removeFromLeft(halfWidth);
    auto rightStats = statsArea;
    
    bpmTitleLabel.setBounds(leftStats.removeFromTop(24).withTrimmedTop(8));
    bpmLabel.setBounds(leftStats.removeFromTop(36));
    
    keyTitleLabel.setBounds(rightStats.removeFromTop(24).withTrimmedTop(8));
    keyLabel.setBounds(rightStats.removeFromTop(36));
    keyDetailLabel.setBounds(rightStats.withTrimmedBottom(6));
    
    area.removeFromTop(15);
    
    auto historyAreaRow = area.removeFromTop(30);
    historyBtn.setBounds(historyAreaRow.removeFromRight(100));
    area.removeFromTop(10);
    
    if (showHistory) {
        historyList.setBounds(area);
    } else {
        dragZone.setBounds(area);
    }
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
    juce::uint32 processFlags = juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr;
    if (process.start(args, processFlags))
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<SampleGrabAudioProcessorEditor>(this), scriptPath = scriptFile.getFullPathName()]() {
            if (auto* editor = safeThis.getComponent()) {
                editor->statusLabel.setText("Status: Starting Python...", juce::dontSendNotification);
                editor->isDownloading = true;
                editor->downloadProgress = -1.0;
                editor->repaint();
            }
        });

        juce::String buffer;
        while (process.isRunning() && !threadShouldExit())
        {
            char bufferData[1024];
            int bytesRead = process.readProcessOutput(bufferData, sizeof(bufferData));
            if (bytesRead > 0) {
                juce::String chunk(juce::String::fromUTF8(bufferData, bytesRead));
                buffer += chunk;
                int startIdx = 0;
                while ((startIdx = buffer.indexOfChar('{')) >= 0) {
                    int endIdx = buffer.indexOfChar(startIdx, '}');
                        if (endIdx > startIdx) {
                            juce::String jsonStr = buffer.substring(startIdx, endIdx + 1);
                            buffer = buffer.substring(endIdx + 1);
                            
                            juce::var parsedResult;
                            juce::Result res = juce::JSON::parse(jsonStr, parsedResult);
                            if (res.wasOk() && parsedResult.isObject()) {
                                auto* obj = parsedResult.getDynamicObject();
                            if (obj->hasProperty("error")) {
                                juce::String err = obj->getProperty("error").toString();
                                juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<SampleGrabAudioProcessorEditor>(this), err]() {
                                    if (auto* editor = safeThis.getComponent()) {
                                        editor->statusLabel.setText("Error: " + err, juce::dontSendNotification);
                                        editor->statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcc4444));
                                        editor->isDownloading = false;
                                        editor->repaint();
                                    }
                                });
                            } else if (obj->hasProperty("progress")) {
                                double p = static_cast<double>(obj->getProperty("progress"));
                                juce::String st = obj->hasProperty("status") ? obj->getProperty("status").toString() : "";
                                juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<SampleGrabAudioProcessorEditor>(this), p, st]() {
                                    if (auto* editor = safeThis.getComponent()) {
                                        if (st.isNotEmpty()) {
                                            editor->statusLabel.setText("Status: " + st, juce::dontSendNotification);
                                            editor->statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff2e6da4));
                                        }
                                        editor->downloadProgress = p / 100.0;
                                        editor->repaint();
                                    }
                                });
                            } else if (obj->hasProperty("status") && !obj->hasProperty("progress") && !obj->hasProperty("file")) {
                                juce::String st = obj->getProperty("status").toString();
                                juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<SampleGrabAudioProcessorEditor>(this), st]() {
                                    if (auto* editor = safeThis.getComponent()) {
                                        editor->statusLabel.setText("Status: " + st, juce::dontSendNotification);
                                        editor->statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff2e6da4));
                                        editor->downloadProgress = -1.0;
                                        editor->repaint();
                                    }
                                });
                            } else if (obj->hasProperty("file") && obj->hasProperty("bpm")
                                       && (obj->hasProperty("key") || obj->hasProperty("display_key") || obj->hasProperty("primary_key"))) {
                                juce::String file = obj->getProperty("file").toString();
                                juce::String bpm = obj->getProperty("bpm").toString();
                                juce::String primaryKey = obj->hasProperty("primary_key") ? obj->getProperty("primary_key").toString()
                                                                                            : (obj->hasProperty("display_key") ? obj->getProperty("display_key").toString()
                                                                                                                               : obj->getProperty("key").toString());
                                juce::String displayKey = obj->hasProperty("display_key") ? obj->getProperty("display_key").toString()
                                                                                            : (obj->hasProperty("key") ? obj->getProperty("key").toString()
                                                                                                                       : primaryKey);
                                juce::String alternateKey = obj->hasProperty("alternate_key") ? obj->getProperty("alternate_key").toString() : "";
                                juce::String tuningDisplay = obj->hasProperty("tuning_display") ? obj->getProperty("tuning_display").toString() : "";
                                juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<SampleGrabAudioProcessorEditor>(this), file, bpm, primaryKey, alternateKey, displayKey, tuningDisplay]() {
                                    if (auto* editor = safeThis.getComponent()) {
                                        editor->statusLabel.setText("Status: Analysis Complete!", juce::dontSendNotification);
                                        editor->statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff3a8a5c));
                                        editor->applyAnalysisResult(file, bpm, primaryKey, alternateKey, displayKey, tuningDisplay);
                                        editor->isDownloading = false;
                                        editor->downloadProgress = -1.0;
                                        editor->loadHistory();
                                        editor->repaint();
                                    }
                                });
                            }
                        }
                    } else {
                        break;
                    }
                }
            } else if (!process.isRunning()) {
                break;
            }
            juce::Thread::sleep(20);
        }
        
        if (threadShouldExit() && process.isRunning()) {
            process.kill();
        }
        
        juce::uint32 exitCode = process.getExitCode();
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<SampleGrabAudioProcessorEditor>(this), exitCode]() {
            if (auto* editor = safeThis.getComponent()) {
                if (editor->isDownloading) {
                    editor->statusLabel.setText("Error: Python closed unexpectedly (Code " + juce::String(exitCode) + ")", juce::dontSendNotification);
                    editor->statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcc4444));
                    editor->isDownloading = false;
                    editor->repaint();
                }
            }
        });
    }
    else
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<SampleGrabAudioProcessorEditor>(this)]() {
            if (auto* editor = safeThis.getComponent()) {
                editor->statusLabel.setText("Status: Failed to start Python process.", juce::dontSendNotification);
                editor->statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcc4444));
                editor->isDownloading = false;
                editor->repaint();
            }
        });
    }
}

juce::String SampleGrabAudioProcessorEditor::buildKeyDetailText(const juce::String& alternateKey, const juce::String& tuningDisplay)
{
    juce::String detail;

    if (alternateKey.isNotEmpty())
        detail << alternateKey;

    if (tuningDisplay.isNotEmpty()) {
        if (detail.isNotEmpty())
            detail << " | ";
        detail << tuningDisplay;
    }

    return detail;
}

void SampleGrabAudioProcessorEditor::applyAnalysisResult(const juce::String& file,
                                                         const juce::String& bpm,
                                                         const juce::String& primaryKey,
                                                         const juce::String& alternateKey,
                                                         const juce::String& displayKey,
                                                         const juce::String& tuningDisplay)
{
    auto mainKey = displayKey.isNotEmpty() ? displayKey : (primaryKey.isNotEmpty() ? primaryKey : juce::String("--"));

    bpmLabel.setText(bpm, juce::dontSendNotification);
    keyLabel.setText(mainKey, juce::dontSendNotification);
    keyDetailLabel.setText(buildKeyDetailText(alternateKey, tuningDisplay), juce::dontSendNotification);
    dragZone.setFile(file);
    audioProcessor.loadFile(file);
    audioProcessor.lastLoadedFile = file;
    audioProcessor.lastBpm = bpm;
    audioProcessor.lastKey = mainKey;
    audioProcessor.lastAlternateKey = alternateKey;
    audioProcessor.lastTuningDisplay = tuningDisplay;
}

bool SampleGrabAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto file : files) {
        if (file.endsWithIgnoreCase(".wav") || file.endsWithIgnoreCase(".mp3") || file.endsWithIgnoreCase(".flac")) {
            return true;
        }
    }
    return false;
}

void SampleGrabAudioProcessorEditor::filesDropped(const juce::StringArray& files, int x, int y)
{
    if (files.size() > 0) {
        juce::String filePath = files[0];
        if (filePath.endsWithIgnoreCase(".wav") || filePath.endsWithIgnoreCase(".mp3") || filePath.endsWithIgnoreCase(".flac")) {
            urlInput.setText(filePath, juce::dontSendNotification);
            downloadBtn.triggerClick();
        }
    }
}

void SampleGrabAudioProcessorEditor::loadHistory()
{
    juce::File appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    juce::File historyFile = appData.getChildFile("jerryrpt").getChildFile("SampleGrab").getChildFile("history.json");
    
    historyItems.clear();
    
    if (historyFile.existsAsFile()) {
        juce::var parsedResult;
        juce::Result res = juce::JSON::parse(historyFile.loadFileAsString(), parsedResult);
        
        if (res.wasOk() && parsedResult.isArray()) {
            auto* arr = parsedResult.getArray();
            for (auto& item : *arr) {
                if (item.isObject()) {
                    auto* obj = item.getDynamicObject();
                    if (obj->hasProperty("file") && obj->hasProperty("bpm")
                        && (obj->hasProperty("key") || obj->hasProperty("display_key") || obj->hasProperty("primary_key"))) {
                        HistoryItem hi;
                        hi.file = obj->getProperty("file").toString();
                        hi.bpm = obj->getProperty("bpm").toString();
                        hi.primaryKey = obj->hasProperty("primary_key") ? obj->getProperty("primary_key").toString() : "";
                        hi.alternateKey = obj->hasProperty("alternate_key") ? obj->getProperty("alternate_key").toString() : "";
                        hi.tuningDisplay = obj->hasProperty("tuning_display") ? obj->getProperty("tuning_display").toString() : "";
                        hi.displayKey = obj->hasProperty("display_key") ? obj->getProperty("display_key").toString()
                                                                         : (obj->hasProperty("key") ? obj->getProperty("key").toString()
                                                                                                    : hi.primaryKey);
                        if (hi.primaryKey.isEmpty())
                            hi.primaryKey = hi.displayKey;
                        historyItems.add(hi);
                    }
                }
            }
        }
    }
    historyList.updateContent();
}

int SampleGrabAudioProcessorEditor::getNumRows()
{
    return historyItems.size();
}

void SampleGrabAudioProcessorEditor::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= historyItems.size()) return;
    
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    
    if (rowIsSelected) {
        g.setColour(juce::Colour(0x38ffffff));
        g.fillRoundedRectangle(bounds.reduced(2.0f), 5.0f);
    }
    
    auto& item = historyItems.getReference(rowNumber);
    
    // Filename in dark navy
    g.setColour(juce::Colour(0xff1a3a5c));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    juce::String filename = juce::File(item.file).getFileName();
    g.drawText(filename, 10, 2, width - 20, 20, juce::Justification::centredLeft, true);
    
    // Detail text in blue-gray
    g.setColour(juce::Colour(0xff5a8aa8));
    g.setFont(juce::FontOptions(12.0f));
    juce::String detail = item.bpm + " BPM";
    if (item.displayKey.isNotEmpty())
        detail << " | " << item.displayKey;
    if (item.alternateKey.isNotEmpty())
        detail << " | " << item.alternateKey;
    if (item.tuningDisplay.isNotEmpty())
        detail << " | " << item.tuningDisplay;
    g.drawText(detail, 10, 22, width - 20, 16, juce::Justification::centredLeft, true);
    
    // Separator — subtle glass line
    g.setColour(juce::Colour(0x22ffffff));
    g.drawLine(10.0f, (float)height - 1.0f, (float)width - 10.0f, (float)height - 1.0f, 1.0f);
}

void SampleGrabAudioProcessorEditor::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= historyItems.size()) return;
    
    auto& item = historyItems.getReference(row);

    applyAnalysisResult(item.file, item.bpm, item.primaryKey, item.alternateKey, item.displayKey, item.tuningDisplay);
}
