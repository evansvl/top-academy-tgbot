#include "journal_client.hpp"
#include <cpr/cpr.h>
#include <iostream>

namespace journal {

const std::string kBaseUrl = "https://msapi.top-academy.ru/api/v2";
const std::string kAppKey = "6a56a5df2667e65aab73ce76d1dd737f7d1faef9c52e8b8c55ac75f565d8e8a6";

JournalClient::JournalClient(const std::string& login, const std::string& password)
    : login_(login), password_(password) {}

bool JournalClient::refresh_token() {
    nlohmann::json payload = {
        {"application_key", kAppKey},
        {"id_city", nullptr},
        {"password", password_},
        {"username", login_}
    };

    cpr::Header headers = {
        {"accept", "application/json, text/plain, */*"},
        {"accept-language", "ru_RU, ru"},
        {"content-type", "application/json"},
        {"origin", "https://journal.top-academy.ru"},
        {"priority", "u=1, i"},
        {"referer", "https://journal.top-academy.ru/"},
        {"sec-ch-ua", "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\""},
        {"sec-ch-ua-mobile", "?0"},
        {"sec-ch-ua-platform", "\"Windows\""},
        {"sec-fetch-dest", "empty"},
        {"sec-fetch-mode", "cors"},
        {"sec-fetch-site", "same-site"},
        {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"}
    };
    cpr::Response r = cpr::Post(cpr::Url{kBaseUrl + "/auth/login"}, cpr::Body{payload.dump()}, headers, cpr::VerifySsl(false));

    if (r.status_code == 200) {
        try {
            auto json = nlohmann::json::parse(r.text);
            if (json.contains("access_token")) {
                access_token_ = json["access_token"];

                auto user_info = make_request("/settings/user-info", {});
                if(user_info != nullptr && user_info.contains("student_id")) {
                    student_id_ = user_info["student_id"];
                }
                
                return true;
            }
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "JSON parse error in refresh_token: " << e.what() << std::endl;
        }
    }
    return false;
}

bool JournalClient::auth_check() {
    if (access_token_.empty()) {
        return refresh_token();
    }
    auto response = make_request("/settings/user-info", {});
    return response != nullptr;
}

nlohmann::json JournalClient::make_request(const std::string& endpoint, const nlohmann::json& params) {
    cpr::Parameters cpr_params;
    for (auto& el : params.items()) {
        if (el.value().is_string()) {
            cpr_params.Add({el.key(), el.value().get<std::string>()});
        } else {
            cpr_params.Add({el.key(), el.value().dump()});
        }
    }

    cpr::Header headers = {
        {"accept", "application/json, text/plain, */*"},
        {"accept-language", "ru_RU, ru"},
        {"authorization", "Bearer " + access_token_},
        {"origin", "https://journal.top-academy.ru"},
        {"priority", "u=1, i"},
        {"referer", "https://journal.top-academy.ru/"},
        {"sec-ch-ua", "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\""},
        {"sec-ch-ua-mobile", "?0"},
        {"sec-ch-ua-platform", "\"Windows\""},
        {"sec-fetch-dest", "empty"},
        {"sec-fetch-mode", "cors"},
        {"sec-fetch-site", "same-site"},
        {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"}
    };
    cpr::Response r = cpr::Get(cpr::Url{kBaseUrl + endpoint}, cpr_params, headers, cpr::VerifySsl(false));

    if (r.status_code == 200) {
        try {
            return nlohmann::json::parse(r.text);
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "JSON parse error in make_request: " << e.what() << std::endl;
            return nullptr;
        }
    } else if (r.status_code == 401) {
        if (refresh_token()) {
            return make_request(endpoint, params);
        }
    }
    return nullptr;
}

std::vector<Lesson> JournalClient::get_schedule(const std::string& date) {
    std::vector<Lesson> lessons;
    auto response = make_request("/schedule/operations/get-by-date", {{"date_filter", date}});
    
    if (response != nullptr && response.is_array()) {
        for (const auto& item : response) {
            Lesson lesson;
            lesson.started_at = item.value("started_at", "");
            lesson.finished_at = item.value("finished_at", "");
            lesson.subject_name = item.value("subject_name", "");
            lesson.room_name = item.value("room_name", "");
            lessons.push_back(lesson);
        }
    }
    return lessons;
}

std::vector<Grade> JournalClient::get_grades(const std::string& date) {
    std::vector<Grade> grades;
    if (student_id_ == 0) {
        if (!auth_check()) return grades;
    }

    auto response = make_request("/progress/operations/student-visits", {{"student_id", student_id_}});

    if (response != nullptr && response.is_array()) {
        for (const auto& item : response) {
            if (item.contains("date_visit") && item["date_visit"] == date) {
                bool found_mark = false;
                
                auto add_mark = [&](const char* key, const std::string& type_name) {
                    if (item.contains(key) && !item[key].is_null()) {
                        grades.push_back({item.value("spec_name", ""), std::to_string(item[key].get<int>()), type_name});
                        found_mark = true;
                    }
                };

                add_mark("class_work_mark", "Классная работа");
                add_mark("control_work_mark", "Контрольная");
                add_mark("final_work_mark", "Экзамен");
                add_mark("home_work_mark", "Домашняя");
                add_mark("lab_work_mark", "Лабораторная");
                add_mark("practical_work_mark", "Практика");

                
            }
        }
    }
    return grades;
}

}