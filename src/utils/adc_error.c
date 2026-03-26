/**
 * @file adc_error.c
 * @brief ADC error handling implementation
 */

#include "adc_types.h"

const char* adc_error_string(adc_error_t error) {
    switch (error) {
        case ADC_OK:
            return "Success";
        case ADC_ERROR_INIT:
            return "Initialization error";
        case ADC_ERROR_BUFFER:
            return "Buffer operation error";
        case ADC_ERROR_NETWORK:
            return "Network operation error";
        case ADC_ERROR_THREAD:
            return "Thread operation error";
        case ADC_ERROR_TIMEOUT:
            return "Operation timeout";
        case ADC_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        default:
            return "Unknown error";
    }
}
