# Vendored quantum_rng provenance

The native engine in this directory is vendored from
[`tsotchke/quantum_rng`](https://github.com/tsotchke/quantum_rng).

| Field | Value |
| --- | --- |
| Release line | `v3.0.1` |
| Pinned revision | `1a77e77f803c63883349b658361c06401cd8ceb7` |
| Pinned date | 2026-07-11 |
| License | MIT; see `LICENSE` in this directory |

The pinned revision is one commit newer than the `v3.0.1` tag. It contains the
full-width output-conditioning fix that prevents the 8-qubit basis index from
being serialized as if it were an unconditioned 64-bit value. This API reports
the engine provenance as `v3.0.1+1a77e77`.

The vendored source files are copied without modification. API-specific code
lives in `src/binding.cc` and `server.js`.

## Updating

1. Review the new upstream tag and every commit since the pinned revision.
2. Replace the vendored source directories and `LICENSE` from the selected
   upstream commit.
3. Update the revision constants in `src/binding.cc` and this document.
4. Run `npm ci --ignore-scripts && npm test` on macOS and Linux.
5. Verify `/v1/health` reports the expected release and revision.
