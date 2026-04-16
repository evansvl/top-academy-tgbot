#pragma once

#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>

namespace journal {

struct Grade {
    std::string lesson;
    std::string value;
    std::string type;
};

struct Lesson {
    std::string started_at;
    std::string finished_at;
    std::string subject_name;
    std::string room_name;
};

class JournalClient {
public:
    JournalClient(const std::string& login, const std::string& password);
    
    bool auth_check();
    std::vector<Lesson> get_schedule(const std::string& date);
    std::vector<Grade> get_grades(const std::string& date);

    void set_cache(const std::string& token, int student_id) { access_token_ = token; student_id_ = student_id; }
    std::string get_token() const { return access_token_; }
    int get_student_id() const { return student_id_; }

private:
    std::string login_;
    std::string password_;
    std::string access_token_;
    int student_id_ = 0;

    bool refresh_token();
    nlohmann::json make_request(const std::string& endpoint, const nlohmann::json& params);
};

}