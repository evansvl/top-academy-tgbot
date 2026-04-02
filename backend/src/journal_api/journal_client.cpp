#include "journal_client.hpp"
#include <cpr/cpr.h>
#include <iostream>

namespace journal {

const std::string kBaseUrl = "https://journal.top-academy.ru/api";
const std::string kAppKey = "13a7d38f-4435-4051-a323-c639c2f90123";

JournalClient::JournalClient(const std::string& login, const std::string& password)
    : login_(login), password_(password) {}

bool JournalClient::refresh_token() {
    nlohmann::json payload = {
        {"application_key", kAppKey},
        {"id_city", nullptr},
        {"password", password_},
        {"username", login_}
    };

    cpr::Response r = cpr::Post(cpr::Url{kBaseUrl + "/auth/login"}, cpr::Body{payload.dump()}, cpr::Header{{"Content-Type", "application/json"}}, cpr::VerifySsl(false));

    if (r.status_code == 200) {
        try {
            auto json = nlohmann::json::parse(r.text);
            if (json.contains("access_token")) {
                access_token_ = json["access_token"];
                
                auto user_info = make_request("/user/info", {});
                if(user_info != nullptr && user_info.contains("student_id")) {
                    student_id_ = user_info["student_id"];
                    return true;
                }
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
    auto response = make_request("/user/info", {});
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

    cpr::Response r = cpr::Get(cpr::Url{kBaseUrl + endpoint}, cpr_params, cpr::Header{{"Authorization", "Bearer " + access_token_}}, cpr::VerifySsl(false));

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

    auto response = make_request("/progress/operations/student-visits-by-date", {{"student_id", student_id_}, {"date", date}});

    if (response != nullptr && response.is_array()) {
        for (const auto& item : response) {
            if (item.contains("control_work_mark") && !item["control_work_mark"].is_null()) {
                grades.push_back({item.value("spec_name", ""), std::to_string(item["control_work_mark"].get<int>()), "Контрольная"});
            }
            if (item.contains("home_work_mark") && !item["home_work_mark"].is_null()) {
                grades.push_back({item.value("spec_name", ""), std::to_string(item["home_work_mark"].get<int>()), "Домашняя"});
            }
            if (item.contains("lab_work_mark") && !item["lab_work_mark"].is_null()) {
                grades.push_back({item.value("spec_name", ""), std::to_string(item["lab_work_mark"].get<int>()), "Лабораторная"});
            }
        }
    }
    return grades;
}

}