'use strict';

const assert = require('node:assert/strict');
const { before, test } = require('node:test');

const { QuantumRNG } = require('../build/Release/quantum_rng.node');

let rng;

before(() => {
    rng = new QuantumRNG({ mode: 'direct', bellMonitoring: false });
});

test('reports exact upstream provenance', () => {
    assert.equal(QuantumRNG.getVersion(), 'v3.0.1+1a77e77');
    assert.equal(
        QuantumRNG.getRevision(),
        '1a77e77f803c63883349b658361c06401cd8ceb7'
    );
    assert.equal(rng.getMode(), 'direct');
});

test('generates full-width byte and uint64 output', () => {
    const bytes = rng.getBytes(256);
    assert.equal(bytes.length, 256);
    assert.notEqual(bytes.equals(Buffer.alloc(bytes.length)), true);

    const values = Array.from({ length: 256 }, () => rng.getUInt64());
    assert.ok(new Set(values.map(String)).size > 240);
    assert.ok(values.some((value) => (value >> 32n) !== 0n));
    assert.ok(values.every((value) => value >= 0n && value <= 0xffffffffffffffffn));
});

test('generates doubles in the half-open unit interval', () => {
    for (let i = 0; i < 256; i += 1) {
        const value = rng.getDouble();
        assert.ok(value >= 0 && value < 1);
    }
});

test('handles the full signed 32-bit span without overflow or SIGFPE', () => {
    const values = Array.from({ length: 256 }, () =>
        rng.getRange32(-2147483648, 2147483647)
    );
    assert.ok(values.every((value) => Number.isInteger(value)));
    assert.ok(values.every((value) => value >= -2147483648 && value <= 2147483647));
    assert.ok(values.some((value) => value < 0));
    assert.ok(values.some((value) => value >= 0));
});

test('handles the full unsigned 64-bit span without a constant result', () => {
    const values = Array.from({ length: 128 }, () =>
        rng.getRange64(0n, 0xffffffffffffffffn)
    );
    assert.ok(new Set(values.map(String)).size > 120);
    assert.ok(values.some((value) => value !== 0xffffffffffffffffn));
});

test('Bell verification validates the simulated quantum-state model', () => {
    const before = rng.getStats();
    const result = rng.verifyQuantum(4000);
    const after = rng.getStats();
    assert.equal(result.measurements, 4000);
    assert.equal(result.violatesClassical, true);
    assert.ok(result.chsh > result.classicalBound);
    assert.ok(result.chsh <= result.quantumBound + 0.2);
    assert.equal(after.bellTestsPerformed, before.bellTestsPerformed + 1n);
    assert.equal(after.bellTestsPassed, before.bellTestsPassed + 1n);
});

test('exposes precise generation counters', () => {
    const stats = rng.getStats();
    assert.equal(typeof stats.bytesGenerated, 'bigint');
    assert.equal(typeof stats.quantumMeasurements, 'bigint');
    assert.ok(stats.bytesGenerated > 0n);
    assert.ok(stats.quantumMeasurements > 0n);
});
