#pragma once
#include <vector>
#include <string>
#include <list>

static constexpr size_t kInitialBuckets = 32;

class Store {
    public:
    Store(size_t num_buckets = kInitialBuckets) : store(num_buckets) {}

    bool get(const std::string& k, std::string& v) const;
    void set(const std::string& k, const std::string& v);
    bool erase(const std::string& k);
    bool exists(const std::string& k) const;

    private:
    struct Node {
        Node (std::string k, std::string v) : key(k), value(v) {}
        std::string key;
        std::string value;
    };
    std::vector<std::list<Node>> store;
};