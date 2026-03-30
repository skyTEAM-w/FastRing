/**
 * @file adc_simulator.h
 * @brief ADC simulator for testing and development
 *
 * This module provides a simulated ADC device for testing
 * and development purposes. It generates realistic ADC sample
 * data with configurable signal frequency and noise levels.
 *
 * @author WuChengpei_Sky
 * @version 1.0.0
 * @date 2026-03-22
 */

#ifndef ADC_SIMULATOR_H
#define ADC_SIMULATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque ADC simulator handle
 */
typedef struct adc_simulator adc_simulator_t;

/**
 * @brief ADC simulator configuration structure
 */
typedef struct {
    uint32_t sample_rate;       /**< Sampling rate in Hz (e.g., 40000 for 40kHz) */
    uint32_t resolution;        /**< ADC resolution in bits (e.g., 32) */
    uint32_t channel_count;     /**< Number of channels */
    double   signal_freq;       /**< Simulated signal frequency in Hz */
    double   noise_level;       /**< Noise level (0.0 to 1.0, where 1.0 is full scale) */
    bool     enable_trigger;    /**< Enable external trigger mode */
} adc_config_t;

/**
 * @brief ADC sample callback function type
 * @param sample Pointer to the acquired sample
 * @param user_data User-provided data pointer
 */
typedef void (*adc_callback_t)(const adc_sample_t *sample, void *user_data);

/**
 * @brief Create a new ADC simulator instance
 * @param config Pointer to the configuration structure
 * @return Pointer to newly created ADC simulator, or NULL on failure
 * @note Caller is responsible for calling adc_simulator_destroy() to free resources
 */
adc_simulator_t* adc_simulator_create(const adc_config_t *config);

/**
 * @brief Destroy an ADC simulator and free all resources
 * @param adc Pointer to the ADC simulator to destroy
 */
void adc_simulator_destroy(adc_simulator_t *adc);

/**
 * @brief Start the ADC simulator
 * @param adc Pointer to the ADC simulator
 * @return ADC_OK on success, error code on failure
 */
adc_error_t adc_simulator_start(adc_simulator_t *adc);

/**
 * @brief Stop the ADC simulator
 * @param adc Pointer to the ADC simulator
 * @return ADC_OK on success, error code on failure
 */
adc_error_t adc_simulator_stop(adc_simulator_t *adc);

/**
 * @brief Check if the ADC simulator is running
 * @param adc Pointer to the ADC simulator
 * @return true if running, false otherwise
 */
bool adc_simulator_is_running(const adc_simulator_t *adc);

/**
 * @brief Set the sample callback function
 * @param adc Pointer to the ADC simulator
 * @param callback Callback function pointer
 * @param user_data User data to pass to the callback
 * @return ADC_OK on success, error code on failure
 */
adc_error_t adc_simulator_set_callback(adc_simulator_t *adc, adc_callback_t callback, void *user_data);

/**
 * @brief Get a single sample from the ADC simulator
 * @param adc Pointer to the ADC simulator
 * @param sample Pointer to store the sample data
 * @return ADC_OK on success, error code on failure
 */
adc_error_t adc_simulator_get_sample(adc_simulator_t *adc, adc_sample_t *sample);

/**
 * @brief Get the default ADC simulator configuration
 * @param config Pointer to the configuration structure to fill with defaults
 */
void adc_config_get_default(adc_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* ADC_SIMULATOR_H */
