#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <tgbot/tgbot.h>

namespace utils {

    inline std::string get_current_date_str() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d");
        return ss.str();
    }

    inline TgBot::InlineKeyboardMarkup::Ptr create_date_keyboard(const std::string& callback_prefix) {
        TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
        std::vector<TgBot::InlineKeyboardButton::Ptr> row;

        auto now = std::chrono::system_clock::now();
        
        for (int i = 0; i < 3; ++i) {
            auto date = now + std::chrono::hours(24 * i);
            auto in_time_t = std::chrono::system_clock::to_time_t(date);
            std::tm tm = *std::localtime(&in_time_t);

            std::stringstream date_ss;
            date_ss << std::put_time(&tm, "%d.%m");
            
            std::stringstream callback_ss;
            callback_ss << callback_prefix << std::put_time(&tm, "%Y-%m-%d");

            TgBot::InlineKeyboardButton::Ptr button(new TgBot::InlineKeyboardButton);
            button->text = date_ss.str();
            button->callbackData = callback_ss.str();
            row.push_back(button);
        }
        keyboard->inlineKeyboard.push_back(row);
        return keyboard;
    }

}