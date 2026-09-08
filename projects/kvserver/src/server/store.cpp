#include "src/server/store.h"

#include <string>

static size_t hash_str(const std::string& s) {
    size_t hash_value = 0;
    for (size_t i=0; i<s.size(); ++i) {
        hash_value = hash_value*31 + static_cast<size_t>(static_cast<unsigned char>(s[i])); // 每次乘一个奇数来记录不同的位置权重
    }

    return hash_value;
}

bool Store::get(const std::string& k, std::string& v) const {
    size_t index = hash_str(k) % store.size();
    for (const auto& node : store[index]) {
        if (node.key == k) {
            v = node.value;
            return true;
        }
    }
    return false;
}

void Store::set(const std::string& k, const std::string& v) {
    size_t index = hash_str(k) % store.size();
    for (auto& node : store[index]) {
        if (node.key == k) {
            node.value = v;
            return ;
        }
    }
    store[index].push_back(Node(k, v));
}

bool Store::erase(const std::string& k) {
    size_t index = hash_str(k) % store.size();
    for (auto it = store[index].begin(); it != store[index].end(); ++it) {
        if (it->key == k) {
            store[index].erase(it);
            return true;
        }
    }
    return false;
}

bool Store::exists(const std::string& k) const {
    size_t index = hash_str(k) % store.size();
    for (const auto& node : store[index]) {
        if (node.key == k) {
            return true;
        }
    }
    return false;
}