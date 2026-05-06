# Granular Polysynth

Build the vst by running the build_vst3.sh script. Update the paths to the locations of Faust and the VST directory for your system.

This generates the FaustPluginProcessor.cpp file which is the dsp back end. 

juce-plugin-soundbrowse.cpp is for file loading.

Effect.dsp applies a soft clipper to the output.
