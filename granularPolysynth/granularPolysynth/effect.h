/* ------------------------------------------------------------
name: "effect"
Code generated with Faust 2.85.5 (https://faust.grame.fr)
Compilation options: -lang cpp -i -fpga-mem-th 4 -ct 1 -cn effect -scn base_dsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __effect_H__
#define  __effect_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>

#ifndef FAUSTCLASS 
#define FAUSTCLASS effect
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

#if defined(_WIN32)
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif

static float effect_faustpower2_f(float value) {
	return value * value;
}

class effect : public base_dsp {
	
 private:
	
	int fSampleRate;
	
 public:
	effect() {
	}
	
	effect(const effect&) = default;
	
	virtual ~effect() = default;
	
	effect& operator=(const effect&) = default;
	
	void metadata(Meta* m) { 
		m->declare("basics.lib/ifNc:author", "Oleg Nesterov");
		m->declare("basics.lib/ifNc:copyright", "Copyright (C) 2023 Oleg Nesterov <oleg@redhat.com>");
		m->declare("basics.lib/ifNc:license", "MIT-style STK-4.3 license");
		m->declare("basics.lib/ifNcNo:author", "Oleg Nesterov");
		m->declare("basics.lib/ifNcNo:copyright", "Copyright (C) 2023 Oleg Nesterov <oleg@redhat.com>");
		m->declare("basics.lib/ifNcNo:license", "MIT-style STK-4.3 license");
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.22.0");
		m->declare("compile_options", "-lang cpp -i -fpga-mem-th 4 -ct 1 -cn effect -scn base_dsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0");
		m->declare("filename", "effect.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LGPL with exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.0");
		m->declare("misceffects.lib/name", "Misc Effects Library");
		m->declare("misceffects.lib/softclipQuadratic:author", "David Braun");
		m->declare("misceffects.lib/softclipQuadratic:copyright", "Copyright (C) 2024 David Braun");
		m->declare("misceffects.lib/softclipQuadratic:license", "MIT license");
		m->declare("misceffects.lib/version", "2.5.2");
		m->declare("name", "effect");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/version", "1.6.0");
	}

	virtual int getNumInputs() {
		return 2;
	}
	virtual int getNumOutputs() {
		return 2;
	}
	
	static void classInit(int sample_rate) {
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
	}
	
	virtual void instanceResetUserInterface() {
	}
	
	virtual void instanceClear() {
	}
	
	virtual void init(int sample_rate) {
		classInit(sample_rate);
		instanceInit(sample_rate);
	}
	
	virtual void instanceInit(int sample_rate) {
		instanceConstants(sample_rate);
		instanceResetUserInterface();
		instanceClear();
	}
	
	virtual effect* clone() {
		return new effect(*this);
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("effect");
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			float fTemp0 = static_cast<float>(input0[i0]);
			float fTemp1 = std::fabs(fTemp0);
			float fTemp2 = static_cast<float>((fTemp0 > 0.0f) - (fTemp0 < 0.0f));
			output0[i0] = static_cast<FAUSTFLOAT>(((fTemp1 < 0.33333334f) ? 2.0f * fTemp0 : ((fTemp1 <= 0.6666667f) ? 0.33333334f * fTemp2 * (3.0f - effect_faustpower2_f(2.0f - 3.0f * fTemp1)) : fTemp2)));
			float fTemp3 = static_cast<float>(input1[i0]);
			float fTemp4 = std::fabs(fTemp3);
			float fTemp5 = static_cast<float>((fTemp3 > 0.0f) - (fTemp3 < 0.0f));
			output1[i0] = static_cast<FAUSTFLOAT>(((fTemp4 < 0.33333334f) ? 2.0f * fTemp3 : ((fTemp4 <= 0.6666667f) ? 0.33333334f * fTemp5 * (3.0f - effect_faustpower2_f(2.0f - 3.0f * fTemp4)) : fTemp5)));
		}
	}

};

#endif
