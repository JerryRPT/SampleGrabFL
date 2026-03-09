#include "PluginProcessor.h"
#include "PluginEditor.h"

SampleGrabAudioProcessor::SampleGrabAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    formatManager.registerBasicFormats();
}

SampleGrabAudioProcessor::~SampleGrabAudioProcessor() {}

const juce::String SampleGrabAudioProcessor::getName() const { return JucePlugin_Name; }
bool SampleGrabAudioProcessor::acceptsMidi() const { return false; }
bool SampleGrabAudioProcessor::producesMidi() const { return false; }
bool SampleGrabAudioProcessor::isMidiEffect() const { return false; }
double SampleGrabAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int SampleGrabAudioProcessor::getNumPrograms() { return 1; }
int SampleGrabAudioProcessor::getCurrentProgram() { return 0; }
void SampleGrabAudioProcessor::setCurrentProgram (int index) {}
const juce::String SampleGrabAudioProcessor::getProgramName (int index) { return {}; }
void SampleGrabAudioProcessor::changeProgramName (int index, const juce::String& newName) {}

void SampleGrabAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    transportSource.prepareToPlay (samplesPerBlock, sampleRate);
}

void SampleGrabAudioProcessor::releaseResources()
{
    transportSource.releaseResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SampleGrabAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif
    return true;
  #endif
}
#endif

void SampleGrabAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (transportSource.isPlaying())
    {
        juce::AudioSourceChannelInfo info(&buffer, 0, buffer.getNumSamples());
        transportSource.getNextAudioBlock(info);
    }
}

bool SampleGrabAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* SampleGrabAudioProcessor::createEditor() { return new SampleGrabAudioProcessorEditor (*this); }
void SampleGrabAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {}
void SampleGrabAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {}

void SampleGrabAudioProcessor::loadFile(const juce::String& path)
{
    auto file = juce::File(path);
    if (!file.existsAsFile()) return;
    
    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr)
    {
        std::unique_ptr<juce::AudioFormatReaderSource> newSource(new juce::AudioFormatReaderSource(reader, true));
        transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        readerSource.reset(newSource.release());
    }
}

void SampleGrabAudioProcessor::playPreview() { transportSource.setPosition(0.0); transportSource.start(); }
void SampleGrabAudioProcessor::stopPreview() { transportSource.stop(); }
bool SampleGrabAudioProcessor::isPreviewPlaying() const { return transportSource.isPlaying(); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SampleGrabAudioProcessor();
}
