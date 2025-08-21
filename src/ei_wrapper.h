#pragma once

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "edge-impulse-sdk/dsp/numpy_types.h"

__attribute__((aligned(16))) 
static uint8_t tensor_arena[EI_CLASSIFIER_TFLITE_LARGEST_ARENA_SIZE] 
__attribute__((section(".ext_ram.bss")));

bool run_classifier_on_96x96_grayscale(uint8_t *input, ei_impulse_result_t *result_out) {
    const size_t image_size = 96 * 96;

    ei::signal_t signal;
    signal.total_length = image_size;
    signal.get_data = [input](size_t offset, size_t length, float *out) -> int {
        if ((offset + length) > image_size) return -1;
        for (size_t i = 0; i < length; i++) {
            out[i] = static_cast<float>(input[offset + i]) / 255.0f;
        }
        return 0;
    };

    EI_IMPULSE_ERROR res = run_classifier(&signal, result_out, false);
    return res == EI_IMPULSE_OK;
}
