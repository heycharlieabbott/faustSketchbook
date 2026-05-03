/************************************************************************
 FAUST Architecture File
 Copyright (C) 2016 GRAME, Centre National de Creation Musicale
 ---------------------------------------------------------------------
 This Architecture section is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 3 of
 the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; If not, see <http://www.gnu.org/licenses/>.

 EXCEPTION : As a special exception, you may create a larger work
 that contains this FAUST architecture section and distribute
 that work under terms of your choice, so long as this FAUST
 architecture section is not modified.

 ************************************************************************
 ************************************************************************/

#include <algorithm>
#include <assert.h>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#if JUCE_WINDOWS
#define JUCE_CORE_INCLUDE_NATIVE_HEADERS 1
#endif

#include <JuceHeader.h>

#include "faust/gui/MapUI.h"
#include "faust/dsp/dsp-adapter.h"
#include "faust/gui/MidiUI.h"
#include "faust/dsp/poly-dsp.h"
#ifndef PLUGIN_MAGIC
#include "faust/gui/JuceGUI.h"
#endif
#include "faust/gui/JuceParameterUI.h"
#include "faust/gui/JuceStateUI.h"

// Always included otherwise -i mode sometimes fails...
#include "faust/gui/DecoratorUI.h"

#if defined(SOUNDFILE)
#include "faust/gui/SoundUI.h"

namespace granularPolySynthSoundFileDetail
{
inline juce::File getSampleCacheDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("granularPolysynth")
        .getChildFile("Samples");
}

/** External samples are copied here so presets store a path the plugin can always open (sandbox-friendly). */
inline juce::File resolveFileForGranularSampler(const juce::File &source, const juce::String &bundleResourcesPath)
{
    if (!source.existsAsFile())
        return {};

    if (bundleResourcesPath.isNotEmpty()
        && source.getFullPathName().startsWithIgnoreCase(bundleResourcesPath))
        return source;

    auto dir = getSampleCacheDirectory();
    if (!dir.createDirectory())
        return source;

    const auto hash = juce::String::toHexString(juce::String(source.getFullPathName()).hashCode());
    auto dest = dir.getChildFile(hash + "_" + source.getFileName());

    if (dest.existsAsFile() && dest.getSize() == source.getSize())
        return dest;

    if (!source.copyFileTo(dest))
        return source;

    return dest;
}
} // namespace granularPolySynthSoundFileDetail

class SoundUIBrowse : public SoundUI
{
public:
    using SoundUI::SoundUI;

    void addSoundfile(const char *label, const char *url, Soundfile **sf_zone) override
    {
        const juce::ScopedLock sl(fZoneLock);
        SoundUI::addSoundfile(label, url, sf_zone);
        fZones.push_back(sf_zone);
        if (!fLastLoaded.existsAsFile() && url != nullptr && *url != '\0')
        {
            std::vector<std::string> file_name_list = {std::string(url)};
            std::vector<std::string> path_name_list = fSoundReader->checkFiles(fSoundfileDir, file_name_list);
            if (!path_name_list.empty() && path_name_list[0] != "__empty_sound__")
            {
                fLastLoaded = juce::File(path_name_list[0]);
            }
        }
    }

    void setOnSampleChanged(std::function<void()> fn) { fOnSampleChanged = std::move(fn); }

    juce::String getSampleDisplayName() const
    {
        if (fLastLoaded.existsAsFile())
        {
            return fLastLoaded.getFileName();
        }
        return "Bundle sample";
    }

    juce::File getLastLoadedFile() const { return fLastLoaded; }

    void reloadFromAbsoluteFile(const juce::File &file)
    {
        std::vector<Soundfile **> zonesCopy;
        {
            const juce::ScopedLock sl(fZoneLock);
            if (fZones.empty())
            {
                return;
            }
            zonesCopy = fZones;
        }
        const juce::String bundleRoot =
            fSoundfileDir.empty() ? juce::String() : juce::String(fSoundfileDir[0]);
        const juce::File fileToLoad =
            granularPolySynthSoundFileDetail::resolveFileForGranularSampler(file, bundleRoot);
        if (!fileToLoad.existsAsFile())
        {
            return;
        }
        // Do not use checkFiles() here: it can map real paths to "__empty_sound__" while we still
        // pointed fLastLoaded at the user's file. Load the verified path directly.
        std::vector<std::string> path_name_list = {fileToLoad.getFullPathName().toStdString()};
        Soundfile *sf = fSoundReader->createSoundfile(path_name_list, MAX_CHAN, fIsDouble);
        if (!sf)
        {
            return;
        }
        for (Soundfile **zp : zonesCopy)
        {
            *zp = sf;
        }
        fLastLoaded = fileToLoad;
        auto cb = fOnSampleChanged;
        if (cb)
        {
            cb();
        }
    }

private:
    std::vector<Soundfile **> fZones;
    juce::File fLastLoaded;
    std::function<void()> fOnSampleChanged;
    juce::CriticalSection fZoneLock;
};

class SampleWaveformStrip : public juce::Component
{
public:
    void clear()
    {
        fPeaks.clear();
        fCaption.clear();
        repaint();
    }

    void rebuildFromFile(const juce::File &file, juce::String caption)
    {
        fPeaks.clear();
        fCaption = std::move(caption);
        if (!file.existsAsFile())
        {
            repaint();
            return;
        }

        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (!reader)
        {
            repaint();
            return;
        }

        const auto len = reader->lengthInSamples;
        if (len <= 0)
        {
            repaint();
            return;
        }

        const int buckets = juce::jmin(4096, juce::jmax(256, (int)(len / 64)));
        fPeaks.assign((size_t)buckets, {0.f, 0.f});
        for (int b = 0; b < buckets; ++b)
        {
            const auto start = (int64_t)b * len / buckets;
            auto num = ((int64_t)(b + 1) * len / buckets) - start;
            if (num <= 0)
            {
                num = 1;
            }
            juce::Range<float> level[1];
            reader->readMaxLevels(start, num, level, 1);
            fPeaks[(size_t)b] = {level[0].getStart(), level[0].getEnd()};
        }
        repaint();
    }

    void paint(juce::Graphics &g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff2a2a33));
        g.fillRoundedRectangle(r, 5.f);
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawRoundedRectangle(r, 5.f, 1.f);

        const int w = getWidth();
        const int h = getHeight();
        if (w <= 2 || fPeaks.empty())
        {
            g.setColour(juce::Colours::grey);
            g.setFont(12.f);
            g.drawFittedText("Drop audio here or use Load…",
                             getLocalBounds().reduced(8), juce::Justification::centred, 3);
            return;
        }

        const float mid = h * 0.48f;
        const float amp = h * 0.38f;
        const int n = (int)fPeaks.size();
        g.setColour(juce::Colour(0xff6ad4e8));
        const int denom = juce::jmax(1, w - 1);
        for (int x = 0; x < w; ++x)
        {
            int b = x * (n - 1) / denom;
            b = juce::jlimit(0, n - 1, b);
            const float lo = fPeaks[(size_t)b].first;
            const float hi = fPeaks[(size_t)b].second;
            const float y1 = mid - hi * amp;
            const float y2 = mid - lo * amp;
            g.drawLine((float)x, y1, (float)x, y2, 1.2f);
        }

        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont((float)juce::jmin(12, juce::jmax(9, h / 6)));
        g.drawText(fCaption, getLocalBounds().removeFromBottom(16).reduced(8, 0),
                   juce::Justification::centredLeft, true);
    }

private:
    std::vector<std::pair<float, float>> fPeaks;
    juce::String fCaption;
};
#endif

#if defined(OSCCTRL)
#include "faust/gui/JuceOSCUI.h"
#endif

#if defined(MIDICTRL)
#include "faust/midi/juce-midi.h"
#include "faust/dsp/timed-dsp.h"
#endif

#if defined(POLY2)
#include "faust/dsp/dsp.h"
#include "faust/dsp/dsp-combiner.h"
#include "effect.h"
#endif

// we require macro declarations
#define FAUST_UIMACROS

// but we will ignore most of them
#define FAUST_ADDBUTTON(l, f)
#define FAUST_ADDCHECKBOX(l, f)
#define FAUST_ADDSOUNDFILE(l, s)
#define FAUST_ADDVERTICALSLIDER(l, f, i, a, b, s)
#define FAUST_ADDHORIZONTALSLIDER(l, f, i, a, b, s)
#define FAUST_ADDNUMENTRY(l, f, i, a, b, s)
#define FAUST_ADDVERTICALBARGRAPH(l, f, a, b)
#define FAUST_ADDHORIZONTALBARGRAPH(l, f, a, b)

<< includeIntrinsic >>

    << includeclass >>

#if defined(JUCE_POLY)

    struct FaustSound : public juce::SynthesiserSound
{

    bool appliesToNote(int /*midiNoteNumber*/) override { return true; }
    bool appliesToChannel(int /*midiChannel*/) override { return true; }
};

// An hybrid JUCE and Faust voice

class FaustVoice : public juce::SynthesiserVoice, public dsp_voice
{

private:
    std::unique_ptr<juce::AudioBuffer<FAUSTFLOAT>> fBuffer;

public:
    FaustVoice(dsp *dsp) : dsp_voice(dsp)
    {
        // Allocate buffer for mixing
        fBuffer = std::make_unique<juce::AudioBuffer<FAUSTFLOAT>>(dsp->getNumOutputs(), 8192);
        fDSP->init(juce::SynthesiserVoice::getSampleRate());
    }

    bool canPlaySound(juce::SynthesiserSound *sound) override
    {
        return dynamic_cast<FaustSound *>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber,
                   float velocity,
                   juce::SynthesiserSound *s,
                   int currentPitchWheelPosition) override
    {
        // Note is triggered
        keyOn(midiNoteNumber, velocity);
    }

    void stopNote(float velocity, bool allowTailOff) override
    {
        keyOff(!allowTailOff);
    }

    void pitchWheelMoved(int newPitchWheelValue) override
    {
        // not implemented for now
    }

    void controllerMoved(int controllerNumber, int newControllerValue) override
    {
        // not implemented for now
    }

    void renderNextBlock(juce::AudioBuffer<FAUSTFLOAT> &outputBuffer,
                         int startSample,
                         int numSamples) override
    {
        // Only plays when the voice is active
        if (isVoiceActive())
        {

            // Play the voice
            compute(numSamples, nullptr, (FAUSTFLOAT **)fBuffer->getArrayOfWritePointers());

            // Mix it in outputs
            for (int i = 0; i < fDSP->getNumOutputs(); i++)
            {
                outputBuffer.addFrom(i, startSample, *fBuffer, i, 0, numSamples);
            }
        }
    }
};

// Decorates the JUCE Synthesiser and adds Faust polyphonic code for GUI handling

class FaustSynthesiser : public juce::Synthesiser, public dsp_voice_group
{

private:
    juce::Synthesiser fSynth;

    static void panic(float val, void *arg)
    {
        static_cast<FaustSynthesiser *>(arg)->allNotesOff(0, false); // 0 stops all voices
    }

public:
    FaustSynthesiser() : dsp_voice_group(panic, this, true, true)
    {
        setNoteStealingEnabled(true);
    }

    virtual ~FaustSynthesiser()
    {
        // Voices will be deallocated by fSynth
        dsp_voice_group::clearVoices();
    }

    void addVoice(FaustVoice *voice)
    {
        fSynth.addVoice(voice);
        dsp_voice_group::addVoice(voice);
    }

    void addSound(juce::SynthesiserSound *sound)
    {
        fSynth.addSound(sound);
    }

    void allNotesOff(int midiChannel, bool allowTailOff)
    {
        fSynth.allNotesOff(midiChannel, allowTailOff);
    }

    void setCurrentPlaybackSampleRate(double newRate)
    {
        fSynth.setCurrentPlaybackSampleRate(newRate);
    }

    void renderNextBlock(juce::AudioBuffer<float> &outputAudio,
                         const juce::MidiBuffer &inputMidi,
                         int startSample,
                         int numSamples)
    {
        fSynth.renderNextBlock(outputAudio, inputMidi, startSample, numSamples);
    }

    void renderNextBlock(juce::AudioBuffer<double> &outputAudio,
                         const juce::MidiBuffer &inputMidi,
                         int startSample,
                         int numSamples)
    {
        fSynth.renderNextBlock(outputAudio, inputMidi, startSample, numSamples);
    }
};

#endif

// Using the PluginGuiMagic project (https://foleysfinest.com/developer/pluginguimagic/)

#if defined(PLUGIN_MAGIC)

class FaustPlugInAudioProcessor : public foleys::MagicProcessor, private juce::Timer
{

public:
    FaustPlugInAudioProcessor();
    virtual ~FaustPlugInAudioProcessor() {}

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override
    {
        jassert(!isUsingDoublePrecision());
        process(buffer, midiMessages);
#ifdef MAGIC_LEVEL_SOURCE
        fOutputMeter->pushSamples(buffer);
#endif
    }

    void processBlock(juce::AudioBuffer<double> &buffer, juce::MidiBuffer &midiMessages) override
    {
        jassert(isUsingDoublePrecision());
        process(buffer, midiMessages);
    }

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    void releaseResources() override
    {
    }

    void timerCallback() override;

    juce::AudioProcessor::BusesProperties getBusesProperties();
    bool supportsDoublePrecisionProcessing() const override;

#ifdef MAGIC_LEVEL_SOURCE
    foleys::MagicLevelSource *fOutputMeter = nullptr;
#endif
    juce::AudioProcessorValueTreeState treeState{*this, nullptr};

#ifdef JUCE_POLY
    std::unique_ptr<FaustSynthesiser> fSynth;
#else
#if defined(MIDICTRL)
    std::unique_ptr<juce_midi_handler> fMIDIHandler;
    std::unique_ptr<MidiUI> fMIDIUI;
#endif
    std::unique_ptr<dsp> fDSP;
#endif

#if defined(OSCCTRL)
    std::unique_ptr<JuceOSCUI> fOSCUI;
#endif

#if defined(SOUNDFILE)
    std::unique_ptr<SoundUIBrowse> fSoundUI;
    SoundUIBrowse *getSoundUIBrowse() { return fSoundUI.get(); }
#endif

    JuceStateUI fStateUI;
    JuceParameterUI fParameterUI;

    std::atomic<bool> fFirstCall = true;

private:
    template <typename FloatType>
    void process(juce::AudioBuffer<FloatType> &buffer, juce::MidiBuffer &midiMessages);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FaustPlugInAudioProcessor)
};

#else

    class FaustPlugInAudioProcessor : public juce::AudioProcessor,
    private juce::Timer
{

public:
    FaustPlugInAudioProcessor();
    virtual ~FaustPlugInAudioProcessor() {}

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override
    {
        jassert(!isUsingDoublePrecision());
        process(buffer, midiMessages);
    }

    void processBlock(juce::AudioBuffer<double> &buffer, juce::MidiBuffer &midiMessages) override
    {
        jassert(isUsingDoublePrecision());
        process(buffer, midiMessages);
    }

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    void releaseResources() override
    {
    }

    void timerCallback() override;

    juce::AudioProcessor::BusesProperties getBusesProperties();
    bool supportsDoublePrecisionProcessing() const override;

#ifdef JUCE_POLY
    std::unique_ptr<FaustSynthesiser> fSynth;
#else
#if defined(MIDICTRL)
    std::unique_ptr<juce_midi_handler> fMIDIHandler;
    std::unique_ptr<MidiUI> fMIDIUI;
#endif
    std::unique_ptr<dsp> fDSP;
#endif

#if defined(OSCCTRL)
    std::unique_ptr<JuceOSCUI> fOSCUI;
#endif

#if defined(SOUNDFILE)
    std::unique_ptr<SoundUIBrowse> fSoundUI;
    SoundUIBrowse *getSoundUIBrowse() { return fSoundUI.get(); }
#endif

    JuceStateUI fStateUI;
    JuceParameterUI fParameterUI;

    std::atomic<bool> fFirstCall = true;

private:
    template <typename FloatType>
    void process(juce::AudioBuffer<FloatType> &buffer, juce::MidiBuffer &midiMessages);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FaustPlugInAudioProcessor)
};

#endif

class FaustPlugInAudioProcessorEditor : public juce::AudioProcessorEditor
#if defined(SOUNDFILE)
    ,
                                        public juce::FileDragAndDropTarget
#endif
{

public:
    FaustPlugInAudioProcessorEditor(FaustPlugInAudioProcessor &);
    virtual ~FaustPlugInAudioProcessorEditor();

    void paint(juce::Graphics &) override;
    void resized() override;

#if defined(SOUNDFILE)
    bool isInterestedInFileDrag(const juce::StringArray &files) override;
    void fileDragEnter(const juce::StringArray &files, int x, int y) override;
    void fileDragExit(const juce::StringArray &files) override;
    void filesDropped(const juce::StringArray &files, int x, int y) override;
    void refreshWaveformAfterExternalSampleChange();
#endif

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FaustPlugInAudioProcessor &processor;

#if defined(SOUNDFILE)
    void loadSampleFromFile(const juce::File &f, bool syncBrowseToLoadedPath = true);
    void refreshWaveformDisplay();
    static bool isAudioExtension(const juce::String &extLower);

    void applyBrowseFolderFromEditor();
    void rebuildBrowseFileList();
    void syncBrowseIndexToCurrentFile();
    void loadBrowseEntry(int index);
    void loadAdjacentBrowseFile(int delta);
    void loadRandomBrowseFile();
    void updateBrowseNavButtonState();

    SampleWaveformStrip fWaveform;
    juce::TextEditor fBrowseFolderPath;
    juce::TextButton fBrowseFolderApply;
    juce::TextButton fPrevAudioFile;
    juce::TextButton fNextAudioFile;
    juce::TextButton fRandomAudioFile;
    juce::TextButton fLoadSample;
    std::shared_ptr<juce::FileChooser> fFileChooser;
    bool fFileDragHover = false;

    juce::File fBrowseFolderRoot;
    std::vector<juce::File> fBrowseAudioFiles;
    int fBrowseFileIndex = -1;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FaustPlugInAudioProcessorEditor)
#ifndef PLUGIN_MAGIC
    JuceGUI fJuceGUI;
#endif
};

#ifndef PLUGIN_MAGIC
FaustPlugInAudioProcessor::FaustPlugInAudioProcessor()
    : juce::AudioProcessor(getBusesProperties()), fParameterUI(this)
#else
FaustPlugInAudioProcessor::FaustPlugInAudioProcessor()
    : foleys::MagicProcessor(getBusesProperties()), fParameterUI(this)
#endif
{
    bool midi_sync = false;
    bool midi = false;
    int nvoices = 0;

    mydsp *tmp_dsp = new mydsp();
    MidiMeta::analyse(tmp_dsp, midi, midi_sync, nvoices);
    delete tmp_dsp;

#ifdef PLUGIN_MAGIC
#ifdef MAGIC_LOAD_BINARY
    // change magic_xml and magic_xmlSize to match the name of your included
    // XML file from Plugin GUI Magic
    magicState.setGuiValueTree(BinaryData::magic_xml, BinaryData::magic_xmlSize);
#endif
// put other GUI Magic sources here, similar to expression below.
#ifdef MAGIC_LEVEL_SOURCE
    fOutputMeter = magicState.createAndAddObject<foleys::MagicLevelSource>("output");
#endif
#endif

#ifdef JUCE_POLY
    assert(nvoices > 0);
    fSynth = std::make_unique<FaustSynthesiser>();
    for (int i = 0; i < nvoices; i++)
    {
        fSynth->addVoice(new FaustVoice(new mydsp()));
    }
    fSynth->init();
    fSynth->addSound(new FaustSound());
#else

    bool group = true;

#ifdef POLY2
    assert(nvoices > 0);
    std::cout << "Started with " << nvoices << " voices\n";
    dsp *dsp = new mydsp_poly(new mydsp(), nvoices, true, group);

#if MIDICTRL
    if (midi_sync)
    {
        fDSP = std::make_unique<timed_dsp>(new dsp_sequencer(dsp, new effect()));
    }
    else
    {
        fDSP = std::make_unique<dsp_sequencer>(dsp, new effect());
    }
#else
    fDSP = std::make_unique<dsp_sequencer>(dsp, new effect());
#endif

#else
    if (nvoices > 0)
    {
        std::cout << "Started with " << nvoices << " voices\n";
        dsp *dsp = new mydsp_poly(new mydsp(), nvoices, true, group);

#if MIDICTRL
        if (midi_sync)
        {
            fDSP = std::make_unique<timed_dsp>(dsp);
        }
        else
        {
            fDSP = std::make_unique<decorator_dsp>(dsp);
        }
#else
        fDSP = std::make_unique<decorator_dsp>(dsp);
#endif
    }
    else
    {
#if MIDICTRL
        if (midi_sync)
        {
            fDSP = std::make_unique<timed_dsp>(new mydsp());
        }
        else
        {
            fDSP = std::make_unique<mydsp>();
        }
#else
        fDSP = std::make_unique<mydsp>();
#endif
    }

#endif

#if defined(MIDICTRL)
    fMIDIHandler = std::make_unique<juce_midi_handler>();
    fMIDIUI = std::make_unique<MidiUI>(fMIDIHandler.get());
    fDSP->buildUserInterface(fMIDIUI.get());
    if (!fMIDIUI->run())
    {
        std::cerr << "JUCE MIDI handler cannot be started..." << std::endl;
    }
#endif

#endif

#if defined(OSCCTRL)
    fOSCUI = std::make_unique<JuceOSCUI>("127.0.0.1", 5510, 5511);
#ifdef JUCE_POLY
    fSynth->buildUserInterface(fOSCUI.get());
#else
    fDSP->buildUserInterface(fOSCUI.get());
#endif
    if (!fOSCUI->run())
    {
        std::cerr << "JUCE OSC handler cannot be started..." << std::endl;
    }
#endif

#if defined(SOUNDFILE)
    // Use bundle path
    auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                    .getParentDirectory()
                    .getParentDirectory()
                    .getChildFile("Resources");
    fSoundUI = std::make_unique<SoundUIBrowse>(file.getFullPathName().toStdString());
#ifdef JUCE_POLY
    fSynth->buildUserInterface(fSoundUI.get());
#else
    fDSP->buildUserInterface(fSoundUI.get());
#endif
#endif

#ifdef JUCE_POLY
    fSynth->buildUserInterface(&fStateUI);
    fSynth->buildUserInterface(&fParameterUI);
    // When no previous state was restored, init DSP controllers with their default values
    if (!fStateUI.fRestored)
    {
        fSynth->instanceResetUserInterface();
    }
#else
    fDSP->buildUserInterface(&fStateUI);
    fDSP->buildUserInterface(&fParameterUI);
    // When no previous state was restored, init DSP controllers with their default values
    if (!fStateUI.fRestored)
    {
        fDSP->instanceResetUserInterface();
    }
#endif

    startTimerHz(25);
}

juce::AudioProcessor::BusesProperties FaustPlugInAudioProcessor::getBusesProperties()
{
    if (juce::PluginHostType::getPluginLoadedAs() == wrapperType_Standalone)
    {
        if (FAUST_INPUTS == 0)
        {
            return BusesProperties().withOutput("Output", juce::AudioChannelSet::canonicalChannelSet(std::min<int>(2, FAUST_OUTPUTS)), true);
        }
        else
        {
            return BusesProperties()
                .withInput("Input", juce::AudioChannelSet::canonicalChannelSet(std::min<int>(2, FAUST_INPUTS)), true)
                .withOutput("Output", juce::AudioChannelSet::canonicalChannelSet(std::min<int>(2, FAUST_OUTPUTS)), true);
        }
    }
    else
    {
        if (FAUST_INPUTS == 0)
        {
            return BusesProperties().withOutput("Output", juce::AudioChannelSet::canonicalChannelSet(FAUST_OUTPUTS), true);
        }
        else
        {
            return BusesProperties()
                .withInput("Input", juce::AudioChannelSet::canonicalChannelSet(FAUST_INPUTS), true)
                .withOutput("Output", juce::AudioChannelSet::canonicalChannelSet(FAUST_OUTPUTS), true);
        }
    }
}

void FaustPlugInAudioProcessor::timerCallback()
{
    GUI::updateAllGuis();
}

//==============================================================================
const juce::String FaustPlugInAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FaustPlugInAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool FaustPlugInAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

double FaustPlugInAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FaustPlugInAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int FaustPlugInAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FaustPlugInAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String FaustPlugInAudioProcessor::getProgramName(int index)
{
    return juce::String();
}

void FaustPlugInAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

bool FaustPlugInAudioProcessor::supportsDoublePrecisionProcessing() const
{
    return sizeof(FAUSTFLOAT) == 8;
}

//==============================================================================
void FaustPlugInAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Reset DSP adaptation
    fFirstCall = true;

#ifdef JUCE_POLY
    fSynth->setCurrentPlaybackSampleRate(sampleRate);
#else

    // Setting the DSP control values has already been done
    // by 'buildUserInterface(&fStateUI)', using the saved values or the default ones.
    // What has to be done to finish the DSP initialization is done now.
    mydsp::classInit(int(sampleRate));
    fDSP->instanceConstants(int(sampleRate));
    fDSP->instanceClear();

    // Get latency metadata
    struct LatencyMeta : public Meta
    {

        float fLatencyFrames = -1.f;
        float fLatencySec = -1.f;

        void declare(const char *key, const char *value)
        {
            if (std::string(key) == "latency_frames" || std::string(key) == "latency_samples")
            {
                fLatencyFrames = std::atof(value);
            }
            else if (std::string(key) == "latency_sec")
            {
                fLatencySec = std::atof(value);
            }
        }
    };

    LatencyMeta meta;
    fDSP->metadata(&meta);
    if (meta.fLatencyFrames > 0)
    {
        setLatencySamples(meta.fLatencyFrames);
    }
    else if (meta.fLatencySec > 0)
    {
        setLatencySamples(meta.fLatencySec * sampleRate);
    }

#endif
#ifdef MAGIC_LEVEL_SOURCE
    magicState.prepareToPlay(sampleRate, samplesPerBlock);
    fOutputMeter->setupSource(getMainBusNumOutputChannels(), sampleRate, 500, 200);
#endif
}

bool FaustPlugInAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    // Always return true and have the DSP adapts its buffer layout with a dsp_adapter (see 'prepareToPlay' and 'process')
    return true;
}

template <typename FloatType>
void FaustPlugInAudioProcessor::process(juce::AudioBuffer<FloatType> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    /*
        prepareToPlay is possibly called several times with different values for sampleRate
        and isUsingDoublePrecision() state (this has been seen in particular with VTS3),
        making proper sample format (float/double) and the inputs/outputs layout adaptation
        more complex at this stage.

        So adapting the sample format (float/double) and the inputs/outputs layout is done
        once at first process call even if this possibly allocates memory, which is not RT safe.
    */
    if (fFirstCall)
    {
        fFirstCall = false;

        // Possible sample size adaptation
        if (supportsDoublePrecisionProcessing())
        {
            if (isUsingDoublePrecision())
            {
                // Nothing to do
            }
            else
            {
                fDSP = std::make_unique<dsp_sample_adapter<double, float, 16384>>(fDSP.release());
            }
        }
        else
        {
            if (isUsingDoublePrecision())
            {
                fDSP = std::make_unique<dsp_sample_adapter<float, double, 16384>>(fDSP.release());
            }
            else
            {
                // Nothing to do
            }
        }

        // Possibly adapt DSP inputs/outputs number
        if (fDSP->getNumInputs() > getTotalNumInputChannels() || fDSP->getNumOutputs() > getTotalNumOutputChannels())
        {
            fDSP = std::make_unique<dsp_adapter>(fDSP.release(), getTotalNumInputChannels(), getTotalNumOutputChannels(), 16384);
        }
    }

#ifdef JUCE_POLY
    fSynth->renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
#else
#if defined(MIDICTRL)
    // Read MIDI input events from midiMessages
    fMIDIHandler->decodeBuffer(midiMessages);
    // Then write MIDI output events to midiMessages
    fMIDIHandler->encodeBuffer(midiMessages);
#endif
    // MIDI timestamp is expressed in frames
    fDSP->compute(-1, buffer.getNumSamples(),
                  (FAUSTFLOAT **)buffer.getArrayOfReadPointers(),
                  (FAUSTFLOAT **)buffer.getArrayOfWritePointers());
#endif
}

//==============================================================================
#ifndef PLUGIN_MAGIC
bool FaustPlugInAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor *FaustPlugInAudioProcessor::createEditor()
{
    return new FaustPlugInAudioProcessorEditor(*this);
}

//==============================================================================
void FaustPlugInAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

#if defined(SOUNDFILE)
    juce::MemoryBlock faustState;
    fStateUI.getStateInformation(faustState);
    juce::MemoryOutputStream out(destData, false);
    // GPS2: sample path first (length-prefixed UTF-8). See FaustPluginProcessor.cpp comments.
    out.writeInt(0x47505332); // 'GPS2'
    juce::String path;
    if (auto *su = getSoundUIBrowse())
    {
        const juce::File f(su->getLastLoadedFile());
        if (f.existsAsFile())
            path = f.getFullPathName();
    }
    {
        const juce::CharPointer_UTF8 utf8 = path.toUTF8();
        const int pathBytes = (int)utf8.sizeInBytes();
        out.writeInt(pathBytes);
        out.write(utf8.getAddress(), (size_t)pathBytes);
    }
    out.writeInt((int)faustState.getSize());
    out.write(faustState.getData(), faustState.getSize());
    out.flush();
#else
    fStateUI.getStateInformation(destData);
#endif
}

void FaustPlugInAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.

#if defined(SOUNDFILE)
    if (data != nullptr && sizeInBytes >= 4)
    {
        juce::MemoryInputStream in(data, (size_t)sizeInBytes, false);
        const int magic = in.readInt();
        if (magic == 0x47505332) // GPS2 (path before Faust blob)
        {
            const int pathLen = in.readInt();
            constexpr int kMaxPathBytes = 65536;
            const auto consumedHeader = (int64_t)8;
            if (pathLen >= 0 && pathLen <= kMaxPathBytes
                && consumedHeader + (int64_t)pathLen + (int64_t)4 <= (int64_t)sizeInBytes)
            {
                juce::MemoryBlock pathBytes((size_t)pathLen);
                in.read(pathBytes.getData(), (size_t)pathLen);
                const juce::String samplePath =
                    juce::String::fromUTF8((const char *)pathBytes.getData(), pathLen);
                const int faustSize = in.readInt();
                const auto afterHeader = consumedHeader + (int64_t)pathLen + (int64_t)4;
                if (faustSize >= 0 && afterHeader + (int64_t)faustSize <= (int64_t)sizeInBytes)
                {
                    juce::MemoryBlock faustBlock;
                    in.readIntoMemoryBlock(faustBlock, (size_t)faustSize);
                    fStateUI.setStateInformation(faustBlock.getData(), (int)faustBlock.getSize());
                    if (samplePath.isNotEmpty())
                    {
                        const juce::File f(samplePath);
                        if (f.existsAsFile())
                        {
                            if (auto *su = getSoundUIBrowse())
                            {
                                const juce::ScopedLock audioLk(getCallbackLock());
                                su->reloadFromAbsoluteFile(f);
                            }
                        }
                    }
                    if (auto *ed = getActiveEditor())
                        if (auto *fed = dynamic_cast<FaustPlugInAudioProcessorEditor *>(ed))
                            fed->refreshWaveformAfterExternalSampleChange();
                    return;
                }
                if (samplePath.isNotEmpty())
                {
                    const juce::File f(samplePath);
                    if (f.existsAsFile())
                    {
                        if (auto *su = getSoundUIBrowse())
                        {
                            const juce::ScopedLock audioLk(getCallbackLock());
                            su->reloadFromAbsoluteFile(f);
                        }
                    }
                    if (auto *ed = getActiveEditor())
                        if (auto *fed = dynamic_cast<FaustPlugInAudioProcessorEditor *>(ed))
                            fed->refreshWaveformAfterExternalSampleChange();
                    return;
                }
            }
        }
        else if (magic == 0x47505331 && sizeInBytes >= 8) // GPS1 legacy
        {
            juce::MemoryInputStream in1(data, (size_t)sizeInBytes, false);
            (void)in1.readInt(); // skip magic
            const int faustSize = in1.readInt();
            const auto payloadBytes = (int64_t)sizeInBytes - 8;
            if (faustSize >= 0 && (int64_t)faustSize <= payloadBytes)
            {
                juce::MemoryBlock faustBlock;
                in1.readIntoMemoryBlock(faustBlock, (size_t)faustSize);
                fStateUI.setStateInformation(faustBlock.getData(), (int)faustBlock.getSize());
                const juce::String samplePath = in1.readString();
                if (samplePath.isNotEmpty())
                {
                    const juce::File f(samplePath);
                    if (f.existsAsFile())
                    {
                        if (auto *su = getSoundUIBrowse())
                        {
                            const juce::ScopedLock audioLk(getCallbackLock());
                            su->reloadFromAbsoluteFile(f);
                        }
                    }
                }
                if (auto *ed = getActiveEditor())
                    if (auto *fed = dynamic_cast<FaustPlugInAudioProcessorEditor *>(ed))
                        fed->refreshWaveformAfterExternalSampleChange();
                return;
            }
        }
        if (magic == 0x47505332 || magic == 0x47505331)
            return;
    }
#endif
    fStateUI.setStateInformation(data, sizeInBytes);
}
#endif
//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new FaustPlugInAudioProcessor();
}

//==============================================================================
#ifndef PLUGIN_MAGIC
FaustPlugInAudioProcessorEditor::FaustPlugInAudioProcessorEditor(FaustPlugInAudioProcessor &p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
#ifdef JUCE_POLY
    p.fSynth->buildUserInterface(&fJuceGUI);
#else
    p.fDSP->buildUserInterface(&fJuceGUI);
#endif

    addAndMakeVisible(fJuceGUI);

#if defined(SOUNDFILE)
    addAndMakeVisible(fWaveform);
    addAndMakeVisible(fBrowseFolderPath);
    addAndMakeVisible(fBrowseFolderApply);
    addAndMakeVisible(fPrevAudioFile);
    addAndMakeVisible(fNextAudioFile);
    addAndMakeVisible(fRandomAudioFile);
    addAndMakeVisible(fLoadSample);

    fBrowseFolderPath.setMultiLine(false);
    fBrowseFolderPath.setReturnKeyStartsNewLine(false);
    fBrowseFolderPath.setTextToShowWhenEmpty("Folder path (Set or Enter)", juce::Colours::grey);
    fBrowseFolderPath.onReturnKey = [this] { applyBrowseFolderFromEditor(); };

    fBrowseFolderApply.setButtonText("Set");
    fBrowseFolderApply.onClick = [this] { applyBrowseFolderFromEditor(); };

    fPrevAudioFile.setButtonText("<");
    fPrevAudioFile.onClick = [this] { loadAdjacentBrowseFile(-1); };
    fNextAudioFile.setButtonText(">");
    fNextAudioFile.onClick = [this] { loadAdjacentBrowseFile(1); };
    fRandomAudioFile.setButtonText("Random");
    fRandomAudioFile.onClick = [this] { loadRandomBrowseFile(); };

    fLoadSample.setButtonText("Load…");
    if (auto *su = p.getSoundUIBrowse())
    {
        juce::Component::SafePointer<FaustPlugInAudioProcessorEditor> safeThis(this);
        su->setOnSampleChanged([safeThis]
                               {
            if (safeThis != nullptr) {
                safeThis->refreshWaveformDisplay();
            } });
        const juce::File cur = su->getLastLoadedFile();
        if (cur.existsAsFile())
        {
            fBrowseFolderRoot = cur.getParentDirectory();
            fBrowseFolderPath.setText(fBrowseFolderRoot.getFullPathName());
            rebuildBrowseFileList();
        }
    }
    updateBrowseNavButtonState();
    fLoadSample.onClick = [this]
    {
        juce::Component::SafePointer<FaustPlugInAudioProcessorEditor> safeThis(this);
        juce::MessageManager::callAsync([safeThis]
                                        {
            if (safeThis == nullptr) {
                return;
            }
            auto* self = safeThis.getComponent();
            if (self == nullptr) {
                return;
            }
            self->fFileChooser = std::make_shared<juce::FileChooser>(
                "Select audio",
                juce::File{},
                "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3",
                false,
                false,
                nullptr);
            const int flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
            self->fFileChooser->launchAsync(flags, [safeThis](const juce::FileChooser& fc)
            {
                if (safeThis == nullptr) {
                    return;
                }
                auto* editor = safeThis.getComponent();
                if (editor == nullptr) {
                    return;
                }
                const juce::File f = fc.getResult();
                if (f.existsAsFile()) {
                    editor->loadSampleFromFile(f);
                }
                editor->fFileChooser.reset();
            }); });
    };
    juce::Component::SafePointer<FaustPlugInAudioProcessorEditor> safeThis(this);
    juce::MessageManager::callAsync([safeThis]
                                    {
        if (safeThis != nullptr) {
            safeThis->refreshWaveformDisplay();
        } });
#endif

    const juce::Rectangle<int> recommendedSize = fJuceGUI.getSize();
    setSize(recommendedSize.getWidth(), recommendedSize.getHeight());
}

FaustPlugInAudioProcessorEditor::~FaustPlugInAudioProcessorEditor()
{
#if defined(SOUNDFILE)
    if (auto *su = processor.getSoundUIBrowse())
    {
        su->setOnSampleChanged({});
    }
    fFileChooser.reset();
#endif
}

//==============================================================================
void FaustPlugInAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colours::white);
#if defined(SOUNDFILE)
    if (fFileDragHover)
    {
        g.setColour(juce::Colours::cyan.withAlpha(0.12f));
        g.fillRect(getLocalBounds().reduced(2));
        g.setColour(juce::Colours::cyan.withAlpha(0.9f));
        g.drawRect(getLocalBounds().reduced(1), 2);
    }
#endif
}

void FaustPlugInAudioProcessorEditor::resized()
{
#if defined(SOUNDFILE)
    auto bounds = getLocalBounds();
    const int toolbarH = 108;
    auto top = bounds.removeFromTop(toolbarH).reduced(5, 4);

    auto row1 = top.removeFromTop(30);
    fBrowseFolderApply.setBounds(row1.removeFromRight(52).reduced(2, 0));
    fBrowseFolderPath.setBounds(row1.reduced(4, 0));

    auto row2 = top;
    const int navBtnW = 40;
    fPrevAudioFile.setBounds(row2.removeFromLeft(navBtnW).reduced(2, 4));
    fNextAudioFile.setBounds(row2.removeFromLeft(navBtnW).reduced(2, 4));
    fRandomAudioFile.setBounds(row2.removeFromLeft(88).reduced(2, 4));
    const int loadW = 96;
    fLoadSample.setBounds(row2.removeFromRight(loadW).reduced(2, 4));
    fWaveform.setBounds(row2.reduced(6, 0));

    fJuceGUI.setBounds(bounds);
#else
    fJuceGUI.setBounds(getLocalBounds());
#endif
}

#if defined(SOUNDFILE)
void FaustPlugInAudioProcessorEditor::loadSampleFromFile(const juce::File &f, bool syncBrowseToLoadedPath)
{
    if (auto *su = processor.getSoundUIBrowse())
    {
        // Strong synchronization with host audio callback while replacing Faust soundfile zones.
        const juce::ScopedLock callbackLock(processor.getCallbackLock());
        su->reloadFromAbsoluteFile(f);
    }
    if (syncBrowseToLoadedPath)
        syncBrowseIndexToCurrentFile();
    updateBrowseNavButtonState();
}

void FaustPlugInAudioProcessorEditor::applyBrowseFolderFromEditor()
{
    const juce::File dir(fBrowseFolderPath.getText().trim());
    if (!dir.isDirectory())
        return;
    fBrowseFolderRoot = dir;
    rebuildBrowseFileList();
}

void FaustPlugInAudioProcessorEditor::rebuildBrowseFileList()
{
    fBrowseAudioFiles.clear();
    fBrowseFileIndex = -1;
    if (!fBrowseFolderRoot.isDirectory())
    {
        updateBrowseNavButtonState();
        return;
    }

    juce::Array<juce::File> hits;
    fBrowseFolderRoot.findChildFiles(hits, juce::File::findFiles, false);
    for (int i = 0; i < hits.size(); ++i)
    {
        const juce::File file(hits.getReference(i));
        if (!file.existsAsFile())
            continue;
        const juce::String ext = file.getFileExtension().toLowerCase();
        if (isAudioExtension(ext.substring(1)))
            fBrowseAudioFiles.push_back(file);
    }

    std::sort(fBrowseAudioFiles.begin(), fBrowseAudioFiles.end(),
              [](const juce::File &a, const juce::File &b)
              { return a.getFileName().compareIgnoreCase(b.getFileName()) < 0; });

    syncBrowseIndexToCurrentFile();
    updateBrowseNavButtonState();
}

void FaustPlugInAudioProcessorEditor::syncBrowseIndexToCurrentFile()
{
    fBrowseFileIndex = -1;
    if (auto *su = processor.getSoundUIBrowse())
    {
        const juce::File cur = su->getLastLoadedFile();
        if (!cur.existsAsFile())
            return;
        for (int i = 0; i < (int)fBrowseAudioFiles.size(); ++i)
        {
            if (fBrowseAudioFiles[(size_t)i] == cur)
            {
                fBrowseFileIndex = i;
                break;
            }
        }
    }
}

void FaustPlugInAudioProcessorEditor::loadBrowseEntry(int index)
{
    const int n = (int)fBrowseAudioFiles.size();
    if (n <= 0 || index < 0 || index >= n)
        return;
    // External samples are copied to Application Support; sync-by-path would miss the list entry.
    loadSampleFromFile(fBrowseAudioFiles[(size_t)index], false);
    fBrowseFileIndex = index;
    updateBrowseNavButtonState();
}

void FaustPlugInAudioProcessorEditor::loadAdjacentBrowseFile(int delta)
{
    const int n = (int)fBrowseAudioFiles.size();
    if (n <= 0)
        return;
    int idx = (fBrowseFileIndex >= 0) ? fBrowseFileIndex : 0;
    idx = ((idx + delta) % n + n) % n;
    loadBrowseEntry(idx);
}

void FaustPlugInAudioProcessorEditor::loadRandomBrowseFile()
{
    const int n = (int)fBrowseAudioFiles.size();
    if (n <= 0)
        return;
    const int idx = juce::Random::getSystemRandom().nextInt(n);
    loadBrowseEntry(idx);
}

void FaustPlugInAudioProcessorEditor::updateBrowseNavButtonState()
{
    const bool ok = !fBrowseAudioFiles.empty();
    fPrevAudioFile.setEnabled(ok);
    fNextAudioFile.setEnabled(ok);
    fRandomAudioFile.setEnabled(ok);
}

void FaustPlugInAudioProcessorEditor::refreshWaveformDisplay()
{
    if (auto *su = processor.getSoundUIBrowse())
    {
        fWaveform.rebuildFromFile(su->getLastLoadedFile(), su->getSampleDisplayName());
    }
    else
    {
        fWaveform.clear();
    }
}

void FaustPlugInAudioProcessorEditor::refreshWaveformAfterExternalSampleChange()
{
    refreshWaveformDisplay();
}

bool FaustPlugInAudioProcessorEditor::isAudioExtension(const juce::String &extLower)
{
    static const char *exts[] = {"wav", "aif", "aiff", "flac", "ogg", "mp3", nullptr};
    for (const char **e = exts; *e != nullptr; ++e)
    {
        if (extLower == *e)
        {
            return true;
        }
    }
    return false;
}

bool FaustPlugInAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray &files)
{
    for (const auto &path : files)
    {
        const juce::String ext = juce::File(path).getFileExtension().toLowerCase().substring(1);
        if (isAudioExtension(ext))
        {
            return true;
        }
    }
    return false;
}

void FaustPlugInAudioProcessorEditor::fileDragEnter(const juce::StringArray &files, int, int)
{
    if (!isInterestedInFileDrag(files))
    {
        return;
    }
    fFileDragHover = true;
    repaint();
}

void FaustPlugInAudioProcessorEditor::fileDragExit(const juce::StringArray &)
{
    fFileDragHover = false;
    repaint();
}

void FaustPlugInAudioProcessorEditor::filesDropped(const juce::StringArray &files, int, int)
{
    fFileDragHover = false;
    repaint();
    for (const auto &path : files)
    {
        juce::File f(path);
        const juce::String ext = f.getFileExtension().toLowerCase().substring(1);
        if (isAudioExtension(ext) && f.existsAsFile())
        {
            loadSampleFromFile(f);
            break;
        }
    }
}
#endif

#endif
// Globals
std::list<GUI *> GUI::fGuiList;
ztimedmap GUI::gTimedZoneMap;
