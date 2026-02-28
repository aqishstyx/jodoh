#include <tgbot/tgbot.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using json   = nlohmann::json;

// ─── Config ───────────────────────────────────────────────────────────────────

static const int         MAX_RESULTS      = 5;
static const int         MAX_DURATION_SEC = 600;
static const std::string AUDIO_QUALITY    = "192";

// ─── Helpers ──────────────────────────────────────────────────────────────────

std::string getEnv(const std::string& key, const std::string& fallback = "") {
    const char* v = std::getenv(key.c_str());
    return v ? std::string(v) : fallback;
}

std::string fmtDuration(int seconds) {
    int m = seconds / 60, s = seconds % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", m, s);
    return buf;
}

std::string runCommand(const std::string& cmd) {
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen failed");
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) result += buf;
    int rc = pclose(pipe);
    if (rc != 0) throw std::runtime_error("Command failed (rc=" + std::to_string(rc) + ")");
    return result;
}

std::string shellEscape(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else           out += c;
    }
    return out + "'";
}

// ─── YouTube search ───────────────────────────────────────────────────────────

struct VideoInfo {
    std::string id;
    std::string title;
    std::string url;
    int         duration;
    std::string durationFmt;
};

std::vector<VideoInfo> searchYouTube(const std::string& query) {
    std::string cmd =
        "yt-dlp --quiet --skip-download --flat-playlist "
        "--dump-json "
        "--default-search ytmsearch" + std::to_string(MAX_RESULTS) + " "
        + shellEscape(query) + " 2>/dev/null";

    std::string raw;
    try { raw = runCommand(cmd); } catch (...) { return {}; }

    std::vector<VideoInfo> results;
    std::istringstream stream(raw);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        try {
            auto j   = json::parse(line);
            int  dur = j.value("duration", 0);
            if (dur > MAX_DURATION_SEC) continue;
            VideoInfo v;
            v.id          = j.value("id", "");
            v.title       = j.value("title", "Unknown");
            v.duration    = dur;
            v.durationFmt = fmtDuration(dur);
            v.url         = "https://www.youtube.com/watch?v=" + v.id;
            if (!v.id.empty()) results.push_back(v);
        } catch (...) { continue; }
    }
    return results;
}

// ─── Download ─────────────────────────────────────────────────────────────────

fs::path downloadMp3(const std::string& url, const fs::path& tmpDir) {
    std::string cmd =
        "yt-dlp --quiet "
        "--format bestaudio/best "
        "--extract-audio "
        "--audio-format mp3 "
        "--audio-quality " + AUDIO_QUALITY + " "
        "--output " + shellEscape((tmpDir / "%(title)s.%(ext)s").string()) + " "
        + shellEscape(url) + " 2>&1";

    runCommand(cmd);

    for (auto& entry : fs::directory_iterator(tmpDir))
        if (entry.path().extension() == ".mp3")
            return entry.path();

    throw std::runtime_error("MP3 not found after download");
}

// ─── Session store ────────────────────────────────────────────────────────────

using ResultMap = std::map<std::string, VideoInfo>;

struct SessionStore {
    std::mutex                   mtx;
    std::map<int64_t, ResultMap> data;

    void store(int64_t userId, const std::vector<VideoInfo>& videos) {
        std::lock_guard<std::mutex> lock(mtx);
        ResultMap m;
        for (auto& v : videos) m[v.id] = v;
        data[userId] = std::move(m);
    }

    std::optional<VideoInfo> fetch(int64_t userId, const std::string& videoId) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = data.find(userId);
        if (it == data.end()) return std::nullopt;
        auto jt = it->second.find(videoId);
        if (jt == it->second.end()) return std::nullopt;
        return jt->second;
    }
};

// ─── sendMessage wrapper (new tgbot-cpp API) ──────────────────────────────────

void sendMsg(const TgBot::Api& api, int64_t chatId, const std::string& text,
             TgBot::InlineKeyboardMarkup::Ptr keyboard = nullptr) {
    api.sendMessage(chatId, text,
        nullptr,    // linkPreviewOptions
        nullptr,    // replyParameters
        keyboard,   // replyMarkup
        "Markdown"  // parseMode
    );
}

void editMsg(const TgBot::Api& api, int64_t chatId, int32_t msgId,
             const std::string& text,
             TgBot::InlineKeyboardMarkup::Ptr keyboard = nullptr) {
    api.editMessageText(text, chatId, msgId, "", "Markdown", nullptr, keyboard);
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main() {
    std::string token = getEnv("TELEGRAM_BOT_TOKEN");
    if (token.empty()) {
        std::cerr << "ERROR: TELEGRAM_BOT_TOKEN not set.\n";
        return 1;
    }

    TgBot::Bot   bot(token);
    SessionStore sessions;

    // /start
    bot.getEvents().onCommand("start", [&](TgBot::Message::Ptr msg) {
        sendMsg(bot.getApi(), msg->chat->id,
            "🎵 *JukeBox Bot* — Search a track to play");
    });

    // /help
    bot.getEvents().onCommand("help", [&](TgBot::Message::Ptr msg) {
        sendMsg(bot.getApi(), msg->chat->id,
            "🚨 Search any music by title or artist.");
    });

    // Text → search
    bot.getEvents().onNonCommandMessage([&](TgBot::Message::Ptr msg) {
        if (msg->text.empty()) return;
        std::string query = msg->text;

        auto statusMsg = bot.getApi().sendMessage(
            msg->chat->id,
            "✨🔍 Searching for *" + query + "*…",
            nullptr, nullptr, nullptr, "Markdown");

        int64_t chatId   = msg->chat->id;
        int32_t statusId = statusMsg->messageId;
        int64_t userId   = msg->from->id;

        std::thread([&bot, &sessions, query, chatId, statusId, userId]() {
            auto results = searchYouTube(query);

            if (results.empty()) {
                editMsg(bot.getApi(), chatId, statusId,
                    "❌ No results found. Try a different search term.");
                return;
            }

            sessions.store(userId, results);

            auto keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
            for (auto& v : results) {
                std::string label = "🎵 ";
                label += v.title.substr(0, 50);
                label += "  [" + v.durationFmt + "]";
                auto btn = std::make_shared<TgBot::InlineKeyboardButton>();
                btn->text         = label;
                btn->callbackData = "dl:" + v.id;
                keyboard->inlineKeyboard.push_back({btn});
            }

            editMsg(bot.getApi(), chatId, statusId,
                "✅ Found *" + std::to_string(results.size()) +
                "* results for _" + query + "_.",
                keyboard);
        }).detach();
    });

    // Callback → download
    bot.getEvents().onCallbackQuery([&](TgBot::CallbackQuery::Ptr query) {
        bot.getApi().answerCallbackQuery(query->id);

        std::string data = query->data;
        if (data.substr(0, 3) != "dl:") return;
        std::string videoId = data.substr(3);

        int64_t chatId = query->message->chat->id;
        int32_t msgId  = query->message->messageId;
        int64_t userId = query->from->id;

        std::thread([&bot, &sessions, videoId, chatId, msgId, userId]() {
            auto videoOpt = sessions.fetch(userId, videoId);
            if (!videoOpt) {
                editMsg(bot.getApi(), chatId, msgId,
                    "⚠️ Session expired. Please search again.");
                return;
            }
            auto video = *videoOpt;

            editMsg(bot.getApi(), chatId, msgId,
                "⬇️ Downloading *" + video.title + "*…\nThis may take a moment.");

            fs::path tmpDir = fs::temp_directory_path() / ("mbot_" + videoId);
            fs::create_directories(tmpDir);

            try {
                fs::path mp3 = downloadMp3(video.url, tmpDir);

                editMsg(bot.getApi(), chatId, msgId,
                    "📤 Sending *" + video.title + "*…");

                std::ifstream f(mp3, std::ios::binary);
                std::string   bytes((std::istreambuf_iterator<char>(f)),
                                     std::istreambuf_iterator<char>());

                // Create InputFile using shared_ptr directly
                auto inputFile = std::make_shared<TgBot::InputFile>();
                inputFile->data     = bytes;
                inputFile->mimeType = "audio/mpeg";
                inputFile->fileName = mp3.filename().string();

                bot.getApi().sendAudio(chatId, inputFile,
                    "🎵 " + video.title,
                    0, "", video.title);

                bot.getApi().deleteMessage(chatId, msgId);

            } catch (std::exception& e) {
                editMsg(bot.getApi(), chatId, msgId,
                    std::string("❌ Download failed: ") + e.what());
            }

            fs::remove_all(tmpDir);
        }).detach();
    });

    std::cout << "Bot: @" << bot.getApi().getMe()->username << "\n";
    std::cout << "Polling…\n";
    TgBot::TgLongPoll longPoll(bot);
    while (true) {
        try { longPoll.start(); }
        catch (TgBot::TgException& e) { std::cerr << "TgBot: " << e.what() << "\n"; }
        catch (std::exception& e)     { std::cerr << "Error: " << e.what() << "\n"; }
    }
}
