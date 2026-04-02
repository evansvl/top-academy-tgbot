#include "tma_auth.hpp"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <nlohmann/json.hpp>

std::string url_decode(const std::string& input) {
    std::string ret;
    char ch;
    int i, ii;
    for (i = 0; i < input.length(); i++) {
        if (input[i] == '%') {
            sscanf(input.substr(i + 1, 2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            ret += ch;
            i = i + 2;
        } else if (input[i] == '+') {
            ret += ' ';
        } else {
            ret += input[i];
        }
    }
    return ret;
}

namespace auth {

TgAuth::TgAuth(const std::string& bot_token) : bot_token_(bot_token) {}

std::map<std::string, std::string> TgAuth::parse_query_string(const std::string& query) {
    std::map<std::string, std::string> result;
    std::istringstream stream(query);
    std::string pair;

    while (std::getline(stream, pair, '&')) {
        auto delimiter = pair.find('=');
        if (delimiter != std::string::npos) {
            std::string key = pair.substr(0, delimiter);
            std::string value = pair.substr(delimiter + 1);
            result[key] = url_decode(value);
        }
    }

    return result;
}

std::string TgAuth::hex_encode(const unsigned char* data, size_t length) {
    std::ostringstream oss;
    for (size_t i = 0; i < length; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::string TgAuth::hmac_sha256(const std::string& key, const std::string& data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;

    HMAC(EVP_sha256(), 
         key.c_str(), key.size(), 
         reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), 
         hash, &length);

    return hex_encode(hash, length);
}

bool TgAuth::validate_init_data(const std::string& init_data) {
    auto parsed_data = parse_query_string(init_data);

    if (parsed_data.find("hash") == parsed_data.end()) {
        return false;
    }
    std::string received_hash = parsed_data["hash"];
    parsed_data.erase("hash");

    std::vector<std::string> data_check_vec;
    for (const auto& [k, v] : parsed_data) {
        data_check_vec.push_back(k + "=" + v);
    }

    std::string data_check_string;
    for (size_t i = 0; i < data_check_vec.size(); ++i) {
        data_check_string += data_check_vec[i];
        if (i < data_check_vec.size() - 1) {
            data_check_string += "\n";
        }
    }

    const std::string constant_key = "WebAppData";
    
    unsigned char secret_key[EVP_MAX_MD_SIZE];
    unsigned int sk_len = 0;

    HMAC(EVP_sha256(),
         constant_key.c_str(), constant_key.size(),
         reinterpret_cast<const unsigned char*>(bot_token_.c_str()), bot_token_.size(),
         secret_key, &sk_len);

    unsigned char final_hash[EVP_MAX_MD_SIZE];
    unsigned int fh_len = 0;

    HMAC(EVP_sha256(),
         secret_key, sk_len,
         reinterpret_cast<const unsigned char*>(data_check_string.c_str()), data_check_string.size(),
         final_hash, &fh_len);

    std::string generated_hash = hex_encode(final_hash, fh_len);

    return generated_hash == received_hash;
}

long long TgAuth::extract_user_id(const std::string& init_data) {
    auto parsed_data = parse_query_string(init_data);
    if (parsed_data.find("user") != parsed_data.end()) {
        std::string user_json_str = parsed_data["user"];
        try {
            auto json = nlohmann::json::parse(user_json_str);
            if (json.contains("id") && json["id"].is_number()) {
                return json["id"].get<long long>();
            }
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

}