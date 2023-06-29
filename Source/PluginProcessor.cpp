/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SquirrelerAudioProcessor::SquirrelerAudioProcessor()
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

SquirrelerAudioProcessor::~SquirrelerAudioProcessor()
{
}

//==============================================================================
const juce::String SquirrelerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SquirrelerAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SquirrelerAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SquirrelerAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SquirrelerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SquirrelerAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SquirrelerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SquirrelerAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SquirrelerAudioProcessor::getProgramName (int index)
{
    return {};
}

void SquirrelerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SquirrelerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = samplesPerBlock;
}

void SquirrelerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SquirrelerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SquirrelerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    juce::dsp::AudioBlock<float> block(buffer);

    juce::dsp::AudioBlock<float> leftBlock = block.getSingleChannelBlock(0);
    juce::dsp::AudioBlock<float> rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    
    ChainSettings chainSettings = getChainSettings(apvts);
    Filter* filters = GetPeakFilters(chainSettings);

    const int numberOfFilters = frequencyRange / chainSettings.cycleLength;
    const Filter* lastFilter = &filters[numberOfFilters];
    for (Filter* filter = filters; filter != lastFilter; ++filter) {
        filter->prepare(spec);
        filter->process(leftContext);
        filter->process(rightContext);
    }
}

//==============================================================================
bool SquirrelerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SquirrelerAudioProcessor::createEditor()
{
    //return new SquirrelerAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SquirrelerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SquirrelerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

juce::AudioProcessorValueTreeState::ParameterLayout SquirrelerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "CycleLength", 
        "CycleLength", 
        juce::NormalisableRange<float>(1000.0f, 10000.0f, 0.01f, 1.0f),
        3000.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "CycleHeight", 
        "CycleHeight",
        juce::NormalisableRange<float>(0.0f, 3.0f, 0.1f, 1.0f),
        0.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Phase",
        "Phase",
        juce::NormalisableRange<float>(-1000.0f, 1000.0f, 0.5f, 1.0f),
        0.0f
    ));

    return layout;
}

juce::dsp::IIR::Filter<float>* SquirrelerAudioProcessor::GetPeakFilters(const ChainSettings& chainSettings)
{
    const int maxAmountOfLoops = 2000;
    const int numberOfFilters = frequencyRange / chainSettings.cycleLength;
    const int amountToIncrement = frequencyRange / numberOfFilters;

    Filter* filters = new Filter[numberOfFilters];
    const Filter* lastFilter = &filters[numberOfFilters];

    int currentloop = 1;
    float currentFrequency = minFrequency + amountToIncrement + chainSettings.phase;
    for (Filter* filter = filters; filter != lastFilter; ++filter) {
        Filter::CoefficientsPtr peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter
        (
            getSampleRate(),
            currentFrequency,
            1 / chainSettings.cycleLength,
            juce::Decibels::decibelsToGain(chainSettings.cycleHeight)
        );

        *filter->coefficients = *peakCoefficients;
        currentFrequency += amountToIncrement;
        if (++currentloop > maxAmountOfLoops) break;
    }

    return filters;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SquirrelerAudioProcessor();
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings {};

    settings.cycleHeight = apvts.getRawParameterValue("CycleHeight")->load();
    settings.cycleLength = apvts.getRawParameterValue("CycleLength")->load();
    settings.phase = apvts.getRawParameterValue("Phase")->load();

    return settings;
}
