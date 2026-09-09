#pragma once
#include <vector>
#include <string>
#include <list>
#include <optional>
#include <chrono>

static constexpr size_t kInitialBuckets = 32;
static constexpr size_t kMaxLoadFactor = 2; // 负载因子 a = 元素个数 ÷ 桶数

class Store {
    public:
    Store(size_t num_buckets = kInitialBuckets) : store(num_buckets), elements_count(0) {}

    bool get(const std::string& k, std::string& v);
    void set(const std::string& k, const std::string& v);
    void set_keep_ttl(const std::string& k, const std::string& v);
    bool erase(const std::string& k);
    bool exists(const std::string& k);
    bool expire(const std::string& k, long long seconds);
    long long ttl(const std::string& k);

    private:
    struct Node {
        Node (std::string k, std::string v) : key(k), value(v) {}
        std::string key;
        std::string value;
        // 过期时间，std::optional 可能含有一个值，也可能不含有值（即 expire_at == nullopt）， steady_clock 只增不减，不受系统时间影响
        std::optional<std::chrono::steady_clock::time_point> expire_at;
    };
    std::vector<std::list<Node>> store;
    size_t elements_count;

    // 这个节点是否过期
    inline bool is_expired(const Node& node) const {
        return node.expire_at != std::nullopt && *(node.expire_at) <= std::chrono::steady_clock::now();
    }

    void rehash();
};