#ifndef TSOTCHKE_QRNG_V3_BRIDGE_H
#define TSOTCHKE_QRNG_V3_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct qrng_api_ctx qrng_api_ctx;

typedef enum {
    QRNG_API_MODE_DIRECT = 0,
    QRNG_API_MODE_GROVER = 1,
    QRNG_API_MODE_BELL_VERIFIED = 2
} qrng_api_mode_t;

typedef struct {
    uint64_t bytes_generated;
    uint64_t quantum_measurements;
    uint64_t grover_searches;
    uint64_t bell_tests_performed;
    uint64_t bell_tests_passed;
    double average_chsh;
    double min_chsh;
    double max_chsh;
    uint64_t hardware_entropy_consumed;
    double entanglement_entropy;
    double throughput_mbps;
} qrng_api_stats_t;

typedef struct {
    double chsh;
    double classical_bound;
    double quantum_bound;
    double p_value;
    double standard_error;
    size_t measurements;
    int violates_classical;
    int confirms_quantum_model;
    int statistically_significant;
} qrng_api_verification_t;

int qrng_api_init(qrng_api_ctx **ctx, qrng_api_mode_t mode,
                  int bell_monitoring);
void qrng_api_free(qrng_api_ctx *ctx);

int qrng_api_bytes(qrng_api_ctx *ctx, uint8_t *buffer, size_t size);
int qrng_api_uint64(qrng_api_ctx *ctx, uint64_t *value);
int qrng_api_double(qrng_api_ctx *ctx, double *value);
int qrng_api_range32(qrng_api_ctx *ctx, int32_t min, int32_t max,
                     int32_t *value);
int qrng_api_range64(qrng_api_ctx *ctx, uint64_t min, uint64_t max,
                     uint64_t *value);

int qrng_api_get_stats(qrng_api_ctx *ctx, qrng_api_stats_t *stats);
int qrng_api_verify(qrng_api_ctx *ctx, size_t measurements,
                    qrng_api_verification_t *verification);
double qrng_api_entanglement_entropy(qrng_api_ctx *ctx);
const char *qrng_api_mode(qrng_api_ctx *ctx);

const char *qrng_api_engine_version(void);
const char *qrng_api_error_string(int error);

#ifdef __cplusplus
}
#endif

#endif
