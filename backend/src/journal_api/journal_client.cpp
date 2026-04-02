#include "journal_client.hpp"
#include <cpr/cpr.h>
#include <iostream>

namespace journal {

JournalClient::JournalClient() {}

LoginResult JournalClient::login(const std::string& username, const std::string& password) {
    LoginResult result{false, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, "{}"_json};

    nlohmann::json payload = {
        {"application_key", kAppKey},
        {"id_city", nullptr},
        {"password", password},
        {"username", username}
    };

    cpr::Response r = cpr::Post(
        cpr::Url{kBaseUrl + "/auth/login"},
        cpr::Header{
            {"accept", "application/json, text/plain, */*"},
            {"accept-language", "ru_RU, ru"},
            {"authorization", "Bearer null"},
            {"content-type", "application/json"},
            {"origin", "https://journal.top-academy.ru"},
            {"referer", "https://journal.top-academy.ru/"},
            {"sec-ch-ua", "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\""},
            {"sec-ch-ua-mobile", "?0"},
            {"sec-ch-ua-platform", "\"Windows\""},
            {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"}
        },
        cpr::Body{payload.dump()},
        cpr::VerifySsl(false)
    );

    try {
        result.raw_response = nlohmann::json::parse(r.text);
    } catch (const nlohmann::json::parse_error& e) {
        result.error_message = "JSON parse error: " + std::string(e.what());
        return result;
    }

    if (r.status_code == 200) {
        result.success = true;
        auto& json = result.raw_response;
        
        if (json.contains("access_token") && !json["access_token"].is_null()) {
            result.access_token = json["access_token"].get<std::string>();
        }
        if (json.contains("refresh_token") && !json["refresh_token"].is_null()) {
            result.refresh_token = json["refresh_token"].get<std::string>();
        }
        if (json.contains("expires_in_access") && !json["expires_in_access"].is_null()) {
            result.expires_in_access = json["expires_in_access"].get<long long>();
        }
        if (json.contains("user_role") && !json["user_role"].is_null()) {
            result.user_role = json["user_role"].get<std::string>();
        }
    } else {
        result.error_message = "HTTP Error " + std::to_string(r.status_code) + ". Message: " + r.text;
    }

    return result;
}

ScheduleResult JournalClient::get_schedule(const std::string& access_token, const std::string& date_filter) {
    ScheduleResult result{false, {}, std::nullopt, "{}"_json};

    cpr::Response r = cpr::Get(
        cpr::Url{kBaseUrl + "/schedule/operations/get-by-date"},
        cpr::Parameters{{"date_filter", date_filter}},
        cpr::Header{
            {"accept", "application/json, text/plain, */*"},
            {"accept-language", "ru_RU, ru"},
            {"authorization", "Bearer " + access_token},
            {"origin", "https://journal.top-academy.ru"},
            {"referer", "https://journal.top-academy.ru/"},
            {"sec-ch-ua", "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\""},
            {"sec-ch-ua-mobile", "?0"},
            {"sec-ch-ua-platform", "\"Windows\""},
            {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"}
        },
        cpr::VerifySsl(false)
    );

    try {
        result.raw_response = nlohmann::json::parse(r.text);
    } catch (const nlohmann::json::parse_error& e) {
        result.error_message = "JSON parse error: " + std::string(e.what());
        return result;
    }

    if (r.status_code == 200) {
        result.success = true;
        
        auto& json_arr = result.raw_response;
        if (json_arr.is_array()) {
            for (const auto& item : json_arr) {
                Lesson lesson;
                
                if (item.contains("date") && !item["date"].is_null()) {
                    lesson.date = item["date"].get<std::string>();
                }
                if (item.contains("lesson_number") && !item["lesson_number"].is_null()) {
                    lesson.lesson_number = item["lesson_number"].get<int>();
                }
                if (item.contains("started_at") && !item["started_at"].is_null()) {
                    lesson.started_at = item["started_at"].get<std::string>();
                }
                if (item.contains("finished_at") && !item["finished_at"].is_null()) {
                    lesson.finished_at = item["finished_at"].get<std::string>();
                }
                if (item.contains("teacher_name") && !item["teacher_name"].is_null()) {
                    lesson.teacher_name = item["teacher_name"].get<std::string>();
                }
                if (item.contains("subject_name") && !item["subject_name"].is_null()) {
                    lesson.subject_name = item["subject_name"].get<std::string>();
                }
                if (item.contains("room_name") && !item["room_name"].is_null()) {
                    lesson.room_name = item["room_name"].get<std::string>();
                }
                
                result.lessons.push_back(lesson);
            }
        }
    } else {
        result.error_message = "HTTP Error " + std::to_string(r.status_code) + ". Message: " + r.text;
    }

    return result;
}

HomeworkResult JournalClient::get_homeworks(const std::string& access_token, int group_id, int page, int status, int type) {
    HomeworkResult result{false, {}, std::nullopt, "{}"_json};

    cpr::Response r = cpr::Get(
        cpr::Url{kBaseUrl + "/homework/operations/list"},
        cpr::Parameters{
            {"page", std::to_string(page)},
            {"id_group", std::to_string(group_id)},
            {"status", std::to_string(status)},
            {"type", std::to_string(type)}
        },
        cpr::Header{
            {"accept", "application/json, text/plain, */*"},
            {"accept-language", "ru_RU, ru"},
            {"authorization", "Bearer " + access_token},
            {"origin", "https://journal.top-academy.ru"},
            {"referer", "https://journal.top-academy.ru/"},
            {"sec-ch-ua", "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\""},
            {"sec-ch-ua-mobile", "?0"},
            {"sec-ch-ua-platform", "\"Windows\""},
            {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"}
        },
        cpr::VerifySsl(false)
    );

    try {
        result.raw_response = nlohmann::json::parse(r.text);
    } catch (const nlohmann::json::parse_error& e) {
        result.error_message = "JSON parse error: " + std::string(e.what());
        return result;
    }

    if (r.status_code == 200) {
        result.success = true;
        
        auto& json_arr = result.raw_response;
        if (json_arr.is_array()) {
            for (const auto& item : json_arr) {
                Homework hw;
                
                if (item.contains("id") && !item["id"].is_null()) {
                    hw.id = item["id"].get<int>();
                }
                if (item.contains("theme") && !item["theme"].is_null()) {
                    hw.theme = item["theme"].get<std::string>();
                }
                if (item.contains("fio_teach") && !item["fio_teach"].is_null()) {
                    hw.fio_teach = item["fio_teach"].get<std::string>();
                }
                if (item.contains("name_spec") && !item["name_spec"].is_null()) {
                    hw.name_spec = item["name_spec"].get<std::string>();
                }
                if (item.contains("creation_time") && !item["creation_time"].is_null()) {
                    hw.creation_time = item["creation_time"].get<std::string>();
                }
                if (item.contains("completion_time") && !item["completion_time"].is_null()) {
                    hw.completion_time = item["completion_time"].get<std::string>();
                }
                if (item.contains("file_path") && !item["file_path"].is_null()) {
                    hw.file_path = item["file_path"].get<std::string>();
                }
                if (item.contains("status") && !item["status"].is_null()) {
                    hw.status = item["status"].get<int>();
                }
                
                if (item.contains("homework_stud") && !item["homework_stud"].is_null()) {
                    auto& hw_stud = item["homework_stud"];
                    HomeworkStud stud;
                    if (hw_stud.contains("mark") && !hw_stud["mark"].is_null()) {
                        stud.mark = hw_stud["mark"].get<int>();
                    }
                    if (hw_stud.contains("file_path") && !hw_stud["file_path"].is_null()) {
                        stud.file_path = hw_stud["file_path"].get<std::string>();
                    }
                    hw.homework_stud = stud;
                }
                
                result.homeworks.push_back(hw);
            }
        }
    } else {
        result.error_message = "HTTP Error " + std::to_string(r.status_code) + ". Message: " + r.text;
    }

    return result;
}

UserInfoResult JournalClient::get_user_info(const std::string& access_token) {
    UserInfoResult result{false, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, "{}"_json};

    cpr::Response r = cpr::Get(
        cpr::Url{kBaseUrl + "/settings/user-info"},
        cpr::Header{
            {"accept", "application/json, text/plain, */*"},
            {"accept-language", "ru_RU, ru"},
            {"authorization", "Bearer " + access_token},
            {"origin", "https://journal.top-academy.ru"},
            {"referer", "https://journal.top-academy.ru/"},
            {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"}
        },
        cpr::VerifySsl(false)
    );

    try {
        result.raw_response = nlohmann::json::parse(r.text);
    } catch (const nlohmann::json::parse_error& e) {
        result.error_message = "JSON parse error: " + std::string(e.what());
        return result;
    }

    if (r.status_code == 200) {
        result.success = true;
        
        auto& json = result.raw_response;
        
        if (json.contains("student_id") && !json["student_id"].is_null()) {
            result.student_id = json["student_id"].get<int>();
        }
        if (json.contains("current_group_id") && !json["current_group_id"].is_null()) {
            result.current_group_id = json["current_group_id"].get<int>();
        }
        if (json.contains("full_name") && !json["full_name"].is_null()) {
            result.full_name = json["full_name"].get<std::string>();
        }
        if (json.contains("group_name") && !json["group_name"].is_null()) {
            result.group_name = json["group_name"].get<std::string>();
        }
        if (json.contains("photo") && !json["photo"].is_null()) {
            result.photo_url = json["photo"].get<std::string>();
        }
    } else {
        result.error_message = "HTTP Error " + std::to_string(r.status_code) + ". Message: " + r.text;
    }

    return result;
}

StudentVisitsResult JournalClient::get_student_visits(const std::string& access_token) {
    StudentVisitsResult result{false, {}, std::nullopt, "{}"_json};

    cpr::Response r = cpr::Get(
        cpr::Url{kBaseUrl + "/progress/operations/student-visits"},
        cpr::Header{
            {"accept", "application/json, text/plain, */*"},
            {"accept-language", "ru_RU, ru"},
            {"authorization", "Bearer " + access_token},
            {"origin", "https://journal.top-academy.ru"},
            {"referer", "https://journal.top-academy.ru/"},
            {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"}
        },
        cpr::VerifySsl(false)
    );

    try {
        result.raw_response = nlohmann::json::parse(r.text);
    } catch (const nlohmann::json::parse_error& e) {
        result.error_message = "JSON parse error: " + std::string(e.what());
        return result;
    }

    if (r.status_code == 200) {
        result.success = true;
        
        auto& json_arr = result.raw_response;
        if (json_arr.is_array()) {
            for (const auto& item : json_arr) {
                StudentVisit visit;
                
                if (item.contains("date_visit") && !item["date_visit"].is_null()) {
                    visit.date_visit = item["date_visit"].get<std::string>();
                }
                if (item.contains("lesson_number") && !item["lesson_number"].is_null()) {
                    visit.lesson_number = item["lesson_number"].get<int>();
                }
                if (item.contains("status_was") && !item["status_was"].is_null()) {
                    visit.status_was = item["status_was"].get<int>();
                }
                if (item.contains("spec_id") && !item["spec_id"].is_null()) {
                    visit.spec_id = item["spec_id"].get<int>();
                }
                if (item.contains("teacher_name") && !item["teacher_name"].is_null()) {
                    visit.teacher_name = item["teacher_name"].get<std::string>();
                }
                if (item.contains("spec_name") && !item["spec_name"].is_null()) {
                    visit.spec_name = item["spec_name"].get<std::string>();
                }
                if (item.contains("lesson_theme") && !item["lesson_theme"].is_null()) {
                    visit.lesson_theme = item["lesson_theme"].get<std::string>();
                }
                
                auto read_mark = [](const nlohmann::json& obj, const std::string& key) -> std::optional<int> {
                    if (obj.contains(key) && !obj[key].is_null()) {
                        return obj[key].get<int>();
                    }
                    return std::nullopt;
                };

                visit.control_work_mark = read_mark(item, "control_work_mark");
                visit.home_work_mark = read_mark(item, "home_work_mark");
                visit.lab_work_mark = read_mark(item, "lab_work_mark");
                visit.class_work_mark = read_mark(item, "class_work_mark");
                visit.practical_work_mark = read_mark(item, "practical_work_mark");
                visit.final_work_mark = read_mark(item, "final_work_mark");
                
                result.visits.push_back(visit);
            }
        }
    } else {
        result.error_message = "HTTP Error " + std::to_string(r.status_code) + ". Message: " + r.text;
    }

    return result;
}

}