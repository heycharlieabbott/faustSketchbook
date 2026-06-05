import("stdfaust.lib");

declare name "Stereo Echo Delay 2";
declare version "0.1";

maxDelayMs = 2000.0;
maxDelaySamples = int(ma.SR * (maxDelayMs / 1000.0));
maxOffSamples = int(192000.0 * (maxDelayMs / 1000.0)); // offset slider range (compile-time)

minDiv = 45;

tempo = hslider("Tempo [unit:bpm]", 120, 20, 200, 1);
intervalL = int(hslider("L Interval (1/16 notes)", 4, 1, minDiv, 1)) : si.smoo;
intervalR = int(hslider("R Interval (1/16 notes)", 4, 1, minDiv, 1)) : si.smoo;
feedback = hslider("Feedback", 0.35, 0.0, 2.5, 0.01);
mix = hslider("Mix", 0.4, 0.0, 1.0, 0.01);
bpLow  = hslider("Feedback BPF Low[unit:Hz]", 200, 20, 8000, 1) : si.smoo;
bpHigh = hslider("Feedback BPF High[unit:Hz]", 4000, 100, 12000, 1) : si.smoo;

// Sixteenth-note grid: delay = (60 / bpm) * (interval / 4) seconds.
sixteenthSamples = ma.SR * 60.0 / max(1.0, tempo) / 4.0;
delaySamplesL = int(min(maxDelaySamples, max(0, sixteenthSamples / intervalL / 0.25)));
delaySamplesR = int(min(maxDelaySamples, max(0, sixteenthSamples / intervalR / 0.25)));

echoVoice(x, delaySamples) = (x * (1.0 - mix)) + (wet * mix)
with {
  modDelaySamples = int(delaySamples);
  wet = x : (+ ~ feedbackPath) : de.delay(maxDelaySamples, modDelaySamples) : fi.bandpass12e(bpLow, bpHigh);
  feedbackPath = *(feedback)
    : fi.bandpass12e(bpLow, bpHigh)
    : de.delay(maxDelaySamples, modDelaySamples);

};

process(inL, inR) = (echoVoice(inL, delaySamplesL), echoVoice(inR, delaySamplesR))
  : (ef.softclipQuadratic, ef.softclipQuadratic);
