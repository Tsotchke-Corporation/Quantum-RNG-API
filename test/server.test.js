'use strict';

const assert = require('node:assert/strict');
const { once } = require('node:events');
const { after, before, test } = require('node:test');

const { API_VERSION, app, startupVerification } = require('../server');

let server;
let baseUrl;

before(async () => {
    server = app.listen(0, '127.0.0.1');
    await once(server, 'listening');
    const address = server.address();
    baseUrl = `http://127.0.0.1:${address.port}`;
});

after(async () => {
    if (!server || !server.listening) return;
    await new Promise((resolve, reject) => {
        server.close((error) => (error ? reject(error) : resolve()));
    });
});

async function request(path, options) {
    const response = await fetch(`${baseUrl}${path}`, options);
    return { response, body: await response.json() };
}

test('root and health disclose API and engine provenance', async () => {
    const root = await request('/');
    assert.equal(root.response.status, 200);
    assert.equal(root.body.apiVersion, API_VERSION);
    assert.equal(root.body.engine.name, 'quantum_rng_v3');
    assert.match(root.body.engine.revision, /^[0-9a-f]{40}$/);

    const health = await request('/v1/health');
    assert.equal(health.response.status, 200);
    assert.equal(health.body.status, 'ok');
    assert.equal(health.body.verification.scope, 'quantum-state simulation model');
    assert.equal(health.body.verification.performed, startupVerification.performed);
    assert.equal(typeof health.body.stats.bytesGenerated, 'string');
});

test('returns bytes in stable encodings', async () => {
    const { response, body } = await request('/v1/qrng/bytes/32');
    assert.equal(response.status, 200);
    assert.equal(body.length, 32);
    assert.match(body.bytes, /^[0-9a-f]{64}$/);
    assert.equal(Buffer.from(body.base64, 'base64').length, 32);
    assert.equal(body.raw.length, 32);
});

test('rejects invalid byte counts', async () => {
    assert.equal((await request('/v1/qrng/bytes/0')).response.status, 400);
    assert.equal((await request('/v1/qrng/bytes/4097')).response.status, 400);
    assert.equal((await request('/v1/qrng/bytes/not-a-number')).response.status, 400);
});

test('returns float and full-width uint64 values', async () => {
    const floating = await request('/v1/qrng/number?type=float');
    assert.equal(floating.response.status, 200);
    assert.ok(floating.body.value >= 0 && floating.body.value < 1);

    const integer = await request('/v1/qrng/number?type=uint64');
    assert.equal(integer.response.status, 200);
    assert.match(integer.body.value, /^\d+$/);
    assert.ok(BigInt(integer.body.value) <= 0xffffffffffffffffn);
});

test('range endpoints safely cover full-width inputs', async () => {
    const signed = await request(
        '/v1/qrng/range?min=-2147483648&max=2147483647'
    );
    assert.equal(signed.response.status, 200);
    assert.ok(signed.body.value >= -2147483648);
    assert.ok(signed.body.value <= 2147483647);

    const unsigned = await request(
        '/v1/qrng/range64?min=0&max=18446744073709551615'
    );
    assert.equal(unsigned.response.status, 200);
    assert.match(unsigned.body.value, /^\d+$/);
    assert.ok(BigInt(unsigned.body.value) <= 0xffffffffffffffffn);
});

test('probability endpoints preserve exact zero and one semantics', async () => {
    const never = await request('/v1/qrng/boolean?probability=0');
    const always = await request('/v1/qrng/boolean?probability=1');
    assert.deepEqual(never.body, { probability: 0, value: false });
    assert.deepEqual(always.body, { probability: 1, value: true });
});

test('choice uses an exact bounded integer draw', async () => {
    const { response, body } = await request('/v1/qrng/choice', {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ array: ['alpha', 'beta', 'gamma'] }),
    });
    assert.equal(response.status, 200);
    assert.ok(Number.isInteger(body.index));
    assert.equal(body.value, ['alpha', 'beta', 'gamma'][body.index]);
});

test('exposes OpenAPI and returns structured 404 errors', async () => {
    const openapi = await request('/v1/openapi.json');
    assert.equal(openapi.response.status, 200);
    assert.equal(openapi.body.openapi, '3.0.3');

    const missing = await request('/does-not-exist');
    assert.equal(missing.response.status, 404);
    assert.deepEqual(missing.body, { error: 'Not found' });
});
