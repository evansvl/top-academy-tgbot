#pragma once

#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>

namespace journal {

struct LoginResult {
    bool success;
    std::optional<std::string> access_token;
    std::optional<std::string> refresh_token;
    std::optional<long long> expires_in_access;
    std::optional<std::string> user_role;
    std::optional<std::string> error_message;
    nlohmann::json raw_response;
};

struct Lesson {
    std::string date;
    int lesson_number;
    std::string started_at;
    std::string finished_at;
    std::string teacher_name;
    std::string subject_name;
    std::string room_name;
};

struct ScheduleResult {
    bool success;
    std::vector<Lesson> lessons;
    std::optional<std::string> error_message;
    nlohmann::json raw_response;
};

struct HomeworkStud {
    std::optional<int> mark;
    std::optional<std::string> file_path;
};

struct Homework {
    int id;
    std::string theme;
    std::string fio_teach;
    std::string name_spec;
    std::string creation_time;
    std::string completion_time;
    std::optional<std::string> file_path;
    int status;
    std::optional<HomeworkStud> homework_stud;
};

struct HomeworkResult {
    bool success;
    std::vector<Homework> homeworks;
    std::optional<std::string> error_message;
    nlohmann::json raw_response;
};

struct UserInfoResult {
    bool success;
    std::optional<int> student_id;
    std::optional<int> current_group_id;
    std::optional<std::string> full_name;
    std::optional<std::string> group_name;
    std::optional<std::string> photo_url;
    std::optional<std::string> error_message;
    nlohmann::json raw_response;
};

struct StudentVisit {
    std::string date_visit;
    int lesson_number;
    int status_was;
    int spec_id;
    std::string teacher_name;
    std::string spec_name;
    std::string lesson_theme;
    std::optional<int> control_work_mark;
    std::optional<int> home_work_mark;
    std::optional<int> lab_work_mark;
    std::optional<int> class_work_mark;
    std::optional<int> practical_work_mark;
    std::optional<int> final_work_mark;
};

struct StudentVisitsResult {
    bool success;
    std::vector<StudentVisit> visits;
    std::optional<std::string> error_message;
    nlohmann::json raw_response;
};

class JournalClient {
public:
    JournalClient();
    
    LoginResult login(const std::string& username, const std::string& password);

    ScheduleResult get_schedule(const std::string& access_token, const std::string& date_filter);

    HomeworkResult get_homeworks(const std::string& access_token, int group_id, int page = 1, int status = 3, int type = 0);

    UserInfoResult get_user_info(const std::string& access_token);

    StudentVisitsResult get_student_visits(const std::string& access_token);

private:
    const std::string kBaseUrl = "https://msapi.top-academy.ru/api/v2";
    const std::string kAppKey = "6a56a5df2667e65aab73ce76d1dd737f7d1faef9c52e8b8c55ac75f565d8e8a6";
};

}