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
    getEngine()->flushClockEvents(this, 1910212691, false);
    getEngine()->flushClockEvents(this, -1245190316, false);
    getEngine()->flushClockEvents(this, -105626027, false);
    getEngine()->flushClockEvents(this, 1033938262, false);
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

SampleIndex currentsampletime() {
    return this->audioProcessSampleCount + this->sampleOffsetIntoNextAudioBuffer;
}

number mstosamps(MillisecondTime ms) {
    return ms * this->sr * 0.001;
}

number maximum(number x, number y) {
    return (x < y ? y : x);
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

    this->average_rms_tilde_01_perform(
        in1,
        this->average_rms_tilde_01_windowSize,
        this->average_rms_tilde_01_reset,
        this->signals[0],
        n
    );

    this->snapshot_01_perform(this->signals[0], n);

    this->average_rms_tilde_02_perform(
        in2,
        this->average_rms_tilde_02_windowSize,
        this->average_rms_tilde_02_reset,
        this->signals[0],
        n
    );

    this->snapshot_02_perform(this->signals[0], n);
    this->linetilde_01_perform(this->signals[0], n);
    this->dspexpr_02_perform(in2, this->signals[0], this->signals[1], n);

    this->average_rms_tilde_04_perform(
        this->signals[1],
        this->average_rms_tilde_04_windowSize,
        this->average_rms_tilde_04_reset,
        this->signals[2],
        n
    );

    this->snapshot_04_perform(this->signals[2], n);
    this->signalforwarder_01_perform(this->signals[1], out2, n);
    this->dspexpr_01_perform(in1, this->signals[0], this->signals[1], n);

    this->average_rms_tilde_03_perform(
        this->signals[1],
        this->average_rms_tilde_03_windowSize,
        this->average_rms_tilde_03_reset,
        this->signals[0],
        n
    );

    this->snapshot_03_perform(this->signals[0], n);
    this->signalforwarder_02_perform(this->signals[1], out1, n);
    this->stackprotect_perform(n);
    this->globaltransport_advance();
    this->audioProcessSampleCount += this->vs;
}

void prepareToProcess(number sampleRate, Index maxBlockSize, bool force) {
    if (this->maxvs < maxBlockSize || !this->didAllocateSignals) {
        Index i;

        for (i = 0; i < 3; i++) {
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

    this->average_rms_tilde_01_dspsetup(forceDSPSetup);
    this->average_rms_tilde_02_dspsetup(forceDSPSetup);
    this->average_rms_tilde_04_dspsetup(forceDSPSetup);
    this->average_rms_tilde_03_dspsetup(forceDSPSetup);
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
    default:
        {
        return nullptr;
        }
    }
}

DataRefIndex getNumDataRefs() const {
    return 4;
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
}

void initialize() {
    this->average_rms_tilde_01_av_bufferobj = initDataRef("average_rms_tilde_01_av_bufferobj", true, nullptr, "buffer~");
    this->average_rms_tilde_02_av_bufferobj = initDataRef("average_rms_tilde_02_av_bufferobj", true, nullptr, "buffer~");
    this->average_rms_tilde_03_av_bufferobj = initDataRef("average_rms_tilde_03_av_bufferobj", true, nullptr, "buffer~");
    this->average_rms_tilde_04_av_bufferobj = initDataRef("average_rms_tilde_04_av_bufferobj", true, nullptr, "buffer~");
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
    this->param_01_getPresetValue(getSubState(preset, "gain"));
}

void setPreset(MillisecondTime time, PatcherStateInterface& preset) {
    this->updateTime(time);
    this->param_01_setPresetValue(getSubState(preset, "gain"));
}

void setParameterValue(ParameterIndex index, ParameterValue v, MillisecondTime time) {
    this->updateTime(time);

    switch (index) {
    case 0:
        {
        this->param_01_value_set(v);
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
    return 1;
}

ConstCharPointer getParameterName(ParameterIndex index) const {
    switch (index) {
    case 0:
        {
        return "gain";
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
        return "gain";
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
    case 0:
        {
        {
            value = (value < -60 ? -60 : (value > 12 ? 12 : value));
            ParameterValue normalizedValue = (value - -60) / (12 - -60);
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
    case 0:
        {
        {
            {
                return -60 + value * (12 - -60);
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
    case 1910212691:
        {
        this->snapshot_01_out_set(value);
        break;
        }
    case -1245190316:
        {
        this->snapshot_02_out_set(value);
        break;
        }
    case -105626027:
        {
        this->snapshot_03_out_set(value);
        break;
        }
    case 1033938262:
        {
        this->snapshot_04_out_set(value);
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
    case TAG("in_rms_L"):
        {
        return "in_rms_L";
        }
    case TAG(""):
        {
        return "";
        }
    case TAG("in_rms_R"):
        {
        return "in_rms_R";
        }
    case TAG("out_rms_L"):
        {
        return "out_rms_L";
        }
    case TAG("out_rms_R"):
        {
        return "out_rms_R";
        }
    }

    return "";
}

MessageIndex getNumMessages() const {
    return 4;
}

const MessageInfo& getMessageInfo(MessageIndex index) const {
    switch (index) {
    case 0:
        {
        static const MessageInfo r0 = {
            "in_rms_L",
            Outport
        };

        return r0;
        }
    case 1:
        {
        static const MessageInfo r1 = {
            "in_rms_R",
            Outport
        };

        return r1;
        }
    case 2:
        {
        static const MessageInfo r2 = {
            "out_rms_L",
            Outport
        };

        return r2;
        }
    case 3:
        {
        static const MessageInfo r3 = {
            "out_rms_R",
            Outport
        };

        return r3;
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

void snapshot_01_out_set(number v) {
    this->snapshot_01_out = v;
    this->expr_01_in1_set(v);
}

void snapshot_02_out_set(number v) {
    this->snapshot_02_out = v;
    this->expr_02_in1_set(v);
}

void snapshot_03_out_set(number v) {
    this->snapshot_03_out = v;
    this->expr_04_in1_set(v);
}

void snapshot_04_out_set(number v) {
    this->snapshot_04_out = v;
    this->expr_05_in1_set(v);
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
}

void initializeObjects() {
    this->average_rms_tilde_01_av_init();
    this->average_rms_tilde_02_av_init();
    this->average_rms_tilde_03_av_init();
    this->average_rms_tilde_04_av_init();
}

void sendOutlet(OutletIndex index, ParameterValue value) {
    this->getEngine()->sendOutlet(this, index, value);
}

void startup() {
    this->updateTime(this->getEngine()->getCurrentTime());

    {
        this->scheduleParamInit(0, 0);
    }

    this->processParamInitEvents();
}

number param_01_value_constrain(number v) const {
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

void expr_03_out1_set(number v) {
    this->expr_03_out1 = v;

    {
        list converted = {this->expr_03_out1};
        this->linetilde_01_segments_set(converted);
    }
}

void expr_03_in1_set(number in1) {
    this->expr_03_in1 = in1;
    this->expr_03_out1_set(rnbo_pow(10, this->expr_03_in1 * 0.05));//#map:dbtoa_obj-8:1
}

void outport_01_input_number_set(number v) {
    this->getEngine()->sendNumMessage(TAG("in_rms_L"), TAG(""), v, this->_currentTime);
}

void expr_01_out1_set(number v) {
    this->expr_01_out1 = v;
    this->outport_01_input_number_set(this->expr_01_out1);
}

void expr_01_in1_set(number in1) {
    this->expr_01_in1 = in1;

    this->expr_01_out1_set(
        (this->expr_01_in1 <= 0 ? -999 : 20 * rnbo_log10((this->expr_01_in1 <= 0.0000000001 ? 0.0000000001 : this->expr_01_in1)))
    );//#map:atodb_obj-30:1
}

void outport_02_input_number_set(number v) {
    this->getEngine()->sendNumMessage(TAG("in_rms_R"), TAG(""), v, this->_currentTime);
}

void expr_02_out1_set(number v) {
    this->expr_02_out1 = v;
    this->outport_02_input_number_set(this->expr_02_out1);
}

void expr_02_in1_set(number in1) {
    this->expr_02_in1 = in1;

    this->expr_02_out1_set(
        (this->expr_02_in1 <= 0 ? -999 : 20 * rnbo_log10((this->expr_02_in1 <= 0.0000000001 ? 0.0000000001 : this->expr_02_in1)))
    );//#map:atodb_obj-31:1
}

void outport_03_input_number_set(number v) {
    this->getEngine()->sendNumMessage(TAG("out_rms_L"), TAG(""), v, this->_currentTime);
}

void expr_04_out1_set(number v) {
    this->expr_04_out1 = v;
    this->outport_03_input_number_set(this->expr_04_out1);
}

void expr_04_in1_set(number in1) {
    this->expr_04_in1 = in1;

    this->expr_04_out1_set(
        (this->expr_04_in1 <= 0 ? -999 : 20 * rnbo_log10((this->expr_04_in1 <= 0.0000000001 ? 0.0000000001 : this->expr_04_in1)))
    );//#map:atodb_obj-32:1
}

void outport_04_input_number_set(number v) {
    this->getEngine()->sendNumMessage(TAG("out_rms_R"), TAG(""), v, this->_currentTime);
}

void expr_05_out1_set(number v) {
    this->expr_05_out1 = v;
    this->outport_04_input_number_set(this->expr_05_out1);
}

void expr_05_in1_set(number in1) {
    this->expr_05_in1 = in1;

    this->expr_05_out1_set(
        (this->expr_05_in1 <= 0 ? -999 : 20 * rnbo_log10((this->expr_05_in1 <= 0.0000000001 ? 0.0000000001 : this->expr_05_in1)))
    );//#map:atodb_obj-33:1
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
                    1910212691,
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
                    -1245190316,
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

void dspexpr_02_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
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
                    1033938262,
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

void signalforwarder_01_perform(const SampleValue * input, SampleValue * output, Index n) {
    for (Index i = 0; i < n; i++) {
        output[(Index)i] = input[(Index)i];
    }
}

void dspexpr_01_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
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
                    -105626027,
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
    snapshot_01_interval = 30;
    snapshot_01_out = 0;
    average_rms_tilde_01_x = 0;
    average_rms_tilde_01_windowSize = 2048;
    average_rms_tilde_01_reset = 0;
    expr_02_in1 = 0;
    expr_02_out1 = 0;
    snapshot_02_interval = 30;
    snapshot_02_out = 0;
    average_rms_tilde_02_x = 0;
    average_rms_tilde_02_windowSize = 2048;
    average_rms_tilde_02_reset = 0;
    dspexpr_01_in1 = 0;
    dspexpr_01_in2 = 0;
    dspexpr_02_in1 = 0;
    dspexpr_02_in2 = 0;
    linetilde_01_time = 0;
    linetilde_01_keepramp = 0;
    expr_03_in1 = 0;
    expr_03_out1 = 0;
    param_01_value = 0;
    expr_04_in1 = 0;
    expr_04_out1 = 0;
    snapshot_03_interval = 30;
    snapshot_03_out = 0;
    average_rms_tilde_03_x = 0;
    average_rms_tilde_03_windowSize = 2048;
    average_rms_tilde_03_reset = 0;
    expr_05_in1 = 0;
    expr_05_out1 = 0;
    snapshot_04_interval = 30;
    snapshot_04_out = 0;
    average_rms_tilde_04_x = 0;
    average_rms_tilde_04_windowSize = 2048;
    average_rms_tilde_04_reset = 0;
    _currentTime = 0;
    audioProcessSampleCount = 0;
    sampleOffsetIntoNextAudioBuffer = 0;
    zeroBuffer = nullptr;
    dummyBuffer = nullptr;
    signals[0] = nullptr;
    signals[1] = nullptr;
    signals[2] = nullptr;
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
    linetilde_01_currentValue = 0;
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
    number dspexpr_01_in1;
    number dspexpr_01_in2;
    number dspexpr_02_in1;
    number dspexpr_02_in2;
    list linetilde_01_segments;
    number linetilde_01_time;
    number linetilde_01_keepramp;
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
    MillisecondTime _currentTime;
    UInt64 audioProcessSampleCount;
    SampleIndex sampleOffsetIntoNextAudioBuffer;
    signal zeroBuffer;
    signal dummyBuffer;
    SampleValue * signals[3];
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
    list linetilde_01_activeRamps;
    number linetilde_01_currentValue;
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
    signal globaltransport_tempo;
    signal globaltransport_state;
    number stackprotect_count;
    DataRef average_rms_tilde_01_av_bufferobj;
    DataRef average_rms_tilde_02_av_bufferobj;
    DataRef average_rms_tilde_03_av_bufferobj;
    DataRef average_rms_tilde_04_av_bufferobj;
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

