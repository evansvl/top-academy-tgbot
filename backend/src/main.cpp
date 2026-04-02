#include <iostream>
#include <thread>
#include <fstream>
#include <unordered_map>
#include <tgbot/tgbot.h>
#include <httplib.h>
#include "db/database.hpp"
#include "journal_api/journal_client.hpp"
#include "tma_auth/tma_auth.hpp"

std::unordered_map<std::string, std::string> load_env(const std::string& filepath) {
    std::unordered_map<std::string, std::string> env;
    std::ifstream file(filepath);
    if (!file.is_open()) return env;
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            if (!val.empty() && val.back() == '\r') val.pop_back();
            env[key] = val;
        }
    }
    return env;
}

int main() {
    auto env = load_env(".env");
    
    const std::string BOT_TOKEN = env.count("BOT_TOKEN") ? env["BOT_TOKEN"] : "";
    const std::string DB_PATH = env.count("DB_PATH") ? env["DB_PATH"] : "journal_bot.db";
    const std::string TMA_URL = env.count("TMA_URL") ? env["TMA_URL"] : "https://your-mini-app-domain.com";

    if (BOT_TOKEN.empty()) {
        std::cerr << "CRITICAL ERROR: BOT_TOKEN is not set in .env file." << std::endl;
        return 1;
    }

    db::Database database(DB_PATH);
    if (!database.init()) {
        std::cerr << "Failed to initialize database." << std::endl;
        return 1;
    }

    journal::JournalClient journal_client;
    auth::TgAuth tma_auth(BOT_TOKEN);

    std::thread server_thread([&]() {
        httplib::Server svr;

        svr.Post("/api/tma/schedule", [&](const httplib::Request& req, httplib::Response& res) {
            std::string init_data = req.get_header_value("X-Telegram-InitData");
            
            if (!tma_auth.validate_init_data(init_data)) {
                res.status = 401;
                res.set_content("{\"error\":\"Invalid InitData\"}", "application/json");
                return;
            }

            long long tg_id = tma_auth.extract_user_id(init_data);
            if (tg_id == 0) {
                res.status = 400;
                res.set_content("{\"error\":\"Invalid User ID\"}", "application/json");
                return;
            }

            auto user_opt = database.get_user(tg_id);
            if (!user_opt) {
                res.status = 403;
                res.set_content("{\"error\":\"User not logged in\"}", "application/json");
                return;
            }

            auto schedule = journal_client.get_schedule(user_opt->access_token, "2026-04-02");
            if (schedule.success) {
                res.set_content(schedule.raw_response.dump(), "application/json");
            } else {
                res.status = 502;
                res.set_content("{\"error\":\"Journal API error: " + schedule.error_message.value_or("unknown") + "\"}", "application/json");
            }
        });

        std::cout << "Starting HTTP server for TMA on port 8080..." << std::endl;
        svr.listen("0.0.0.0", 8080);
    });

    TgBot::Bot bot(BOT_TOKEN);

    bot.getEvents().onCommand("start", [&](TgBot::Message::Ptr message) {
        long long chat_id = message->chat->id;
        auto user_opt = database.get_user(chat_id);

        if (user_opt) {
            TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
            std::vector<TgBot::InlineKeyboardButton::Ptr> row;
            
            TgBot::InlineKeyboardButton::Ptr web_app_btn(new TgBot::InlineKeyboardButton);
            web_app_btn->text = "Открыть Журнал (Mini App)";
            
            TgBot::WebAppInfo::Ptr web_app_info(new TgBot::WebAppInfo);
            web_app_info->url = TMA_URL;
            web_app_btn->webApp = web_app_info;
            
            row.push_back(web_app_btn);
            keyboard->inlineKeyboard.push_back(row);

            bot.getApi().sendMessage(chat_id, "Привет, " + user_opt->full_name + "! Ты уже авторизован. Нажми кнопку ниже, чтобы открыть журнал.", nullptr, nullptr, keyboard);
        } else {
            bot.getApi().sendMessage(chat_id, "Привет! Для начала работы отправь свой логин и пароль в формате:\n\n`login password`", nullptr, nullptr, nullptr, "Markdown");
        }
    });

    bot.getEvents().onAnyMessage([&](TgBot::Message::Ptr message) {
        if (message->text.find("/") == 0) return;

        long long chat_id = message->chat->id;
        
        if (database.get_user(chat_id)) {
            bot.getApi().sendMessage(chat_id, "Ты уже авторизован! Вызови /start чтобы открыть Mini App.");
            return;
        }

        std::istringstream iss(message->text);
        std::string username, password;
        iss >> username >> password;

        if (username.empty() || password.empty()) {
            bot.getApi().sendMessage(chat_id, "Неверный формат. Нужно отправить `логин пароль` через пробел.", nullptr, nullptr, nullptr, "Markdown");
            return;
        }

        bot.getApi().sendMessage(chat_id, "Проверяю учетные данные...");

        auto login_result = journal_client.login(username, password);
        if (login_result.success && login_result.access_token) {
            auto profile = journal_client.get_user_info(*login_result.access_token);
            
            db::UserRecord record;
            record.telegram_id = chat_id;
            record.access_token = *login_result.access_token;
            record.refresh_token = login_result.refresh_token.value_or("");
            record.student_id = profile.student_id.value_or(0);
            record.group_id = profile.current_group_id.value_or(0);
            record.full_name = profile.full_name.value_or("Студент");
            record.photo_url = profile.photo_url.value_or("");

            if (database.save_user(record)) {
                bot.getApi().sendMessage(chat_id, "✅ Успешная авторизация!\nДобро пожаловать, " + record.full_name + "!\nНажми /start чтобы открыть Mini App.");
            } else {
                bot.getApi().sendMessage(chat_id, "❌ Ошибка базы данных при сохранении токенов.");
            }
        } else {
            std::string err = login_result.error_message.value_or("Неизвестная ошибка");
            bot.getApi().sendMessage(chat_id, "❌ Ошибка входа: " + err);
        }
    });

    try {
        std::cout << "Bot is running..." << std::endl;
        TgBot::TgLongPoll longPoll(bot);
        while (true) {
            longPoll.start();
        }
    } catch (std::exception& e) {
        std::cerr << "Bot exception: " << e.what() << std::endl;
    }

    server_thread.join();
    return 0;
}