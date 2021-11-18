#pragma once

#ifndef SND_RESAMPLER_H
#define SND_RESAMPLER_H

#include "soxr/soxr.h"
#include <cstring>


#ifndef RESAMPLER_EMPTY_BUFFER_SIZE
#define RESAMPLER_EMPTY_BUFFER_SIZE 1000
#endif

class piecewiseResample {

	int64_t oDoneTotal = 0;
	soxr_t soxrRef;
	soxr_error_t error;
	const soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_HQ, SOXR_VR); // Apparentrly VR is only available in HQ
	//const soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_VHQ | SOXR_STEEP_FILTER, SOXR_VR);
	const soxr_io_spec_t io_spec = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);

	//inline static short emptyBuffer[RESAMPLER_EMPTY_BUFFER_SIZE];

	bool isFinished = false;

public:
	bool IsFinished() { return isFinished; }
	piecewiseResample(int channelCount = 1) {

		soxrRef = soxr_create(1, 1, channelCount, &error, &io_spec, &q_spec, NULL);
	}
	~piecewiseResample() {
		soxr_delete(soxrRef);
	}
	// Just zero out the emptyBuffer
	/*static const bool init() { // no longer needed because we can simply flush instead, which also is better to give us an idea when we're done.
		memset(emptyBuffer, 0, sizeof(emptyBuffer));
		return true;
	}*/
	inline size_t getSamples(double speed, short* outputBuffer, size_t outSamples, short* inputBuffer, size_t inputBufferLength, size_t inputBufferOffset = 0, bool loop = false);
};


//static bool blahblah4235327634 = piecewiseResample::init();

#ifdef RELDEBUG
//#pragma optimize("", off)
#endif
// Returns input sample count used.
size_t piecewiseResample::getSamples(double speed, short* outputBuffer, size_t outSamples, short* inputBuffer, size_t inputBufferLength, size_t inputBufferOffset, bool loop ) {
	soxr_set_io_ratio(soxrRef, speed, outSamples);
	size_t inputDone = 0;
	size_t odone=0,idone=0;
	bool need_input = 1;
	bool is_flushing = false;
	do {
		int64_t len = (int64_t)inputBufferLength-(int64_t)inputBufferOffset; // Need to write into int64_t instead of size_t because value might be negative
		if (len <= 0) {	// Must check if <0 because in some cases that can apparently happen. Just a ! isn't enough because only 0 evaluates to 0
			// If sound is looping just continue from start again.
			if (loop && inputBufferLength > 0) {
				//inputBufferOffset = 0;
				//len = inputBufferLength - inputBufferOffset;
				while (inputBufferOffset >= inputBufferLength) {
					inputBufferOffset -= inputBufferLength;
				}
				len = inputBufferLength - inputBufferOffset;
			}
			/*// Fill with silence until we got enough output.
			else {
				inputBuffer = (short*)&emptyBuffer;
				inputBufferLength = len = sizeof(emptyBuffer);
				inputBufferOffset = 0;
			}*/
			else {
				// flush
				len = 0;
				inputBuffer = nullptr;
				inputBufferOffset = 0;
				is_flushing = true;
			}
		}

		soxr_t arg1 = soxrRef;
		soxr_in_t arg2 = inputBuffer + inputBufferOffset;
		size_t arg3 = (size_t)len;
		size_t* arg4 = &idone;
		soxr_out_t arg5 = outputBuffer;
		size_t arg6 = outSamples;
		size_t* arg7 = &odone;
		error = soxr_process(soxrRef, inputBuffer + inputBufferOffset, (size_t)len, &idone, outputBuffer, outSamples, &odone);

		outSamples -= odone;
		outputBuffer += odone;
		oDoneTotal += odone;
		inputDone += idone;
		inputBufferOffset += idone;

		/* If soxr_process did not provide the complete block, we must call it
		 * again, supplying more input samples: */
		need_input = outSamples != 0;

	} while ((need_input && !is_flushing) && !error);

	if (is_flushing && need_input) {
		isFinished = true;
	}

	return inputDone; // let calling code know by how much to advance input buffer index
}

#ifdef RELDEBUG
//#pragma optimize("", on)
#endif 


#endif