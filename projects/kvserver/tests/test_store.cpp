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

TEST(store, grow_rehash) {
    // 2 桶起步,a=2:阈值 count > 2*m-1,于是第 5/9/17 个键各触发一次扩容(m: 2→4→8→16),
    // 24 个键能跨三次 rehash——旧键扩容后仍须按"hash % 新桶数"找得回来。
    Store store(2);
    for (int i=0; i<24; ++i) {
        store.set("k" + std::to_string(i), "v" + std::to_string(i));
    }

    // 全部能取回,值未丢
    std::string out;
    for (int i=0; i<24; ++i) {
        CHECK(store.get("k" + std::to_string(i), out) == true);
        CHECK(out == "v" + std::to_string(i));
    }

    // 扩容后覆盖已存在键仍生效(命中,不新增节点)
    store.set("k7", "overwritten");
    CHECK(store.get("k7", out) == true && out == "overwritten");
    CHECK(store.exists("k7") == true);

    // erase 一半(偶数索引),确认删掉的不再 exists、留下的没被误伤
    for (int i=0; i<24; i+=2) {
        CHECK(store.erase("k" + std::to_string(i)) == true);
    }
    for (int i=0; i<24; ++i) {
        std::string k = "k" + std::to_string(i);
        if (i % 2 == 0) {
            CHECK(store.exists(k) == false);
        } else {
            CHECK(store.exists(k) == true);
            CHECK(store.get(k, out) == true);
            CHECK(out == (i == 7 ? "overwritten" : "v" + std::to_string(i)));
        }
    }

    // 删除计数下降后,继续插入新键仍正常工作
    store.set("extra", "x");
    CHECK(store.get("extra", out) == true && out == "x");
}

TEST(store, expire_ttl) {
    Store store;
    std::string out;

    // 缺键:ttl 回 -2,expire 无从设
    CHECK(store.ttl("nope") == -2);
    CHECK(store.expire("nope", 100) == false);

    // 无期限的键:ttl 回 -1
    store.set("k", "v");
    CHECK(store.ttl("k") == -1);

    // 设期限成功,剩余秒数落在 (0,100];GET 仍读得到(期限是"将来",不算过期)
    CHECK(store.expire("k", 100) == true);
    long long t = store.ttl("k");
    CHECK(t > 0 && t <= 100);
    CHECK(store.get("k", out) == true && out == "v");

    // set() 整值替换 → 清期限(SET/GETSET 路径)
    store.set("k", "v2");
    CHECK(store.ttl("k") == -1);
    CHECK(store.get("k", out) == true && out == "v2");

    // set_keep_ttl() 就地改值 → 保留期限(INCR/APPEND 路径)
    store.expire("k", 100);
    store.set_keep_ttl("k", "v3");
    t = store.ttl("k");
    CHECK(t > 0 && t <= 100);
    CHECK(store.get("k", out) == true && out == "v3");

    // 负秒/0 视为"期限落在过去"→ 直接删键
    store.set("d", "x");
    CHECK(store.expire("d", -1) == true);
    CHECK(store.exists("d") == false);
    store.set("d2", "x");
    CHECK(store.expire("d2", 0) == true);
    CHECK(store.exists("d2") == false);
}