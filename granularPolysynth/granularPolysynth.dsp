declare name "Granular poly sampler";
declare options "[midi:on][nvoices:8]";

// JUCE VST3: CMakeLists.txt + build_vst3.sh (same layout as basicFMPolysynth; set JUCE_DIR).

import("stdfaust.lib");
import("../utils.dsp");

// Default WAV sits next to this .dsp (works from this folder or repo root).

smp = soundfile("granularSample[url:{'placeholder.wav'}]", 2);

file_idx = 0;
MAX_CLOCK_VOICES = 50;

clockVoices = int(hslider("h:Granular/v:A/clockVoices", 4, 1, MAX_CLOCK_VOICES, 1));
grainSpeed = max(1e-4, hslider("h:Granular/v:A/grainSpeed", 1, 0.03, 50, 0.001));
density = max(0.01, hslider("h:Granular/v:A/grainDensity [unit:Hz]", 8, 0.5, 10, 0.01));
// startNow: read index (instant when blend=0). startSmooth: smoothed index (pitch glide when blend=1).
// When blend=0, |Δstart| feeds a decaying peak; extra Hann exponent softens grain *edges* after moves
// (volume window on the grain envelope), without sliding the read pointer.
startNow = hslider("h:Granular/v:A/start", 0.05, 0, 1, 0.001) : si.smooth(0.999);
startSmooth = startNow : si.smooth(0.999);
startInterpBlend = pow(hslider("h:Granular/v:A/startInterpBlend", 0, 0, 1, 0.001) : si.smooth(0.999), 10);
startPos = startNow * (1 - startInterpBlend) + startSmooth * startInterpBlend;
// No self-loop: |start - fastFollower| spikes when start moves, then decays → extra grain-edge taper.
startFast = startNow : si.smooth(0.92);
startDev = abs(startNow - startFast);
startChangeEnv = startDev : si.smooth(0.94);
edgeBoost = min(22, startChangeEnv * 9000) * (1 - startInterpBlend);
grainWinExp = 1 + edgeBoost;
// LFO on playhead (center = startPos). startInterpBlend: 0 = sample-hold LFO at each grain trigger
// (offset constant within a grain → no intra-grain Doppler from LFO); 1 = continuous LFO on index
// (slewing read → vibrato / pitch modulation). LFO path still uses raw vs si.smooth like startPos.
startModAmp = hslider("h:Granular/v:A/startModAmp", 0, 0, 0.5, 0.001);
startModFreq = hslider("h:Granular/v:A/startModFreq [unit:Hz]", 0.1, 0.001, 12, 0.001);
grainLen = hslider("h:Granular/v:A/grainLength", 4096, 1, 120000, 1) : si.smooth(0.999);
grainSpeedMul = hslider("h:Granular/v:A/grainSpeedMultiplier", 1, 0.1, 10, 0.01);
densityMul = hslider("h:Granular/v:A/grainDensityMultiplier", 1, 0.5, 1, 0.01);
grainLenMul = hslider("h:Granular/v:A/grainLengthMultiplier", 1, 0.1, 10, 0.01) : si.smooth(0.999);
startModAmpMul = hslider("h:Granular/v:A/startModAmpMultiplier", 1, 0.1, 10, 0.01);
startModFreqMul = hslider("h:Granular/v:A/startModFreqMultiplier", 1, 0.1, 10, 0.01);
dispersal = int(hslider("h:Granular/v:A/dispersal", 1, 1, MAX_CLOCK_VOICES, 1) / clockVoices);
grainSpeedEff = max(1e-4, grainSpeed * grainSpeedMul);
densityEff = max(0.01, density * densityMul);
grainLenEff = max(1, grainLen * grainLenMul) : si.smooth(0.999);
startModAmpEff = startModAmp * startModAmpMul;
startModFreqEff = max(1e-4, startModFreq * startModFreqMul);
startLfo = startModAmpEff * os.osc(startModFreqEff);
startLfoSmooth = startLfo : si.smooth(0.999);
startLfoBlended = startLfo * (1 - startInterpBlend) + startLfoSmooth * startInterpBlend;
gate = button("gate");
freq = hslider("h:Granular/v:B/freq [unit:Hz] [midi:keyon]", 440, 16, 20000, 0.01) : si.polySmooth(gate, 0.999, 96);
rootHz = ba.midikey2hz(hslider("h:Granular/v:B/baseKey", 69, 20, 130, 0.1));
pitchRatio = freq / rootHz;
att = max(0.002, hslider("h:Granular/v:C/attack [unit:s]", 0.01, 0.002, 2, 0.001));
dec = max(0.005, hslider("h:Granular/v:C/decay [unit:s]", 0.06, 0.002, 2, 0.001));
sus = max(1e-3, hslider("h:Granular/v:C/sustain", 0.85, 0, 1, 0.01));
rel = max(0.02, hslider("h:Granular/v:C/release [unit:s]", 0.35, 0.02, 12, 0.01));
master = hslider("h:Granular/v:C/masterGain", 0.5, 0, 2, 0.01);

slen(sf) = (file_idx, 0) : sf : (_, si.block(outputs(sf) - 1));
outsSf(s, lvl) = s : si.block(2), si.bus(outputs(s) - 2);
sh(x, trig) = ba.sAndH(trig,x);
readAt(idx) = (file_idx, idx) : outsSf(smp, 1);

// One-shot grain phase: resets to 0 on trig, ramps 0→1 over durSamples, then HOLDS at 1 so the
// Hann window (sin(rr*PI)) closes and the grain goes silent until its next trigger. grainDensity
// (via the clock) sets how often grains fire; grainLength sets each grain's duration. Smoothness
// comes from overlap-add: the clock spreads grainVoices grains evenly so their Hann windows sum.
grainPhasor(durSamples, trig) = loop ~ _
with {
    inc = sh(1.0 / max(1.0, durSamples), trig);
    loop(fb) = select2(trig, min(1.0, fb + inc), 0.0);
};

grain(trigPulse) = readAt(pf) : par(i, 2, *(win))
with {

    fl = float(slen(smp));
    lm = min(grainLenEff, max(127, fl - 16));
    grainDur = lm / grainSpeedEff;
    rr = grainPhasor(grainDur, trigPulse);
    startLfoGrain = ba.sAndH(trigPulse, startLfoBlended);
    startLfoUsed = startLfoGrain * (1 - startInterpBlend) + startLfoBlended * startInterpBlend;
    startRead = ma.modulo(startPos + startLfoUsed, 1);
    // Read advances at exactly pitchRatio source-samples per output-sample, so pitch stays tied to
    // the keyboard regardless of grainLength/grainSpeed/grainDensity.
    pf = ma.modulo(pitchRatio * rr * grainDur + startRead * fl, max(127, fl));
    win = sin(rr * ma.PI);

};


// Spread the active grain voices evenly across one density period (hop = period/clockVoices) so
// their grains tile/overlap instead of clustering. Effective grain rate = clockVoices * density,
// which makes grainDensity audibly control how dense the grain cloud is. dispersal permutes the
// even spacing (a textural offset) while keeping full coverage of the period.
voiceClk(fr) = par(i, MAX_CLOCK_VOICES, clk(i))
with {
    periodSamples = (1.0 / max(1e-9, fr)) * ma.SR;
    enabled(i) = float(i < clockVoices);
    spread(i) = min(0.999, max(0.0, ma.frac(float(i * (1 + dispersal)) / float(max(1, clockVoices)))));
    clk(i) = (os.lf_imptrain(fr) : @(int(spread(i) * periodSamples))) * enabled(i);
};

grains = voiceClk(densityEff) : par(i, MAX_CLOCK_VOICES, grain) :> _,_;

env = en.adsre(att, dec, sus, rel, gate);

process = grains
    : par(i, 2, *(env * master))
    : par(i, 2, fi.dcblocker)
    : par(i, 2, clamp(-1, 1));

effect = ef.softclipQuadratic;
