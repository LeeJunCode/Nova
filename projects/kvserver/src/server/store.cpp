#include "src/server/store.h"

#include <string>
#include <limits>
#include <utility>

static size_t hash_str(const std::string& s) {
    size_t hash_value = 0;
    for (size_t i=0; i<s.size(); ++i) {
        hash_value = hash_value*31 + static_cast<size_t>(static_cast<unsigned char>(s[i])); // 每次乘一个奇数来记录不同的位置权重
    }

    return hash_value;
}

void Store::rehash() {
    size_t new_buckets = this->store.size() << 1; // 2倍扩充 有可能越界 size_t

    std::vector<std::list<Node>> new_store(new_buckets);

    for (auto& l : this->store) {
        for (auto& node : l) {
            size_t index = hash_str(node.key) % new_buckets;
            new_store[index].push_back(std::move(node));
        }
    }

    store = std::move(new_store);
}

bool Store::get(const std::string& k, std::string& v) {
    size_t index = hash_str(k) % store.size();
    for (auto it = store[index].begin(); it != store[index].end(); ++it) {
        if (it->key == k) {
            if (is_expired(*it)) {
                store[index].erase(it);
                --elements_count;
                return false;
            }
            v = it->value;
            return true;
        }
    }
    return false;
}

void Store::set(const std::string& k, const std::string& v) {
    if (elements_count > kMaxLoadFactor * store.size() - 1) rehash();
    size_t index = hash_str(k) % store.size();
    for (auto& node : store[index]) {
        if (node.key == k) {
            node.value = v;
            node.expire_at = std::nullopt;
            return ;
        }
    }
    store[index].push_back(Node(k, v));
    ++elements_count;
}

void Store::set_keep_ttl(const std::string& k, const std::string& v) {
    if (elements_count > kMaxLoadFactor * store.size() - 1) rehash();
    size_t index = hash_str(k) % store.size();
    for (auto& node : store[index]) {
        if (node.key == k) {
            node.value = v;
            return ;
        }
    }
    store[index].push_back(Node(k, v));
    ++elements_count;
}

bool Store::erase(const std::string& k) {
    size_t index = hash_str(k) % store.size();
    for (auto it = store[index].begin(); it != store[index].end(); ++it) {
        if (it->key == k) {
            store[index].erase(it);
            --elements_count;
            return true;
        }
    }
    return false;
}

bool Store::exists(const std::string& k) {
    size_t index = hash_str(k) % store.size();
    for (auto it = store[index].begin(); it != store[index].end(); ++it) {
        if (it->key == k) {
            if (is_expired(*it)) {
                store[index].erase(it);
                --elements_count;
                return false;
            }
            return true;
        }
    }
    return false;
}

bool Store::expire(const std::string& k, long long seconds) {
    if (!exists(k)) return false;
    if (seconds <= 0) return erase(k);
    size_t index = hash_str(k) % store.size();
    for (auto it = store[index].begin(); it != store[index].end(); ++it) {
        if (it->key == k) {
            it->expire_at = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
            break;
        }
    }
    return true;
}

long long Store::ttl(const std::string& k){
    if (!exists(k)) return -2;
    size_t index = hash_str(k) % store.size();
    for (auto it = store[index].begin(); it != store[index].end(); ++it) {
        if (it->key == k) {
            if (it->expire_at == std::nullopt) return -1;
            return std::chrono::duration_cast<std::chrono::seconds>(*(it->expire_at) - std::chrono::steady_clock::now()).count();
        }
    }
    return -1;
}
