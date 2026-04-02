#pragma once

#include <string>
#include <map>

namespace auth {

class TgAuth {
public:
    TgAuth(const std::string& bot_token);

    bool validate_init_data(const std::string& init_data);

    long long extract_user_id(const std::string& init_data);

private:
    std::string bot_token_;

    std::map<std::string, std::string> parse_query_string(const std::string& query);
    
    std::string hmac_sha256(const std::string& key, const std::string& data);
    std::string hex_encode(const unsigned char* data, size_t length);
};

}