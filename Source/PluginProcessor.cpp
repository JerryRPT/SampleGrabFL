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

void SampleGrabAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {}
void SampleGrabAudioProcessor::releaseResources() {}

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
}

bool SampleGrabAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* SampleGrabAudioProcessor::createEditor() { return new SampleGrabAudioProcessorEditor (*this); }
void SampleGrabAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {}
void SampleGrabAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SampleGrabAudioProcessor();
}
