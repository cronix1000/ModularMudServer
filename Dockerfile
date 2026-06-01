# Modular MUD Server - Production Dockerfile
# Multi-stage build for smaller final image

# === Build stage ===
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake \
    nlohmann-json3-dev \
    libsqlite3-dev \
    liblua5.3-dev \
    sol2 \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app/

RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --parallel $(nproc)

# === Runtime stage ===
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    nlohmann-json3-dev \
    libsqlite3-0 \
    liblua5.3-0 \
    libstdc++6 \
    libgcc-s1 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /mud
COPY --from=builder /app/build/bin/ModularMudServer /mud/
COPY --from=builder /app/*.json /mud/
COPY --from=builder /app/*.db /mud/
COPY --from=builder /app/*.lua /mud/
COPY --from=builder /app/scripts/ /mud/scripts/
COPY --from=builder /app/regions/ /mud/regions/

RUN useradd -m -u 1000 mud && chown -R mud:mud /mud
USER mud

EXPOSE 27015

HEALTHCHECK --interval=30s --timeout=5s --retries=3 \
    CMD ss -tlnp | grep -q :27015 || exit 1

CMD ["./ModularMudServer"]
