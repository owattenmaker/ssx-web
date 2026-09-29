# SSX 3 online server + the built game, one process on one port (docs/multiplayer.md "Hosting").
# Build context: web/   docker build -f server/Dockerfile -t ssx3-online .   (server/Dockerfile.dockerignore keeps web/public out)
# Run:                  docker run -p 8080:8080 -v "$PWD/public:/app/public:ro" ssx3-online
# The extracted game data (web/public, from your own disc) is mounted, never baked into the image. Put a TLS reverse
# proxy (Caddy, nginx) in front for https/wss: WebGPU needs a secure context.
FROM node:22-alpine AS build
WORKDIR /app
COPY package.json package-lock.json* ./
RUN npm ci --no-audit --no-fund || npm install --no-audit --no-fund
COPY . .
# The code only (runtime/core.* must have been built with web/build-core.sh beforehand).
RUN npx vite build --config server/vite.online.config.js

FROM node:22-alpine
WORKDIR /app
COPY --from=build /app/dist-online ./dist-online
COPY server/mp-server.mjs ./server/mp-server.mjs
ENV MP_PORT=8080 MP_HOST=0.0.0.0 MP_STATIC=/app/dist-online,/app/public NODE_ENV=production
EXPOSE 8080
USER node
CMD ["node", "server/mp-server.mjs"]
