#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"
#include "redisdal/redisdal.hpp"
#include "test_env.hpp"

class BinaryArgsTest: public testing::Test {
protected:
    redisdal::redis_client conn{get_redis_connection_url()};
    std::string prefix;
    std::string key;
    const std::string value{"value\0tail", 10};
    const std::string field{"field\0tail", 10};

    void SetUp() override {
        prefix = std::string("redisdal:binary:") + testing::UnitTest::GetInstance()->current_test_info()->name();
        key = prefix + std::string("\0key", 4);
        conn.command("DEL", {key, prefix});
    }

    void TearDown() override {
        // Raw argv cleanup works even when an operation under test truncates its key.
        conn.command("DEL", {key, prefix});
    }
};

TEST_F(BinaryArgsTest, StringWritesPreserveKeysAndValues) {
    EXPECT_TRUE(conn.set(key, value));
    EXPECT_EQ(conn.command("GET", {key}).get_string(), value);
    EXPECT_EQ(conn.get(key), value);
    EXPECT_FALSE(conn.command("EXISTS", {prefix}).get_integer().value());
    EXPECT_EQ(conn.ping(value), value);
    EXPECT_FALSE(conn.set_not_exists(key, "replacement"));
    EXPECT_EQ(conn.getset(key, field), value);
    EXPECT_EQ(conn.append(key, value), field.size() + value.size());
    EXPECT_EQ(conn.command("GET", {key}).get_string(), field + value);
    EXPECT_EQ(conn.del(key), 1);
    EXPECT_TRUE(conn.set_not_exists(key, value));
    EXPECT_EQ(conn.get(key), value);
    EXPECT_TRUE(conn.set_ex(key, field, 60));
    EXPECT_EQ(conn.get(key), field);
    EXPECT_GT(conn.ttl(key), 0);
    EXPECT_TRUE(conn.set_px(key, value, 60000));
    EXPECT_EQ(conn.get(key), value);
    EXPECT_GT(conn.pttl(key), 0);
}

TEST_F(BinaryArgsTest, KeyOperationsAndScanPreserveBinaryPatterns) {
    conn.command("SET", {key, "10"});
    EXPECT_TRUE(conn.exists(key));
    EXPECT_EQ(conn.type(key), "string");
    EXPECT_EQ(conn.incr(key, 5), 15);
    EXPECT_EQ(conn.decr(key, 3), 12);
    EXPECT_EQ(conn.command("GET", {key}).get_string(), "12");
    EXPECT_TRUE(conn.expire(key, 60));
    EXPECT_GT(conn.ttl(key), 0);
    EXPECT_TRUE(conn.pexpire(key, 60000));
    EXPECT_GT(conn.pttl(key), 0);
    EXPECT_TRUE(conn.persist(key));
    EXPECT_EQ(conn.ttl(key), -1);
    std::unordered_set<std::string> keys;
    conn.keys(key, keys);
    EXPECT_EQ(keys, (std::unordered_set<std::string>{key}));
    std::unordered_set<std::string> scanned;
    uint64_t cursor = 0;
    do {
        auto page = conn.scan(cursor, key, 100);
        scanned.insert(page.keys.begin(), page.keys.end());
        cursor = page.cursor;
    } while (cursor != 0);
    EXPECT_EQ(scanned, (std::unordered_set<std::string>{key}));
    EXPECT_EQ(conn.del(key), 1);
    EXPECT_FALSE(conn.exists(key));
}

TEST_F(BinaryArgsTest, HashOperationsPreserveFieldsAndValues) {
    EXPECT_TRUE(conn.hset(key, field, value));
    EXPECT_EQ(conn.command("HGET", {key, field}).get_string(), value);
    EXPECT_EQ(conn.hget(key, field), value);
    EXPECT_EQ(conn.hgetall(key), (std::unordered_map<std::string, std::string>{{field, value}}));
    EXPECT_EQ(conn.hkeys(key), (std::vector<std::string>{field}));
    EXPECT_EQ(conn.hvals(key), (std::vector<std::string>{value}));
    std::unordered_map<std::string, std::string> scanned;
    uint64_t cursor = 0;
    do {
        cursor = conn.hscan(key, cursor, field, 100, scanned);
    } while (cursor != 0);
    EXPECT_EQ(scanned, (std::unordered_map<std::string, std::string>{{field, value}}));
    EXPECT_EQ(conn.hdel(key, field), 1);
    EXPECT_FALSE(conn.hget(key, field));
}

TEST_F(BinaryArgsTest, ListOperationsPreserveElements) {
    EXPECT_EQ(conn.lpush(key, value), 1);
    EXPECT_EQ(conn.rpush(key, field), 2);
    EXPECT_EQ(conn.command("LINDEX", {key, "0"}).get_string(), value);
    EXPECT_EQ(conn.llen(key), 2);
    EXPECT_EQ(conn.lindex(key, -1), field);
    EXPECT_EQ(conn.lrange(key, 0, -1), (std::vector<std::string>{value, field}));
    EXPECT_EQ(conn.lpop(key), value);
    EXPECT_EQ(conn.rpop(key), field);
    conn.lpush(key, std::vector<std::string>{value});
    EXPECT_EQ(conn.lpop(key, 2), (std::vector<std::string>{value}));
    conn.rpush(key, std::vector<std::string>{field});
    EXPECT_EQ(conn.rpop(key, 2), (std::vector<std::string>{field}));
}

TEST_F(BinaryArgsTest, SetOperationsPreserveMembers) {
    EXPECT_EQ(conn.sadd(key, {value}), 1);
    EXPECT_TRUE(conn.sismember(key, value));
    EXPECT_FALSE(conn.sismember(key, "value"));
    EXPECT_EQ(conn.scard(key), 1);
    EXPECT_EQ(conn.smembers(key), (std::vector<std::string>{value}));
    EXPECT_EQ(conn.spop(key), value);
    EXPECT_EQ(conn.scard(key), 0);
}

TEST_F(BinaryArgsTest, SortedSetOperationsPreserveMembers) {
    EXPECT_EQ(conn.zadd(key, {{value, 1.5}}), 1);
    EXPECT_EQ(conn.zscore(key, value), 1.5);
    EXPECT_FALSE(conn.zscore(key, "value"));
    EXPECT_EQ(conn.zincrby(key, 0.25, value), 1.75);
    EXPECT_EQ(conn.zrange(key, 0, -1), (std::vector<std::string>{value}));
    EXPECT_EQ(conn.zrevrange(key, 0, -1), (std::vector<std::string>{value}));
    const std::vector<std::pair<std::string, double>> expected{{value, 1.75}};
    EXPECT_EQ(conn.zrange_withscores(key, 0, -1), expected);
    EXPECT_EQ(conn.zrevrange_withscores(key, 0, -1), expected);
    EXPECT_EQ(conn.zrem(key, {value}), 1);
}

TEST_F(BinaryArgsTest, LuaAndRawCommandsPreserveArguments) {
    const std::string script = "return redis.call('SET', KEYS[1], ARGV[1])";
    conn.eval(script, {key}, {value});
    EXPECT_EQ(conn.command("GET", {key}).get_string(), value);
    const auto sha = conn.script_load(script);
    conn.eval_sha1(sha, {key}, {field});
    EXPECT_EQ(conn.get(key), field);
}

TEST_F(BinaryArgsTest, TemplatePreservesBinarySerialization) {
    redisdal::string_redis_template tpl(conn);
    EXPECT_TRUE(tpl.ops_for_value().set(key, value));
    EXPECT_EQ(conn.command("GET", {key}).get_string(), value);
    EXPECT_EQ(tpl.ops_for_value().get(key), value);
}
