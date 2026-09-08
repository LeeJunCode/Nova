#include "tests/test_kit.h"
#include "src/server/store.h"

TEST(store, get) {
    Store store;
    std::string k = "act";
    std::string v = "this is a act";
    store.set(k, v);
    std::string result;
    CHECK(store.get(k, result) == true);
    CHECK(result == v);
}

TEST(store, set) {
    Store store;
    std::string k = "act";
    std::string v = "this is a act";
    store.set(k, v);
    CHECK(store.exists(k) == true);
}

TEST(store, erase) {
    Store store;
    std::string k = "act";
    std::string v = "this is a act";
    store.set(k, v);
    CHECK(store.exists(k) == true);
    store.erase(k);
    CHECK(store.exists(k) == false);
}

TEST(store, get_missing) {
    Store store;
    std::string v = "sentinel";
    CHECK(store.get("nope", v) == false);
    CHECK(v == "sentinel");   // 证明 get 没偷偷改写 v
}

TEST(store, set_overwrite) {
    Store store;
    std::string k = "act";
    store.set(k, "old");
    store.set(k, "new");      // 第二次命中,应覆盖而非新增
    std::string out;
    CHECK(store.get(k, out) == true);
    CHECK(out == "new");
}

TEST(store, null_value) {
    Store store;
    store.set("k", "");
    std::string out;
    CHECK(store.get("k", out) == true);
    CHECK(out == "");
}

TEST(store, erase_not_hit) {
    Store store;
    CHECK(store.exists("nope") == false);
    store.set("k", "v");
    CHECK(store.erase("nope") == false);       // 未命中返回 false
    CHECK(store.exists("k") == true);          // 别人不受影响
    std::string out;
    CHECK(store.get("k", out) == true && out == "v");
}

TEST(store, force_collision) {
    Store store(1);                    // 一个桶:所有 key 挤同一条链
    store.set("a","va");
    store.set("b","vb");
    store.set("c","vc");
    std::string out;
    CHECK(store.get("a", out) == true && out == "va");
    CHECK(store.get("b", out) == true && out == "vb");
    CHECK(store.get("c", out) == true && out == "vc");
    CHECK(store.erase("b") == true);   // 删链中间那个
    CHECK(store.get("c", out) == true && out == "vc");
    CHECK(store.exists("a") == true);  // 邻居没被误伤
    CHECK(store.exists("c") == true);
    CHECK(store.exists("b") == false);
}