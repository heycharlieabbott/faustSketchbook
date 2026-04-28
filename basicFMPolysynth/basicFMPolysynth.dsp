declare options "[midi:on][nvoices:10]";

import("stdfaust.lib");

gain = hslider("gain", 0.25, 0, 1, 0.01);

freq = hslider("v:Osc/freq [unit:Hz] [midi:keyon]", 440, 20, 20000, 0.1) : si.polySmooth(gate, 0.999, 32);
gate = button("gate");

att = max(0.005, hslider("v:Env/attack [unit:s]", 0.02, 0.001, 2, 0.001));
dec = max(0.005, hslider("v:Env/decay [unit:s]", 0.1, 0.001, 2, 0.001));
sus = hslider("v:Env/sustain", 0.7, 0, 1, 0.01);
rel = max(0.03, hslider("v:Env/release [unit:s]", 0.35, 0.03, 10, 0.01));

mod = hslider("v:Mod/mod", 1., 0, 300., 1.);
modVol = hslider("v:Mod/modVol", 1., 0., 300., 1.);

env = en.adsre(att, dec, sus, rel, gate);

modOsc = os.osc(mod) * modVol;

osc = os.osc(freq + modOsc) * env * gain;
stereo = osc <: _,_;

process = stereo;
