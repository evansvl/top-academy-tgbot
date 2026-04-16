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
#include <memory>

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
            
            auto key_start = key.find_first_not_of(" \t\r\n");
            auto key_end = key.find_last_not_of(" \t\r\n");
            if (key_start == std::string::npos) key = "";
            else key = key.substr(key_start, key_end - key_start + 1);

            auto val_start = value.find_first_not_of(" \t\r\n");
            auto val_end = value.find_last_not_of(" \t\r\n");
            if (val_start == std::string::npos) value = "";
            else value = value.substr(val_start, val_end - val_start + 1);

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
            }

            auto user_opt = db.get_user(chat_id);
            if (user_opt) {
                auto user = *user_opt;
                if (message->text == "Оценки") {
                    auto now = std::chrono::system_clock::now();
                    auto in_time_t = std::chrono::system_clock::to_time_t(now);
                    std::tm tm = *std::localtime(&in_time_t);
                    int year = tm.tm_year + 1900;
                    int month = tm.tm_mon + 1;
                    bot.getApi().sendMessage(chat_id, "Выберите дату:", nullptr, 0, utils::create_calendar_keyboard("grades_", year, month));
                } else if (message->text == "Расписание") {
                    auto now = std::chrono::system_clock::now();
                    auto in_time_t = std::chrono::system_clock::to_time_t(now);
                    std::tm tm = *std::localtime(&in_time_t);
                    int year = tm.tm_year + 1900;
                    int month = tm.tm_mon + 1;
                    bot.getApi().sendMessage(chat_id, "Выберите дату:", nullptr, 0, utils::create_calendar_keyboard("schedule_", year, month));
                }
                }
        });

        bot.getEvents().onCallbackQuery([&bot, &db](TgBot::CallbackQuery::Ptr query) {
            long long chat_id = query->message->chat->id;
            std::string data = query->data;

            if (data == "IGNORE") {
                try { bot.getApi().answerCallbackQuery(query->id, ""); } catch(...) {}
                return;
            }

            std::string prefix = data.substr(0, data.find("_") + 1);
            std::string payload = data.substr(data.find("_") + 1);

            if (payload.substr(0, 4) == "NAV_" || payload.substr(0, 4) == "CAL_") {
                std::string nav_data = payload.substr(4); // YYYY-MM
                try {
                    int year = std::stoi(nav_data.substr(0, 4));
                    int month = std::stoi(nav_data.substr(5, 2));
                    
                    std::string header_text = (prefix == "grades_") ? "Выберите дату (Оценки):" : "Выберите дату (Расписание):";
                    bot.getApi().editMessageText(header_text, chat_id, query->message->messageId, "", "", nullptr, utils::create_calendar_keyboard(prefix, year, month));
                } catch(...) {}
                try { bot.getApi().answerCallbackQuery(query->id, ""); } catch(...) {}
                return;
            }

            std::string date = payload;
            if (payload.substr(0, 4) == "DAY_") {
                date = payload.substr(4); // YYYY-MM-DD
            }

            auto user_opt = db.get_user(chat_id);
            if (!user_opt) {
                bot.getApi().sendMessage(chat_id, "Не удалось найти ваши данные. Пожалуйста, попробуйте /start снова.");
                return;
            }
            auto user = *user_opt;

try { bot.getApi().answerCallbackQuery(query->id, "Загрузка..."); } catch(...) {}

            if (prefix == "grades_") {
                std::thread([chat_id, user, date, query_msg_id=query->message->messageId, prefix, &bot, &db]() {
                    try {
                        journal::JournalClient client(user.login, user.password);
                        client.set_cache(user.access_token, user.student_id);

                        if (client.auth_check()) {
                            // Update cache if changed
                            if (client.get_token() != user.access_token || client.get_student_id() != user.student_id) {
                                auto u_copy = user;
                                u_copy.access_token = client.get_token();
                                u_copy.student_id = client.get_student_id();
                                db.save_user(u_copy);
                            }

                            auto grades = client.get_grades(date);
                            std::string text;
                            if (grades.empty()) {
                                text = "Оценок за " + date + " нет.";
                            } else {
                                std::stringstream ss;
                                ss << "Оценки за " << date << ":\n\n";      
                                for (const auto& grade : grades) {
                                    ss << "🔹 " << grade.lesson << ": " << grade.value << " (" << grade.type << ")\n";
                                }
                                text = ss.str();
                                if (text.length() > 4096) text = text.substr(0, 4090) + "\n...";
                            }
                            try { bot.getApi().editMessageText(text, chat_id, query_msg_id, "", "", nullptr, utils::create_day_navigation_keyboard(prefix, date)); } catch(...) {}
                        } else {
                            try { bot.getApi().editMessageText("Ошибка авторизации. Попробуйте /start снова.", chat_id, query_msg_id); } catch(...) {}
                        }
                    } catch (const std::exception& e) { std::cerr << "Thread error: " << e.what() << std::endl; }
                }).detach();
            } else if (prefix == "schedule_") {
                 std::thread([chat_id, user, date, query_msg_id=query->message->messageId, prefix, &bot, &db]() {
                    try {
                        journal::JournalClient client(user.login, user.password);   
                        client.set_cache(user.access_token, user.student_id);

                        if (client.auth_check()) {
                            // Update cache if changed
                            if (client.get_token() != user.access_token || client.get_student_id() != user.student_id) {
                                auto u_copy = user;
                                u_copy.access_token = client.get_token();
                                u_copy.student_id = client.get_student_id();
                                db.save_user(u_copy);
                            }

                            auto schedule = client.get_schedule(date);
                            std::string text;
                            if (schedule.empty()) {
                                text = "Занятий на " + date + " нет.";
                            } else {
                                std::stringstream ss;
                                ss << "Расписание на " << date << ":\n\n";
                                for (const auto& item : schedule) {
                                    ss << "🕒 " << item.started_at << " - " << item.finished_at << "\n"
                                       << "   " << item.subject_name << "\n"        
                                       << "   📍 " << item.room_name << "\n\n";    
                                }
                                text = ss.str();
                                if (text.length() > 4096) text = text.substr(0, 4090) + "\n...";
                            }
                            try { bot.getApi().editMessageText(text, chat_id, query_msg_id, "", "", nullptr, utils::create_day_navigation_keyboard(prefix, date)); } catch(...) {}
                        } else {
                            try { bot.getApi().editMessageText("Ошибка авторизации. Попробуйте /start снова.", chat_id, query_msg_id); } catch(...) {}
                        }
                    } catch (const std::exception& e) { std::cerr << "Thread error: " << e.what() << std::endl; }
                }).detach();
            }
        });

        std::thread api_server([&db, bot_token]() {
            httplib::Server svr;
            
            // CORS middleware
            svr.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, x-telegram-initdata");
            });
            
            svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
                res.status = 204;
            });

            // POST /api/auth/login - Web client authentication
            svr.Post("/api/auth/login", [&db](const httplib::Request& req, httplib::Response& res) {
                res.set_header("Content-Type", "application/json");
                try {
                    auto body = nlohmann::json::parse(req.body);
                    std::string login = body.value("login", "");
                    std::string password = body.value("password", "");
                    
                    if (login.empty() || password.empty()) {
                        res.status = 400;
                        res.set_content(R"({"error":"Missing login or password"})", "application/json");
                        return;
                    }
                    
                    // Verify credentials with Journal API
                    journal::JournalClient client(login, password);
                    if (!client.auth_check()) {
                        res.status = 401;
                        res.set_content(R"({"error":"Invalid credentials"})", "application/json");
                        return;
                    }
                    
                    // Save user to DB with access token
                    db::UserRecord user;
                    user.telegram_id = 0;
                    user.login = login;
                    user.password = password;  // Stored for Journal API calls only
                    user.access_token = "token_" + std::to_string(std::time(nullptr));
                    user.refresh_token = "";
                    user.student_id = 0;
                    user.group_id = 0;
                    db.save_user(user);
                    
                    // Return response WITHOUT password
                    nlohmann::json response = {
                        {"access_token", user.access_token},
                        {"login", user.login},
                        {"student_id", user.student_id},
                        {"group_id", user.group_id}
                    };
                    res.set_content(response.dump(), "application/json");
                } catch (const std::exception& e) {
                    res.status = 400;
                    nlohmann::json error = {{"error", "Invalid request"}};
                    res.set_content(error.dump(), "application/json");
                }
            });

            // GET /api/schedule - Web client schedule
            svr.Get("/api/schedule", [&db](const httplib::Request& req, httplib::Response& res) {
                res.set_header("Content-Type", "application/json");
                try {
                    std::string date = req.get_param_value("date");
                    if (date.empty()) date = get_date_string(0);
                    
                    // Get user from request - for web client we'll use a demo mode
                    // In production, implement proper token validation
                    nlohmann::json schedule = nlohmann::json::array();
                    
                    // Demo data
                    schedule.push_back({
                        {"started_at", "09:00"},
                        {"finished_at", "10:30"},
                        {"subject_name", "Математика"},
                        {"room_name", "101"}
                    });
                    schedule.push_back({
                        {"started_at", "10:45"},
                        {"finished_at", "12:15"},
                        {"subject_name", "Информатика"},
                        {"room_name", "305"}
                    });
                    
                    res.set_content(schedule.dump(), "application/json");
                } catch (const std::exception& e) {
                    res.status = 500;
                    nlohmann::json error = {{"error", "Server error"}};
                    res.set_content(error.dump(), "application/json");
                }
            });

            // GET /api/grades - Web client grades
            svr.Get("/api/grades", [&db](const httplib::Request& req, httplib::Response& res) {
                res.set_header("Content-Type", "application/json");
                try {
                    std::string month_str = req.get_param_value("month");
                    std::string year_str = req.get_param_value("year");
                    
                    nlohmann::json grades = nlohmann::json::array();
                    
                    // Demo data
                    grades.push_back({
                        {"subject", "Математика"},
                        {"grade", 5},
                        {"date", "2024-04-15"}
                    });
                    grades.push_back({
                        {"subject", "Информатика"},
                        {"grade", 4},
                        {"date", "2024-04-14"}
                    });
                    grades.push_back({
                        {"subject", "Математика"},
                        {"grade", 5},
                        {"date", "2024-04-10"}
                    });
                    
                    res.set_content(grades.dump(), "application/json");
                } catch (const std::exception& e) {
                    res.status = 500;
                    nlohmann::json error = {{"error", "Server error"}};
                    res.set_content(error.dump(), "application/json");
                }
            });

            // GET /api/health - Health check
            svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
                res.set_header("Content-Type", "application/json");
                nlohmann::json response = {{"status", "ok"}};
                res.set_content(response.dump(), "application/json");
            });

            // POST /api/tma/auth - Telegram Mini App auth
            svr.Post("/api/tma/auth", [&db, bot_token](const httplib::Request& req, httplib::Response& res) {
                res.set_header("Content-Type", "application/json");
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
            
            // GET /api/tma/schedule - Telegram Mini App schedule
            svr.Get("/api/tma/schedule", [&db, bot_token](const httplib::Request& req, httplib::Response& res) {
                res.set_header("Content-Type", "application/json");
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

            std::cout << "Starting HTTP server on port 8080..." << std::endl;
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

        api_server.join();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
