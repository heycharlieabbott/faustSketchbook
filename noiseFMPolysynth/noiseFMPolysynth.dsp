declare options "[midi:on][nvoices:10]";

import("stdfaust.lib");
import("utils.dsp");

gain = hslider("gain", 0.25, 0, 1, 0.01);
gain2 = hslider("gain2", 0.25, 0, 1, 0.01);
gate = button("gate");
freq = hslider("v:Osc/freq [unit:Hz] [midi:keyon]", 440, 20, 20000, 0.1) : si.polySmooth(gate, 0.999, 32);

att = max(0.005, hslider("v:A/attack [unit:s]", 0.02, 0.001, 2, 0.001));
dec = max(0.005, hslider("v:A/decay [unit:s]", 0.1, 0.001, 2, 0.001));
sus = hslider("v:A/sustain", 0.7, 0, 1, 0.01);
rel = max(0.03, hslider("v:A/release [unit:s]", 0.09, 0.03, 10, 0.01));

noiseVol = hslider("v:B/noiseVol", 0.007, 0, 5, 0.1);
noiseFreq = hslider("v:B/noiseFreq", 1, 0, 100, 0.1);

mod = hslider("v:B/mod", 1., 0, 300., 1.);
modVol = hslider("v:B/modVol", 1., 0., 300., 1.);
modMix = hslider("v:B/modMix", 1., 0., 300., 1.);
modOffset = hslider("v:B/modOffset", 0., 0., 100., .001);

modDelaySig = hslider("v:C/modDelaySig", 1., 0., 100., .001);
modDelayVol = hslider("v:C/modDelayVol", 0.5, 0., 2., .001);
modDelayFb = hslider("v:C/modDelayFb", 0., 0., 0.9, .001);

minClip = hslider("v:D/minClip", -1., -1., 1., 0.001);
maxClip = hslider("v:D/maxClip", 1., -1., 1., 0.001);

env = en.adsre(att, dec, sus, rel, gate);
modOsc = os.osc(mod) * modVol * noiseVol;
noiseOsc = no.lfnoise(freq * noiseFreq);

oscLeft = os.polyblep_saw(freq + modOsc * noiseOsc) * env * gain2;
oscRight = os.polyblep_square(freq + modOffset + modOsc * noiseOsc) * env * gain2;

mixSignal = os.osc(modMix);

delayTimeSignal = os.osc(modDelaySig) * modDelayVol;

intLeft = si.interpolate(mixSignal, oscLeft, oscRight);
intRight = si.interpolate(mixSignal, oscRight, oscLeft);

intDelLeft = intLeft : ef.echo(1, delayTimeSignal * 0.75, modDelayFb);
intDelRight = intRight: ef.echo(1, delayTimeSignal, modDelayFb);

stereo = intDelLeft, intDelRight;

output = stereo : par(i, 2, clamp(minClip, maxClip));


process = output;
