#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <tgbot/tgbot.h>

namespace utils {

    inline std::string get_current_date_str();

    inline int get_days_in_month(int year, int month) {
        if (month == 2) {
            bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
            return is_leap ? 29 : 28;
        }
        if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
        return 31;
    }

    inline int get_first_weekday(int year, int month) {
        std::tm time_in = {};
        time_in.tm_year = year - 1900;
        time_in.tm_mon = month - 1;
        time_in.tm_mday = 1;
        time_in.tm_isdst = -1;
        std::time_t time_temp = std::mktime(&time_in);
        const std::tm* time_out = std::localtime(&time_temp);
        int wday = time_out->tm_wday;
        return (wday == 0) ? 6 : wday - 1;
    }

    inline std::string get_month_name(int month) {
        const char* months[] = {"Январь", "Февраль", "Март", "Апрель", "Май", "Июнь",
                                "Июль", "Август", "Сентябрь", "Октябрь", "Ноябрь", "Декабрь"};
        if (month >= 1 && month <= 12) return months[month - 1];
        return "";
    }

    inline TgBot::InlineKeyboardMarkup::Ptr create_calendar_keyboard(const std::string& callback_prefix, int year, int month) {
        TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
        
        int prev_month = month - 1;
        int prev_year = year;
        if (prev_month < 1) { prev_month = 12; prev_year--; }

        int next_month = month + 1;
        int next_year = year;
        if (next_month > 12) { next_month = 1; next_year++; }

        std::vector<TgBot::InlineKeyboardButton::Ptr> header_row;
        TgBot::InlineKeyboardButton::Ptr btn_prev(new TgBot::InlineKeyboardButton);
        btn_prev->text = "<";
        std::ostringstream ss_prev;
        ss_prev << callback_prefix << "NAV_" << prev_year << "-" << std::setfill('0') << std::setw(2) << prev_month;
        btn_prev->callbackData = ss_prev.str();
        header_row.push_back(btn_prev);

        TgBot::InlineKeyboardButton::Ptr btn_title(new TgBot::InlineKeyboardButton);
        btn_title->text = get_month_name(month) + " " + std::to_string(year);
        btn_title->callbackData = "IGNORE";
        header_row.push_back(btn_title);

        TgBot::InlineKeyboardButton::Ptr btn_next(new TgBot::InlineKeyboardButton);
        btn_next->text = ">";
        std::ostringstream ss_next;
        ss_next << callback_prefix << "NAV_" << next_year << "-" << std::setfill('0') << std::setw(2) << next_month;
        btn_next->callbackData = ss_next.str();
        header_row.push_back(btn_next);

        keyboard->inlineKeyboard.push_back(header_row);

        std::vector<TgBot::InlineKeyboardButton::Ptr> dow_row;
        const char* dows[] = {"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
        for (int i = 0; i < 7; ++i) {
            TgBot::InlineKeyboardButton::Ptr btn(new TgBot::InlineKeyboardButton);
            btn->text = dows[i];
            btn->callbackData = "IGNORE";
            dow_row.push_back(btn);
        }
        keyboard->inlineKeyboard.push_back(dow_row);

        int days_in_month = get_days_in_month(year, month);
        int first_weekday = get_first_weekday(year, month);

        std::vector<TgBot::InlineKeyboardButton::Ptr> current_row;
        for (int i = 0; i < first_weekday; ++i) {
            TgBot::InlineKeyboardButton::Ptr btn(new TgBot::InlineKeyboardButton);
            btn->text = " ";
            btn->callbackData = "IGNORE";
            current_row.push_back(btn);
        }

        for (int day = 1; day <= days_in_month; ++day) {
            TgBot::InlineKeyboardButton::Ptr btn(new TgBot::InlineKeyboardButton);
            btn->text = std::to_string(day);
            std::ostringstream ss_day;
            ss_day << callback_prefix << "DAY_" << year << "-" << std::setfill('0') << std::setw(2) << month << "-" << std::setfill('0') << std::setw(2) << day;
            btn->callbackData = ss_day.str();
            current_row.push_back(btn);

            if (current_row.size() == 7) {
                keyboard->inlineKeyboard.push_back(current_row);
                current_row.clear();
            }
        }

        if (!current_row.empty()) {
            while (current_row.size() < 7) {
                TgBot::InlineKeyboardButton::Ptr btn(new TgBot::InlineKeyboardButton);
                btn->text = " ";
                btn->callbackData = "IGNORE";
                current_row.push_back(btn);
            }
            keyboard->inlineKeyboard.push_back(current_row);
        }

        return keyboard;
    }

    
    inline std::string get_shifted_date(const std::string& date_str, int days_offset) {
        if (date_str.length() < 10) return get_current_date_str();
        int year = std::stoi(date_str.substr(0, 4));
        int month = std::stoi(date_str.substr(5, 2));
        int day = std::stoi(date_str.substr(8, 2));

        std::tm time_in = {};
        time_in.tm_year = year - 1900;
        time_in.tm_mon = month - 1;
        time_in.tm_mday = day + days_offset;
        time_in.tm_isdst = -1;
        
        std::time_t time_temp = std::mktime(&time_in);
        std::tm* time_out = std::localtime(&time_temp);

        std::stringstream ss;
        ss << std::put_time(time_out, "%Y-%m-%d");
        return ss.str();
    }

    inline TgBot::InlineKeyboardMarkup::Ptr create_day_navigation_keyboard(const std::string& callback_prefix, const std::string& current_date_str) {
        TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
        std::vector<TgBot::InlineKeyboardButton::Ptr> row;

        TgBot::InlineKeyboardButton::Ptr btn_prev(new TgBot::InlineKeyboardButton);
        btn_prev->text = "< Пред."; 
        btn_prev->callbackData = callback_prefix + "DAY_" + get_shifted_date(current_date_str, -1);
        row.push_back(btn_prev);

        TgBot::InlineKeyboardButton::Ptr btn_cal(new TgBot::InlineKeyboardButton);
        btn_cal->text = "Календарь";
        btn_cal->callbackData = callback_prefix + "CAL_" + current_date_str.substr(0, 7); 
        row.push_back(btn_cal);

        TgBot::InlineKeyboardButton::Ptr btn_next(new TgBot::InlineKeyboardButton);
        btn_next->text = "След. >"; 
        btn_next->callbackData = callback_prefix + "DAY_" + get_shifted_date(current_date_str, 1);
        row.push_back(btn_next);

        keyboard->inlineKeyboard.push_back(row);
        return keyboard;
    }

    inline std::string get_current_date_str() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d");
        return ss.str();
    }
}
