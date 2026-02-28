# 🎵 C++ Telegram Music Bot

Searches YouTube via **yt-dlp** and sends MP3 files — written in C++17.

## Architecture

```
User message
    → C++ bot (tgbot-cpp, long-poll)
        → shells out to yt-dlp for search (JSON output)
        → shells out to yt-dlp + ffmpeg for download/convert
        → sends MP3 via Telegram Bot API
```

## Dependencies

| Library | Purpose |
|---|---|
| tgbot-cpp | Telegram Bot API client |
| nlohmann/json | Parse yt-dlp JSON output |
| libcurl | HTTP (used by tgbot-cpp) |
| OpenSSL | TLS |
| Boost.System | Used by tgbot-cpp |
| yt-dlp (Python CLI) | YouTube search + download |
| ffmpeg | Audio conversion to MP3 |

---

## Local Build (Linux / macOS)

```bash
# 1. Install system deps
sudo apt install -y build-essential cmake libssl-dev libcurl4-openssl-dev \
                    libboost-system-dev ffmpeg python3-pip
pip3 install yt-dlp

# 2. Build tgbot-cpp
git clone https://github.com/reo7sp/tgbot-cpp.git
cmake -S tgbot-cpp -B tgbot-cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build tgbot-cpp/build -j$(nproc)
sudo cmake --install tgbot-cpp/build

# 3. Build the bot
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 4. Run
export TELEGRAM_BOT_TOKEN="123456:ABC-your-token"
./build/music_bot
```

---

## Deploy to Railway

1. Push this folder to a GitHub repository.
2. Go to [railway.app](https://railway.app) → **New Project** → **Deploy from GitHub repo**.
3. Railway auto-detects the `Dockerfile` and builds it.
4. Go to your service → **Variables** → add:
   ```
   TELEGRAM_BOT_TOKEN = 123456:ABC-your-token-here
   ```
5. Railway redeploys automatically. Your bot is now live 24/7. ✅

> The Dockerfile uses a two-stage build:
> - **Stage 1** compiles tgbot-cpp and the bot binary
> - **Stage 2** is a slim runtime image with yt-dlp + ffmpeg

---

## Bot Commands

| Command | Description |
|---|---|
| `/start` | Welcome message |
| `/help` | Usage instructions |
| _(any text)_ | Search YouTube, shows up to 5 results |
| _(tap result button)_ | Downloads and sends MP3 |

## Configuration (main.cpp top)

```cpp
static const int    MAX_RESULTS      = 5;    // search result count
static const int    MAX_DURATION_SEC = 600;  // skip videos > 10 min
static const std::string AUDIO_QUALITY = "192"; // kbps
```
