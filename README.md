# 🎵 C++ Telegram Music Bot

Music bot — but in C++.
ᴮᵉᶜᵃᵘˢᵉ ʷʰʸ ⁿᵒᵗˀ

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
