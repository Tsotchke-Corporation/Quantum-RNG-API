#include "qrng_v3_bridge.h"

#include "quantum_rng_v3.h"

#include <stdlib.h>

struct qrng_api_ctx {
    qrng_v3_ctx_t *engine;
    uint64_t manual_bell_tests_passed;
};

int qrng_api_init(qrng_api_ctx **ctx_out, qrng_api_mode_t mode,
                  int bell_monitoring) {
    if (!ctx_out || mode < QRNG_API_MODE_DIRECT ||
        mode > QRNG_API_MODE_BELL_VERIFIED) {
        return QRNG_V3_ERROR_INVALID_PARAM;
    }

    *ctx_out = NULL;
    qrng_api_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return QRNG_V3_ERROR_OUT_OF_MEMORY;

    qrng_v3_config_t config;
    qrng_v3_get_default_config(&config);
    config.mode = (qrng_v3_mode_t)mode;
    config.enable_bell_monitoring = bell_monitoring ? 1 : 0;

    qrng_v3_error_t error = qrng_v3_init_with_config(&ctx->engine, &config);
    if (error != QRNG_V3_SUCCESS) {
        free(ctx);
        return error;
    }

    *ctx_out = ctx;
    return QRNG_V3_SUCCESS;
}

void qrng_api_free(qrng_api_ctx *ctx) {
    if (!ctx) return;
    qrng_v3_free(ctx->engine);
    ctx->engine = NULL;
    free(ctx);
}

int qrng_api_bytes(qrng_api_ctx *ctx, uint8_t *buffer, size_t size) {
    if (!ctx) return QRNG_V3_ERROR_NULL_CONTEXT;
    return qrng_v3_bytes(ctx->engine, buffer, size);
}

int qrng_api_uint64(qrng_api_ctx *ctx, uint64_t *value) {
    if (!ctx) return QRNG_V3_ERROR_NULL_CONTEXT;
    return qrng_v3_uint64(ctx->engine, value);
}

int qrng_api_double(qrng_api_ctx *ctx, double *value) {
    if (!ctx) return QRNG_V3_ERROR_NULL_CONTEXT;
    return qrng_v3_double(ctx->engine, value);
}

int qrng_api_range32(qrng_api_ctx *ctx, int32_t min, int32_t max,
                     int32_t *value) {
    if (!ctx) return QRNG_V3_ERROR_NULL_CONTEXT;
    if (!value) return QRNG_V3_ERROR_NULL_BUFFER;
    if (min > max) return QRNG_V3_ERROR_INVALID_PARAM;

    const uint64_t span =
        (uint64_t)((int64_t)max - (int64_t)min) + UINT64_C(1);
    const uint64_t threshold = (UINT64_C(0) - span) % span;
    uint64_t random = 0;
    int error;
    do {
        error = qrng_v3_uint64(ctx->engine, &random);
        if (error != QRNG_V3_SUCCESS) return error;
    } while (random < threshold);

    *value = (int32_t)((int64_t)min + (int64_t)(random % span));
    return QRNG_V3_SUCCESS;
}

int qrng_api_range64(qrng_api_ctx *ctx, uint64_t min, uint64_t max,
                     uint64_t *value) {
    if (!ctx) return QRNG_V3_ERROR_NULL_CONTEXT;
    return qrng_v3_range(ctx->engine, min, max, value);
}

int qrng_api_get_stats(qrng_api_ctx *ctx, qrng_api_stats_t *stats_out) {
    if (!ctx) return QRNG_V3_ERROR_NULL_CONTEXT;
    if (!stats_out) return QRNG_V3_ERROR_NULL_BUFFER;

    qrng_v3_stats_t stats;
    const int error = qrng_v3_get_stats(ctx->engine, &stats);
    if (error != QRNG_V3_SUCCESS) return error;

    stats_out->bytes_generated = stats.bytes_generated;
    stats_out->quantum_measurements = stats.quantum_measurements;
    stats_out->grover_searches = stats.grover_searches;
    stats_out->bell_tests_performed = stats.bell_tests_performed;
    /*
     * The pinned engine increments bell_tests_performed for explicit
     * qrng_v3_verify_quantum() calls but only increments bell_tests_passed in
     * its automatic monitoring path. Account for successful explicit checks
     * here so the API's public counters describe what actually happened.
     */
    stats_out->bell_tests_passed =
        stats.bell_tests_passed + ctx->manual_bell_tests_passed;
    stats_out->average_chsh = stats.average_chsh;
    stats_out->min_chsh = stats.min_chsh;
    stats_out->max_chsh = stats.max_chsh;
    stats_out->hardware_entropy_consumed = stats.hardware_entropy_consumed;
    stats_out->entanglement_entropy =
        qrng_v3_get_entanglement_entropy(ctx->engine);
    stats_out->throughput_mbps = stats.throughput_mbps;
    return QRNG_V3_SUCCESS;
}

int qrng_api_verify(qrng_api_ctx *ctx, size_t measurements,
                    qrng_api_verification_t *verification) {
    if (!ctx) return QRNG_V3_ERROR_NULL_CONTEXT;
    if (!verification) return QRNG_V3_ERROR_NULL_BUFFER;

    const bell_test_result_t result =
        qrng_v3_verify_quantum(ctx->engine, measurements);
    verification->chsh = result.chsh_value;
    verification->classical_bound = result.classical_bound;
    verification->quantum_bound = result.quantum_bound;
    verification->p_value = result.p_value;
    verification->standard_error = result.standard_error;
    verification->measurements = result.measurements;
    verification->violates_classical = result.violates_classical;
    verification->confirms_quantum_model = result.confirms_quantum;
    verification->statistically_significant = result.statistically_significant;
    if (result.confirms_quantum) {
        ctx->manual_bell_tests_passed++;
    }
    return QRNG_V3_SUCCESS;
}

double qrng_api_entanglement_entropy(qrng_api_ctx *ctx) {
    if (!ctx) return 0.0;
    return qrng_v3_get_entanglement_entropy(ctx->engine);
}

const char *qrng_api_mode(qrng_api_ctx *ctx) {
    if (!ctx) return "UNKNOWN";
    switch (qrng_v3_get_mode(ctx->engine)) {
        case QRNG_V3_MODE_DIRECT:
            return "direct";
        case QRNG_V3_MODE_GROVER:
            return "grover";
        case QRNG_V3_MODE_BELL_VERIFIED:
            return "bell-verified";
        default:
            return "unknown";
    }
}

const char *qrng_api_engine_version(void) {
    return qrng_v3_version();
}

const char *qrng_api_error_string(int error) {
    return qrng_v3_error_string((qrng_v3_error_t)error);
}
