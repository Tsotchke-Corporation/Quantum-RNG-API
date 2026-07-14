# Tsotchke Quantum RNG API

[![CI](https://github.com/Tsotchke-Corporation/Quantum-RNG-API/actions/workflows/ci.yml/badge.svg)](https://github.com/Tsotchke-Corporation/Quantum-RNG-API/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-111111.svg)](LICENSE)
[![Engine](https://img.shields.io/badge/engine-quantum__rng_v3-6f42c1.svg)](https://github.com/tsotchke/quantum_rng)

A production-oriented HTTP interface to the
[`quantum_rng`](https://github.com/tsotchke/quantum_rng) v3 state-vector
simulation engine. The service exposes random bytes, numbers, bounded draws,
health information, engine statistics, and the startup CHSH verification
result through a versioned REST API.

The native engine is pinned to `v3.0.1+1a77e77`: the v3.0.1 release line plus
the critical full-width output-conditioning fix now present on upstream
`master`.

## Scope of the claim

This project does **not** claim access to physical quantum hardware.

The engine performs real state-vector evolution, unitary gates, Born-rule
measurement, and wavefunction collapse in software. Measurement and final
output are driven and conditioned by operating-system and CPU entropy sources
that pass continuous health tests. The built-in CHSH test checks that the
simulated quantum model produces the expected non-classical correlations; it
is not device-independent certification of a physical entropy source.

The repository is not FIPS validated and the public server should not be
treated as a drop-in key-generation service without deployment-specific TLS,
authentication, authorization, rate limiting, monitoring, and independent
security review.

## What changed in API 2.0

- The separate legacy v1.1 generator has been removed.
- The Node-API addon now wraps the actual QRNG v3 engine.
- Upstream source and revision provenance are explicit and reproducible.
- Startup runs a CHSH verification of the simulation model by default.
- Full signed 32-bit and unsigned 64-bit ranges use unbiased, overflow-safe
  sampling.
- `probability=0` and `probability=1` have exact boolean semantics.
- Health responses expose the engine mode, revision, verification scope, and
  generation counters.
- Native and HTTP regression tests run on Linux and macOS in CI.

## Architecture

```text
HTTP request
    │
    ▼
Express API
    │
    ▼
Node-API C++ binding
    │
    ▼
quantum_rng v3
    ├── health-tested OS / CPU entropy pool
    ├── state-vector evolution and Born-rule measurement
    ├── direct, Grover, and Bell-verified modes
    └── conditioned full-width output
```

The upstream C source is vendored under `vendor/quantum_rng` so builds do not
depend on mutable network state. See
[UPSTREAM.md](vendor/quantum_rng/UPSTREAM.md) for the pinned revision and update
procedure.

## API

The default local base URL is `http://localhost:3000`. The documented
production URL is `https://api.tsotchke.net`.

| Method | Endpoint | Result |
| --- | --- | --- |
| `GET` | `/` | API and engine identity |
| `GET` | `/v1/health` | Operational status, provenance, verification, and counters |
| `GET` | `/v1/openapi.json` | OpenAPI 3.0 document |
| `GET` | `/v1/docs` | Swagger UI |
| `GET` | `/v1/qrng/bytes/:count` | 1–4096 random bytes in hex, Base64, and array form |
| `GET` | `/v1/qrng/number?type=float\|uint64` | Unit-interval float or decimal uint64 string |
| `GET` | `/v1/qrng/range?min=…&max=…` | Inclusive signed 32-bit bounded draw |
| `GET` | `/v1/qrng/range64?min=…&max=…` | Inclusive unsigned 64-bit bounded draw |
| `GET` | `/v1/qrng/boolean?probability=0.5` | Boolean with the requested true probability |
| `POST` | `/v1/qrng/choice` | Unbiased index and element from a JSON array |
| `GET` | `/v1/qrng/stats` | Native engine counters |
| `GET` | `/v1/qrng/verification` | Cached startup CHSH result and scope |

Unsigned 64-bit values are returned as decimal strings because JSON numbers
cannot represent the full range exactly.

### Examples

```sh
curl https://api.tsotchke.net/v1/qrng/bytes/32
curl 'https://api.tsotchke.net/v1/qrng/number?type=uint64'
curl 'https://api.tsotchke.net/v1/qrng/range?min=-100&max=100'
curl 'https://api.tsotchke.net/v1/qrng/range64?min=0&max=18446744073709551615'
curl 'https://api.tsotchke.net/v1/qrng/boolean?probability=0.25'
curl -X POST https://api.tsotchke.net/v1/qrng/choice \
  -H 'content-type: application/json' \
  -d '{"array":["alpha","beta","gamma"]}'
```

## Runtime configuration

| Variable | Default | Meaning |
| --- | --- | --- |
| `PORT` | `3000` | HTTP listen port |
| `HOST` | `0.0.0.0` | HTTP listen address |
| `QRNG_MODE` | `direct` | `direct`, `grover`, or `bell-verified` |
| `QRNG_VERIFY_ON_START` | `1` | Set to `0` to skip startup CHSH verification |
| `QRNG_VERIFY_MEASUREMENTS` | `4000` | CHSH sample count, from 1000 to 100000 |

`direct` is the default because it is the simplest and fastest state-vector
measurement path. Enabling `bell-verified` additionally activates periodic
verification in the upstream engine; it has materially higher latency.

## Build and test

Requirements:

- Node.js 20.17 or newer
- Python 3 for `node-gyp`
- A C11 and C++17 compiler
- `make`

```sh
git clone https://github.com/Tsotchke-Corporation/Quantum-RNG-API.git
cd Quantum-RNG-API
npm ci
npm test
npm start
```

The build is tested with Node.js 20 and 22 on Ubuntu and macOS. On Apple
platforms the native engine links against Accelerate; other platforms use its
portable SIMD/scalar paths.

## Deployment

`railway.toml` supplies the Railway build packages, start command, and
`/v1/health` probe. A production deployment must place an authenticated,
rate-limited TLS gateway in front of this process when the output is used by
untrusted clients or security-sensitive systems.

The service starts only after the native engine initializes. Unless disabled,
it also requires the startup CHSH simulation check to exceed the classical
bound.

## Compatibility

The original v1 endpoint paths remain available. Responses now include more
provenance and encoding fields, while `uint64` values remain decimal strings.
The old native-only `entangleStates`, `measureState`, seed, and manual reseed
methods were not part of the HTTP contract and have been removed with the
legacy engine.

## Verification

```sh
npm test
```

The suite checks:

- full-width v3 byte and uint64 output;
- CHSH violation by the simulated quantum-state model;
- unit-interval floating-point output;
- full-span signed and unsigned range safety;
- exact probability boundary behavior;
- API validation, encodings, provenance, and OpenAPI publication.

## License and citation

This API and the vendored `quantum_rng` source are available under the MIT
License. The vendored copy retains its upstream license and exact revision
record.

If the engine is used in research, cite the upstream project:

```bibtex
@software{quantum_rng,
  title  = {Quantum RNG},
  author = {tsotchke},
  year   = {2026},
  url    = {https://github.com/tsotchke/quantum_rng}
}
```
