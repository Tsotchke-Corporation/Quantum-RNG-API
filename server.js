'use strict';

const express = require('express');
const swaggerJsdoc = require('swagger-jsdoc');
const swaggerUi = require('swagger-ui-express');
const packageMetadata = require('./package.json');

const { QuantumRNG } = require('./build/Release/quantum_rng.node');

const API_VERSION = packageMetadata.version;
const MAX_BYTES_PER_REQUEST = 4096;
const MAX_CHOICE_ITEMS = 100000;
const UINT64_MAX = (1n << 64n) - 1n;
const PORT = parsePort(process.env.PORT || '3000');
const HOST = process.env.HOST || '0.0.0.0';
const QRNG_MODE = parseMode(process.env.QRNG_MODE || 'direct');
const VERIFY_ON_START = process.env.QRNG_VERIFY_ON_START !== '0';
const VERIFY_MEASUREMENTS = parseVerificationMeasurements(
    process.env.QRNG_VERIFY_MEASUREMENTS || '4000'
);

class ClientError extends Error {
    constructor(message, status = 400) {
        super(message);
        this.name = 'ClientError';
        this.status = status;
    }
}

function parsePort(value) {
    const parsed = Number(value);
    if (!Number.isInteger(parsed) || parsed < 0 || parsed > 65535) {
        throw new Error('PORT must be an integer between 0 and 65535');
    }
    return parsed;
}

function parseMode(value) {
    if (!['direct', 'grover', 'bell-verified'].includes(value)) {
        throw new Error('QRNG_MODE must be direct, grover, or bell-verified');
    }
    return value;
}

function parseVerificationMeasurements(value) {
    const parsed = Number(value);
    if (!Number.isInteger(parsed) || parsed < 1000 || parsed > 100000) {
        throw new Error(
            'QRNG_VERIFY_MEASUREMENTS must be an integer between 1000 and 100000'
        );
    }
    return parsed;
}

function parseInteger(value, name, min, max) {
    if (typeof value !== 'string' || !/^-?\d+$/.test(value)) {
        throw new ClientError(`${name} must be an integer`);
    }
    const parsed = Number(value);
    if (!Number.isSafeInteger(parsed) || parsed < min || parsed > max) {
        throw new ClientError(`${name} must be between ${min} and ${max}`);
    }
    return parsed;
}

function parseUInt64(value, name) {
    if (typeof value !== 'string' || !/^\d+$/.test(value)) {
        throw new ClientError(`${name} must be an unsigned decimal integer`);
    }
    const parsed = BigInt(value);
    if (parsed > UINT64_MAX) {
        throw new ClientError(`${name} must be at most 18446744073709551615`);
    }
    return parsed;
}

function parseProbability(value) {
    if (value === undefined) return 0.5;
    if (typeof value !== 'string' || value.trim() === '') {
        throw new ClientError('probability must be a number between 0 and 1');
    }
    const parsed = Number(value);
    if (!Number.isFinite(parsed) || parsed < 0 || parsed > 1) {
        throw new ClientError('probability must be a number between 0 and 1');
    }
    return parsed;
}

function serializeStats(stats) {
    return Object.fromEntries(
        Object.entries(stats).map(([key, value]) => [
            key,
            typeof value === 'bigint' ? value.toString() : value,
        ])
    );
}

function engineMetadata(rngInstance) {
    return {
        name: 'quantum_rng_v3',
        release: QuantumRNG.getVersion(),
        engineVersion: QuantumRNG.getEngineVersion(),
        revision: QuantumRNG.getRevision(),
        mode: rngInstance.getMode(),
    };
}

function runStartupVerification(rngInstance) {
    if (!VERIFY_ON_START) {
        return {
            performed: false,
            scope: 'quantum-state simulation model',
            reason: 'disabled by QRNG_VERIFY_ON_START=0',
        };
    }

    const result = rngInstance.verifyQuantum(VERIFY_MEASUREMENTS);
    if (!result.violatesClassical) {
        throw new Error(
            `QRNG v3 startup CHSH verification failed: S=${result.chsh}`
        );
    }

    return {
        performed: true,
        scope: 'quantum-state simulation model',
        ...result,
    };
}

const rng = new QuantumRNG({
    mode: QRNG_MODE,
    bellMonitoring: QRNG_MODE === 'bell-verified',
});
const startupVerification = runStartupVerification(rng);

const swaggerOptions = {
    definition: {
        openapi: '3.0.3',
        info: {
            title: 'Tsotchke Quantum RNG API',
            version: API_VERSION,
            description:
                'HTTP interface to the quantum_rng v3 state-vector simulation engine. ' +
                'Output is conditioned with health-tested operating-system and CPU entropy. ' +
                'CHSH verification validates the simulation model; it is not a claim of physical quantum hardware.',
            license: {
                name: 'MIT',
                url: 'https://opensource.org/license/mit',
            },
        },
        servers: [
            {
                url: 'https://api.tsotchke.net',
                description: 'Production',
            },
        ],
        tags: [
            { name: 'System', description: 'Runtime status and provenance' },
            { name: 'Random', description: 'Random data generation' },
        ],
    },
    apis: ['./server.js'],
};

const swaggerDocument = swaggerJsdoc(swaggerOptions);

function createApp({ rngInstance = rng, verification = startupVerification } = {}) {
    const app = express();
    app.disable('x-powered-by');
    app.use(express.json({ limit: '256kb' }));

    app.get('/', (req, res) => {
        res.json({
            name: 'Tsotchke Quantum RNG API',
            apiVersion: API_VERSION,
            engine: engineMetadata(rngInstance),
            documentation: '/v1/docs',
            source: 'https://github.com/Tsotchke-Corporation/Quantum-RNG-API',
        });
    });

    const v1 = express.Router();
    v1.get('/openapi.json', (req, res) => res.json(swaggerDocument));
    v1.use('/docs', swaggerUi.serve, swaggerUi.setup(swaggerDocument));

    /**
     * @swagger
     * /v1/health:
     *   get:
     *     summary: Get runtime health, provenance, verification, and counters
     *     tags: [System]
     *     responses:
     *       200:
     *         description: The generator is initialized and operational
     */
    v1.get('/health', (req, res) => {
        res.json({
            status: 'ok',
            apiVersion: API_VERSION,
            engine: engineMetadata(rngInstance),
            verification,
            stats: serializeStats(rngInstance.getStats()),
        });
    });

    /**
     * @swagger
     * /v1/qrng/stats:
     *   get:
     *     summary: Get QRNG v3 generation and simulation counters
     *     tags: [System]
     *     responses:
     *       200:
     *         description: Current engine counters
     */
    v1.get('/qrng/stats', (req, res) => {
        res.json({
            engine: engineMetadata(rngInstance),
            stats: serializeStats(rngInstance.getStats()),
        });
    });

    /**
     * @swagger
     * /v1/qrng/verification:
     *   get:
     *     summary: Get the startup CHSH simulation-verification result
     *     tags: [System]
     *     responses:
     *       200:
     *         description: Startup verification result and its scope
     */
    v1.get('/qrng/verification', (req, res) => {
        res.json(verification);
    });

    /**
     * @swagger
     * /v1/qrng/bytes/{count}:
     *   get:
     *     summary: Generate random bytes
     *     tags: [Random]
     *     parameters:
     *       - in: path
     *         name: count
     *         required: true
     *         schema:
     *           type: integer
     *           minimum: 1
     *           maximum: 4096
     *     responses:
     *       200:
     *         description: Hex, Base64, and byte-array encodings of one draw
     *       400:
     *         description: Invalid count
     */
    v1.get('/qrng/bytes/:count', (req, res, next) => {
        try {
            const count = parseInteger(
                req.params.count,
                'count',
                1,
                MAX_BYTES_PER_REQUEST
            );
            const bytes = rngInstance.getBytes(count);
            res.json({
                length: bytes.length,
                bytes: bytes.toString('hex'),
                base64: bytes.toString('base64'),
                raw: Array.from(bytes),
            });
        } catch (error) {
            next(error);
        }
    });

    /**
     * @swagger
     * /v1/qrng/number:
     *   get:
     *     summary: Generate a floating-point or unsigned 64-bit value
     *     tags: [Random]
     *     parameters:
     *       - in: query
     *         name: type
     *         schema:
     *           type: string
     *           enum: [float, uint64]
     *           default: float
     *     responses:
     *       200:
     *         description: Generated value; uint64 values are decimal strings
     *       400:
     *         description: Invalid type
     */
    v1.get('/qrng/number', (req, res, next) => {
        try {
            const type = req.query.type || 'float';
            if (type === 'float') {
                return res.json({ type, value: rngInstance.getDouble() });
            }
            if (type === 'uint64') {
                return res.json({ type, value: rngInstance.getUInt64().toString() });
            }
            throw new ClientError('type must be float or uint64');
        } catch (error) {
            next(error);
        }
    });

    /**
     * @swagger
     * /v1/qrng/range:
     *   get:
     *     summary: Generate an unbiased signed 32-bit integer in an inclusive range
     *     tags: [Random]
     *     parameters:
     *       - in: query
     *         name: min
     *         required: true
     *         schema: { type: integer, minimum: -2147483648, maximum: 2147483647 }
     *       - in: query
     *         name: max
     *         required: true
     *         schema: { type: integer, minimum: -2147483648, maximum: 2147483647 }
     *     responses:
     *       200:
     *         description: Generated signed integer
     *       400:
     *         description: Invalid range
     */
    v1.get('/qrng/range', (req, res, next) => {
        try {
            const min = parseInteger(
                req.query.min,
                'min',
                -2147483648,
                2147483647
            );
            const max = parseInteger(
                req.query.max,
                'max',
                -2147483648,
                2147483647
            );
            if (min > max) throw new ClientError('min must not exceed max');
            res.json({ min, max, value: rngInstance.getRange32(min, max) });
        } catch (error) {
            next(error);
        }
    });

    /**
     * @swagger
     * /v1/qrng/range64:
     *   get:
     *     summary: Generate an unbiased unsigned 64-bit integer in an inclusive range
     *     tags: [Random]
     *     parameters:
     *       - in: query
     *         name: min
     *         required: true
     *         schema: { type: string, pattern: '^\\d+$' }
     *       - in: query
     *         name: max
     *         required: true
     *         schema: { type: string, pattern: '^\\d+$' }
     *     responses:
     *       200:
     *         description: Generated unsigned integer as a decimal string
     *       400:
     *         description: Invalid range
     */
    v1.get('/qrng/range64', (req, res, next) => {
        try {
            const min = parseUInt64(req.query.min, 'min');
            const max = parseUInt64(req.query.max, 'max');
            if (min > max) throw new ClientError('min must not exceed max');
            res.json({
                min: min.toString(),
                max: max.toString(),
                value: rngInstance.getRange64(min, max).toString(),
            });
        } catch (error) {
            next(error);
        }
    });

    /**
     * @swagger
     * /v1/qrng/boolean:
     *   get:
     *     summary: Generate a boolean with a requested true probability
     *     tags: [Random]
     *     parameters:
     *       - in: query
     *         name: probability
     *         schema: { type: number, minimum: 0, maximum: 1, default: 0.5 }
     *     responses:
     *       200:
     *         description: Generated boolean
     *       400:
     *         description: Invalid probability
     */
    v1.get('/qrng/boolean', (req, res, next) => {
        try {
            const probability = parseProbability(req.query.probability);
            const value = rngInstance.getDouble() < probability;
            res.json({ probability, value });
        } catch (error) {
            next(error);
        }
    });

    /**
     * @swagger
     * /v1/qrng/choice:
     *   post:
     *     summary: Select one element from a non-empty JSON array
     *     tags: [Random]
     *     requestBody:
     *       required: true
     *       content:
     *         application/json:
     *           schema:
     *             type: object
     *             required: [array]
     *             properties:
     *               array:
     *                 type: array
     *                 minItems: 1
     *                 maxItems: 100000
     *                 items: {}
     *     responses:
     *       200:
     *         description: Selected element and its exact unbiased index
     *       400:
     *         description: Invalid request body
     */
    v1.post('/qrng/choice', (req, res, next) => {
        try {
            const { array } = req.body || {};
            if (!Array.isArray(array) || array.length === 0) {
                throw new ClientError('array must be non-empty');
            }
            if (array.length > MAX_CHOICE_ITEMS) {
                throw new ClientError(`array must contain at most ${MAX_CHOICE_ITEMS} items`);
            }
            const index = Number(
                rngInstance.getRange64(0n, BigInt(array.length - 1))
            );
            res.json({ index, value: array[index] });
        } catch (error) {
            next(error);
        }
    });

    app.use('/v1', v1);

    app.use((req, res) => {
        res.status(404).json({ error: 'Not found' });
    });

    app.use((error, req, res, next) => {
        if (res.headersSent) return next(error);
        if (error instanceof ClientError) {
            return res.status(error.status).json({ error: error.message });
        }
        console.error(error);
        return res.status(500).json({ error: 'Internal server error' });
    });

    return app;
}

const app = createApp();

function startServer(port = PORT, host = HOST) {
    const server = app.listen(port, host, () => {
        const address = server.address();
        const boundPort = typeof address === 'object' && address ? address.port : port;
        console.log(`Tsotchke Quantum RNG API ${API_VERSION} listening on ${host}:${boundPort}`);
        console.log(
            `Engine ${QuantumRNG.getVersion()} (${QuantumRNG.getRevision().slice(0, 7)}), mode ${rng.getMode()}`
        );
        if (startupVerification.performed) {
            console.log(
                `Startup CHSH simulation verification: S=${startupVerification.chsh.toFixed(4)}`
            );
        }
    });
    return server;
}

if (require.main === module) {
    startServer();
}

module.exports = {
    API_VERSION,
    MAX_BYTES_PER_REQUEST,
    app,
    createApp,
    engineMetadata,
    rng,
    serializeStats,
    startServer,
    startupVerification,
};
