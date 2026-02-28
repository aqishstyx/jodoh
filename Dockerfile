# ── Stage 1: Build ────────────────────────────────────────────────────────────
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates \
        libssl-dev libcurl4-openssl-dev \
        libboost-system-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Build tgbot-cpp from source
RUN git clone --depth 1 https://github.com/reo7sp/tgbot-cpp.git /tgbot-cpp \
    && cmake -S /tgbot-cpp -B /tgbot-cpp/build \
        -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /tgbot-cpp/build -j$(nproc) \
    && cmake --install /tgbot-cpp/build

# Build our bot
WORKDIR /src
COPY CMakeLists.txt main.cpp ./

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j$(nproc)

# ── Stage 2: Runtime ──────────────────────────────────────────────────────────
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates libssl3 libcurl4 libboost-system1.74.0 \
        python3 python3-pip ffmpeg \
    && pip3 install --no-cache-dir yt-dlp --break-system-packages \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/music_bot /usr/local/bin/music_bot

# Railway injects TELEGRAM_BOT_TOKEN at runtime
ENV TELEGRAM_BOT_TOKEN=""

CMD ["music_bot"]
