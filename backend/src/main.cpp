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
#include "utils/date_utils.hpp"

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

TgBot::ReplyKeyboardMarkup::Ptr get_main_keyboard(const std::string& TMA_URL) {
    TgBot::ReplyKeyboardMarkup::Ptr keyboard(new TgBot::ReplyKeyboardMarkup);
    keyboard->oneTimeKeyboard = false;
    keyboard->resizeKeyboard = true;

    std::vector<TgBot::KeyboardButton::Ptr> row1;
    TgBot::KeyboardButton::Ptr grades_button(new TgBot::KeyboardButton);
    grades_button->text = "Оценки";
    grades_button->requestContact = false;
    grades_button->requestLocation = false;
    row1.push_back(grades_button);

    TgBot::KeyboardButton::Ptr schedule_button(new TgBot::KeyboardButton);
    schedule_button->text = "Расписание";
    schedule_button->requestContact = false;
    schedule_button->requestLocation = false;
    row1.push_back(schedule_button);

    keyboard->keyboard.push_back(row1);

    std::vector<TgBot::KeyboardButton::Ptr> row2;
    TgBot::KeyboardButton::Ptr mini_app_button(new TgBot::KeyboardButton);
    mini_app_button->text = "Перейти в приложение";
    mini_app_button->webApp = TgBot::WebAppInfo::Ptr(new TgBot::WebAppInfo);
    mini_app_button->webApp->url = TMA_URL;
    row2.push_back(mini_app_button);
    keyboard->keyboard.push_back(row2);

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
    int password_message_id = 0;
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
                    auto msg = bot.getApi().sendMessage(chat_id, "Отлично! Теперь введите ваш *пароль*:", nullptr, nullptr, nullptr, "Markdown");                    session.password_message_id = msg->messageId;                    session.messages_to_delete.push_back(msg->messageId);
                    return;
                } 
                else if (session.state == AWAITING_PASSWORD) {
                    std::string password = message->text;
                    std::string login = session.login;
                    bot.getApi().sendMessage(chat_id, "Проверяем данные...");

                    journal::JournalClient client(login, password);
                        if (client.auth_check()) {
                            db::UserRecord new_user;
                            new_user.telegram_id = chat_id;
                            new_user.login = login;
                            new_user.password = password;
                            new_user.access_token = "";
                            new_user.refresh_token = "";
                            new_user.student_id = 0;
                            new_user.group_id = 0;
                            db.save_user(new_user);
                            bot.getApi().sendMessage(chat_id, "Вы успешно вошли!", nullptr, 0, get_main_keyboard(TMA_URL));
                        } else {
                            bot.getApi().sendMessage(chat_id, "Неверный логин или пароль. Попробуйте снова.");
                        }
                    
                    
                    auth_sessions.erase(chat_id);
                    return;
                }

                auto user_opt = db.get_user(chat_id);
                if (user_opt) {
                    auto user = *user_opt;
                    if (message->text == "Оценки") {
                        bot.getApi().sendMessage(chat_id, "Выберите дату:", nullptr, 0, utils::create_date_keyboard("grades_"));
                    } else if (message->text == "Расписание") {
                        bot.getApi().sendMessage(chat_id, "Выберите дату:", nullptr, 0, utils::create_date_keyboard("schedule_"));
                    }
                }
            }
        });

        bot.getEvents().onCallbackQuery([&bot, &db](TgBot::CallbackQuery::Ptr query) {
            long long chat_id = query->message->chat->id;
            std::string data = query->data;

            std::string prefix = data.substr(0, data.find("_") + 1);
            std::string date = data.substr(data.find("_") + 1);

            auto user_opt = db.get_user(chat_id);
            if (!user_opt) {
                bot.getApi().sendMessage(chat_id, "Не удалось найти ваши данные. Пожалуйста, попробуйте /start снова.");
                return;
            }
            auto user = *user_opt;

bot.getApi().answerCallbackQuery(query->id, "Загрузка...");
            try {
                bot.getApi().deleteMessage(chat_id, query->message->messageId);
            } catch (const std::exception&) {}

            if (prefix == "grades_") {
                std::thread([chat_id, user, date, &bot, &db]() {
                    journal::JournalClient client(user.login, user.password);
                    if (client.auth_check()) {
                        auto grades = client.get_grades(date);
                        if (grades.empty()) {
                            bot.getApi().sendMessage(chat_id, "Оценок за " + date + " нет.");
                        } else {
                            std::stringstream ss;
                            ss << "Оценки за " << date << ":\n\n";
                            for (const auto& grade : grades) {
                                ss << "🔹 " << grade.lesson << ": " << grade.value << " (" << grade.type << ")\n";
                            }
                            
                            std::string text = ss.str();
                            if (text.length() > 4096) {
                                text = text.substr(0, 4090) + "\n...";
                            }
                            bot.getApi().sendMessage(chat_id, text);
                        }
                    } else {
                        bot.getApi().sendMessage(chat_id, "Ошибка авторизации. Попробуйте /start снова.");
                    }
                }).detach();
            } else if (prefix == "schedule_") {
                 std::thread([chat_id, user, date, &bot, &db]() {
                    journal::JournalClient client(user.login, user.password);
                    if (client.auth_check()) {
                        auto schedule = client.get_schedule(date);
                        if (schedule.empty()) {
                            bot.getApi().sendMessage(chat_id, "Занятий на " + date + " нет.");
                        } else {
                            std::stringstream ss;
                            ss << "Расписание на " << date << ":\n\n";
                            for (const auto& item : schedule) {
                                ss << "🕒 " << item.started_at << " - " << item.finished_at << "\n"
                                   << "   " << item.subject_name << "\n"
                                   << "   📍 " << item.room_name << "\n\n";
                            }
                            std::string text = ss.str();
                            if (text.length() > 4096) {
                                text = text.substr(0, 4090) + "\n...";
                            }
                            bot.getApi().sendMessage(chat_id, text);
                        }
                    } else {
                        bot.getApi().sendMessage(chat_id, "Ошибка авторизации. Попробуйте /start снова.");
                    }
                }).detach();
            }
        });

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
            
            svr.Get("/api/tma/schedule", [&db, bot_token](const httplib::Request& req, httplib::Response& res) {
                if (!req.has_header("x-telegram-initdata")) {
                    res.status = 401;
                    res.set_content("{\"error\":\"No init data\"}", "application/json");
                    return;
                }
                std::string init_data = req.get_header_value("x-telegram-initdata");
                
                try {
                    auth::TgAuth tg_auth(bot_token);
                    if (!tg_auth.validate_init_data(init_data)) {
                        res.status = 401;
                        res.set_content("{\"error\":\"Invalid TMA data\"}", "application/json");
                        return;
                    }
                    long long chat_id = tg_auth.extract_user_id(init_data);
                    auto user_opt = db.get_user(chat_id);
                    if (!user_opt) {
                        res.status = 401;
                        res.set_content("{\"error\":\"User not found\"}", "application/json");
                        return;
                    }
                    auto user = *user_opt;
                    
                    journal::JournalClient client(user.login, user.password);
                    std::string today = get_date_string(0);
                    if (client.auth_check()) {
                        auto schedule = client.get_schedule(today);
                        nlohmann::json j = nlohmann::json::array();
                        for (const auto& lesson : schedule) {
                            j.push_back({
                                {"started_at", lesson.started_at},
                                {"finished_at", lesson.finished_at},
                                {"subject_name", lesson.subject_name},
                                {"room_name", lesson.room_name}
                            });
                        }
                        res.set_content(j.dump(), "application/json");
                    } else {
                        res.status = 401;
                        res.set_content("{\"error\":\"Auth failed\"}", "application/json");
                    }
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
