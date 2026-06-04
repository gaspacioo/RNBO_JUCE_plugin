/*******************************************************************************************************************
Copyright (c) 2023 Cycling '74

The code that Max generates automatically and that end users are capable of
exporting and using, and any associated documentation files (the “Software”)
is a work of authorship for which Cycling '74 is the author and owner for
copyright purposes.

This Software is dual-licensed either under the terms of the Cycling '74
License for Max-Generated Code for Export, or alternatively under the terms
of the General Public License (GPL) Version 3. You may use the Software
according to either of these licenses as it is most appropriate for your
project on a case-by-case basis (proprietary or not).

A) Cycling '74 License for Max-Generated Code for Export

A license is hereby granted, free of charge, to any person obtaining a copy
of the Software (“Licensee”) to use, copy, modify, merge, publish, and
distribute copies of the Software, and to permit persons to whom the Software
is furnished to do so, subject to the following conditions:

The Software is licensed to Licensee for all uses that do not include the sale,
sublicensing, or commercial distribution of software that incorporates this
source code. This means that the Licensee is free to use this software for
educational, research, and prototyping purposes, to create musical or other
creative works with software that incorporates this source code, or any other
use that does not constitute selling software that makes use of this source
code. Commercial distribution also includes the packaging of free software with
other paid software, hardware, or software-provided commercial services.

For entities with UNDER $200k in annual revenue or funding, a license is hereby
granted, free of charge, for the sale, sublicensing, or commercial distribution
of software that incorporates this source code, for as long as the entity's
annual revenue remains below $200k annual revenue or funding.

For entities with OVER $200k in annual revenue or funding interested in the
sale, sublicensing, or commercial distribution of software that incorporates
this source code, please send inquiries to licensing@cycling74.com.

The above copyright notice and this license shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Please see
https://support.cycling74.com/hc/en-us/articles/10730637742483-RNBO-Export-Licensing-FAQ
for additional information

B) General Public License Version 3 (GPLv3)
Details of the GPLv3 license can be found at: https://www.gnu.org/licenses/gpl-3.0.html
*******************************************************************************************************************/

#include "RNBO_Common.h"
#include "RNBO_AudioSignal.h"

namespace RNBO {


#define trunc(x) ((Int)(x))

#if defined(__GNUC__) || defined(__clang__)
    #define RNBO_RESTRICT __restrict__
#elif defined(_MSC_VER)
    #define RNBO_RESTRICT __restrict
#endif

#define FIXEDSIZEARRAYINIT(...) { }

class rnbomatic : public PatcherInterfaceImpl {
public:

rnbomatic()
{
}

~rnbomatic()
{
}

rnbomatic* getTopLevelPatcher() {
    return this;
}

void cancelClockEvents()
{
    getEngine()->flushClockEvents(this, 879365773, false);
    getEngine()->flushClockEvents(this, 1033938262, false);
    getEngine()->flushClockEvents(this, 1220262738, false);
    getEngine()->flushClockEvents(this, 2018930062, false);
    getEngine()->flushClockEvents(this, -2121464745, false);
    getEngine()->flushClockEvents(this, 848255507, false);
    getEngine()->flushClockEvents(this, -1136472945, false);
    getEngine()->flushClockEvents(this, 3091344, false);
    getEngine()->flushClockEvents(this, 1142655633, false);
    getEngine()->flushClockEvents(this, -281953904, false);
}

template <typename T> void listquicksort(T& arr, T& sortindices, Int l, Int h, bool ascending) {
    if (l < h) {
        Int p = (Int)(this->listpartition(arr, sortindices, l, h, ascending));
        this->listquicksort(arr, sortindices, l, p - 1, ascending);
        this->listquicksort(arr, sortindices, p + 1, h, ascending);
    }
}

template <typename T> Int listpartition(T& arr, T& sortindices, Int l, Int h, bool ascending) {
    number x = arr[(Index)h];
    Int i = (Int)(l - 1);

    for (Int j = (Int)(l); j <= h - 1; j++) {
        bool asc = (bool)((bool)(ascending) && arr[(Index)j] <= x);
        bool desc = (bool)((bool)(!(bool)(ascending)) && arr[(Index)j] >= x);

        if ((bool)(asc) || (bool)(desc)) {
            i++;
            this->listswapelements(arr, i, j);
            this->listswapelements(sortindices, i, j);
        }
    }

    i++;
    this->listswapelements(arr, i, h);
    this->listswapelements(sortindices, i, h);
    return i;
}

template <typename T> void listswapelements(T& arr, Int a, Int b) {
    auto tmp = arr[(Index)a];
    arr[(Index)a] = arr[(Index)b];
    arr[(Index)b] = tmp;
}

inline number safesqrt(number num) {
    return (num > 0.0 ? rnbo_sqrt(num) : 0.0);
}

MillisecondTime currenttime() {
    return this->_currentTime;
}

inline number linearinterp(number frac, number x, number y) {
    return x + (y - x) * frac;
}

inline number cubicinterp(number a, number w, number x, number y, number z) {
    number a1 = 1. + a;
    number aa = a * a1;
    number b = 1. - a;
    number b1 = 2. - a;
    number bb = b * b1;
    number fw = -.1666667 * bb * a;
    number fx = .5 * bb * a1;
    number fy = .5 * aa * b1;
    number fz = -.1666667 * aa * b;
    return w * fw + x * fx + y * fy + z * fz;
}

inline number fastcubicinterp(number a, number w, number x, number y, number z) {
    number a2 = a * a;
    number f0 = z - y - w + x;
    number f1 = w - x - f0;
    number f2 = y - w;
    number f3 = x;
    return f0 * a * a2 + f1 * a2 + f2 * a + f3;
}

inline number splineinterp(number a, number w, number x, number y, number z) {
    number a2 = a * a;
    number f0 = -0.5 * w + 1.5 * x - 1.5 * y + 0.5 * z;
    number f1 = w - 2.5 * x + 2 * y - 0.5 * z;
    number f2 = -0.5 * w + 0.5 * y;
    return f0 * a * a2 + f1 * a2 + f2 * a + x;
}

inline number spline6interp(number a, number y0, number y1, number y2, number y3, number y4, number y5) {
    number ym2py2 = y0 + y4;
    number ym1py1 = y1 + y3;
    number y2mym2 = y4 - y0;
    number y1mym1 = y3 - y1;
    number sixthym1py1 = (number)1 / (number)6.0 * ym1py1;
    number c0 = (number)1 / (number)120.0 * ym2py2 + (number)13 / (number)60.0 * ym1py1 + (number)11 / (number)20.0 * y2;
    number c1 = (number)1 / (number)24.0 * y2mym2 + (number)5 / (number)12.0 * y1mym1;
    number c2 = (number)1 / (number)12.0 * ym2py2 + sixthym1py1 - (number)1 / (number)2.0 * y2;
    number c3 = (number)1 / (number)12.0 * y2mym2 - (number)1 / (number)6.0 * y1mym1;
    number c4 = (number)1 / (number)24.0 * ym2py2 - sixthym1py1 + (number)1 / (number)4.0 * y2;
    number c5 = (number)1 / (number)120.0 * (y5 - y0) + (number)1 / (number)24.0 * (y1 - y4) + (number)1 / (number)12.0 * (y3 - y2);
    return ((((c5 * a + c4) * a + c3) * a + c2) * a + c1) * a + c0;
}

inline number cosT8(number r) {
    number t84 = 56.0;
    number t83 = 1680.0;
    number t82 = 20160.0;
    number t81 = 2.4801587302e-05;
    number t73 = 42.0;
    number t72 = 840.0;
    number t71 = 1.9841269841e-04;

    if (r < 0.785398163397448309615660845819875721 && r > -0.785398163397448309615660845819875721) {
        number rr = r * r;
        return 1.0 - rr * t81 * (t82 - rr * (t83 - rr * (t84 - rr)));
    } else if (r > 0.0) {
        r -= 1.57079632679489661923132169163975144;
        number rr = r * r;
        return -r * (1.0 - t71 * rr * (t72 - rr * (t73 - rr)));
    } else {
        r += 1.57079632679489661923132169163975144;
        number rr = r * r;
        return r * (1.0 - t71 * rr * (t72 - rr * (t73 - rr)));
    }
}

inline number cosineinterp(number frac, number x, number y) {
    number a2 = (1.0 - this->cosT8(frac * 3.14159265358979323846)) / (number)2.0;
    return x * (1.0 - a2) + y * a2;
}

number mstosamps(MillisecondTime ms) {
    return ms * this->sr * 0.001;
}

number samplerate() const {
    return this->sr;
}

Index vectorsize() const {
    return this->vs;
}

number maximum(number x, number y) {
    return (x < y ? y : x);
}

SampleIndex currentsampletime() {
    return this->audioProcessSampleCount + this->sampleOffsetIntoNextAudioBuffer;
}

inline number safediv(number num, number denom) {
    return (denom == 0.0 ? 0.0 : num / denom);
}

MillisecondTime sampstoms(number samps) {
    return samps * 1000 / this->sr;
}

Index getNumMidiInputPorts() const {
    return 0;
}

void processMidiEvent(MillisecondTime , int , ConstByteArray , Index ) {}

Index getNumMidiOutputPorts() const {
    return 0;
}

void process(
    const SampleValue * const* inputs,
    Index numInputs,
    SampleValue * const* outputs,
    Index numOutputs,
    Index n
) {
    this->vs = n;
    this->updateTime(this->getEngine()->getCurrentTime());
    SampleValue * out1 = (numOutputs >= 1 && outputs[0] ? outputs[0] : this->dummyBuffer);
    SampleValue * out2 = (numOutputs >= 2 && outputs[1] ? outputs[1] : this->dummyBuffer);
    const SampleValue * in1 = (numInputs >= 1 && inputs[0] ? inputs[0] : this->zeroBuffer);
    const SampleValue * in2 = (numInputs >= 2 && inputs[1] ? inputs[1] : this->zeroBuffer);
    this->delaytilde_01_perform(this->delaytilde_01_delay, in1, this->signals[0], n);
    this->selector_01_perform(this->selector_01_onoff, in1, this->signals[0], this->signals[1], n);
    this->dspexpr_07_perform(this->signals[1], this->dspexpr_07_in2, this->signals[0], n);

    this->average_rms_tilde_02_perform(
        in1,
        this->average_rms_tilde_02_windowSize,
        this->average_rms_tilde_02_reset,
        this->signals[1],
        n
    );

    this->snapshot_02_perform(this->signals[1], n);
    this->delaytilde_02_perform(this->delaytilde_02_delay, in2, this->signals[1], n);
    this->selector_02_perform(this->selector_02_onoff, in2, this->signals[1], this->signals[2], n);
    this->dspexpr_11_perform(this->signals[2], this->dspexpr_11_in2, this->signals[1], n);
    this->dspexpr_10_perform(this->signals[0], this->signals[1], this->signals[2], n);
    this->dspexpr_14_perform(this->signals[2], this->dspexpr_14_in2, this->signals[3], n);
    this->dspexpr_13_perform(this->signals[3], this->dspexpr_13_in2, this->signals[2], n);
    this->dspexpr_06_perform(this->signals[0], this->signals[1], this->signals[3], n);
    this->dspexpr_03_perform(this->signals[3], this->dspexpr_03_in2, this->signals[1], n);
    this->dspexpr_02_perform(this->signals[1], this->dspexpr_02_in2, this->signals[3], n);
    this->dspexpr_12_perform(this->signals[3], this->signals[2], this->signals[1], n);
    this->dspexpr_09_perform(this->signals[1], this->dspexpr_09_in2, this->signals[0], n);
    this->dspexpr_01_perform(this->signals[3], this->signals[2], this->signals[1], n);
    this->dspexpr_04_perform(this->signals[1], this->dspexpr_04_in2, this->signals[2], n);

    this->average_rms_tilde_04_perform(
        in2,
        this->average_rms_tilde_04_windowSize,
        this->average_rms_tilde_04_reset,
        this->signals[1],
        n
    );

    this->snapshot_04_perform(this->signals[1], n);
    this->linetilde_01_perform(this->signals[1], n);
    this->dspexpr_08_perform(this->signals[0], this->signals[1], this->signals[3], n);

    this->average_rms_tilde_03_perform(
        this->signals[3],
        this->average_rms_tilde_03_windowSize,
        this->average_rms_tilde_03_reset,
        this->signals[0],
        n
    );

    this->snapshot_03_perform(this->signals[0], n);
    this->signalforwarder_01_perform(this->signals[3], out2, n);
    this->dspexpr_05_perform(this->signals[2], this->signals[1], this->signals[4], n);
    this->dspexpr_20_perform(this->signals[4], this->signals[3], this->signals[1], n);
    this->snapshot_07_perform(this->signals[1], n);
    this->dspexpr_19_perform(this->signals[4], this->signals[3], this->signals[1], n);
    this->snapshot_06_perform(this->signals[1], n);
    this->dspexpr_15_perform(this->signals[4], this->signals[3], this->signals[1], n);

    this->slide_tilde_01_perform(
        this->signals[1],
        this->slide_tilde_01_up,
        this->slide_tilde_01_down,
        this->signals[3],
        n
    );

    this->average_rms_tilde_01_perform(
        this->signals[4],
        this->average_rms_tilde_01_windowSize,
        this->average_rms_tilde_01_reset,
        this->signals[1],
        n
    );

    this->snapshot_01_perform(this->signals[1], n);
    this->dspexpr_18_perform(this->signals[1], this->signals[0], this->signals[2], n);
    this->dspexpr_17_perform(this->signals[2], this->dspexpr_17_in2, this->signals[0], n);

    this->slide_tilde_02_perform(
        this->signals[0],
        this->slide_tilde_02_up,
        this->slide_tilde_02_down,
        this->signals[2],
        n
    );

    this->dspexpr_16_perform(this->signals[3], this->signals[2], this->signals[0], n);
    this->snapshot_05_perform(this->signals[0], n);
    this->signalforwarder_02_perform(this->signals[4], out1, n);
    this->stackprotect_perform(n);
    this->globaltransport_advance();
    this->audioProcessSampleCount += this->vs;
}

void prepareToProcess(number sampleRate, Index maxBlockSize, bool force) {
    if (this->maxvs < maxBlockSize || !this->didAllocateSignals) {
        Index i;

        for (i = 0; i < 5; i++) {
            this->signals[i] = resizeSignal(this->signals[i], this->maxvs, maxBlockSize);
        }

        this->globaltransport_tempo = resizeSignal(this->globaltransport_tempo, this->maxvs, maxBlockSize);
        this->globaltransport_state = resizeSignal(this->globaltransport_state, this->maxvs, maxBlockSize);
        this->zeroBuffer = resizeSignal(this->zeroBuffer, this->maxvs, maxBlockSize);
        this->dummyBuffer = resizeSignal(this->dummyBuffer, this->maxvs, maxBlockSize);
        this->didAllocateSignals = true;
    }

    const bool sampleRateChanged = sampleRate != this->sr;
    const bool maxvsChanged = maxBlockSize != this->maxvs;
    const bool forceDSPSetup = sampleRateChanged || maxvsChanged || force;

    if (sampleRateChanged || maxvsChanged) {
        this->vs = maxBlockSize;
        this->maxvs = maxBlockSize;
        this->sr = sampleRate;
        this->invsr = 1 / sampleRate;
    }

    this->delaytilde_01_dspsetup(forceDSPSetup);
    this->average_rms_tilde_02_dspsetup(forceDSPSetup);
    this->delaytilde_02_dspsetup(forceDSPSetup);
    this->average_rms_tilde_04_dspsetup(forceDSPSetup);
    this->average_rms_tilde_03_dspsetup(forceDSPSetup);
    this->average_rms_tilde_01_dspsetup(forceDSPSetup);
    this->globaltransport_dspsetup(forceDSPSetup);

    if (sampleRateChanged)
        this->onSampleRateChanged(sampleRate);
}

void setProbingTarget(MessageTag id) {
    switch (id) {
    default:
        {
        this->setProbingIndex(-1);
        break;
        }
    }
}

void setProbingIndex(ProbingIndex ) {}

Index getProbingChannels(MessageTag outletId) const {
    RNBO_UNUSED(outletId);
    return 0;
}

DataRef* getDataRef(DataRefIndex index)  {
    switch (index) {
    case 0:
        {
        return addressOf(this->average_rms_tilde_01_av_bufferobj);
        break;
        }
    case 1:
        {
        return addressOf(this->average_rms_tilde_02_av_bufferobj);
        break;
        }
    case 2:
        {
        return addressOf(this->average_rms_tilde_03_av_bufferobj);
        break;
        }
    case 3:
        {
        return addressOf(this->average_rms_tilde_04_av_bufferobj);
        break;
        }
    case 4:
        {
        return addressOf(this->delaytilde_01_del_bufferobj);
        break;
        }
    case 5:
        {
        return addressOf(this->delaytilde_02_del_bufferobj);
        break;
        }
    default:
        {
        return nullptr;
        }
    }
}

DataRefIndex getNumDataRefs() const {
    return 6;
}

void fillDataRef(DataRefIndex , DataRef& ) {}

void zeroDataRef(DataRef& ref) {
    ref->setZero();
}

void processDataViewUpdate(DataRefIndex index, MillisecondTime time) {
    this->updateTime(time);

    if (index == 0) {
        this->average_rms_tilde_01_av_buffer = new Float64Buffer(this->average_rms_tilde_01_av_bufferobj);
    }

    if (index == 1) {
        this->average_rms_tilde_02_av_buffer = new Float64Buffer(this->average_rms_tilde_02_av_bufferobj);
    }

    if (index == 2) {
        this->average_rms_tilde_03_av_buffer = new Float64Buffer(this->average_rms_tilde_03_av_bufferobj);
    }

    if (index == 3) {
        this->average_rms_tilde_04_av_buffer = new Float64Buffer(this->average_rms_tilde_04_av_bufferobj);
    }

    if (index == 4) {
        this->delaytilde_01_del_buffer = new Float64Buffer(this->delaytilde_01_del_bufferobj);
    }

    if (index == 5) {
        this->delaytilde_02_del_buffer = new Float64Buffer(this->delaytilde_02_del_bufferobj);
    }
}

void initialize() {
    this->average_rms_tilde_01_av_bufferobj = initDataRef("average_rms_tilde_01_av_bufferobj", true, nullptr, "buffer~");
    this->average_rms_tilde_02_av_bufferobj = initDataRef("average_rms_tilde_02_av_bufferobj", true, nullptr, "buffer~");
    this->average_rms_tilde_03_av_bufferobj = initDataRef("average_rms_tilde_03_av_bufferobj", true, nullptr, "buffer~");
    this->average_rms_tilde_04_av_bufferobj = initDataRef("average_rms_tilde_04_av_bufferobj", true, nullptr, "buffer~");
    this->delaytilde_01_del_bufferobj = initDataRef("delaytilde_01_del_bufferobj", true, nullptr, "buffer~");
    this->delaytilde_02_del_bufferobj = initDataRef("delaytilde_02_del_bufferobj", true, nullptr, "buffer~");
    this->assign_defaults();
    this->setState();
    this->average_rms_tilde_01_av_bufferobj->setIndex(0);
    this->average_rms_tilde_01_av_buffer = new Float64Buffer(this->average_rms_tilde_01_av_bufferobj);
    this->average_rms_tilde_02_av_bufferobj->setIndex(1);
    this->average_rms_tilde_02_av_buffer = new Float64Buffer(this->average_rms_tilde_02_av_bufferobj);
    this->average_rms_tilde_03_av_bufferobj->setIndex(2);
    this->average_rms_tilde_03_av_buffer = new Float64Buffer(this->average_rms_tilde_03_av_bufferobj);
    this->average_rms_tilde_04_av_bufferobj->setIndex(3);
    this->average_rms_tilde_04_av_buffer = new Float64Buffer(this->average_rms_tilde_04_av_bufferobj);
    this->delaytilde_01_del_bufferobj->setIndex(4);
    this->delaytilde_01_del_buffer = new Float64Buffer(this->delaytilde_01_del_bufferobj);
    this->delaytilde_02_del_bufferobj->setIndex(5);
    this->delaytilde_02_del_buffer = new Float64Buffer(this->delaytilde_02_del_bufferobj);
    this->initializeObjects();
    this->allocateDataRefs();
    this->startup();
}

Index getIsMuted()  {
    return this->isMuted;
}

void setIsMuted(Index v)  {
    this->isMuted = v;
}

void onSampleRateChanged(double ) {}

Index getPatcherSerial() const {
    return 0;
}

void getState(PatcherStateInterface& ) {}

void setState() {}

void getPreset(PatcherStateInterface& preset) {
    preset["__presetid"] = "rnbo";
    this->param_01_getPresetValue(getSubState(preset, "mid_gain"));
    this->param_02_getPresetValue(getSubState(preset, "mid_mute"));
    this->param_03_getPresetValue(getSubState(preset, "gain"));
    this->param_04_getPresetValue(getSubState(preset, "side_gain"));
    this->param_05_getPresetValue(getSubState(preset, "phase_inv"));
    this->param_06_getPresetValue(getSubState(preset, "distance"));
    this->param_07_getPresetValue(getSubState(preset, "side_mute"));
    this->param_08_getPresetValue(getSubState(preset, "temperature"));
}

void setPreset(MillisecondTime time, PatcherStateInterface& preset) {
    this->updateTime(time);
    this->param_01_setPresetValue(getSubState(preset, "mid_gain"));
    this->param_02_setPresetValue(getSubState(preset, "mid_mute"));
    this->param_03_setPresetValue(getSubState(preset, "gain"));
    this->param_04_setPresetValue(getSubState(preset, "side_gain"));
    this->param_05_setPresetValue(getSubState(preset, "phase_inv"));
    this->param_06_setPresetValue(getSubState(preset, "distance"));
    this->param_07_setPresetValue(getSubState(preset, "side_mute"));
    this->param_08_setPresetValue(getSubState(preset, "temperature"));
}

void setParameterValue(ParameterIndex index, ParameterValue v, MillisecondTime time) {
    this->updateTime(time);

    switch (index) {
    case 0:
        {
        this->param_01_value_set(v);
        break;
        }
    case 1:
        {
        this->param_02_value_set(v);
        break;
        }
    case 2:
        {
        this->param_03_value_set(v);
        break;
        }
    case 3:
        {
        this->param_04_value_set(v);
        break;
        }
    case 4:
        {
        this->param_05_value_set(v);
        break;
        }
    case 5:
        {
        this->param_06_value_set(v);
        break;
        }
    case 6:
        {
        this->param_07_value_set(v);
        break;
        }
    case 7:
        {
        this->param_08_value_set(v);
        break;
        }
    }
}

void processParameterEvent(ParameterIndex index, ParameterValue value, MillisecondTime time) {
    this->setParameterValue(index, value, time);
}

void processParameterBangEvent(ParameterIndex index, MillisecondTime time) {
    this->setParameterValue(index, this->getParameterValue(index), time);
}

void processNormalizedParameterEvent(ParameterIndex index, ParameterValue value, MillisecondTime time) {
    this->setParameterValueNormalized(index, value, time);
}

ParameterValue getParameterValue(ParameterIndex index)  {
    switch (index) {
    case 0:
        {
        return this->param_01_value;
        }
    case 1:
        {
        return this->param_02_value;
        }
    case 2:
        {
        return this->param_03_value;
        }
    case 3:
        {
        return this->param_04_value;
        }
    case 4:
        {
        return this->param_05_value;
        }
    case 5:
        {
        return this->param_06_value;
        }
    case 6:
        {
        return this->param_07_value;
        }
    case 7:
        {
        return this->param_08_value;
        }
    default:
        {
        return 0;
        }
    }
}

ParameterIndex getNumSignalInParameters() const {
    return 0;
}

ParameterIndex getNumSignalOutParameters() const {
    return 0;
}

ParameterIndex getNumParameters() const {
    return 8;
}

ConstCharPointer getParameterName(ParameterIndex index) const {
    switch (index) {
    case 0:
        {
        return "mid_gain";
        }
    case 1:
        {
        return "mid_mute";
        }
    case 2:
        {
        return "gain";
        }
    case 3:
        {
        return "side_gain";
        }
    case 4:
        {
        return "phase_inv";
        }
    case 5:
        {
        return "distance";
        }
    case 6:
        {
        return "side_mute";
        }
    case 7:
        {
        return "temperature";
        }
    default:
        {
        return "bogus";
        }
    }
}

ConstCharPointer getParameterId(ParameterIndex index) const {
    switch (index) {
    case 0:
        {
        return "mid_gain";
        }
    case 1:
        {
        return "mid_mute";
        }
    case 2:
        {
        return "gain";
        }
    case 3:
        {
        return "side_gain";
        }
    case 4:
        {
        return "phase_inv";
        }
    case 5:
        {
        return "distance";
        }
    case 6:
        {
        return "side_mute";
        }
    case 7:
        {
        return "temperature";
        }
    default:
        {
        return "bogus";
        }
    }
}

void getParameterInfo(ParameterIndex index, ParameterInfo * info) const {
    {
        switch (index) {
        case 0:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = -60;
            info->max = 12;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 1:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 2;
            static const char * eVal1[] = {"0", "1"};
            info->enumValues = eVal1;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 2:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = -60;
            info->max = 12;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 3:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = -60;
            info->max = 12;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 4:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 2;
            static const char * eVal4[] = {"0", "1"};
            info->enumValues = eVal4;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 5:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = 0;
            info->max = 60;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 6:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 2;
            static const char * eVal6[] = {"0", "1"};
            info->enumValues = eVal6;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 7:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 20;
            info->min = -10;
            info->max = 40;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        }
    }
}

void sendParameter(ParameterIndex index, bool ignoreValue) {
    this->getEngine()->notifyParameterValueChanged(index, (ignoreValue ? 0 : this->getParameterValue(index)), ignoreValue);
}

ParameterValue applyStepsToNormalizedParameterValue(ParameterValue normalizedValue, int steps) const {
    if (steps == 1) {
        if (normalizedValue > 0) {
            normalizedValue = 1.;
        }
    } else {
        ParameterValue oneStep = (number)1. / (steps - 1);
        ParameterValue numberOfSteps = rnbo_fround(normalizedValue / oneStep * 1 / (number)1) * (number)1;
        normalizedValue = numberOfSteps * oneStep;
    }

    return normalizedValue;
}

ParameterValue convertToNormalizedParameterValue(ParameterIndex index, ParameterValue value) const {
    switch (index) {
    case 1:
    case 4:
    case 6:
        {
        {
            value = (value < 0 ? 0 : (value > 1 ? 1 : value));
            ParameterValue normalizedValue = (value - 0) / (1 - 0);

            {
                normalizedValue = this->applyStepsToNormalizedParameterValue(normalizedValue, 2);
            }

            return normalizedValue;
        }
        }
    case 5:
        {
        {
            value = (value < 0 ? 0 : (value > 60 ? 60 : value));
            ParameterValue normalizedValue = (value - 0) / (60 - 0);
            return normalizedValue;
        }
        }
    case 0:
    case 2:
    case 3:
        {
        {
            value = (value < -60 ? -60 : (value > 12 ? 12 : value));
            ParameterValue normalizedValue = (value - -60) / (12 - -60);
            return normalizedValue;
        }
        }
    case 7:
        {
        {
            value = (value < -10 ? -10 : (value > 40 ? 40 : value));
            ParameterValue normalizedValue = (value - -10) / (40 - -10);
            return normalizedValue;
        }
        }
    default:
        {
        return value;
        }
    }
}

ParameterValue convertFromNormalizedParameterValue(ParameterIndex index, ParameterValue value) const {
    value = (value < 0 ? 0 : (value > 1 ? 1 : value));

    switch (index) {
    case 1:
    case 4:
    case 6:
        {
        {
            {
                value = this->applyStepsToNormalizedParameterValue(value, 2);
            }

            {
                return 0 + value * (1 - 0);
            }
        }
        }
    case 5:
        {
        {
            {
                return 0 + value * (60 - 0);
            }
        }
        }
    case 0:
    case 2:
    case 3:
        {
        {
            {
                return -60 + value * (12 - -60);
            }
        }
        }
    case 7:
        {
        {
            {
                return -10 + value * (40 - -10);
            }
        }
        }
    default:
        {
        return value;
        }
    }
}

ParameterValue constrainParameterValue(ParameterIndex index, ParameterValue value) const {
    switch (index) {
    case 0:
        {
        return this->param_01_value_constrain(value);
        }
    case 1:
        {
        return this->param_02_value_constrain(value);
        }
    case 2:
        {
        return this->param_03_value_constrain(value);
        }
    case 3:
        {
        return this->param_04_value_constrain(value);
        }
    case 4:
        {
        return this->param_05_value_constrain(value);
        }
    case 5:
        {
        return this->param_06_value_constrain(value);
        }
    case 6:
        {
        return this->param_07_value_constrain(value);
        }
    case 7:
        {
        return this->param_08_value_constrain(value);
        }
    default:
        {
        return value;
        }
    }
}

void scheduleParamInit(ParameterIndex index, Index order) {
    this->paramInitIndices->push(index);
    this->paramInitOrder->push(order);
}

void processParamInitEvents() {
    this->listquicksort(
        this->paramInitOrder,
        this->paramInitIndices,
        0,
        (int)(this->paramInitOrder->length - 1),
        true
    );

    for (Index i = 0; i < this->paramInitOrder->length; i++) {
        this->getEngine()->scheduleParameterBang(this->paramInitIndices[i], 0);
    }
}

void processClockEvent(MillisecondTime time, ClockId index, bool hasValue, ParameterValue value) {
    RNBO_UNUSED(hasValue);
    this->updateTime(time);

    switch (index) {
    case 879365773:
        {
        this->snapshot_01_out_set(value);
        break;
        }
    case 1033938262:
        {
        this->snapshot_02_out_set(value);
        break;
        }
    case 1220262738:
        {
        this->line_01_tick_set(value);
        break;
        }
    case 2018930062:
        {
        this->snapshot_03_out_set(value);
        break;
        }
    case -2121464745:
        {
        this->snapshot_04_out_set(value);
        break;
        }
    case 848255507:
        {
        this->line_02_tick_set(value);
        break;
        }
    case -1136472945:
        {
        this->snapshot_05_out_set(value);
        break;
        }
    case 3091344:
        {
        this->snapshot_06_out_set(value);
        break;
        }
    case 1142655633:
        {
        this->snapshot_07_out_set(value);
        break;
        }
    case -281953904:
        {
        this->linetilde_01_target_bang();
        break;
        }
    }
}

void processOutletAtCurrentTime(EngineLink* , OutletIndex , ParameterValue ) {}

void processOutletEvent(
    EngineLink* sender,
    OutletIndex index,
    ParameterValue value,
    MillisecondTime time
) {
    this->updateTime(time);
    this->processOutletAtCurrentTime(sender, index, value);
}

void processNumMessage(MessageTag , MessageTag , MillisecondTime , number ) {}

void processListMessage(MessageTag , MessageTag , MillisecondTime , const list& ) {}

void processBangMessage(MessageTag , MessageTag , MillisecondTime ) {}

MessageTagInfo resolveTag(MessageTag tag) const {
    switch (tag) {
    case TAG("out_rms_L"):
        {
        return "out_rms_L";
        }
    case TAG(""):
        {
        return "";
        }
    case TAG("in_rms_L"):
        {
        return "in_rms_L";
        }
    case TAG("out_rms_R"):
        {
        return "out_rms_R";
        }
    case TAG("in_rms_R"):
        {
        return "in_rms_R";
        }
    case TAG("correlation_value"):
        {
        return "correlation_value";
        }
    case TAG("delay_time"):
        {
        return "delay_time";
        }
    case TAG("scopeY"):
        {
        return "scopeY";
        }
    case TAG("scopeX"):
        {
        return "scopeX";
        }
    }

    return "";
}

MessageIndex getNumMessages() const {
    return 8;
}

const MessageInfo& getMessageInfo(MessageIndex index) const {
    switch (index) {
    case 0:
        {
        static const MessageInfo r0 = {
            "out_rms_L",
            Outport
        };

        return r0;
        }
    case 1:
        {
        static const MessageInfo r1 = {
            "in_rms_L",
            Outport
        };

        return r1;
        }
    case 2:
        {
        static const MessageInfo r2 = {
            "out_rms_R",
            Outport
        };

        return r2;
        }
    case 3:
        {
        static const MessageInfo r3 = {
            "in_rms_R",
            Outport
        };

        return r3;
        }
    case 4:
        {
        static const MessageInfo r4 = {
            "correlation_value",
            Outport
        };

        return r4;
        }
    case 5:
        {
        static const MessageInfo r5 = {
            "delay_time",
            Outport
        };

        return r5;
        }
    case 6:
        {
        static const MessageInfo r6 = {
            "scopeY",
            Outport
        };

        return r6;
        }
    case 7:
        {
        static const MessageInfo r7 = {
            "scopeX",
            Outport
        };

        return r7;
        }
    }

    return NullMessageInfo;
}

protected:

void param_01_value_set(number v) {
    v = this->param_01_value_constrain(v);
    this->param_01_value = v;
    this->sendParameter(0, false);

    if (this->param_01_value != this->param_01_lastValue) {
        this->getEngine()->presetTouched();
        this->param_01_lastValue = this->param_01_value;
    }

    this->expr_03_in1_set(v);
}

void param_02_value_set(number v) {
    v = this->param_02_value_constrain(v);
    this->param_02_value = v;
    this->sendParameter(1, false);

    if (this->param_02_value != this->param_02_lastValue) {
        this->getEngine()->presetTouched();
        this->param_02_lastValue = this->param_02_value;
    }

    this->trigger_01_input_number_set(v);
}

void param_03_value_set(number v) {
    v = this->param_03_value_constrain(v);
    this->param_03_value = v;
    this->sendParameter(2, false);

    if (this->param_03_value != this->param_03_lastValue) {
        this->getEngine()->presetTouched();
        this->param_03_lastValue = this->param_03_value;
    }

    this->expr_09_in1_set(v);
}

void param_04_value_set(number v) {
    v = this->param_04_value_constrain(v);
    this->param_04_value = v;
    this->sendParameter(3, false);

    if (this->param_04_value != this->param_04_lastValue) {
        this->getEngine()->presetTouched();
        this->param_04_lastValue = this->param_04_value;
    }

    this->expr_10_in1_set(v);
}

void param_05_value_set(number v) {
    v = this->param_05_value_constrain(v);
    this->param_05_value = v;
    this->sendParameter(4, false);

    if (this->param_05_value != this->param_05_lastValue) {
        this->getEngine()->presetTouched();
        this->param_05_lastValue = this->param_05_value;
    }

    this->expr_11_$in1_set(v);
}

void param_06_value_set(number v) {
    v = this->param_06_value_constrain(v);
    this->param_06_value = v;
    this->sendParameter(5, false);

    if (this->param_06_value != this->param_06_lastValue) {
        this->getEngine()->presetTouched();
        this->param_06_lastValue = this->param_06_value;
    }

    this->expr_13_$in1_set(v);
    this->expr_12_$in1_set(v);
}

void param_07_value_set(number v) {
    v = this->param_07_value_constrain(v);
    this->param_07_value = v;
    this->sendParameter(6, false);

    if (this->param_07_value != this->param_07_lastValue) {
        this->getEngine()->presetTouched();
        this->param_07_lastValue = this->param_07_value;
    }

    this->trigger_02_input_number_set(v);
}

void param_08_value_set(number v) {
    v = this->param_08_value_constrain(v);
    this->param_08_value = v;
    this->sendParameter(7, false);

    if (this->param_08_value != this->param_08_lastValue) {
        this->getEngine()->presetTouched();
        this->param_08_lastValue = this->param_08_value;
    }

    this->trigger_03_input_number_set(v);
}

void snapshot_01_out_set(number v) {
    this->snapshot_01_out = v;
    this->expr_01_in1_set(v);
}

void snapshot_02_out_set(number v) {
    this->snapshot_02_out = v;
    this->expr_02_in1_set(v);
}

void line_01_tick_set(number v) {
    this->line_01_output_set(v);

    if ((bool)(this->line_01_isFinished(v))) {
        this->line_01_slope = 0;
        this->line_01_startValue = v;
        this->line_01_startPendingRamp();
    } else {
        this->line_01_scheduleNext();
    }
}

void snapshot_03_out_set(number v) {
    this->snapshot_03_out = v;
    this->expr_04_in1_set(v);
}

void snapshot_04_out_set(number v) {
    this->snapshot_04_out = v;
    this->expr_05_in1_set(v);
}

void line_02_tick_set(number v) {
    this->line_02_output_set(v);

    if ((bool)(this->line_02_isFinished(v))) {
        this->line_02_slope = 0;
        this->line_02_startValue = v;
        this->line_02_startPendingRamp();
    } else {
        this->line_02_scheduleNext();
    }
}

void snapshot_05_out_set(number v) {
    this->snapshot_05_out = v;
    this->outport_05_input_number_set(v);
}

void snapshot_06_out_set(number v) {
    this->snapshot_06_out = v;
    this->outport_07_input_number_set(v);
}

void snapshot_07_out_set(number v) {
    this->snapshot_07_out = v;
    this->outport_08_input_number_set(v);
}

void linetilde_01_target_bang() {}

number msToSamps(MillisecondTime ms, number sampleRate) {
    return ms * sampleRate * 0.001;
}

MillisecondTime sampsToMs(SampleIndex samps) {
    return samps * (this->invsr * 1000);
}

Index getMaxBlockSize() const {
    return this->maxvs;
}

number getSampleRate() const {
    return this->sr;
}

bool hasFixedVectorSize() const {
    return false;
}

Index getNumInputChannels() const {
    return 2;
}

Index getNumOutputChannels() const {
    return 2;
}

void allocateDataRefs() {
    this->average_rms_tilde_01_av_buffer = this->average_rms_tilde_01_av_buffer->allocateIfNeeded();

    if (this->average_rms_tilde_01_av_bufferobj->hasRequestedSize()) {
        if (this->average_rms_tilde_01_av_bufferobj->wantsFill())
            this->zeroDataRef(this->average_rms_tilde_01_av_bufferobj);

        this->getEngine()->sendDataRefUpdated(0);
    }

    this->average_rms_tilde_02_av_buffer = this->average_rms_tilde_02_av_buffer->allocateIfNeeded();

    if (this->average_rms_tilde_02_av_bufferobj->hasRequestedSize()) {
        if (this->average_rms_tilde_02_av_bufferobj->wantsFill())
            this->zeroDataRef(this->average_rms_tilde_02_av_bufferobj);

        this->getEngine()->sendDataRefUpdated(1);
    }

    this->average_rms_tilde_03_av_buffer = this->average_rms_tilde_03_av_buffer->allocateIfNeeded();

    if (this->average_rms_tilde_03_av_bufferobj->hasRequestedSize()) {
        if (this->average_rms_tilde_03_av_bufferobj->wantsFill())
            this->zeroDataRef(this->average_rms_tilde_03_av_bufferobj);

        this->getEngine()->sendDataRefUpdated(2);
    }

    this->average_rms_tilde_04_av_buffer = this->average_rms_tilde_04_av_buffer->allocateIfNeeded();

    if (this->average_rms_tilde_04_av_bufferobj->hasRequestedSize()) {
        if (this->average_rms_tilde_04_av_bufferobj->wantsFill())
            this->zeroDataRef(this->average_rms_tilde_04_av_bufferobj);

        this->getEngine()->sendDataRefUpdated(3);
    }

    this->delaytilde_01_del_buffer = this->delaytilde_01_del_buffer->allocateIfNeeded();

    if (this->delaytilde_01_del_bufferobj->hasRequestedSize()) {
        if (this->delaytilde_01_del_bufferobj->wantsFill())
            this->zeroDataRef(this->delaytilde_01_del_bufferobj);

        this->getEngine()->sendDataRefUpdated(4);
    }

    this->delaytilde_02_del_buffer = this->delaytilde_02_del_buffer->allocateIfNeeded();

    if (this->delaytilde_02_del_bufferobj->hasRequestedSize()) {
        if (this->delaytilde_02_del_bufferobj->wantsFill())
            this->zeroDataRef(this->delaytilde_02_del_bufferobj);

        this->getEngine()->sendDataRefUpdated(5);
    }
}

void initializeObjects() {
    this->average_rms_tilde_01_av_init();
    this->average_rms_tilde_02_av_init();
    this->average_rms_tilde_03_av_init();
    this->average_rms_tilde_04_av_init();
    this->delaytilde_01_del_init();
    this->delaytilde_02_del_init();
}

void sendOutlet(OutletIndex index, ParameterValue value) {
    this->getEngine()->sendOutlet(this, index, value);
}

void startup() {
    this->updateTime(this->getEngine()->getCurrentTime());

    {
        this->scheduleParamInit(0, 0);
    }

    {
        this->scheduleParamInit(1, 0);
    }

    {
        this->scheduleParamInit(2, 0);
    }

    {
        this->scheduleParamInit(3, 0);
    }

    {
        this->scheduleParamInit(4, 0);
    }

    {
        this->scheduleParamInit(5, 0);
    }

    {
        this->scheduleParamInit(6, 0);
    }

    {
        this->scheduleParamInit(7, 0);
    }

    this->processParamInitEvents();
}

number param_01_value_constrain(number v) const {
    v = (v > 12 ? 12 : (v < -60 ? -60 : v));
    return v;
}

number line_01_time_constrain(number v) const {
    if (v < 0)
        v = 0;

    return v;
}

void line_01_time_set(number v) {
    v = this->line_01_time_constrain(v);
    this->line_01_time = v;
}

void dspexpr_02_in2_set(number v) {
    this->dspexpr_02_in2 = v;
}

void expr_07_out1_set(number v) {
    this->expr_07_out1 = v;
    this->dspexpr_02_in2_set(this->expr_07_out1);
}

void expr_07_in1_set(number in1) {
    this->expr_07_in1 = in1;
    this->expr_07_out1_set(this->expr_07_in1 * this->expr_07_in2);//#map:*_obj-54:1
}

void line_01_output_set(number v) {
    this->line_01_output = v;
    this->expr_07_in1_set(v);
}

void line_01_stop_bang() {
    this->getEngine()->flushClockEvents(this, 1220262738, false);;
    this->line_01_pendingRamps->length = 0;
    this->line_01_startValue = this->line_01_output;
    this->line_01_slope = 0;
    this->line_01__time = 0;
    this->line_01_time_set(0);
}

number line_01_grain_constrain(number v) const {
    if (v < 0)
        v = 0;

    return v;
}

void line_01_grain_set(number v) {
    v = this->line_01_grain_constrain(v);
    this->line_01_grain = v;

    if ((bool)(!(bool)(this->line_01_isFinished(this->line_01_startValue)))) {
        this->getEngine()->flushClockEvents(this, 1220262738, false);;
        this->line_01_scheduleNext();
    }
}

void line_01_end_bang() {}

void line_01_target_set(const list& v) {
    this->line_01_target = jsCreateListCopy(v);
    this->line_01_pendingRamps->length = 0;

    if (v->length == 1) {
        this->line_01__time = this->line_01_time;
        this->line_01_time_set(0);

        if ((bool)(this->line_01__time)) {
            this->line_01_startRamp(v[0], this->line_01__time);
        } else {
            this->line_01_output_set(v[0]);
            this->line_01_startValue = v[0];
            this->line_01_stop_bang();
        }
    } else if (v->length == 2) {
        this->line_01_time_set(0);
        this->line_01__time = (v[1] < 0 ? 0 : v[1]);
        this->line_01_startRamp(v[0], this->line_01__time);
    } else if (v->length == 3) {
        this->line_01_time_set(0);
        this->line_01_grain_set(v[2]);
        this->line_01__time = (v[1] < 0 ? 0 : v[1]);
        this->line_01_startRamp(v[0], this->line_01__time);
    } else {
        this->line_01_time_set(0);
        this->line_01_pendingRamps = jsCreateListCopy(v);
        this->line_01_startPendingRamp();
    }
}

void expr_03_out1_set(number v) {
    this->expr_03_out1 = v;

    {
        list converted = {this->expr_03_out1};
        this->line_01_target_set(converted);
    }
}

void expr_03_in1_set(number in1) {
    this->expr_03_in1 = in1;
    this->expr_03_out1_set(rnbo_pow(10, this->expr_03_in1 * 0.05));//#map:dbtoa_obj-42:1
}

number param_02_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));

    {
        number oneStep = (number)1 / (number)1;
        number oneStepInv = (oneStep != 0 ? (number)1 / oneStep : 0);
        number numberOfSteps = rnbo_fround(v * oneStepInv * 1 / (number)1) * 1;
        v = numberOfSteps * oneStep;
    }

    return v;
}

void expr_07_in2_set(number v) {
    this->expr_07_in2 = v;
}

void expr_06_out1_set(number v) {
    this->expr_06_out1 = v;
    this->expr_07_in2_set(this->expr_06_out1);
}

void expr_06_$in1_set(number $in1) {
    this->expr_06_$in1 = $in1;
    this->expr_06_out1_set(1 - this->expr_06_$in1);//#map:expr_obj-53:1
}

void trigger_01_out2_set(number v) {
    this->expr_06_$in1_set(v);
}

void param_01_value_bang() {
    number v = this->param_01_value;
    this->sendParameter(0, false);

    if (this->param_01_value != this->param_01_lastValue) {
        this->getEngine()->presetTouched();
        this->param_01_lastValue = this->param_01_value;
    }

    this->expr_03_in1_set(v);
}

void trigger_01_out1_bang() {
    this->param_01_value_bang();
}

void trigger_01_input_number_set(number v) {
    this->trigger_01_out2_set(v);
    this->trigger_01_out1_bang();
}

number param_03_value_constrain(number v) const {
    v = (v > 12 ? 12 : (v < -60 ? -60 : v));
    return v;
}

void linetilde_01_time_set(number v) {
    this->linetilde_01_time = v;
}

void linetilde_01_segments_set(const list& v) {
    this->linetilde_01_segments = jsCreateListCopy(v);

    if ((bool)(v->length)) {
        if (v->length == 1 && this->linetilde_01_time == 0) {
            this->linetilde_01_activeRamps->length = 0;
            this->linetilde_01_currentValue = v[0];
        } else {
            auto currentTime = this->currentsampletime();
            number lastRampValue = this->linetilde_01_currentValue;
            number rampEnd = currentTime - this->sampleOffsetIntoNextAudioBuffer;

            for (Index i = 0; i < this->linetilde_01_activeRamps->length; i += 3) {
                rampEnd = this->linetilde_01_activeRamps[(Index)(i + 2)];

                if (rampEnd > currentTime) {
                    this->linetilde_01_activeRamps[(Index)(i + 2)] = currentTime;
                    number diff = rampEnd - currentTime;
                    number valueDiff = diff * this->linetilde_01_activeRamps[(Index)(i + 1)];
                    lastRampValue = this->linetilde_01_activeRamps[(Index)i] - valueDiff;
                    this->linetilde_01_activeRamps[(Index)i] = lastRampValue;
                    this->linetilde_01_activeRamps->length = i + 3;
                    rampEnd = currentTime;
                } else {
                    lastRampValue = this->linetilde_01_activeRamps[(Index)i];
                }
            }

            if (rampEnd < currentTime) {
                this->linetilde_01_activeRamps->push(lastRampValue);
                this->linetilde_01_activeRamps->push(0);
                this->linetilde_01_activeRamps->push(currentTime);
            }

            number lastRampEnd = currentTime;

            for (Index i = 0; i < v->length; i += 2) {
                number destinationValue = v[(Index)i];
                number inc = 0;
                number rampTimeInSamples;

                if (v->length > i + 1) {
                    rampTimeInSamples = this->mstosamps(v[(Index)(i + 1)]);

                    if ((bool)(this->linetilde_01_keepramp)) {
                        this->linetilde_01_time_set(v[(Index)(i + 1)]);
                    }
                } else {
                    rampTimeInSamples = this->mstosamps(this->linetilde_01_time);
                }

                if (rampTimeInSamples <= 0) {
                    rampTimeInSamples = 1;
                }

                inc = (destinationValue - lastRampValue) / rampTimeInSamples;
                lastRampEnd += rampTimeInSamples;
                this->linetilde_01_activeRamps->push(destinationValue);
                this->linetilde_01_activeRamps->push(inc);
                this->linetilde_01_activeRamps->push(lastRampEnd);
                lastRampValue = destinationValue;
            }
        }
    }
}

void expr_09_out1_set(number v) {
    this->expr_09_out1 = v;

    {
        list converted = {this->expr_09_out1};
        this->linetilde_01_segments_set(converted);
    }
}

void expr_09_in1_set(number in1) {
    this->expr_09_in1 = in1;
    this->expr_09_out1_set(rnbo_pow(10, this->expr_09_in1 * 0.05));//#map:dbtoa_obj-8:1
}

number param_04_value_constrain(number v) const {
    v = (v > 12 ? 12 : (v < -60 ? -60 : v));
    return v;
}

number line_02_time_constrain(number v) const {
    if (v < 0)
        v = 0;

    return v;
}

void line_02_time_set(number v) {
    v = this->line_02_time_constrain(v);
    this->line_02_time = v;
}

void dspexpr_13_in2_set(number v) {
    this->dspexpr_13_in2 = v;
}

void expr_08_out1_set(number v) {
    this->expr_08_out1 = v;
    this->dspexpr_13_in2_set(this->expr_08_out1);
}

void expr_08_in1_set(number in1) {
    this->expr_08_in1 = in1;
    this->expr_08_out1_set(this->expr_08_in1 * this->expr_08_in2);//#map:*_obj-56:1
}

void line_02_output_set(number v) {
    this->line_02_output = v;
    this->expr_08_in1_set(v);
}

void line_02_stop_bang() {
    this->getEngine()->flushClockEvents(this, 848255507, false);;
    this->line_02_pendingRamps->length = 0;
    this->line_02_startValue = this->line_02_output;
    this->line_02_slope = 0;
    this->line_02__time = 0;
    this->line_02_time_set(0);
}

number line_02_grain_constrain(number v) const {
    if (v < 0)
        v = 0;

    return v;
}

void line_02_grain_set(number v) {
    v = this->line_02_grain_constrain(v);
    this->line_02_grain = v;

    if ((bool)(!(bool)(this->line_02_isFinished(this->line_02_startValue)))) {
        this->getEngine()->flushClockEvents(this, 848255507, false);;
        this->line_02_scheduleNext();
    }
}

void line_02_end_bang() {}

void line_02_target_set(const list& v) {
    this->line_02_target = jsCreateListCopy(v);
    this->line_02_pendingRamps->length = 0;

    if (v->length == 1) {
        this->line_02__time = this->line_02_time;
        this->line_02_time_set(0);

        if ((bool)(this->line_02__time)) {
            this->line_02_startRamp(v[0], this->line_02__time);
        } else {
            this->line_02_output_set(v[0]);
            this->line_02_startValue = v[0];
            this->line_02_stop_bang();
        }
    } else if (v->length == 2) {
        this->line_02_time_set(0);
        this->line_02__time = (v[1] < 0 ? 0 : v[1]);
        this->line_02_startRamp(v[0], this->line_02__time);
    } else if (v->length == 3) {
        this->line_02_time_set(0);
        this->line_02_grain_set(v[2]);
        this->line_02__time = (v[1] < 0 ? 0 : v[1]);
        this->line_02_startRamp(v[0], this->line_02__time);
    } else {
        this->line_02_time_set(0);
        this->line_02_pendingRamps = jsCreateListCopy(v);
        this->line_02_startPendingRamp();
    }
}

void expr_10_out1_set(number v) {
    this->expr_10_out1 = v;

    {
        list converted = {this->expr_10_out1};
        this->line_02_target_set(converted);
    }
}

void expr_10_in1_set(number in1) {
    this->expr_10_in1 = in1;
    this->expr_10_out1_set(rnbo_pow(10, this->expr_10_in1 * 0.05));//#map:dbtoa_obj-45:1
}

number param_05_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));

    {
        number oneStep = (number)1 / (number)1;
        number oneStepInv = (oneStep != 0 ? (number)1 / oneStep : 0);
        number numberOfSteps = rnbo_fround(v * oneStepInv * 1 / (number)1) * 1;
        v = numberOfSteps * oneStep;
    }

    return v;
}

void dspexpr_11_in2_set(number v) {
    this->dspexpr_11_in2 = v;
}

void dspexpr_07_in2_set(number v) {
    this->dspexpr_07_in2 = v;
}

void expr_11_out1_set(number v) {
    this->expr_11_out1 = v;
    this->dspexpr_11_in2_set(this->expr_11_out1);
    this->dspexpr_07_in2_set(this->expr_11_out1);
}

void expr_11_$in1_set(number $in1) {
    this->expr_11_$in1 = $in1;
    this->expr_11_out1_set(1 - 2 * this->expr_11_$in1);//#map:expr_obj-28:1
}

number param_06_value_constrain(number v) const {
    v = (v > 60 ? 60 : (v < 0 ? 0 : v));
    return v;
}

void outport_06_input_number_set(number v) {
    this->getEngine()->sendNumMessage(TAG("delay_time"), TAG(""), v, this->_currentTime);
}

void delaytilde_02_delay_set(number v) {
    this->delaytilde_02_delay = v;
}

void delaytilde_01_delay_set(number v) {
    this->delaytilde_01_delay = v;
}

void mstosamps_01_out1_set(number v) {
    this->delaytilde_02_delay_set(v);
    this->delaytilde_01_delay_set(v);
}

void mstosamps_01_ms_set(number ms) {
    this->mstosamps_01_ms = ms;

    {
        this->mstosamps_01_out1_set(ms * this->sr * 0.001);
        return;
    }
}

void expr_13_out1_set(number v) {
    this->expr_13_out1 = v;
    this->outport_06_input_number_set(this->expr_13_out1);
    this->mstosamps_01_ms_set(this->expr_13_out1);
}

void expr_13_$in1_set(number $in1) {
    this->expr_13_$in1 = $in1;

    this->expr_13_out1_set(
        ((331.4 + 0.6 * this->expr_13_$in2 == 0. ? 0. : this->expr_13_$in1 / (331.4 + 0.6 * this->expr_13_$in2))) * 1000
    );//#map:expr_obj-24:1
}

void selector_02_onoff_set(number v) {
    this->selector_02_onoff = v;
}

void selector_01_onoff_set(number v) {
    this->selector_01_onoff = v;
}

void expr_12_out1_set(number v) {
    this->expr_12_out1 = v;
    this->selector_02_onoff_set(this->expr_12_out1);
    this->selector_01_onoff_set(this->expr_12_out1);
}

void expr_12_$in1_set(number $in1) {
    this->expr_12_$in1 = $in1;
    this->expr_12_out1_set((this->expr_12_$in1 > 0) + 1);//#map:expr_obj-35:1
}

number param_07_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));

    {
        number oneStep = (number)1 / (number)1;
        number oneStepInv = (oneStep != 0 ? (number)1 / oneStep : 0);
        number numberOfSteps = rnbo_fround(v * oneStepInv * 1 / (number)1) * 1;
        v = numberOfSteps * oneStep;
    }

    return v;
}

void expr_08_in2_set(number v) {
    this->expr_08_in2 = v;
}

void expr_14_out1_set(number v) {
    this->expr_14_out1 = v;
    this->expr_08_in2_set(this->expr_14_out1);
}

void expr_14_$in1_set(number $in1) {
    this->expr_14_$in1 = $in1;
    this->expr_14_out1_set(1 - this->expr_14_$in1);//#map:expr_obj-55:1
}

void trigger_02_out2_set(number v) {
    this->expr_14_$in1_set(v);
}

void param_04_value_bang() {
    number v = this->param_04_value;
    this->sendParameter(3, false);

    if (this->param_04_value != this->param_04_lastValue) {
        this->getEngine()->presetTouched();
        this->param_04_lastValue = this->param_04_value;
    }

    this->expr_10_in1_set(v);
}

void trigger_02_out1_bang() {
    this->param_04_value_bang();
}

void trigger_02_input_number_set(number v) {
    this->trigger_02_out2_set(v);
    this->trigger_02_out1_bang();
}

number param_08_value_constrain(number v) const {
    v = (v > 40 ? 40 : (v < -10 ? -10 : v));
    return v;
}

void expr_13_$in2_set(number v) {
    this->expr_13_$in2 = v;
}

void trigger_03_out2_set(number v) {
    this->expr_13_$in2_set(v);
}

void param_06_value_bang() {
    number v = this->param_06_value;
    this->sendParameter(5, false);

    if (this->param_06_value != this->param_06_lastValue) {
        this->getEngine()->presetTouched();
        this->param_06_lastValue = this->param_06_value;
    }

    this->expr_13_$in1_set(v);
    this->expr_12_$in1_set(v);
}

void trigger_03_out1_bang() {
    this->param_06_value_bang();
}

void trigger_03_input_number_set(number v) {
    this->trigger_03_out2_set(v);
    this->trigger_03_out1_bang();
}

void outport_01_input_number_set(number v) {
    this->getEngine()->sendNumMessage(TAG("out_rms_L"), TAG(""), v, this->_currentTime);
}

void expr_01_out1_set(number v) {
    this->expr_01_out1 = v;
    this->outport_01_input_number_set(this->expr_01_out1);
}

void expr_01_in1_set(number in1) {
    this->expr_01_in1 = in1;

    this->expr_01_out1_set(
        (this->expr_01_in1 <= 0 ? -999 : 20 * rnbo_log10((this->expr_01_in1 <= 0.0000000001 ? 0.0000000001 : this->expr_01_in1)))
    );//#map:atodb_obj-32:1
}

void outport_02_input_number_set(number v) {
    this->getEngine()->sendNumMessage(TAG("in_rms_L"), TAG(""), v, this->_currentTime);
}

void expr_02_out1_set(number v) {
    this->expr_02_out1 = v;
    this->outport_02_input_number_set(this->expr_02_out1);
}

void expr_02_in1_set(number in1) {
    this->expr_02_in1 = in1;

    this->expr_02_out1_set(
        (this->expr_02_in1 <= 0 ? -999 : 20 * rnbo_log10((this->expr_02_in1 <= 0.0000000001 ? 0.0000000001 : this->expr_02_in1)))
    );//#map:atodb_obj-30:1
}

void outport_03_input_number_set(number v) {
    this->getEngine()->sendNumMessage(TAG("out_rms_R"), TAG(""), v, this->_currentTime);
}

void expr_04_out1_set(number v) {
    this->expr_04_out1 = v;
    this->outport_03_input_number_set(this->expr_04_out1);
}

void expr_04_in1_set(number in1) {
    this->expr_04_in1 = in1;

    this->expr_04_out1_set(
        (this->expr_04_in1 <= 0 ? -999 : 20 * rnbo_log10((this->expr_04_in1 <= 0.0000000001 ? 0.0000000001 : this->expr_04_in1)))
    );//#map:atodb_obj-33:1
}

void outport_04_input_number_set(number v) {
    this->getEngine()->sendNumMessage(TAG("in_rms_R"), TAG(""), v, this->_currentTime);
}

void expr_05_out1_set(number v) {
    this->expr_05_out1 = v;
    this->outport_04_input_number_set(this->expr_05_out1);
}

void expr_05_in1_set(number in1) {
    this->expr_05_in1 = in1;

    this->expr_05_out1_set(
        (this->expr_05_in1 <= 0 ? -999 : 20 * rnbo_log10((this->expr_05_in1 <= 0.0000000001 ? 0.0000000001 : this->expr_05_in1)))
    );//#map:atodb_obj-31:1
}

void outport_05_input_number_set(number v) {
    this->getEngine()->sendNumMessage(TAG("correlation_value"), TAG(""), v, this->_currentTime);
}

void outport_07_input_number_set(number v) {
    this->getEngine()->sendNumMessage(TAG("scopeY"), TAG(""), v, this->_currentTime);
}

void outport_08_input_number_set(number v) {
    this->getEngine()->sendNumMessage(TAG("scopeX"), TAG(""), v, this->_currentTime);
}

void delaytilde_01_perform(number delay, const SampleValue * input, SampleValue * output, Index n) {
    auto __delaytilde_01_crossfadeDelay = this->delaytilde_01_crossfadeDelay;
    auto __delaytilde_01_rampInSamples = this->delaytilde_01_rampInSamples;
    auto __delaytilde_01_ramp = this->delaytilde_01_ramp;
    auto __delaytilde_01_lastDelay = this->delaytilde_01_lastDelay;

    for (Index i = 0; i < n; i++) {
        if (__delaytilde_01_lastDelay == -1) {
            __delaytilde_01_lastDelay = delay;
        }

        if (__delaytilde_01_ramp > 0) {
            number factor = __delaytilde_01_ramp / __delaytilde_01_rampInSamples;
            output[(Index)i] = this->delaytilde_01_del_read(__delaytilde_01_crossfadeDelay, 0) * factor + this->delaytilde_01_del_read(__delaytilde_01_lastDelay, 0) * (1. - factor);
            __delaytilde_01_ramp--;
        } else {
            number effectiveDelay = delay;

            if (effectiveDelay != __delaytilde_01_lastDelay) {
                __delaytilde_01_ramp = __delaytilde_01_rampInSamples;
                __delaytilde_01_crossfadeDelay = __delaytilde_01_lastDelay;
                __delaytilde_01_lastDelay = effectiveDelay;
                output[(Index)i] = this->delaytilde_01_del_read(__delaytilde_01_crossfadeDelay, 0);
                __delaytilde_01_ramp--;
            } else {
                output[(Index)i] = this->delaytilde_01_del_read(effectiveDelay, 0);
            }
        }

        this->delaytilde_01_del_write(input[(Index)i]);
        this->delaytilde_01_del_step();
    }

    this->delaytilde_01_lastDelay = __delaytilde_01_lastDelay;
    this->delaytilde_01_ramp = __delaytilde_01_ramp;
    this->delaytilde_01_crossfadeDelay = __delaytilde_01_crossfadeDelay;
}

void selector_01_perform(
    number onoff,
    const SampleValue * in1,
    const SampleValue * in2,
    SampleValue * out,
    Index n
) {
    Index i;

    for (i = 0; i < n; i++) {
        if (onoff >= 1 && onoff < 2)
            out[(Index)i] = in1[(Index)i];
        else if (onoff >= 2 && onoff < 3)
            out[(Index)i] = in2[(Index)i];
        else
            out[(Index)i] = 0;
    }
}

void dspexpr_07_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2;//#map:_###_obj_###_:1
    }
}

void average_rms_tilde_02_perform(
    const Sample * x,
    number windowSize,
    number reset,
    SampleValue * out1,
    Index n
) {
    RNBO_UNUSED(reset);
    RNBO_UNUSED(windowSize);
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = this->safesqrt(this->average_rms_tilde_02_av_next(x[(Index)i] * x[(Index)i], 2048, 0));
    }
}

void snapshot_02_perform(const SampleValue * input_signal, Index n) {
    auto __snapshot_02_lastValue = this->snapshot_02_lastValue;
    auto __snapshot_02_calc = this->snapshot_02_calc;
    auto __snapshot_02_count = this->snapshot_02_count;
    auto __snapshot_02_nextTime = this->snapshot_02_nextTime;
    auto __snapshot_02_interval = this->snapshot_02_interval;
    number timeInSamples = this->msToSamps(__snapshot_02_interval, this->sr);

    if (__snapshot_02_interval > 0) {
        for (Index i = 0; i < n; i++) {
            if (__snapshot_02_nextTime <= __snapshot_02_count + (SampleIndex)(i)) {
                {
                    __snapshot_02_calc = input_signal[(Index)i];
                }

                this->getEngine()->scheduleClockEventWithValue(
                    this,
                    1033938262,
                    this->sampsToMs((SampleIndex)(this->vs)) + this->_currentTime,
                    __snapshot_02_calc
                );;

                __snapshot_02_calc = 0;
                __snapshot_02_nextTime += timeInSamples;
            }
        }

        __snapshot_02_count += this->vs;
    }

    __snapshot_02_lastValue = input_signal[(Index)(n - 1)];
    this->snapshot_02_nextTime = __snapshot_02_nextTime;
    this->snapshot_02_count = __snapshot_02_count;
    this->snapshot_02_calc = __snapshot_02_calc;
    this->snapshot_02_lastValue = __snapshot_02_lastValue;
}

void delaytilde_02_perform(number delay, const SampleValue * input, SampleValue * output, Index n) {
    auto __delaytilde_02_crossfadeDelay = this->delaytilde_02_crossfadeDelay;
    auto __delaytilde_02_rampInSamples = this->delaytilde_02_rampInSamples;
    auto __delaytilde_02_ramp = this->delaytilde_02_ramp;
    auto __delaytilde_02_lastDelay = this->delaytilde_02_lastDelay;

    for (Index i = 0; i < n; i++) {
        if (__delaytilde_02_lastDelay == -1) {
            __delaytilde_02_lastDelay = delay;
        }

        if (__delaytilde_02_ramp > 0) {
            number factor = __delaytilde_02_ramp / __delaytilde_02_rampInSamples;
            output[(Index)i] = this->delaytilde_02_del_read(__delaytilde_02_crossfadeDelay, 0) * factor + this->delaytilde_02_del_read(__delaytilde_02_lastDelay, 0) * (1. - factor);
            __delaytilde_02_ramp--;
        } else {
            number effectiveDelay = delay;

            if (effectiveDelay != __delaytilde_02_lastDelay) {
                __delaytilde_02_ramp = __delaytilde_02_rampInSamples;
                __delaytilde_02_crossfadeDelay = __delaytilde_02_lastDelay;
                __delaytilde_02_lastDelay = effectiveDelay;
                output[(Index)i] = this->delaytilde_02_del_read(__delaytilde_02_crossfadeDelay, 0);
                __delaytilde_02_ramp--;
            } else {
                output[(Index)i] = this->delaytilde_02_del_read(effectiveDelay, 0);
            }
        }

        this->delaytilde_02_del_write(input[(Index)i]);
        this->delaytilde_02_del_step();
    }

    this->delaytilde_02_lastDelay = __delaytilde_02_lastDelay;
    this->delaytilde_02_ramp = __delaytilde_02_ramp;
    this->delaytilde_02_crossfadeDelay = __delaytilde_02_crossfadeDelay;
}

void selector_02_perform(
    number onoff,
    const SampleValue * in1,
    const SampleValue * in2,
    SampleValue * out,
    Index n
) {
    Index i;

    for (i = 0; i < n; i++) {
        if (onoff >= 1 && onoff < 2)
            out[(Index)i] = in1[(Index)i];
        else if (onoff >= 2 && onoff < 3)
            out[(Index)i] = in2[(Index)i];
        else
            out[(Index)i] = 0;
    }
}

void dspexpr_11_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2;//#map:_###_obj_###_:1
    }
}

void dspexpr_10_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] - in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void dspexpr_14_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] * 0.707107;//#map:_###_obj_###_:1
    }
}

void dspexpr_13_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2;//#map:_###_obj_###_:1
    }
}

void dspexpr_06_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] + in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void dspexpr_03_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] * 0.707107;//#map:_###_obj_###_:1
    }
}

void dspexpr_02_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2;//#map:_###_obj_###_:1
    }
}

void dspexpr_12_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] - in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void dspexpr_09_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] * 0.707107;//#map:_###_obj_###_:1
    }
}

void dspexpr_01_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] + in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void dspexpr_04_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] * 0.707107;//#map:_###_obj_###_:1
    }
}

void average_rms_tilde_04_perform(
    const Sample * x,
    number windowSize,
    number reset,
    SampleValue * out1,
    Index n
) {
    RNBO_UNUSED(reset);
    RNBO_UNUSED(windowSize);
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = this->safesqrt(this->average_rms_tilde_04_av_next(x[(Index)i] * x[(Index)i], 2048, 0));
    }
}

void snapshot_04_perform(const SampleValue * input_signal, Index n) {
    auto __snapshot_04_lastValue = this->snapshot_04_lastValue;
    auto __snapshot_04_calc = this->snapshot_04_calc;
    auto __snapshot_04_count = this->snapshot_04_count;
    auto __snapshot_04_nextTime = this->snapshot_04_nextTime;
    auto __snapshot_04_interval = this->snapshot_04_interval;
    number timeInSamples = this->msToSamps(__snapshot_04_interval, this->sr);

    if (__snapshot_04_interval > 0) {
        for (Index i = 0; i < n; i++) {
            if (__snapshot_04_nextTime <= __snapshot_04_count + (SampleIndex)(i)) {
                {
                    __snapshot_04_calc = input_signal[(Index)i];
                }

                this->getEngine()->scheduleClockEventWithValue(
                    this,
                    -2121464745,
                    this->sampsToMs((SampleIndex)(this->vs)) + this->_currentTime,
                    __snapshot_04_calc
                );;

                __snapshot_04_calc = 0;
                __snapshot_04_nextTime += timeInSamples;
            }
        }

        __snapshot_04_count += this->vs;
    }

    __snapshot_04_lastValue = input_signal[(Index)(n - 1)];
    this->snapshot_04_nextTime = __snapshot_04_nextTime;
    this->snapshot_04_count = __snapshot_04_count;
    this->snapshot_04_calc = __snapshot_04_calc;
    this->snapshot_04_lastValue = __snapshot_04_lastValue;
}

void linetilde_01_perform(SampleValue * out, Index n) {
    auto __linetilde_01_time = this->linetilde_01_time;
    auto __linetilde_01_keepramp = this->linetilde_01_keepramp;
    auto __linetilde_01_currentValue = this->linetilde_01_currentValue;
    Index i = 0;

    if ((bool)(this->linetilde_01_activeRamps->length)) {
        while ((bool)(this->linetilde_01_activeRamps->length) && i < n) {
            number destinationValue = this->linetilde_01_activeRamps[0];
            number inc = this->linetilde_01_activeRamps[1];
            number rampTimeInSamples = this->linetilde_01_activeRamps[2] - this->audioProcessSampleCount - i;
            number val = __linetilde_01_currentValue;

            while (rampTimeInSamples > 0 && i < n) {
                out[(Index)i] = val;
                val += inc;
                i++;
                rampTimeInSamples--;
            }

            if (rampTimeInSamples <= 0) {
                val = destinationValue;
                this->linetilde_01_activeRamps->splice(0, 3);

                if ((bool)(!(bool)(this->linetilde_01_activeRamps->length))) {
                    this->getEngine()->scheduleClockEventWithValue(
                        this,
                        -281953904,
                        this->sampsToMs((SampleIndex)(this->vs)) + this->_currentTime,
                        0
                    );;

                    if ((bool)(!(bool)(__linetilde_01_keepramp))) {
                        __linetilde_01_time = 0;
                    }
                }
            }

            __linetilde_01_currentValue = val;
        }
    }

    while (i < n) {
        out[(Index)i] = __linetilde_01_currentValue;
        i++;
    }

    this->linetilde_01_currentValue = __linetilde_01_currentValue;
    this->linetilde_01_time = __linetilde_01_time;
}

void dspexpr_08_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void average_rms_tilde_03_perform(
    const Sample * x,
    number windowSize,
    number reset,
    SampleValue * out1,
    Index n
) {
    RNBO_UNUSED(reset);
    RNBO_UNUSED(windowSize);
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = this->safesqrt(this->average_rms_tilde_03_av_next(x[(Index)i] * x[(Index)i], 2048, 0));
    }
}

void snapshot_03_perform(const SampleValue * input_signal, Index n) {
    auto __snapshot_03_lastValue = this->snapshot_03_lastValue;
    auto __snapshot_03_calc = this->snapshot_03_calc;
    auto __snapshot_03_count = this->snapshot_03_count;
    auto __snapshot_03_nextTime = this->snapshot_03_nextTime;
    auto __snapshot_03_interval = this->snapshot_03_interval;
    number timeInSamples = this->msToSamps(__snapshot_03_interval, this->sr);

    if (__snapshot_03_interval > 0) {
        for (Index i = 0; i < n; i++) {
            if (__snapshot_03_nextTime <= __snapshot_03_count + (SampleIndex)(i)) {
                {
                    __snapshot_03_calc = input_signal[(Index)i];
                }

                this->getEngine()->scheduleClockEventWithValue(
                    this,
                    2018930062,
                    this->sampsToMs((SampleIndex)(this->vs)) + this->_currentTime,
                    __snapshot_03_calc
                );;

                __snapshot_03_calc = 0;
                __snapshot_03_nextTime += timeInSamples;
            }
        }

        __snapshot_03_count += this->vs;
    }

    __snapshot_03_lastValue = input_signal[(Index)(n - 1)];
    this->snapshot_03_nextTime = __snapshot_03_nextTime;
    this->snapshot_03_count = __snapshot_03_count;
    this->snapshot_03_calc = __snapshot_03_calc;
    this->snapshot_03_lastValue = __snapshot_03_lastValue;
}

void signalforwarder_01_perform(const SampleValue * input, SampleValue * output, Index n) {
    for (Index i = 0; i < n; i++) {
        output[(Index)i] = input[(Index)i];
    }
}

void dspexpr_05_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void dspexpr_20_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] - in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void snapshot_07_perform(const SampleValue * input_signal, Index n) {
    auto __snapshot_07_lastValue = this->snapshot_07_lastValue;
    auto __snapshot_07_calc = this->snapshot_07_calc;
    auto __snapshot_07_count = this->snapshot_07_count;
    auto __snapshot_07_nextTime = this->snapshot_07_nextTime;
    auto __snapshot_07_interval = this->snapshot_07_interval;
    number timeInSamples = this->msToSamps(__snapshot_07_interval, this->sr);

    if (__snapshot_07_interval > 0) {
        for (Index i = 0; i < n; i++) {
            if (__snapshot_07_nextTime <= __snapshot_07_count + (SampleIndex)(i)) {
                {
                    __snapshot_07_calc = input_signal[(Index)i];
                }

                this->getEngine()->scheduleClockEventWithValue(
                    this,
                    1142655633,
                    this->sampsToMs((SampleIndex)(this->vs)) + this->_currentTime,
                    __snapshot_07_calc
                );;

                __snapshot_07_calc = 0;
                __snapshot_07_nextTime += timeInSamples;
            }
        }

        __snapshot_07_count += this->vs;
    }

    __snapshot_07_lastValue = input_signal[(Index)(n - 1)];
    this->snapshot_07_nextTime = __snapshot_07_nextTime;
    this->snapshot_07_count = __snapshot_07_count;
    this->snapshot_07_calc = __snapshot_07_calc;
    this->snapshot_07_lastValue = __snapshot_07_lastValue;
}

void dspexpr_19_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] + in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void snapshot_06_perform(const SampleValue * input_signal, Index n) {
    auto __snapshot_06_lastValue = this->snapshot_06_lastValue;
    auto __snapshot_06_calc = this->snapshot_06_calc;
    auto __snapshot_06_count = this->snapshot_06_count;
    auto __snapshot_06_nextTime = this->snapshot_06_nextTime;
    auto __snapshot_06_interval = this->snapshot_06_interval;
    number timeInSamples = this->msToSamps(__snapshot_06_interval, this->sr);

    if (__snapshot_06_interval > 0) {
        for (Index i = 0; i < n; i++) {
            if (__snapshot_06_nextTime <= __snapshot_06_count + (SampleIndex)(i)) {
                {
                    __snapshot_06_calc = input_signal[(Index)i];
                }

                this->getEngine()->scheduleClockEventWithValue(
                    this,
                    3091344,
                    this->sampsToMs((SampleIndex)(this->vs)) + this->_currentTime,
                    __snapshot_06_calc
                );;

                __snapshot_06_calc = 0;
                __snapshot_06_nextTime += timeInSamples;
            }
        }

        __snapshot_06_count += this->vs;
    }

    __snapshot_06_lastValue = input_signal[(Index)(n - 1)];
    this->snapshot_06_nextTime = __snapshot_06_nextTime;
    this->snapshot_06_count = __snapshot_06_count;
    this->snapshot_06_calc = __snapshot_06_calc;
    this->snapshot_06_lastValue = __snapshot_06_lastValue;
}

void dspexpr_15_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void slide_tilde_01_perform(const Sample * x, number up, number down, SampleValue * out1, Index n) {
    RNBO_UNUSED(down);
    RNBO_UNUSED(up);
    auto __slide_tilde_01_prev = this->slide_tilde_01_prev;
    auto iup = this->safediv(1., this->maximum(1., rnbo_abs(5000)));
    auto idown = this->safediv(1., this->maximum(1., rnbo_abs(5000)));
    Index i;

    for (i = 0; i < n; i++) {
        number temp = x[(Index)i] - __slide_tilde_01_prev;
        __slide_tilde_01_prev = __slide_tilde_01_prev + ((x[(Index)i] > __slide_tilde_01_prev ? iup : idown)) * temp;
        out1[(Index)i] = __slide_tilde_01_prev;
    }

    this->slide_tilde_01_prev = __slide_tilde_01_prev;
}

void average_rms_tilde_01_perform(
    const Sample * x,
    number windowSize,
    number reset,
    SampleValue * out1,
    Index n
) {
    RNBO_UNUSED(reset);
    RNBO_UNUSED(windowSize);
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = this->safesqrt(this->average_rms_tilde_01_av_next(x[(Index)i] * x[(Index)i], 2048, 0));
    }
}

void snapshot_01_perform(const SampleValue * input_signal, Index n) {
    auto __snapshot_01_lastValue = this->snapshot_01_lastValue;
    auto __snapshot_01_calc = this->snapshot_01_calc;
    auto __snapshot_01_count = this->snapshot_01_count;
    auto __snapshot_01_nextTime = this->snapshot_01_nextTime;
    auto __snapshot_01_interval = this->snapshot_01_interval;
    number timeInSamples = this->msToSamps(__snapshot_01_interval, this->sr);

    if (__snapshot_01_interval > 0) {
        for (Index i = 0; i < n; i++) {
            if (__snapshot_01_nextTime <= __snapshot_01_count + (SampleIndex)(i)) {
                {
                    __snapshot_01_calc = input_signal[(Index)i];
                }

                this->getEngine()->scheduleClockEventWithValue(
                    this,
                    879365773,
                    this->sampsToMs((SampleIndex)(this->vs)) + this->_currentTime,
                    __snapshot_01_calc
                );;

                __snapshot_01_calc = 0;
                __snapshot_01_nextTime += timeInSamples;
            }
        }

        __snapshot_01_count += this->vs;
    }

    __snapshot_01_lastValue = input_signal[(Index)(n - 1)];
    this->snapshot_01_nextTime = __snapshot_01_nextTime;
    this->snapshot_01_count = __snapshot_01_count;
    this->snapshot_01_calc = __snapshot_01_calc;
    this->snapshot_01_lastValue = __snapshot_01_lastValue;
}

void dspexpr_18_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void dspexpr_17_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] + 0.0001;//#map:_###_obj_###_:1
    }
}

void slide_tilde_02_perform(const Sample * x, number up, number down, SampleValue * out1, Index n) {
    RNBO_UNUSED(down);
    RNBO_UNUSED(up);
    auto __slide_tilde_02_prev = this->slide_tilde_02_prev;
    auto iup = this->safediv(1., this->maximum(1., rnbo_abs(5000)));
    auto idown = this->safediv(1., this->maximum(1., rnbo_abs(5000)));
    Index i;

    for (i = 0; i < n; i++) {
        number temp = x[(Index)i] - __slide_tilde_02_prev;
        __slide_tilde_02_prev = __slide_tilde_02_prev + ((x[(Index)i] > __slide_tilde_02_prev ? iup : idown)) * temp;
        out1[(Index)i] = __slide_tilde_02_prev;
    }

    this->slide_tilde_02_prev = __slide_tilde_02_prev;
}

void dspexpr_16_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = (in2[(Index)i] == 0 ? 0 : (in2[(Index)i] == 0. ? 0. : in1[(Index)i] / in2[(Index)i]));//#map:_###_obj_###_:1
    }
}

void snapshot_05_perform(const SampleValue * input_signal, Index n) {
    auto __snapshot_05_lastValue = this->snapshot_05_lastValue;
    auto __snapshot_05_calc = this->snapshot_05_calc;
    auto __snapshot_05_count = this->snapshot_05_count;
    auto __snapshot_05_nextTime = this->snapshot_05_nextTime;
    auto __snapshot_05_interval = this->snapshot_05_interval;
    number timeInSamples = this->msToSamps(__snapshot_05_interval, this->sr);

    if (__snapshot_05_interval > 0) {
        for (Index i = 0; i < n; i++) {
            if (__snapshot_05_nextTime <= __snapshot_05_count + (SampleIndex)(i)) {
                {
                    __snapshot_05_calc = input_signal[(Index)i];
                }

                this->getEngine()->scheduleClockEventWithValue(
                    this,
                    -1136472945,
                    this->sampsToMs((SampleIndex)(this->vs)) + this->_currentTime,
                    __snapshot_05_calc
                );;

                __snapshot_05_calc = 0;
                __snapshot_05_nextTime += timeInSamples;
            }
        }

        __snapshot_05_count += this->vs;
    }

    __snapshot_05_lastValue = input_signal[(Index)(n - 1)];
    this->snapshot_05_nextTime = __snapshot_05_nextTime;
    this->snapshot_05_count = __snapshot_05_count;
    this->snapshot_05_calc = __snapshot_05_calc;
    this->snapshot_05_lastValue = __snapshot_05_lastValue;
}

void signalforwarder_02_perform(const SampleValue * input, SampleValue * output, Index n) {
    for (Index i = 0; i < n; i++) {
        output[(Index)i] = input[(Index)i];
    }
}

void stackprotect_perform(Index n) {
    RNBO_UNUSED(n);
    auto __stackprotect_count = this->stackprotect_count;
    __stackprotect_count = 0;
    this->stackprotect_count = __stackprotect_count;
}

number average_rms_tilde_01_av_next(number x, int windowSize, bool reset) {
    if (windowSize > 0)
        this->average_rms_tilde_01_av_setwindowsize(windowSize);

    if (reset != 0) {
        if (this->average_rms_tilde_01_av_resetFlag != 1) {
            this->average_rms_tilde_01_av_wantsReset = 1;
            this->average_rms_tilde_01_av_resetFlag = 1;
        }
    } else {
        this->average_rms_tilde_01_av_resetFlag = 0;
    }

    if (this->average_rms_tilde_01_av_wantsReset == 1) {
        this->average_rms_tilde_01_av_doReset();
    }

    this->average_rms_tilde_01_av_accum += x;
    this->average_rms_tilde_01_av_buffer[(Index)this->average_rms_tilde_01_av_bufferPos] = x;
    number bufferSize = this->average_rms_tilde_01_av_buffer->getSize();

    if (this->average_rms_tilde_01_av_effectiveWindowSize < this->average_rms_tilde_01_av_currentWindowSize) {
        this->average_rms_tilde_01_av_effectiveWindowSize++;
    } else {
        number bufferReadPos = this->average_rms_tilde_01_av_bufferPos - this->average_rms_tilde_01_av_effectiveWindowSize;

        while (bufferReadPos < 0)
            bufferReadPos += bufferSize;

        this->average_rms_tilde_01_av_accum -= this->average_rms_tilde_01_av_buffer[(Index)bufferReadPos];
    }

    this->average_rms_tilde_01_av_bufferPos++;

    if (this->average_rms_tilde_01_av_bufferPos >= bufferSize) {
        this->average_rms_tilde_01_av_bufferPos -= bufferSize;
    }

    return this->average_rms_tilde_01_av_accum / this->average_rms_tilde_01_av_effectiveWindowSize;
}

void average_rms_tilde_01_av_setwindowsize(int wsize) {
    wsize = trunc(wsize);

    if (wsize != this->average_rms_tilde_01_av_currentWindowSize && wsize > 0 && wsize <= this->sr) {
        this->average_rms_tilde_01_av_currentWindowSize = wsize;
        this->average_rms_tilde_01_av_wantsReset = 1;
    }
}

void average_rms_tilde_01_av_reset() {
    this->average_rms_tilde_01_av_wantsReset = 1;
}

void average_rms_tilde_01_av_dspsetup() {
    this->average_rms_tilde_01_av_wantsReset = 1;

    if (this->sr > this->average_rms_tilde_01_av_buffer->getSize()) {
        this->average_rms_tilde_01_av_buffer->setSize(this->sr + 1);
        updateDataRef(this, this->average_rms_tilde_01_av_buffer);
    }
}

void average_rms_tilde_01_av_doReset() {
    this->average_rms_tilde_01_av_accum = 0;
    this->average_rms_tilde_01_av_effectiveWindowSize = 0;
    this->average_rms_tilde_01_av_bufferPos = 0;
    this->average_rms_tilde_01_av_wantsReset = 0;
}

void average_rms_tilde_01_av_init() {
    this->average_rms_tilde_01_av_currentWindowSize = this->sr;
    this->average_rms_tilde_01_av_buffer->requestSize(this->sr + 1, 1);
    this->average_rms_tilde_01_av_doReset();
}

void average_rms_tilde_01_dspsetup(bool force) {
    if ((bool)(this->average_rms_tilde_01_setupDone) && (bool)(!(bool)(force)))
        return;

    this->average_rms_tilde_01_setupDone = true;
    this->average_rms_tilde_01_av_dspsetup();
}

number average_rms_tilde_02_av_next(number x, int windowSize, bool reset) {
    if (windowSize > 0)
        this->average_rms_tilde_02_av_setwindowsize(windowSize);

    if (reset != 0) {
        if (this->average_rms_tilde_02_av_resetFlag != 1) {
            this->average_rms_tilde_02_av_wantsReset = 1;
            this->average_rms_tilde_02_av_resetFlag = 1;
        }
    } else {
        this->average_rms_tilde_02_av_resetFlag = 0;
    }

    if (this->average_rms_tilde_02_av_wantsReset == 1) {
        this->average_rms_tilde_02_av_doReset();
    }

    this->average_rms_tilde_02_av_accum += x;
    this->average_rms_tilde_02_av_buffer[(Index)this->average_rms_tilde_02_av_bufferPos] = x;
    number bufferSize = this->average_rms_tilde_02_av_buffer->getSize();

    if (this->average_rms_tilde_02_av_effectiveWindowSize < this->average_rms_tilde_02_av_currentWindowSize) {
        this->average_rms_tilde_02_av_effectiveWindowSize++;
    } else {
        number bufferReadPos = this->average_rms_tilde_02_av_bufferPos - this->average_rms_tilde_02_av_effectiveWindowSize;

        while (bufferReadPos < 0)
            bufferReadPos += bufferSize;

        this->average_rms_tilde_02_av_accum -= this->average_rms_tilde_02_av_buffer[(Index)bufferReadPos];
    }

    this->average_rms_tilde_02_av_bufferPos++;

    if (this->average_rms_tilde_02_av_bufferPos >= bufferSize) {
        this->average_rms_tilde_02_av_bufferPos -= bufferSize;
    }

    return this->average_rms_tilde_02_av_accum / this->average_rms_tilde_02_av_effectiveWindowSize;
}

void average_rms_tilde_02_av_setwindowsize(int wsize) {
    wsize = trunc(wsize);

    if (wsize != this->average_rms_tilde_02_av_currentWindowSize && wsize > 0 && wsize <= this->sr) {
        this->average_rms_tilde_02_av_currentWindowSize = wsize;
        this->average_rms_tilde_02_av_wantsReset = 1;
    }
}

void average_rms_tilde_02_av_reset() {
    this->average_rms_tilde_02_av_wantsReset = 1;
}

void average_rms_tilde_02_av_dspsetup() {
    this->average_rms_tilde_02_av_wantsReset = 1;

    if (this->sr > this->average_rms_tilde_02_av_buffer->getSize()) {
        this->average_rms_tilde_02_av_buffer->setSize(this->sr + 1);
        updateDataRef(this, this->average_rms_tilde_02_av_buffer);
    }
}

void average_rms_tilde_02_av_doReset() {
    this->average_rms_tilde_02_av_accum = 0;
    this->average_rms_tilde_02_av_effectiveWindowSize = 0;
    this->average_rms_tilde_02_av_bufferPos = 0;
    this->average_rms_tilde_02_av_wantsReset = 0;
}

void average_rms_tilde_02_av_init() {
    this->average_rms_tilde_02_av_currentWindowSize = this->sr;
    this->average_rms_tilde_02_av_buffer->requestSize(this->sr + 1, 1);
    this->average_rms_tilde_02_av_doReset();
}

void average_rms_tilde_02_dspsetup(bool force) {
    if ((bool)(this->average_rms_tilde_02_setupDone) && (bool)(!(bool)(force)))
        return;

    this->average_rms_tilde_02_setupDone = true;
    this->average_rms_tilde_02_av_dspsetup();
}

number line_01_valueAtTime(MillisecondTime time) {
    return this->line_01_startValue + this->line_01_slope * (time - this->line_01_startTime);
}

void line_01_scheduleNext() {
    MillisecondTime currentTime = (MillisecondTime)(this->currenttime());
    number nextTime = currentTime + this->line_01_grain;
    number nextValue;

    if (nextTime - this->line_01_startTime >= this->line_01__time || this->line_01_grain == 0) {
        nextTime = this->line_01_startTime + this->line_01__time;
        nextValue = this->line_01_currentTarget;
    } else {
        nextValue = this->line_01_valueAtTime(nextTime);
    }

    this->getEngine()->scheduleClockEventWithValue(this, 1220262738, nextTime - currentTime + this->_currentTime, nextValue);;
}

void line_01_startRamp(number target, MillisecondTime time) {
    MillisecondTime currentTime = (MillisecondTime)(this->currenttime());
    this->line_01_startValue = this->line_01_valueAtTime(currentTime);
    this->line_01_startTime = currentTime;
    this->line_01_currentTarget = target;
    this->getEngine()->flushClockEvents(this, 1220262738, false);;
    number rise = target - this->line_01_startValue;
    this->line_01_slope = rise / time;
    this->line_01_scheduleNext();
}

bool line_01_isFinished(number value) {
    return value == this->line_01_currentTarget && this->currenttime() - this->line_01_startTime >= this->line_01__time;
}

void line_01_startPendingRamp() {
    if (this->line_01_pendingRamps->length < 2) {
        this->line_01_pendingRamps->length = 0;
        this->line_01__time = 0;
        this->line_01_time_set(0);
        this->line_01_end_bang();
        return;
    }

    if (this->line_01_pendingRamps->length > 1) {
        number target = this->line_01_pendingRamps->shift();
        this->line_01__time = this->line_01_pendingRamps->shift();
        this->line_01__time = (this->line_01__time < 0 ? 0 : this->line_01__time);
        this->line_01_startRamp(target, this->line_01__time);
    }
}

void param_01_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_01_value;
}

void param_01_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_01_value_set(preset["value"]);
}

number average_rms_tilde_03_av_next(number x, int windowSize, bool reset) {
    if (windowSize > 0)
        this->average_rms_tilde_03_av_setwindowsize(windowSize);

    if (reset != 0) {
        if (this->average_rms_tilde_03_av_resetFlag != 1) {
            this->average_rms_tilde_03_av_wantsReset = 1;
            this->average_rms_tilde_03_av_resetFlag = 1;
        }
    } else {
        this->average_rms_tilde_03_av_resetFlag = 0;
    }

    if (this->average_rms_tilde_03_av_wantsReset == 1) {
        this->average_rms_tilde_03_av_doReset();
    }

    this->average_rms_tilde_03_av_accum += x;
    this->average_rms_tilde_03_av_buffer[(Index)this->average_rms_tilde_03_av_bufferPos] = x;
    number bufferSize = this->average_rms_tilde_03_av_buffer->getSize();

    if (this->average_rms_tilde_03_av_effectiveWindowSize < this->average_rms_tilde_03_av_currentWindowSize) {
        this->average_rms_tilde_03_av_effectiveWindowSize++;
    } else {
        number bufferReadPos = this->average_rms_tilde_03_av_bufferPos - this->average_rms_tilde_03_av_effectiveWindowSize;

        while (bufferReadPos < 0)
            bufferReadPos += bufferSize;

        this->average_rms_tilde_03_av_accum -= this->average_rms_tilde_03_av_buffer[(Index)bufferReadPos];
    }

    this->average_rms_tilde_03_av_bufferPos++;

    if (this->average_rms_tilde_03_av_bufferPos >= bufferSize) {
        this->average_rms_tilde_03_av_bufferPos -= bufferSize;
    }

    return this->average_rms_tilde_03_av_accum / this->average_rms_tilde_03_av_effectiveWindowSize;
}

void average_rms_tilde_03_av_setwindowsize(int wsize) {
    wsize = trunc(wsize);

    if (wsize != this->average_rms_tilde_03_av_currentWindowSize && wsize > 0 && wsize <= this->sr) {
        this->average_rms_tilde_03_av_currentWindowSize = wsize;
        this->average_rms_tilde_03_av_wantsReset = 1;
    }
}

void average_rms_tilde_03_av_reset() {
    this->average_rms_tilde_03_av_wantsReset = 1;
}

void average_rms_tilde_03_av_dspsetup() {
    this->average_rms_tilde_03_av_wantsReset = 1;

    if (this->sr > this->average_rms_tilde_03_av_buffer->getSize()) {
        this->average_rms_tilde_03_av_buffer->setSize(this->sr + 1);
        updateDataRef(this, this->average_rms_tilde_03_av_buffer);
    }
}

void average_rms_tilde_03_av_doReset() {
    this->average_rms_tilde_03_av_accum = 0;
    this->average_rms_tilde_03_av_effectiveWindowSize = 0;
    this->average_rms_tilde_03_av_bufferPos = 0;
    this->average_rms_tilde_03_av_wantsReset = 0;
}

void average_rms_tilde_03_av_init() {
    this->average_rms_tilde_03_av_currentWindowSize = this->sr;
    this->average_rms_tilde_03_av_buffer->requestSize(this->sr + 1, 1);
    this->average_rms_tilde_03_av_doReset();
}

void average_rms_tilde_03_dspsetup(bool force) {
    if ((bool)(this->average_rms_tilde_03_setupDone) && (bool)(!(bool)(force)))
        return;

    this->average_rms_tilde_03_setupDone = true;
    this->average_rms_tilde_03_av_dspsetup();
}

number average_rms_tilde_04_av_next(number x, int windowSize, bool reset) {
    if (windowSize > 0)
        this->average_rms_tilde_04_av_setwindowsize(windowSize);

    if (reset != 0) {
        if (this->average_rms_tilde_04_av_resetFlag != 1) {
            this->average_rms_tilde_04_av_wantsReset = 1;
            this->average_rms_tilde_04_av_resetFlag = 1;
        }
    } else {
        this->average_rms_tilde_04_av_resetFlag = 0;
    }

    if (this->average_rms_tilde_04_av_wantsReset == 1) {
        this->average_rms_tilde_04_av_doReset();
    }

    this->average_rms_tilde_04_av_accum += x;
    this->average_rms_tilde_04_av_buffer[(Index)this->average_rms_tilde_04_av_bufferPos] = x;
    number bufferSize = this->average_rms_tilde_04_av_buffer->getSize();

    if (this->average_rms_tilde_04_av_effectiveWindowSize < this->average_rms_tilde_04_av_currentWindowSize) {
        this->average_rms_tilde_04_av_effectiveWindowSize++;
    } else {
        number bufferReadPos = this->average_rms_tilde_04_av_bufferPos - this->average_rms_tilde_04_av_effectiveWindowSize;

        while (bufferReadPos < 0)
            bufferReadPos += bufferSize;

        this->average_rms_tilde_04_av_accum -= this->average_rms_tilde_04_av_buffer[(Index)bufferReadPos];
    }

    this->average_rms_tilde_04_av_bufferPos++;

    if (this->average_rms_tilde_04_av_bufferPos >= bufferSize) {
        this->average_rms_tilde_04_av_bufferPos -= bufferSize;
    }

    return this->average_rms_tilde_04_av_accum / this->average_rms_tilde_04_av_effectiveWindowSize;
}

void average_rms_tilde_04_av_setwindowsize(int wsize) {
    wsize = trunc(wsize);

    if (wsize != this->average_rms_tilde_04_av_currentWindowSize && wsize > 0 && wsize <= this->sr) {
        this->average_rms_tilde_04_av_currentWindowSize = wsize;
        this->average_rms_tilde_04_av_wantsReset = 1;
    }
}

void average_rms_tilde_04_av_reset() {
    this->average_rms_tilde_04_av_wantsReset = 1;
}

void average_rms_tilde_04_av_dspsetup() {
    this->average_rms_tilde_04_av_wantsReset = 1;

    if (this->sr > this->average_rms_tilde_04_av_buffer->getSize()) {
        this->average_rms_tilde_04_av_buffer->setSize(this->sr + 1);
        updateDataRef(this, this->average_rms_tilde_04_av_buffer);
    }
}

void average_rms_tilde_04_av_doReset() {
    this->average_rms_tilde_04_av_accum = 0;
    this->average_rms_tilde_04_av_effectiveWindowSize = 0;
    this->average_rms_tilde_04_av_bufferPos = 0;
    this->average_rms_tilde_04_av_wantsReset = 0;
}

void average_rms_tilde_04_av_init() {
    this->average_rms_tilde_04_av_currentWindowSize = this->sr;
    this->average_rms_tilde_04_av_buffer->requestSize(this->sr + 1, 1);
    this->average_rms_tilde_04_av_doReset();
}

void average_rms_tilde_04_dspsetup(bool force) {
    if ((bool)(this->average_rms_tilde_04_setupDone) && (bool)(!(bool)(force)))
        return;

    this->average_rms_tilde_04_setupDone = true;
    this->average_rms_tilde_04_av_dspsetup();
}

void param_02_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_02_value;
}

void param_02_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_02_value_set(preset["value"]);
}

void delaytilde_01_del_step() {
    this->delaytilde_01_del_reader++;

    if (this->delaytilde_01_del_reader >= (int)(this->delaytilde_01_del_buffer->getSize()))
        this->delaytilde_01_del_reader = 0;
}

number delaytilde_01_del_read(number size, Int interp) {
    if (interp == 0) {
        number r = (int)(this->delaytilde_01_del_buffer->getSize()) + this->delaytilde_01_del_reader - ((size > this->delaytilde_01_del__maxdelay ? this->delaytilde_01_del__maxdelay : (size < (this->delaytilde_01_del_reader != this->delaytilde_01_del_writer) ? this->delaytilde_01_del_reader != this->delaytilde_01_del_writer : size)));
        long index1 = (long)(rnbo_floor(r));
        number frac = r - index1;
        long index2 = (long)(index1 + 1);

        return this->linearinterp(frac, this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->delaytilde_01_del_wrap))
        ));
    } else if (interp == 1) {
        number r = (int)(this->delaytilde_01_del_buffer->getSize()) + this->delaytilde_01_del_reader - ((size > this->delaytilde_01_del__maxdelay ? this->delaytilde_01_del__maxdelay : (size < (1 + this->delaytilde_01_del_reader != this->delaytilde_01_del_writer) ? 1 + this->delaytilde_01_del_reader != this->delaytilde_01_del_writer : size)));
        long index1 = (long)(rnbo_floor(r));
        number frac = r - index1;
        Index index2 = (Index)(index1 + 1);
        Index index3 = (Index)(index2 + 1);
        Index index4 = (Index)(index3 + 1);

        return this->cubicinterp(frac, this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index3 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index4 & (BinOpInt)this->delaytilde_01_del_wrap))
        ));
    } else if (interp == 6) {
        number r = (int)(this->delaytilde_01_del_buffer->getSize()) + this->delaytilde_01_del_reader - ((size > this->delaytilde_01_del__maxdelay ? this->delaytilde_01_del__maxdelay : (size < (1 + this->delaytilde_01_del_reader != this->delaytilde_01_del_writer) ? 1 + this->delaytilde_01_del_reader != this->delaytilde_01_del_writer : size)));
        long index1 = (long)(rnbo_floor(r));
        number frac = r - index1;
        Index index2 = (Index)(index1 + 1);
        Index index3 = (Index)(index2 + 1);
        Index index4 = (Index)(index3 + 1);

        return this->fastcubicinterp(frac, this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index3 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index4 & (BinOpInt)this->delaytilde_01_del_wrap))
        ));
    } else if (interp == 2) {
        number r = (int)(this->delaytilde_01_del_buffer->getSize()) + this->delaytilde_01_del_reader - ((size > this->delaytilde_01_del__maxdelay ? this->delaytilde_01_del__maxdelay : (size < (1 + this->delaytilde_01_del_reader != this->delaytilde_01_del_writer) ? 1 + this->delaytilde_01_del_reader != this->delaytilde_01_del_writer : size)));
        long index1 = (long)(rnbo_floor(r));
        number frac = r - index1;
        Index index2 = (Index)(index1 + 1);
        Index index3 = (Index)(index2 + 1);
        Index index4 = (Index)(index3 + 1);

        return this->splineinterp(frac, this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index3 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index4 & (BinOpInt)this->delaytilde_01_del_wrap))
        ));
    } else if (interp == 7) {
        number r = (int)(this->delaytilde_01_del_buffer->getSize()) + this->delaytilde_01_del_reader - ((size > this->delaytilde_01_del__maxdelay ? this->delaytilde_01_del__maxdelay : (size < (1 + this->delaytilde_01_del_reader != this->delaytilde_01_del_writer) ? 1 + this->delaytilde_01_del_reader != this->delaytilde_01_del_writer : size)));
        long index1 = (long)(rnbo_floor(r));
        number frac = r - index1;
        Index index2 = (Index)(index1 + 1);
        Index index3 = (Index)(index2 + 1);
        Index index4 = (Index)(index3 + 1);
        Index index5 = (Index)(index4 + 1);
        Index index6 = (Index)(index5 + 1);

        return this->spline6interp(frac, this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index3 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index4 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index5 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index6 & (BinOpInt)this->delaytilde_01_del_wrap))
        ));
    } else if (interp == 3) {
        number r = (int)(this->delaytilde_01_del_buffer->getSize()) + this->delaytilde_01_del_reader - ((size > this->delaytilde_01_del__maxdelay ? this->delaytilde_01_del__maxdelay : (size < (this->delaytilde_01_del_reader != this->delaytilde_01_del_writer) ? this->delaytilde_01_del_reader != this->delaytilde_01_del_writer : size)));
        long index1 = (long)(rnbo_floor(r));
        number frac = r - index1;
        Index index2 = (Index)(index1 + 1);

        return this->cosineinterp(frac, this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_01_del_wrap))
        ), this->delaytilde_01_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->delaytilde_01_del_wrap))
        ));
    }

    number r = (int)(this->delaytilde_01_del_buffer->getSize()) + this->delaytilde_01_del_reader - ((size > this->delaytilde_01_del__maxdelay ? this->delaytilde_01_del__maxdelay : (size < (this->delaytilde_01_del_reader != this->delaytilde_01_del_writer) ? this->delaytilde_01_del_reader != this->delaytilde_01_del_writer : size)));
    long index1 = (long)(rnbo_floor(r));

    return this->delaytilde_01_del_buffer->getSample(
        0,
        (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_01_del_wrap))
    );
}

void delaytilde_01_del_write(number v) {
    this->delaytilde_01_del_writer = this->delaytilde_01_del_reader;
    this->delaytilde_01_del_buffer[(Index)this->delaytilde_01_del_writer] = v;
}

number delaytilde_01_del_next(number v, int size) {
    number effectiveSize = (size == -1 ? this->delaytilde_01_del__maxdelay : size);
    number val = this->delaytilde_01_del_read(effectiveSize, 0);
    this->delaytilde_01_del_write(v);
    this->delaytilde_01_del_step();
    return val;
}

array<Index, 2> delaytilde_01_del_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        this->delaytilde_01_del_sizemode = 1;
        sizeInSamples = this->mstosamps(200);
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void delaytilde_01_del_init() {
    auto result = this->delaytilde_01_del_calcSizeInSamples();
    this->delaytilde_01_del__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->delaytilde_01_del_buffer->requestSize(requestedSizeInSamples, 1);
    this->delaytilde_01_del_wrap = requestedSizeInSamples - 1;
}

void delaytilde_01_del_clear() {
    this->delaytilde_01_del_buffer->setZero();
}

void delaytilde_01_del_reset() {
    auto result = this->delaytilde_01_del_calcSizeInSamples();
    this->delaytilde_01_del__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->delaytilde_01_del_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->delaytilde_01_del_buffer);
    this->delaytilde_01_del_wrap = this->delaytilde_01_del_buffer->getSize() - 1;
    this->delaytilde_01_del_clear();

    if (this->delaytilde_01_del_reader >= this->delaytilde_01_del__maxdelay || this->delaytilde_01_del_writer >= this->delaytilde_01_del__maxdelay) {
        this->delaytilde_01_del_reader = 0;
        this->delaytilde_01_del_writer = 0;
    }
}

void delaytilde_01_del_dspsetup() {
    this->delaytilde_01_del_reset();
}

number delaytilde_01_del_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    return samplerate;
}

number delaytilde_01_del_size() {
    return this->delaytilde_01_del__maxdelay;
}

void delaytilde_01_dspsetup(bool force) {
    if ((bool)(this->delaytilde_01_setupDone) && (bool)(!(bool)(force)))
        return;

    this->delaytilde_01_rampInSamples = (long)(this->mstosamps(50));
    this->delaytilde_01_lastDelay = -1;
    this->delaytilde_01_setupDone = true;
    this->delaytilde_01_del_dspsetup();
}

void delaytilde_02_del_step() {
    this->delaytilde_02_del_reader++;

    if (this->delaytilde_02_del_reader >= (int)(this->delaytilde_02_del_buffer->getSize()))
        this->delaytilde_02_del_reader = 0;
}

number delaytilde_02_del_read(number size, Int interp) {
    if (interp == 0) {
        number r = (int)(this->delaytilde_02_del_buffer->getSize()) + this->delaytilde_02_del_reader - ((size > this->delaytilde_02_del__maxdelay ? this->delaytilde_02_del__maxdelay : (size < (this->delaytilde_02_del_reader != this->delaytilde_02_del_writer) ? this->delaytilde_02_del_reader != this->delaytilde_02_del_writer : size)));
        long index1 = (long)(rnbo_floor(r));
        number frac = r - index1;
        long index2 = (long)(index1 + 1);

        return this->linearinterp(frac, this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->delaytilde_02_del_wrap))
        ));
    } else if (interp == 1) {
        number r = (int)(this->delaytilde_02_del_buffer->getSize()) + this->delaytilde_02_del_reader - ((size > this->delaytilde_02_del__maxdelay ? this->delaytilde_02_del__maxdelay : (size < (1 + this->delaytilde_02_del_reader != this->delaytilde_02_del_writer) ? 1 + this->delaytilde_02_del_reader != this->delaytilde_02_del_writer : size)));
        long index1 = (long)(rnbo_floor(r));
        number frac = r - index1;
        Index index2 = (Index)(index1 + 1);
        Index index3 = (Index)(index2 + 1);
        Index index4 = (Index)(index3 + 1);

        return this->cubicinterp(frac, this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index3 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index4 & (BinOpInt)this->delaytilde_02_del_wrap))
        ));
    } else if (interp == 6) {
        number r = (int)(this->delaytilde_02_del_buffer->getSize()) + this->delaytilde_02_del_reader - ((size > this->delaytilde_02_del__maxdelay ? this->delaytilde_02_del__maxdelay : (size < (1 + this->delaytilde_02_del_reader != this->delaytilde_02_del_writer) ? 1 + this->delaytilde_02_del_reader != this->delaytilde_02_del_writer : size)));
        long index1 = (long)(rnbo_floor(r));
        number frac = r - index1;
        Index index2 = (Index)(index1 + 1);
        Index index3 = (Index)(index2 + 1);
        Index index4 = (Index)(index3 + 1);

        return this->fastcubicinterp(frac, this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index3 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index4 & (BinOpInt)this->delaytilde_02_del_wrap))
        ));
    } else if (interp == 2) {
        number r = (int)(this->delaytilde_02_del_buffer->getSize()) + this->delaytilde_02_del_reader - ((size > this->delaytilde_02_del__maxdelay ? this->delaytilde_02_del__maxdelay : (size < (1 + this->delaytilde_02_del_reader != this->delaytilde_02_del_writer) ? 1 + this->delaytilde_02_del_reader != this->delaytilde_02_del_writer : size)));
        long index1 = (long)(rnbo_floor(r));
        number frac = r - index1;
        Index index2 = (Index)(index1 + 1);
        Index index3 = (Index)(index2 + 1);
        Index index4 = (Index)(index3 + 1);

        return this->splineinterp(frac, this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index3 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index4 & (BinOpInt)this->delaytilde_02_del_wrap))
        ));
    } else if (interp == 7) {
        number r = (int)(this->delaytilde_02_del_buffer->getSize()) + this->delaytilde_02_del_reader - ((size > this->delaytilde_02_del__maxdelay ? this->delaytilde_02_del__maxdelay : (size < (1 + this->delaytilde_02_del_reader != this->delaytilde_02_del_writer) ? 1 + this->delaytilde_02_del_reader != this->delaytilde_02_del_writer : size)));
        long index1 = (long)(rnbo_floor(r));
        number frac = r - index1;
        Index index2 = (Index)(index1 + 1);
        Index index3 = (Index)(index2 + 1);
        Index index4 = (Index)(index3 + 1);
        Index index5 = (Index)(index4 + 1);
        Index index6 = (Index)(index5 + 1);

        return this->spline6interp(frac, this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index3 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index4 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index5 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index6 & (BinOpInt)this->delaytilde_02_del_wrap))
        ));
    } else if (interp == 3) {
        number r = (int)(this->delaytilde_02_del_buffer->getSize()) + this->delaytilde_02_del_reader - ((size > this->delaytilde_02_del__maxdelay ? this->delaytilde_02_del__maxdelay : (size < (this->delaytilde_02_del_reader != this->delaytilde_02_del_writer) ? this->delaytilde_02_del_reader != this->delaytilde_02_del_writer : size)));
        long index1 = (long)(rnbo_floor(r));
        number frac = r - index1;
        Index index2 = (Index)(index1 + 1);

        return this->cosineinterp(frac, this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_02_del_wrap))
        ), this->delaytilde_02_del_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->delaytilde_02_del_wrap))
        ));
    }

    number r = (int)(this->delaytilde_02_del_buffer->getSize()) + this->delaytilde_02_del_reader - ((size > this->delaytilde_02_del__maxdelay ? this->delaytilde_02_del__maxdelay : (size < (this->delaytilde_02_del_reader != this->delaytilde_02_del_writer) ? this->delaytilde_02_del_reader != this->delaytilde_02_del_writer : size)));
    long index1 = (long)(rnbo_floor(r));

    return this->delaytilde_02_del_buffer->getSample(
        0,
        (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->delaytilde_02_del_wrap))
    );
}

void delaytilde_02_del_write(number v) {
    this->delaytilde_02_del_writer = this->delaytilde_02_del_reader;
    this->delaytilde_02_del_buffer[(Index)this->delaytilde_02_del_writer] = v;
}

number delaytilde_02_del_next(number v, int size) {
    number effectiveSize = (size == -1 ? this->delaytilde_02_del__maxdelay : size);
    number val = this->delaytilde_02_del_read(effectiveSize, 0);
    this->delaytilde_02_del_write(v);
    this->delaytilde_02_del_step();
    return val;
}

array<Index, 2> delaytilde_02_del_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        this->delaytilde_02_del_sizemode = 1;
        sizeInSamples = this->mstosamps(200);
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void delaytilde_02_del_init() {
    auto result = this->delaytilde_02_del_calcSizeInSamples();
    this->delaytilde_02_del__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->delaytilde_02_del_buffer->requestSize(requestedSizeInSamples, 1);
    this->delaytilde_02_del_wrap = requestedSizeInSamples - 1;
}

void delaytilde_02_del_clear() {
    this->delaytilde_02_del_buffer->setZero();
}

void delaytilde_02_del_reset() {
    auto result = this->delaytilde_02_del_calcSizeInSamples();
    this->delaytilde_02_del__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->delaytilde_02_del_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->delaytilde_02_del_buffer);
    this->delaytilde_02_del_wrap = this->delaytilde_02_del_buffer->getSize() - 1;
    this->delaytilde_02_del_clear();

    if (this->delaytilde_02_del_reader >= this->delaytilde_02_del__maxdelay || this->delaytilde_02_del_writer >= this->delaytilde_02_del__maxdelay) {
        this->delaytilde_02_del_reader = 0;
        this->delaytilde_02_del_writer = 0;
    }
}

void delaytilde_02_del_dspsetup() {
    this->delaytilde_02_del_reset();
}

number delaytilde_02_del_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    return samplerate;
}

number delaytilde_02_del_size() {
    return this->delaytilde_02_del__maxdelay;
}

void delaytilde_02_dspsetup(bool force) {
    if ((bool)(this->delaytilde_02_setupDone) && (bool)(!(bool)(force)))
        return;

    this->delaytilde_02_rampInSamples = (long)(this->mstosamps(50));
    this->delaytilde_02_lastDelay = -1;
    this->delaytilde_02_setupDone = true;
    this->delaytilde_02_del_dspsetup();
}

void param_03_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_03_value;
}

void param_03_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_03_value_set(preset["value"]);
}

number line_02_valueAtTime(MillisecondTime time) {
    return this->line_02_startValue + this->line_02_slope * (time - this->line_02_startTime);
}

void line_02_scheduleNext() {
    MillisecondTime currentTime = (MillisecondTime)(this->currenttime());
    number nextTime = currentTime + this->line_02_grain;
    number nextValue;

    if (nextTime - this->line_02_startTime >= this->line_02__time || this->line_02_grain == 0) {
        nextTime = this->line_02_startTime + this->line_02__time;
        nextValue = this->line_02_currentTarget;
    } else {
        nextValue = this->line_02_valueAtTime(nextTime);
    }

    this->getEngine()->scheduleClockEventWithValue(this, 848255507, nextTime - currentTime + this->_currentTime, nextValue);;
}

void line_02_startRamp(number target, MillisecondTime time) {
    MillisecondTime currentTime = (MillisecondTime)(this->currenttime());
    this->line_02_startValue = this->line_02_valueAtTime(currentTime);
    this->line_02_startTime = currentTime;
    this->line_02_currentTarget = target;
    this->getEngine()->flushClockEvents(this, 848255507, false);;
    number rise = target - this->line_02_startValue;
    this->line_02_slope = rise / time;
    this->line_02_scheduleNext();
}

bool line_02_isFinished(number value) {
    return value == this->line_02_currentTarget && this->currenttime() - this->line_02_startTime >= this->line_02__time;
}

void line_02_startPendingRamp() {
    if (this->line_02_pendingRamps->length < 2) {
        this->line_02_pendingRamps->length = 0;
        this->line_02__time = 0;
        this->line_02_time_set(0);
        this->line_02_end_bang();
        return;
    }

    if (this->line_02_pendingRamps->length > 1) {
        number target = this->line_02_pendingRamps->shift();
        this->line_02__time = this->line_02_pendingRamps->shift();
        this->line_02__time = (this->line_02__time < 0 ? 0 : this->line_02__time);
        this->line_02_startRamp(target, this->line_02__time);
    }
}

void param_04_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_04_value;
}

void param_04_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_04_value_set(preset["value"]);
}

void param_05_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_05_value;
}

void param_05_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_05_value_set(preset["value"]);
}

void param_06_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_06_value;
}

void param_06_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_06_value_set(preset["value"]);
}

void param_07_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_07_value;
}

void param_07_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_07_value_set(preset["value"]);
}

void param_08_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_08_value;
}

void param_08_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_08_value_set(preset["value"]);
}

void globaltransport_advance() {}

void globaltransport_dspsetup(bool ) {}

bool stackprotect_check() {
    this->stackprotect_count++;

    if (this->stackprotect_count > 128) {
        console->log("STACK OVERFLOW DETECTED - stopped processing branch !");
        return true;
    }

    return false;
}

void updateTime(MillisecondTime time) {
    this->_currentTime = time;
    this->sampleOffsetIntoNextAudioBuffer = (SampleIndex)(rnbo_fround(this->msToSamps(time - this->getEngine()->getCurrentTime(), this->sr)));

    if (this->sampleOffsetIntoNextAudioBuffer >= (SampleIndex)(this->vs))
        this->sampleOffsetIntoNextAudioBuffer = (SampleIndex)(this->vs) - 1;

    if (this->sampleOffsetIntoNextAudioBuffer < 0)
        this->sampleOffsetIntoNextAudioBuffer = 0;
}

void assign_defaults()
{
    expr_01_in1 = 0;
    expr_01_out1 = 0;
    snapshot_01_interval = 15;
    snapshot_01_out = 0;
    average_rms_tilde_01_x = 0;
    average_rms_tilde_01_windowSize = 2048;
    average_rms_tilde_01_reset = 0;
    expr_02_in1 = 0;
    expr_02_out1 = 0;
    snapshot_02_interval = 15;
    snapshot_02_out = 0;
    average_rms_tilde_02_x = 0;
    average_rms_tilde_02_windowSize = 2048;
    average_rms_tilde_02_reset = 0;
    line_01_time = 0;
    line_01_grain = 20;
    line_01_output = 0;
    expr_03_in1 = 0;
    expr_03_out1 = 0;
    param_01_value = 0;
    expr_04_in1 = 0;
    expr_04_out1 = 0;
    snapshot_03_interval = 15;
    snapshot_03_out = 0;
    average_rms_tilde_03_x = 0;
    average_rms_tilde_03_windowSize = 2048;
    average_rms_tilde_03_reset = 0;
    expr_05_in1 = 0;
    expr_05_out1 = 0;
    snapshot_04_interval = 15;
    snapshot_04_out = 0;
    average_rms_tilde_04_x = 0;
    average_rms_tilde_04_windowSize = 2048;
    average_rms_tilde_04_reset = 0;
    param_02_value = 0;
    expr_06_$in1 = 0;
    expr_06_out1 = 0;
    expr_07_in1 = 0;
    expr_07_in2 = 0;
    expr_07_out1 = 0;
    delaytilde_01_delay = 0;
    dspexpr_01_in1 = 0;
    dspexpr_01_in2 = 0;
    dspexpr_02_in1 = 0;
    dspexpr_02_in2 = 0;
    dspexpr_03_in1 = 0;
    dspexpr_03_in2 = 0.707107;
    dspexpr_04_in1 = 0;
    dspexpr_04_in2 = 0.707107;
    dspexpr_05_in1 = 0;
    dspexpr_05_in2 = 0;
    delaytilde_02_delay = 0;
    dspexpr_06_in1 = 0;
    dspexpr_06_in2 = 0;
    dspexpr_07_in1 = 0;
    dspexpr_07_in2 = 0;
    selector_01_onoff = 1;
    dspexpr_08_in1 = 0;
    dspexpr_08_in2 = 0;
    dspexpr_09_in1 = 0;
    dspexpr_09_in2 = 0.707107;
    dspexpr_10_in1 = 0;
    dspexpr_10_in2 = 0;
    dspexpr_11_in1 = 0;
    dspexpr_11_in2 = 0;
    selector_02_onoff = 1;
    dspexpr_12_in1 = 0;
    dspexpr_12_in2 = 0;
    dspexpr_13_in1 = 0;
    dspexpr_13_in2 = 0;
    dspexpr_14_in1 = 0;
    dspexpr_14_in2 = 0.707107;
    expr_08_in1 = 0;
    expr_08_in2 = 0;
    expr_08_out1 = 0;
    linetilde_01_time = 0;
    linetilde_01_keepramp = 0;
    expr_09_in1 = 0;
    expr_09_out1 = 0;
    param_03_value = 0;
    slide_tilde_01_x = 0;
    slide_tilde_01_up = 5000;
    slide_tilde_01_down = 5000;
    dspexpr_15_in1 = 0;
    dspexpr_15_in2 = 0;
    line_02_time = 0;
    line_02_grain = 20;
    line_02_output = 0;
    expr_10_in1 = 0;
    expr_10_out1 = 0;
    param_04_value = 0;
    expr_11_$in1 = 0;
    expr_11_out1 = 0;
    param_05_value = 0;
    expr_12_$in1 = 0;
    expr_12_out1 = 0;
    param_06_value = 0;
    mstosamps_01_ms = 0;
    param_07_value = 0;
    snapshot_05_interval = 15;
    snapshot_05_out = 0;
    dspexpr_16_in1 = 0;
    dspexpr_16_in2 = 0;
    expr_13_$in1 = 0;
    expr_13_$in2 = 0;
    expr_13_out1 = 0;
    expr_14_$in1 = 0;
    expr_14_out1 = 0;
    dspexpr_17_in1 = 0;
    dspexpr_17_in2 = 0.0001;
    slide_tilde_02_x = 0;
    slide_tilde_02_up = 5000;
    slide_tilde_02_down = 5000;
    dspexpr_18_in1 = 0;
    dspexpr_18_in2 = 0;
    param_08_value = 20;
    snapshot_06_interval = 5;
    snapshot_06_out = 0;
    dspexpr_19_in1 = 0;
    dspexpr_19_in2 = 0;
    snapshot_07_interval = 5;
    snapshot_07_out = 0;
    dspexpr_20_in1 = 0;
    dspexpr_20_in2 = 0;
    _currentTime = 0;
    audioProcessSampleCount = 0;
    sampleOffsetIntoNextAudioBuffer = 0;
    zeroBuffer = nullptr;
    dummyBuffer = nullptr;
    signals[0] = nullptr;
    signals[1] = nullptr;
    signals[2] = nullptr;
    signals[3] = nullptr;
    signals[4] = nullptr;
    didAllocateSignals = 0;
    vs = 0;
    maxvs = 0;
    sr = 44100;
    invsr = 0.00002267573696;
    snapshot_01_calc = 0;
    snapshot_01_nextTime = 0;
    snapshot_01_count = 0;
    snapshot_01_lastValue = 0;
    average_rms_tilde_01_av_currentWindowSize = 44100;
    average_rms_tilde_01_av_accum = 0;
    average_rms_tilde_01_av_effectiveWindowSize = 0;
    average_rms_tilde_01_av_bufferPos = 0;
    average_rms_tilde_01_av_wantsReset = false;
    average_rms_tilde_01_av_resetFlag = false;
    average_rms_tilde_01_setupDone = false;
    snapshot_02_calc = 0;
    snapshot_02_nextTime = 0;
    snapshot_02_count = 0;
    snapshot_02_lastValue = 0;
    average_rms_tilde_02_av_currentWindowSize = 44100;
    average_rms_tilde_02_av_accum = 0;
    average_rms_tilde_02_av_effectiveWindowSize = 0;
    average_rms_tilde_02_av_bufferPos = 0;
    average_rms_tilde_02_av_wantsReset = false;
    average_rms_tilde_02_av_resetFlag = false;
    average_rms_tilde_02_setupDone = false;
    line_01_startTime = 0;
    line_01_startValue = 0;
    line_01_currentTarget = 0;
    line_01_slope = 0;
    line_01__time = 0;
    param_01_lastValue = 0;
    snapshot_03_calc = 0;
    snapshot_03_nextTime = 0;
    snapshot_03_count = 0;
    snapshot_03_lastValue = 0;
    average_rms_tilde_03_av_currentWindowSize = 44100;
    average_rms_tilde_03_av_accum = 0;
    average_rms_tilde_03_av_effectiveWindowSize = 0;
    average_rms_tilde_03_av_bufferPos = 0;
    average_rms_tilde_03_av_wantsReset = false;
    average_rms_tilde_03_av_resetFlag = false;
    average_rms_tilde_03_setupDone = false;
    snapshot_04_calc = 0;
    snapshot_04_nextTime = 0;
    snapshot_04_count = 0;
    snapshot_04_lastValue = 0;
    average_rms_tilde_04_av_currentWindowSize = 44100;
    average_rms_tilde_04_av_accum = 0;
    average_rms_tilde_04_av_effectiveWindowSize = 0;
    average_rms_tilde_04_av_bufferPos = 0;
    average_rms_tilde_04_av_wantsReset = false;
    average_rms_tilde_04_av_resetFlag = false;
    average_rms_tilde_04_setupDone = false;
    param_02_lastValue = 0;
    delaytilde_01_lastDelay = -1;
    delaytilde_01_crossfadeDelay = 0;
    delaytilde_01_ramp = 0;
    delaytilde_01_rampInSamples = 0;
    delaytilde_01_del__maxdelay = 0;
    delaytilde_01_del_sizemode = 0;
    delaytilde_01_del_wrap = 0;
    delaytilde_01_del_reader = 0;
    delaytilde_01_del_writer = 0;
    delaytilde_01_setupDone = false;
    delaytilde_02_lastDelay = -1;
    delaytilde_02_crossfadeDelay = 0;
    delaytilde_02_ramp = 0;
    delaytilde_02_rampInSamples = 0;
    delaytilde_02_del__maxdelay = 0;
    delaytilde_02_del_sizemode = 0;
    delaytilde_02_del_wrap = 0;
    delaytilde_02_del_reader = 0;
    delaytilde_02_del_writer = 0;
    delaytilde_02_setupDone = false;
    linetilde_01_currentValue = 0;
    param_03_lastValue = 0;
    slide_tilde_01_prev = 0;
    line_02_startTime = 0;
    line_02_startValue = 0;
    line_02_currentTarget = 0;
    line_02_slope = 0;
    line_02__time = 0;
    param_04_lastValue = 0;
    param_05_lastValue = 0;
    param_06_lastValue = 0;
    param_07_lastValue = 0;
    snapshot_05_calc = 0;
    snapshot_05_nextTime = 0;
    snapshot_05_count = 0;
    snapshot_05_lastValue = 0;
    slide_tilde_02_prev = 0;
    param_08_lastValue = 0;
    snapshot_06_calc = 0;
    snapshot_06_nextTime = 0;
    snapshot_06_count = 0;
    snapshot_06_lastValue = 0;
    snapshot_07_calc = 0;
    snapshot_07_nextTime = 0;
    snapshot_07_count = 0;
    snapshot_07_lastValue = 0;
    globaltransport_tempo = nullptr;
    globaltransport_state = nullptr;
    stackprotect_count = 0;
    _voiceIndex = 0;
    _noteNumber = 0;
    isMuted = 1;
}

// member variables

    number expr_01_in1;
    number expr_01_out1;
    number snapshot_01_interval;
    number snapshot_01_out;
    number average_rms_tilde_01_x;
    number average_rms_tilde_01_windowSize;
    number average_rms_tilde_01_reset;
    number expr_02_in1;
    number expr_02_out1;
    number snapshot_02_interval;
    number snapshot_02_out;
    number average_rms_tilde_02_x;
    number average_rms_tilde_02_windowSize;
    number average_rms_tilde_02_reset;
    list line_01_target;
    number line_01_time;
    number line_01_grain;
    number line_01_output;
    number expr_03_in1;
    number expr_03_out1;
    number param_01_value;
    number expr_04_in1;
    number expr_04_out1;
    number snapshot_03_interval;
    number snapshot_03_out;
    number average_rms_tilde_03_x;
    number average_rms_tilde_03_windowSize;
    number average_rms_tilde_03_reset;
    number expr_05_in1;
    number expr_05_out1;
    number snapshot_04_interval;
    number snapshot_04_out;
    number average_rms_tilde_04_x;
    number average_rms_tilde_04_windowSize;
    number average_rms_tilde_04_reset;
    number param_02_value;
    number expr_06_$in1;
    number expr_06_out1;
    number expr_07_in1;
    number expr_07_in2;
    number expr_07_out1;
    number delaytilde_01_delay;
    number dspexpr_01_in1;
    number dspexpr_01_in2;
    number dspexpr_02_in1;
    number dspexpr_02_in2;
    number dspexpr_03_in1;
    number dspexpr_03_in2;
    number dspexpr_04_in1;
    number dspexpr_04_in2;
    number dspexpr_05_in1;
    number dspexpr_05_in2;
    number delaytilde_02_delay;
    number dspexpr_06_in1;
    number dspexpr_06_in2;
    number dspexpr_07_in1;
    number dspexpr_07_in2;
    number selector_01_onoff;
    number dspexpr_08_in1;
    number dspexpr_08_in2;
    number dspexpr_09_in1;
    number dspexpr_09_in2;
    number dspexpr_10_in1;
    number dspexpr_10_in2;
    number dspexpr_11_in1;
    number dspexpr_11_in2;
    number selector_02_onoff;
    number dspexpr_12_in1;
    number dspexpr_12_in2;
    number dspexpr_13_in1;
    number dspexpr_13_in2;
    number dspexpr_14_in1;
    number dspexpr_14_in2;
    number expr_08_in1;
    number expr_08_in2;
    number expr_08_out1;
    list linetilde_01_segments;
    number linetilde_01_time;
    number linetilde_01_keepramp;
    number expr_09_in1;
    number expr_09_out1;
    number param_03_value;
    number slide_tilde_01_x;
    number slide_tilde_01_up;
    number slide_tilde_01_down;
    number dspexpr_15_in1;
    number dspexpr_15_in2;
    list line_02_target;
    number line_02_time;
    number line_02_grain;
    number line_02_output;
    number expr_10_in1;
    number expr_10_out1;
    number param_04_value;
    number expr_11_$in1;
    number expr_11_out1;
    number param_05_value;
    number expr_12_$in1;
    number expr_12_out1;
    number param_06_value;
    number mstosamps_01_ms;
    number param_07_value;
    number snapshot_05_interval;
    number snapshot_05_out;
    number dspexpr_16_in1;
    number dspexpr_16_in2;
    number expr_13_$in1;
    number expr_13_$in2;
    number expr_13_out1;
    number expr_14_$in1;
    number expr_14_out1;
    number dspexpr_17_in1;
    number dspexpr_17_in2;
    number slide_tilde_02_x;
    number slide_tilde_02_up;
    number slide_tilde_02_down;
    number dspexpr_18_in1;
    number dspexpr_18_in2;
    number param_08_value;
    number snapshot_06_interval;
    number snapshot_06_out;
    number dspexpr_19_in1;
    number dspexpr_19_in2;
    number snapshot_07_interval;
    number snapshot_07_out;
    number dspexpr_20_in1;
    number dspexpr_20_in2;
    MillisecondTime _currentTime;
    UInt64 audioProcessSampleCount;
    SampleIndex sampleOffsetIntoNextAudioBuffer;
    signal zeroBuffer;
    signal dummyBuffer;
    SampleValue * signals[5];
    bool didAllocateSignals;
    Index vs;
    Index maxvs;
    number sr;
    number invsr;
    number snapshot_01_calc;
    number snapshot_01_nextTime;
    SampleIndex snapshot_01_count;
    number snapshot_01_lastValue;
    int average_rms_tilde_01_av_currentWindowSize;
    number average_rms_tilde_01_av_accum;
    int average_rms_tilde_01_av_effectiveWindowSize;
    int average_rms_tilde_01_av_bufferPos;
    bool average_rms_tilde_01_av_wantsReset;
    bool average_rms_tilde_01_av_resetFlag;
    Float64BufferRef average_rms_tilde_01_av_buffer;
    bool average_rms_tilde_01_setupDone;
    number snapshot_02_calc;
    number snapshot_02_nextTime;
    SampleIndex snapshot_02_count;
    number snapshot_02_lastValue;
    int average_rms_tilde_02_av_currentWindowSize;
    number average_rms_tilde_02_av_accum;
    int average_rms_tilde_02_av_effectiveWindowSize;
    int average_rms_tilde_02_av_bufferPos;
    bool average_rms_tilde_02_av_wantsReset;
    bool average_rms_tilde_02_av_resetFlag;
    Float64BufferRef average_rms_tilde_02_av_buffer;
    bool average_rms_tilde_02_setupDone;
    MillisecondTime line_01_startTime;
    number line_01_startValue;
    number line_01_currentTarget;
    number line_01_slope;
    MillisecondTime line_01__time;
    list line_01_pendingRamps;
    number param_01_lastValue;
    number snapshot_03_calc;
    number snapshot_03_nextTime;
    SampleIndex snapshot_03_count;
    number snapshot_03_lastValue;
    int average_rms_tilde_03_av_currentWindowSize;
    number average_rms_tilde_03_av_accum;
    int average_rms_tilde_03_av_effectiveWindowSize;
    int average_rms_tilde_03_av_bufferPos;
    bool average_rms_tilde_03_av_wantsReset;
    bool average_rms_tilde_03_av_resetFlag;
    Float64BufferRef average_rms_tilde_03_av_buffer;
    bool average_rms_tilde_03_setupDone;
    number snapshot_04_calc;
    number snapshot_04_nextTime;
    SampleIndex snapshot_04_count;
    number snapshot_04_lastValue;
    int average_rms_tilde_04_av_currentWindowSize;
    number average_rms_tilde_04_av_accum;
    int average_rms_tilde_04_av_effectiveWindowSize;
    int average_rms_tilde_04_av_bufferPos;
    bool average_rms_tilde_04_av_wantsReset;
    bool average_rms_tilde_04_av_resetFlag;
    Float64BufferRef average_rms_tilde_04_av_buffer;
    bool average_rms_tilde_04_setupDone;
    number param_02_lastValue;
    number delaytilde_01_lastDelay;
    number delaytilde_01_crossfadeDelay;
    number delaytilde_01_ramp;
    long delaytilde_01_rampInSamples;
    Float64BufferRef delaytilde_01_del_buffer;
    Index delaytilde_01_del__maxdelay;
    Int delaytilde_01_del_sizemode;
    Index delaytilde_01_del_wrap;
    Int delaytilde_01_del_reader;
    Int delaytilde_01_del_writer;
    bool delaytilde_01_setupDone;
    number delaytilde_02_lastDelay;
    number delaytilde_02_crossfadeDelay;
    number delaytilde_02_ramp;
    long delaytilde_02_rampInSamples;
    Float64BufferRef delaytilde_02_del_buffer;
    Index delaytilde_02_del__maxdelay;
    Int delaytilde_02_del_sizemode;
    Index delaytilde_02_del_wrap;
    Int delaytilde_02_del_reader;
    Int delaytilde_02_del_writer;
    bool delaytilde_02_setupDone;
    list linetilde_01_activeRamps;
    number linetilde_01_currentValue;
    number param_03_lastValue;
    number slide_tilde_01_prev;
    MillisecondTime line_02_startTime;
    number line_02_startValue;
    number line_02_currentTarget;
    number line_02_slope;
    MillisecondTime line_02__time;
    list line_02_pendingRamps;
    number param_04_lastValue;
    number param_05_lastValue;
    number param_06_lastValue;
    number param_07_lastValue;
    number snapshot_05_calc;
    number snapshot_05_nextTime;
    SampleIndex snapshot_05_count;
    number snapshot_05_lastValue;
    number slide_tilde_02_prev;
    number param_08_lastValue;
    number snapshot_06_calc;
    number snapshot_06_nextTime;
    SampleIndex snapshot_06_count;
    number snapshot_06_lastValue;
    number snapshot_07_calc;
    number snapshot_07_nextTime;
    SampleIndex snapshot_07_count;
    number snapshot_07_lastValue;
    signal globaltransport_tempo;
    signal globaltransport_state;
    number stackprotect_count;
    DataRef average_rms_tilde_01_av_bufferobj;
    DataRef average_rms_tilde_02_av_bufferobj;
    DataRef average_rms_tilde_03_av_bufferobj;
    DataRef average_rms_tilde_04_av_bufferobj;
    DataRef delaytilde_01_del_bufferobj;
    DataRef delaytilde_02_del_bufferobj;
    Index _voiceIndex;
    Int _noteNumber;
    Index isMuted;
    indexlist paramInitIndices;
    indexlist paramInitOrder;

};

PatcherInterface* creaternbomatic()
{
    return new rnbomatic();
}

#ifndef RNBO_NO_PATCHERFACTORY

extern "C" PatcherFactoryFunctionPtr GetPatcherFactoryFunction(PlatformInterface* platformInterface)
#else

extern "C" PatcherFactoryFunctionPtr rnbomaticFactoryFunction(PlatformInterface* platformInterface)
#endif

{
    Platform::set(platformInterface);
    return creaternbomatic;
}

} // end RNBO namespace

