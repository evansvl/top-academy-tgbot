#include <iostream>
#include <thread>
#include <fstream>
#include <unordered_map>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <cstdlib>
#include <vector>
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
            std::string value = line.substr(pos + 1);
            env[key] = value;
        }
    }
    return env;
}

std::unordered_map<std::string, std::string> env = load_env(".env");
std::string get_env_var(const std::string& key) {
    if (const char* val = std::getenv(key.c_str())) return val;
    auto it = env.find(key);
    return it != env.end() ? it->second : "";
}

TgBot::ReplyKeyboardMarkup::Ptr get_main_keyboard(const std::string& tma_url) {
    TgBot::ReplyKeyboardMarkup::Ptr keyboard(new TgBot::ReplyKeyboardMarkup);
    keyboard->resizeKeyboard = true;

    TgBot::KeyboardButton::Ptr btn_tma(new TgBot::KeyboardButton);
    btn_tma->text = "Открыть Mini App";
    TgBot::WebAppInfo::Ptr web_app(new TgBot::WebAppInfo);
    web_app->url = tma_url;
    btn_tma->webApp = web_app;
    btn_tma->requestContact = false;
    btn_tma->requestLocation = false;

    TgBot::KeyboardButton::Ptr btn_today(new TgBot::KeyboardButton);
    btn_today->text = "На сегодня";
    btn_today->requestContact = false;
    btn_today->requestLocation = false;
    
    TgBot::KeyboardButton::Ptr btn_tomorrow(new TgBot::KeyboardButton);
    btn_tomorrow->text = "На завтра";
    btn_tomorrow->requestContact = false;
    btn_tomorrow->requestLocation = false;

    TgBot::KeyboardButton::Ptr btn_grades(new TgBot::KeyboardButton);
    btn_grades->text = "Оценки";
    btn_grades->requestContact = false;
    btn_grades->requestLocation = false;

    TgBot::KeyboardButton::Ptr btn_hw(new TgBot::KeyboardButton);
    btn_hw->text = "Домашние задания";
    btn_hw->requestContact = false;
    btn_hw->requestLocation = false;

    // Layout: 1-2-2
    std::vector<TgBot::KeyboardButton::Ptr> row1 = { btn_tma };
    std::vector<TgBot::KeyboardButton::Ptr> row2 = { btn_today, btn_tomorrow };
    std::vector<TgBot::KeyboardButton::Ptr> row3 = { btn_grades, btn_hw };

    keyboard->keyboard.push_back(row1);
    keyboard->keyboard.push_back(row2);
    keyboard->keyboard.push_back(row3);

    return keyboard;
}

enum AuthState {
    NONE,
    AWAITING_LOGIN,
    AWAITING_PASSWORD
};

struct AuthSession {
    AuthState state = NONE;
    std::string login = "";
    std::vector<int> messages_to_delete;
};

std::string get_date_string(int offset_days = 0) {
    std::time_t t = std::time(nullptr);
    t += offset_days * 24 * 60 * 60;
    std::tm* tm = std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%d");
    return ss.str();
}

int main() {
    std::string bot_token = get_env_var("BOT_TOKEN");
    std::string TMA_URL = get_env_var("TMA_URL");
    std::string db_path = get_env_var("DB_PATH");
    if (db_path.empty()) db_path = "journal.db";

    if (bot_token.empty()) {
        std::cerr << "BOT_TOKEN not found!" << std::endl;
        return 1;
    }
    if (TMA_URL.empty()) {
        TMA_URL = "https://example.com"; 
    }

    try {
        db::Database db(db_path);
        db.init();

        TgBot::Bot bot(bot_token);
        std::unordered_map<long long, AuthSession> auth_sessions;

        bot.getEvents().onCommand("start", [&bot, &db, &auth_sessions, TMA_URL](TgBot::Message::Ptr message) {
            long long chat_id = message->chat->id;
            auto user = db.get_user(chat_id);
            if (user) {
                bot.getApi().sendMessage(chat_id, "Вы уже авторизованы!", nullptr, nullptr, get_main_keyboard(TMA_URL), "Markdown");
                return;
            }

            bot.getApi().sendMessage(chat_id, "Добро пожаловать в неофициальный бот Топ Академии!\nДля работы необходимо войти в аккаунт журнала.");
            auto msg = bot.getApi().sendMessage(chat_id, "Введите ваш *логин* от журнала:", nullptr, nullptr, nullptr, "Markdown");
            
            AuthSession session;
            session.state = AWAITING_LOGIN;
            session.messages_to_delete.push_back(message->messageId);
            session.messages_to_delete.push_back(msg->messageId);
            auth_sessions[chat_id] = session;
        });

        bot.getEvents().onAnyMessage([&bot, &db, &auth_sessions, TMA_URL](TgBot::Message::Ptr message) {
            if (message->text.empty() || message->text == "/start") return;
            
            long long chat_id = message->chat->id;
            std::string text = message->text;

            if (auth_sessions.count(chat_id)) {
                auto& session = auth_sessions[chat_id];
                session.messages_to_delete.push_back(message->messageId);

                if (session.state == AWAITING_LOGIN) {
                    session.login = text;
                    session.state = AWAITING_PASSWORD;
                    auto msg = bot.getApi().sendMessage(chat_id, "Отлично! Теперь введите ваш *пароль*:", nullptr, nullptr, nullptr, "Markdown");
                    session.messages_to_delete.push_back(msg->messageId);
                    return;
                } 
                else if (session.state == AWAITING_PASSWORD) {
                    auto user_opt = db.get_user(chat_id);
                    if (!user_opt) {
                        bot.getApi().sendMessage(chat_id, "Произошла ошибка. Пожалуйста, попробуйте /start снова.");
                        auth_sessions.erase(chat_id);
                        return;
                    }
                    auto user = *user_opt;
                    user.password = message->text;
                    bot.getApi().deleteMessage(chat_id, message->messageId);
                    if (auth_sessions[chat_id].password_message_id != 0) {
                        bot.getApi().deleteMessage(chat_id, auth_sessions[chat_id].password_message_id);
                    }
                    bot.getApi().sendMessage(chat_id, "Проверяем данные...");

                    std::thread([&, chat_id, user, message]() {
                        journal::JournalClient client(user.login, user.password);
                        if (client.auth_check()) {
                            db.update_user(user);
                            bot.getApi().sendMessage(chat_id, "Вы успешно вошли!", nullptr, 0, get_main_keyboard());
                        } else {
                            bot.getApi().sendMessage(chat_id, "Неверный логин или пароль. Попробуйте снова.");
                            db.delete_user(chat_id);
                        }
                        auth_sessions.erase(chat_id);
                    }).detach();
                    return;
                }

                auto user_opt = db.get_user(chat_id);
                if (user_opt) {
                    auto user = *user_opt;
                    if (message->text == "Оценки") {
                        bot.getApi().sendMessage(chat_id, "Раздел 'Оценки' в разработке.");
                    } else if (message->text == "Домашнее задание") {
                        std::thread([&, chat_id, user]() {
                            journal::JournalClient client(user.login, user.password);
                            if (client.auth_check()) {
                                auto homeworks = client.get_homeworks();
                                if (homeworks.empty()) {
                                    bot.getApi().sendMessage(chat_id, "Домашних заданий нет.");
                                } else {
                                    std::stringstream ss;
                                    for (const auto& hw : homeworks) {
                                        ss << "📝 " << hw.lesson << "\n"
                                           << "   " << hw.task << "\n\n";
                                    }
                                    bot.getApi().sendMessage(chat_id, ss.str());
                                }
                            } else {
                                bot.getApi().sendMessage(chat_id, "Не удалось обновить данные. Попробуйте /start и войдите снова.");
                            }
                        }).detach();
                    }
                }
            }
        } catch (TgBot::TgException& e) {
            std::cerr << "TgBot error: " << e.what() << std::endl;
        }

        std::thread tma_server([&db, bot_token]() {
            httplib::Server svr;
            svr.Post("/api/tma/auth", [&db, bot_token](const httplib::Request& req, httplib::Response& res) {
                try {
                    auth::TgAuth tg_auth(bot_token);
                    if (!tg_auth.validate_init_data(req.body)) {
                         throw std::runtime_error("Invalid TMA data");
                    }
                    long long chat_id = tg_auth.extract_user_id(req.body);
                    auto user = db.get_user(chat_id);
                    if (!user) {
                        res.status = 401;
                        res.set_content("{\"error\":\"User not found\"}", "application/json");
                        return;
                    }
                    res.set_content("{\"status\":\"ok\", \"token\":\"" + user->access_token + "\"}", "application/json");
                } catch (const std::exception& e) {
                    res.status = 401;
                    res.set_content(std::string("{\"error\":\"") + e.what() + "\"}", "application/json");
                }
            });
            
            svr.Get("/api/tma/schedule", [](const httplib::Request& req, httplib::Response& res) {
                if (!req.has_header("Authorization")) {
                    res.status = 401;
                    return;
                }
                std::string token = req.get_header_value("Authorization");
                
                try {
                    journal::JournalClient client;
                    std::string today = get_date_string(0);
                    auto schedule_res = client.get_schedule(token, today);
                    res.set_content(schedule_res.raw_response.dump(), "application/json");
                } catch (const std::exception& e) {
                    res.status = 500;
                    res.set_content("{\"error\":\"API Error\"}", "application/json");
                }
            });

            std::cout << "Starting HTTP server for TMA on port 8080..." << std::endl;
            svr.listen("0.0.0.0", 8080);
        });

        std::cout << "Bot is running..." << std::endl;
        TgBot::TgLongPoll longPoll(bot);
        while (true) {
            try {
                longPoll.start();
            } catch (const std::exception& e) {
                std::cerr << "LongPoll error: " << e.what() << std::endl;
            }
        }

        tma_server.join();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
