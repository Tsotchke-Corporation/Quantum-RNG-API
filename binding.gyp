{
  "targets": [
    {
      "target_name": "quantum_rng",
      "sources": [
        "src/binding.cc",
        "src/qrng_v3_bridge.c",
        "vendor/quantum_rng/src/quantum_rng/quantum_rng_v3.c",
        "vendor/quantum_rng/src/quantum_rng/quantum_state.c",
        "vendor/quantum_rng/src/quantum_rng/quantum_gates.c",
        "vendor/quantum_rng/src/quantum_rng/bell_test.c",
        "vendor/quantum_rng/src/quantum_rng/grover.c",
        "vendor/quantum_rng/src/quantum_rng/matrix_math.c",
        "vendor/quantum_rng/src/quantum_rng/simd_ops.c",
        "vendor/quantum_rng/src/quantum_rng/accelerate_ops.c",
        "vendor/quantum_rng/src/entropy/entropy_pool.c",
        "vendor/quantum_rng/src/entropy/hardware_entropy.c",
        "vendor/quantum_rng/src/health/health_tests.c",
        "vendor/quantum_rng/src/profiling/performance_monitor.c"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "vendor/quantum_rng/src/quantum_rng",
        "vendor/quantum_rng/src/entropy",
        "vendor/quantum_rng/src/health",
        "vendor/quantum_rng/src/profiling",
        "vendor/quantum_rng/src/common"
      ],
      "defines": [
        "NAPI_VERSION=8",
        "NAPI_CPP_EXCEPTIONS",
        "_GNU_SOURCE"
      ],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "cflags": [
        "-O3",
        "-fPIC",
        "-Wall",
        "-Wextra"
      ],
      "cflags_cc": [
        "-O3",
        "-fPIC",
        "-Wall",
        "-Wextra",
        "-std=c++17"
      ],
      "libraries": ["-lm", "-lpthread"],
      "conditions": [
        ["OS=='mac'", {
          "libraries": ["-framework Accelerate"],
          "xcode_settings": {
            "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES"
          }
        }],
        ["OS=='linux' and target_arch=='x64'", {
          "cflags": ["-msse3"]
        }]
      ]
    }
  ]
}
