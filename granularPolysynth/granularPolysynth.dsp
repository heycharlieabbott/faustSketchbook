declare name "Granular poly sampler";
declare options "[midi:on][nvoices:8]";

// JUCE VST3: CMakeLists.txt + build_vst3.sh (same layout as basicFMPolysynth; set JUCE_DIR).

import("stdfaust.lib");
import("../utils.dsp");

// Default WAV sits next to this .dsp (works from this folder or repo root).

smp = soundfile("granularSample[url:{'placeholder.wav'}]", 2);

file_idx = 0;
MAX_CLOCK_VOICES = 50;

clockVoices = int(hslider("v:A/clockVoices", 4, 1, MAX_CLOCK_VOICES, 1));
grainSpeed = max(1e-4, hslider("v:A/grainSpeed", 1, 0.03, 50, 0.001));
density = max(0.01, hslider("v:A/grainDensity [unit:Hz]", 8, 0.1, 800, 0.01));
start = hslider("v:A/start", 0.05, 0, 1, 0.001) : si.smooth(0.999);
grainLen = hslider("v:A/grainLength", 4096, 1, 120000, 1) : si.smooth(0.999);
grainSpeedMul = hslider("v:A/grainSpeedMultiplier", 1, 0.1, 10, 0.01);
densityMul = hslider("v:A/grainDensityMultiplier", 1, 0.1, 1, 0.01);
grainLenMul = hslider("v:A/grainLengthMultiplier", 1, 0.1, 10, 0.01) : si.smooth(0.999);
dispersal = int(hslider("v:A/dispersal", 1, 1, MAX_CLOCK_VOICES, 1) / clockVoices);
grainSpeedEff = max(1e-4, grainSpeed * grainSpeedMul);
densityEff = max(0.01, density * densityMul);
grainLenEff = max(1, grainLen * grainLenMul) : si.smooth(0.999);
gate = button("gate");
freq = hslider("v:B/freq [unit:Hz] [midi:keyon]", 440, 16, 20000, 0.01) : si.polySmooth(gate, 0.999, 96);
rootHz = ba.midikey2hz(hslider("v:B/baseKey", 69, 20, 130, 0.1));
pitchRatio = freq / rootHz;
att = max(0.002, hslider("v:C/attack [unit:s]", 0.01, 0.002, 2, 0.001));
dec = max(0.005, hslider("v:C/decay [unit:s]", 0.06, 0.002, 2, 0.001));
sus = max(1e-3, hslider("v:C/sustain", 0.85, 0, 1, 0.01));
rel = max(0.02, hslider("v:C/release [unit:s]", 0.35, 0.02, 12, 0.01));
master = hslider("v:C/masterGain", 0.5, 0, 2, 0.01);

slen(sf) = (file_idx, 0) : sf : (_, si.block(outputs(sf) - 1));
outsSf(s, lvl) = s : si.block(2), si.bus(outputs(s) - 2);
sh(x, trig) = ba.sAndH(trig,x);

ramp(playRate, trig) = delta :
    (+ : select2(trig, _, delta < 0) : max(0)) ~ _ : raz
with {

    raz(z) = select2(z > 1, z, 0);
    delta = sh(playRate, trig) / ma.SR;

};

grain(trigPulse) =
    (file_idx, pf) :
    outsSf(smp, 1) :
        par(i, 2, *(gain))
with {

    rr = ramp(grainSpeedEff * pitchRatio, trigPulse);
    fl = float(slen(smp));
    lm = min(grainLenEff, max(127, fl - 16));
    pf = ma.modulo(rr * lm + start * fl, max(127, fl));
    gain = sin(rr * ma.PI);

};


voiceClk(fr) = par(i, MAX_CLOCK_VOICES, clk(i + dispersal))
with {
    periodSamples = (1.0 / max(1e-9, fr)) * ma.SR;
    enabled(i) = float(i < clockVoices);
    clk(i) = (os.lf_imptrain(fr) : @((float(i) / float(MAX_CLOCK_VOICES)) * periodSamples)) * enabled(i);
};

grains = voiceClk(densityEff) : par(i, MAX_CLOCK_VOICES, grain) :> _,_;

env = en.adsre(att, dec, sus, rel, gate);

process = grains
    : par(i, 2, *(env * master))
    : par(i, 2, fi.dcblocker)
    : par(i, 2, clamp(-1, 1));

effect = ef.softclipQuadratic;
