#include "database.hpp"
#include <iostream>
#include <stdexcept>

namespace db {

Database::Database(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Cannot open database: " + db_path);
    }
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool Database::init() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            telegram_id INTEGER PRIMARY KEY,
            access_token TEXT NOT NULL,
            refresh_token TEXT NOT NULL,
            student_id INTEGER,
            group_id INTEGER,
            full_name TEXT,
            photo_url TEXT
        );
    )";

    char* err_msg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::cerr << "SQL error during init: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool Database::save_user(const UserRecord& user) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        INSERT INTO users (telegram_id, access_token, refresh_token, student_id, group_id, full_name, photo_url)
        VALUES (?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(telegram_id) DO UPDATE SET
            access_token=excluded.access_token,
            refresh_token=excluded.refresh_token,
            student_id=excluded.student_id,
            group_id=excluded.group_id,
            full_name=excluded.full_name,
            photo_url=excluded.photo_url;
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "SQL prepare error (save_user): " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_int64(stmt, 1, user.telegram_id);
    sqlite3_bind_text(stmt, 2, user.access_token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, user.refresh_token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, user.student_id);
    sqlite3_bind_int(stmt, 5, user.group_id);
    sqlite3_bind_text(stmt, 6, user.full_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, user.photo_url.c_str(), -1, SQLITE_TRANSIENT);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

std::optional<UserRecord> Database::get_user(long long telegram_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "SELECT access_token, refresh_token, student_id, group_id, full_name, photo_url FROM users WHERE telegram_id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "SQL prepare error (get_user): " << sqlite3_errmsg(db_) << std::endl;
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, telegram_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        UserRecord user;
        user.telegram_id = telegram_id;
        
        user.access_token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        user.refresh_token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        
        user.student_id = sqlite3_column_int(stmt, 2);
        user.group_id = sqlite3_column_int(stmt, 3);
        
        const char* full_name_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        user.full_name = full_name_ptr ? full_name_ptr : "";
        
        const char* photo_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        user.photo_url = photo_ptr ? photo_ptr : "";

        sqlite3_finalize(stmt);
        return user;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool Database::delete_user(long long telegram_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM users WHERE telegram_id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, telegram_id);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

}