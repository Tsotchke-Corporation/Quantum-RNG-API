FROM node:22-bookworm-slim AS build

WORKDIR /app

RUN apt-get update \
    && apt-get install --yes --no-install-recommends python3 make g++ \
    && rm -rf /var/lib/apt/lists/*

COPY package.json package-lock.json binding.gyp ./
COPY src ./src
COPY vendor ./vendor

RUN npm ci --omit=dev

FROM node:22-bookworm-slim AS runtime

ENV NODE_ENV=production \
    HOST=0.0.0.0 \
    PORT=8080

WORKDIR /app

COPY package.json package-lock.json server.js ./
COPY --from=build /app/node_modules ./node_modules
COPY --from=build /app/build ./build

USER node

EXPOSE 8080

CMD ["npm", "start"]
