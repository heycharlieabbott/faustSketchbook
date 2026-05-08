import("stdfaust.lib");

declare name "Stereo Echo Delay";
declare version "0.1";

maxDelayMs = 2000.0;
maxDelaySamples = int(ma.SR * (maxDelayMs / 1000.0));
delaySamplesL = int(ma.SR * (delayMsL / 1000.0));
delaySamplesR = int(ma.SR * (delayMsR / 1000.0));

toneHz = hslider("Feedback Tone[unit:Hz]", 4000, 00, 12000, 1) : si.smoo;
toneQ = hslider("Feedback Q", 0.707, 0.1, 1.7, 0.001) : si.smoo;
toneBlend = hslider("Feedback Blend", 0, 0, 2, 0.001) : si.smoo;
ringModFreq = hslider("RingMod Freq[unit:Hz]", 0.0, 0., 100.0, 0.001) : si.smoo;
ringModAmp = hslider("RingMod Amp", 0.0, 0.0, 2.0, 0.001) : si.smoo;
delayMsL = hslider("Delay Left (ms)", 500.0, 1.0, maxDelayMs, 1.0): si.smoo;
delayMsR = hslider("Delay Right (ms)", 500.0, 1.0, maxDelayMs, 1.0): si.smoo;
feedback = hslider("Feedback", 0.35, 0.0, 2.5, 0.01);
modOscFreq = hslider("modOscFreq", 0., 0.0, 3., 0.001): si.smoo;
modOscAmp = hslider("modOscAmp", 0., 0.0, 0.5, 0.001) : si.smoo;
modOscMix = hslider("modOscMix", 0., 0.0, 1., 0.01) : si.smoo;
clipBlend = hslider("Clip/Atten Blend", 1.0, 0.0, 1.0, 0.001) : si.smoo;
mix = hslider("Mix", 0.4, 0.0, 1.0, 0.01);

sampleMod = si.interpolate(modOscMix, 1, (os.osc(modOscFreq) + 1) * modOscAmp);
ringModCarrier = si.interpolate(ringModAmp, 1.0, os.osc(ringModFreq));
attenGain = 0.6;
clipOrAtten(x) = si.interpolate(clipBlend, x * attenGain, ef.softclipQuadratic(x));
outputCeiling = ba.db2linear(-1.0); // Keep peaks below 0 dBFS.
brickwall(x) = min(outputCeiling, max(-outputCeiling, x));

echoVoice(x, delaySamples) = (x * (1.0 - mix)) + (wet * mix)
with {
  modDelaySamples = int(delaySamples * sampleMod);
  wet = x : (+ ~ feedbackPath) : de.delay(maxDelaySamples, modDelaySamples);
  feedbackPath = *(feedback)
    : fi.svf_morph(toneHz, toneQ, toneBlend)
    : *(ringModCarrier)
    : clipOrAtten
    : de.delay(maxDelaySamples, modDelaySamples);

};

// Single stereo input -> stereo echo with independent channel delay times.
process(inL, inR) = (echoVoice(inL, delaySamplesL), echoVoice(inR, delaySamplesR))
  : (ef.softclipQuadratic, ef.softclipQuadratic);
