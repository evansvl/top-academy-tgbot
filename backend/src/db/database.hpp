#pragma once

#include <string>
#include <optional>
#include <sqlite3.h>
#include <mutex>

namespace db {

struct UserRecord {
    long long telegram_id;
    std::string login;
    std::string password;
    std::string access_token;
    std::string refresh_token;
    int student_id;
    int group_id;
    std::string full_name;
    std::string photo_url;
};

class Database {
public:
    Database(const std::string& db_path);
    ~Database();

    bool init();

    bool save_user(const UserRecord& user);

    std::optional<UserRecord> get_user(long long telegram_id);

    bool delete_user(long long telegram_id);

private:
    sqlite3* db_ = nullptr;
    std::mutex db_mutex_;
};

}