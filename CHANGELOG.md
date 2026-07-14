# Changelog

## 2.0.0 — 2026-07-14

- Replaced the embedded legacy v1.1 generator with the vendored
  `quantum_rng` v3 state-vector engine.
- Pinned upstream revision `1a77e77`, including the post-v3.0.1 full-width
  output-conditioning fix.
- Added startup CHSH verification with explicit simulation-only scope.
- Added engine provenance, statistics, verification, and unsigned 64-bit
  range endpoints.
- Fixed signed full-span range overflow, unsigned full-span constant output,
  and `probability=0` handling.
- Added native and HTTP regression tests plus Linux/macOS CI.
- Rewrote public documentation to remove physical-hardware, FIPS, and
  deployment-security claims not established by the repository.
