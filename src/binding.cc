#include <napi.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include "qrng_v3_bridge.h"

namespace {

constexpr const char *kUpstreamRelease = "v3.0.1+1a77e77";
constexpr const char *kUpstreamRevision =
    "1a77e77f803c63883349b658361c06401cd8ceb7";
constexpr uint32_t kMaxNativeRequestBytes = 1024 * 1024;

bool CheckError(Napi::Env env, int error) {
    if (error == 0) return true;
    Napi::Error::New(env, qrng_api_error_string(error))
        .ThrowAsJavaScriptException();
    return false;
}

bool ParseInt32(Napi::Env env, const Napi::Value &input,
                const char *name, int32_t *output) {
    if (!input.IsNumber()) {
        Napi::TypeError::New(env, std::string(name) + " must be a number")
            .ThrowAsJavaScriptException();
        return false;
    }

    const double value = input.As<Napi::Number>().DoubleValue();
    if (!std::isfinite(value) || std::trunc(value) != value ||
        value < std::numeric_limits<int32_t>::min() ||
        value > std::numeric_limits<int32_t>::max()) {
        Napi::RangeError::New(
            env, std::string(name) + " must be a signed 32-bit integer")
            .ThrowAsJavaScriptException();
        return false;
    }

    *output = static_cast<int32_t>(value);
    return true;
}

qrng_api_mode_t ParseMode(Napi::Env env, const Napi::Value &input,
                          bool *valid) {
    *valid = false;
    if (!input.IsString()) {
        Napi::TypeError::New(env, "mode must be a string")
            .ThrowAsJavaScriptException();
        return QRNG_API_MODE_DIRECT;
    }

    const std::string mode = input.As<Napi::String>().Utf8Value();
    *valid = true;
    if (mode == "direct") return QRNG_API_MODE_DIRECT;
    if (mode == "grover") return QRNG_API_MODE_GROVER;
    if (mode == "bell-verified") return QRNG_API_MODE_BELL_VERIFIED;

    *valid = false;
    Napi::RangeError::New(
        env, "mode must be direct, grover, or bell-verified")
        .ThrowAsJavaScriptException();
    return QRNG_API_MODE_DIRECT;
}

}  // namespace

class QuantumRNG : public Napi::ObjectWrap<QuantumRNG> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    explicit QuantumRNG(const Napi::CallbackInfo &info);
    ~QuantumRNG() override;

private:
    static Napi::FunctionReference constructor;
    qrng_api_ctx *ctx_ = nullptr;

    Napi::Value GetBytes(const Napi::CallbackInfo &info);
    Napi::Value GetUInt64(const Napi::CallbackInfo &info);
    Napi::Value GetDouble(const Napi::CallbackInfo &info);
    Napi::Value GetRange32(const Napi::CallbackInfo &info);
    Napi::Value GetRange64(const Napi::CallbackInfo &info);
    Napi::Value GetStats(const Napi::CallbackInfo &info);
    Napi::Value VerifyQuantum(const Napi::CallbackInfo &info);
    Napi::Value GetEntanglementEntropy(const Napi::CallbackInfo &info);
    Napi::Value GetMode(const Napi::CallbackInfo &info);

    static Napi::Value GetVersion(const Napi::CallbackInfo &info);
    static Napi::Value GetEngineVersion(const Napi::CallbackInfo &info);
    static Napi::Value GetRevision(const Napi::CallbackInfo &info);
};

Napi::FunctionReference QuantumRNG::constructor;

Napi::Object QuantumRNG::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function function = DefineClass(env, "QuantumRNG", {
        InstanceMethod("getBytes", &QuantumRNG::GetBytes),
        InstanceMethod("getUInt64", &QuantumRNG::GetUInt64),
        InstanceMethod("getDouble", &QuantumRNG::GetDouble),
        InstanceMethod("getRange32", &QuantumRNG::GetRange32),
        InstanceMethod("getRange64", &QuantumRNG::GetRange64),
        InstanceMethod("getStats", &QuantumRNG::GetStats),
        InstanceMethod("verifyQuantum", &QuantumRNG::VerifyQuantum),
        InstanceMethod("getEntanglementEntropy", &QuantumRNG::GetEntanglementEntropy),
        InstanceMethod("getMode", &QuantumRNG::GetMode),
        StaticMethod("getVersion", &QuantumRNG::GetVersion),
        StaticMethod("getEngineVersion", &QuantumRNG::GetEngineVersion),
        StaticMethod("getRevision", &QuantumRNG::GetRevision),
    });

    constructor = Napi::Persistent(function);
    constructor.SuppressDestruct();
    exports.Set("QuantumRNG", function);
    return exports;
}

QuantumRNG::QuantumRNG(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<QuantumRNG>(info) {
    Napi::Env env = info.Env();
    qrng_api_mode_t mode = QRNG_API_MODE_DIRECT;
    int bell_monitoring = 0;

    if (info.Length() > 0) {
        if (!info[0].IsObject()) {
            Napi::TypeError::New(env, "options must be an object")
                .ThrowAsJavaScriptException();
            return;
        }

        const Napi::Object options = info[0].As<Napi::Object>();
        if (options.Has("mode")) {
            bool valid = false;
            mode = ParseMode(env, options.Get("mode"), &valid);
            if (!valid) return;
        }
        if (options.Has("bellMonitoring")) {
            const Napi::Value value = options.Get("bellMonitoring");
            if (!value.IsBoolean()) {
                Napi::TypeError::New(env, "bellMonitoring must be a boolean")
                    .ThrowAsJavaScriptException();
                return;
            }
            bell_monitoring = value.As<Napi::Boolean>().Value() ? 1 : 0;
        }
    }

    const int error = qrng_api_init(&ctx_, mode, bell_monitoring);
    if (error != 0) {
        ctx_ = nullptr;
        Napi::Error::New(env, qrng_api_error_string(error))
            .ThrowAsJavaScriptException();
    }
}

QuantumRNG::~QuantumRNG() {
    qrng_api_free(ctx_);
    ctx_ = nullptr;
}

Napi::Value QuantumRNG::GetBytes(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() != 1 || !info[0].IsNumber()) {
        Napi::TypeError::New(env, "length must be a number")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    const double raw_length = info[0].As<Napi::Number>().DoubleValue();
    if (!std::isfinite(raw_length) || std::trunc(raw_length) != raw_length ||
        raw_length < 1 || raw_length > kMaxNativeRequestBytes) {
        Napi::RangeError::New(
            env, "length must be an integer between 1 and 1048576")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    const size_t length = static_cast<size_t>(raw_length);
    Napi::Buffer<uint8_t> buffer = Napi::Buffer<uint8_t>::New(env, length);
    if (!CheckError(env, qrng_api_bytes(ctx_, buffer.Data(), length))) {
        return env.Null();
    }
    return buffer;
}

Napi::Value QuantumRNG::GetUInt64(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    uint64_t value = 0;
    if (!CheckError(env, qrng_api_uint64(ctx_, &value))) return env.Null();
    return Napi::BigInt::New(env, value);
}

Napi::Value QuantumRNG::GetDouble(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    double value = 0.0;
    if (!CheckError(env, qrng_api_double(ctx_, &value))) return env.Null();
    return Napi::Number::New(env, value);
}

Napi::Value QuantumRNG::GetRange32(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() != 2) {
        Napi::TypeError::New(env, "min and max are required")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    int32_t min = 0;
    int32_t max = 0;
    if (!ParseInt32(env, info[0], "min", &min) ||
        !ParseInt32(env, info[1], "max", &max)) {
        return env.Null();
    }
    if (min > max) {
        Napi::RangeError::New(env, "min must be less than or equal to max")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    int32_t value = 0;
    if (!CheckError(env, qrng_api_range32(ctx_, min, max, &value))) {
        return env.Null();
    }
    return Napi::Number::New(env, value);
}

Napi::Value QuantumRNG::GetRange64(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() != 2 || !info[0].IsBigInt() || !info[1].IsBigInt()) {
        Napi::TypeError::New(env, "min and max must be unsigned BigInt values")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    bool min_lossless = false;
    bool max_lossless = false;
    const uint64_t min = info[0].As<Napi::BigInt>().Uint64Value(&min_lossless);
    const uint64_t max = info[1].As<Napi::BigInt>().Uint64Value(&max_lossless);
    if (!min_lossless || !max_lossless || min > max) {
        Napi::RangeError::New(
            env, "min and max must satisfy 0 <= min <= max <= 2^64-1")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    uint64_t value = 0;
    if (!CheckError(env, qrng_api_range64(ctx_, min, max, &value))) {
        return env.Null();
    }
    return Napi::BigInt::New(env, value);
}

Napi::Value QuantumRNG::GetStats(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    qrng_api_stats_t stats{};
    if (!CheckError(env, qrng_api_get_stats(ctx_, &stats))) return env.Null();

    Napi::Object result = Napi::Object::New(env);
    result.Set("bytesGenerated", Napi::BigInt::New(env, stats.bytes_generated));
    result.Set("quantumMeasurements", Napi::BigInt::New(env, stats.quantum_measurements));
    result.Set("groverSearches", Napi::BigInt::New(env, stats.grover_searches));
    result.Set("bellTestsPerformed", Napi::BigInt::New(env, stats.bell_tests_performed));
    result.Set("bellTestsPassed", Napi::BigInt::New(env, stats.bell_tests_passed));
    result.Set("averageChsh", Napi::Number::New(env, stats.average_chsh));
    result.Set("minChsh", Napi::Number::New(env, stats.min_chsh));
    result.Set("maxChsh", Napi::Number::New(env, stats.max_chsh));
    result.Set("hardwareEntropyConsumed",
               Napi::BigInt::New(env, stats.hardware_entropy_consumed));
    result.Set("entanglementEntropy",
               Napi::Number::New(env, stats.entanglement_entropy));
    result.Set("throughputMbps", Napi::Number::New(env, stats.throughput_mbps));
    result.Set("mode", Napi::String::New(env, qrng_api_mode(ctx_)));
    return result;
}

Napi::Value QuantumRNG::VerifyQuantum(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    size_t measurements = 4000;
    if (info.Length() > 0) {
        if (!info[0].IsNumber()) {
            Napi::TypeError::New(env, "measurements must be a number")
                .ThrowAsJavaScriptException();
            return env.Null();
        }
        const double value = info[0].As<Napi::Number>().DoubleValue();
        if (!std::isfinite(value) || std::trunc(value) != value ||
            value < 1000 || value > 100000) {
            Napi::RangeError::New(
                env, "measurements must be an integer between 1000 and 100000")
                .ThrowAsJavaScriptException();
            return env.Null();
        }
        measurements = static_cast<size_t>(value);
    }

    qrng_api_verification_t result{};
    if (!CheckError(env, qrng_api_verify(ctx_, measurements, &result))) {
        return env.Null();
    }

    Napi::Object output = Napi::Object::New(env);
    output.Set("chsh", Napi::Number::New(env, result.chsh));
    output.Set("classicalBound", Napi::Number::New(env, result.classical_bound));
    output.Set("quantumBound", Napi::Number::New(env, result.quantum_bound));
    output.Set("pValue", Napi::Number::New(env, result.p_value));
    output.Set("standardError", Napi::Number::New(env, result.standard_error));
    output.Set("measurements", Napi::Number::New(env, result.measurements));
    output.Set("violatesClassical", Napi::Boolean::New(env, result.violates_classical != 0));
    output.Set("confirmsQuantumModel",
               Napi::Boolean::New(env, result.confirms_quantum_model != 0));
    output.Set("statisticallySignificant",
               Napi::Boolean::New(env, result.statistically_significant != 0));
    return output;
}

Napi::Value QuantumRNG::GetEntanglementEntropy(const Napi::CallbackInfo &info) {
    return Napi::Number::New(info.Env(), qrng_api_entanglement_entropy(ctx_));
}

Napi::Value QuantumRNG::GetMode(const Napi::CallbackInfo &info) {
    return Napi::String::New(info.Env(), qrng_api_mode(ctx_));
}

Napi::Value QuantumRNG::GetVersion(const Napi::CallbackInfo &info) {
    return Napi::String::New(info.Env(), kUpstreamRelease);
}

Napi::Value QuantumRNG::GetEngineVersion(const Napi::CallbackInfo &info) {
    return Napi::String::New(info.Env(), qrng_api_engine_version());
}

Napi::Value QuantumRNG::GetRevision(const Napi::CallbackInfo &info) {
    return Napi::String::New(info.Env(), kUpstreamRevision);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    return QuantumRNG::Init(env, exports);
}

NODE_API_MODULE(quantum_rng, Init)
