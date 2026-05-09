import("stdfaust.lib");

declare name "Reverb Effect 1";
declare version "0.1";
declare description "Basic from-scratch algorithmic stereo reverb.";

MAX_COMBS = 50;
time = hslider("time[s]", 2.5, 0., 50.0, 0.01);
mix  = hslider("mix", 0.35, 0.0, 1.0, 0.01);
combs = int(hslider("combs", 4, 1, MAX_COMBS, 1));
noiseFreq = hslider("noiseFreq", 0., 0.0, 1000.0, 0.01);
noiseAmp = hslider("noiseAmp", 0., 0.0, 1000.0, 0.01);
damp = hslider("damp", 0., 0.0, 1.0, 0.001);
leftStart = int(hslider("leftStart", 100, 1, 5000, 1));
rightStart = int(hslider("rightStart", 100, 1, 5000, 1));
// Clamp base delay (samples) — invalid @(...) lengths cannot be recovered from the UI alone.
guardStart(z) = max(1., float(z));

// One-pole lowpass used inside feedback loops to tame high frequencies.
loopDamp(d) = *(1.0 - d) : + ~ *(d);

// Feedback comb: y[n] = x[n] + g * LP(y[n - N]).
comb(delaySamples, g, damp) = + ~ (@(int(max(1., delaySamples + (os.osc(no.lfnoise(noiseFreq)) * noiseAmp)))) : loopDamp(damp) : *(g));

feedback(delaySamples) = pow(0.001, float(delaySamples) / (time * ma.SR));

tapDelay(base, i) = base + 137 * i + 17 * i * i;
tapActive(i) = float(i < combs);

reverbMono(base, damp) =
    _ <: par(i, MAX_COMBS,
        comb(tapDelay(base, i + 1), feedback(tapDelay(base, i + 1)), damp) * tapActive(i)
    )
      :> /(float(combs));

// Combs mix input into their output (y = x + feedback), so the bank still carries unity dry pre.
// wetOnly restores true wet: monoOut - dryPre.
wetOnly(core, damp) = _ <: _,_ : reverbMono(core, damp), *( -1.0) : +;

xfeedAmt = 0.20;

wetStereo =
    // Crossfeed: L_in = L + x*R, R_in = R + x*L — group so each + is strictly binary.
    _,_ <: ((_, *(xfeedAmt)) : +), ((*(xfeedAmt), _) : +)
        <: wetOnly(guardStart(leftStart), damp), wetOnly(guardStart(rightStart), damp);

process = _,_ <: _,_, wetStereo
        : *(1.0 - mix), *(1.0 - mix), *(mix), *(mix)
        :> _,_ : ef.softclipQuadratic, ef.softclipQuadratic;
