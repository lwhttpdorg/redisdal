#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "redisdal/redisdal.hpp"
#include "test_env.hpp"

// Define a custom object for hash mapping demonstration
struct User {
    long long id{};
    std::string username;
    int level{};
};

// Create a hash mapper for your custom type
class user_hash_mapper {
public:
    static std::unordered_map<std::string, std::string> to_hash(const User &user) {
        return {{"id", std::to_string(user.id)}, {"username", user.username}, {"level", std::to_string(user.level)}};
    }

    static User from_hash(const std::unordered_map<std::string, std::string> &hash) {
        User user;
        if (hash.count("id")) {
            user.id = std::stoll(hash.at("id"));
        }
        if (hash.count("username")) {
            user.username = hash.at("username");
        }
        if (hash.count("level")) {
            user.level = std::stoi(hash.at("level"));
        }
        return user;
    }
};

int main() {
    // 1. Create the underlying connection using a URL from the environment
    std::string redis_url = get_redis_connection_url();
    auto conn = std::make_shared<redisdal::redis_connection>(redis_url);

    // 2. Create serializers for different data types
    auto string_serializer = std::make_shared<redisdal::string_serializer<std::string>>();
    auto int_serializer = std::make_shared<redisdal::string_serializer<long long>>();

    // Create a template for string keys and string values
    redisdal::redis_template<std::string, std::string> string_tpl(*conn, *string_serializer, *string_serializer);
    // Create a template for string keys and integer values
    redisdal::redis_template<std::string, long long> int_tpl(*conn, *string_serializer, *int_serializer);

    // === String Operations ===
    std::cout << "\n--- String Operations ---" << std::endl;
    auto &value_ops = string_tpl.ops_for_value();
    value_ops.set("my_string", "hello");
    // Use the integer template for INCR operations
    int_tpl.ops_for_value().incr("my_counter", 10);
    if (auto val = value_ops.get("my_string")) {
        std::cout << "GET my_string: " << *val << std::endl;
    }

    // === Hash Operations ===
    std::cout << "\n--- Hash Operations ---" << std::endl;
    auto &hash_ops = string_tpl.ops_for_hash();
    hash_ops.hset("my_hash", {{"field1", "value1"}, {"field2", "value2"}});
    if (auto h_val = hash_ops.hget("my_hash", "field1")) {
        std::cout << "HGET my_hash field1: " << *h_val << std::endl;
    }

    // === Custom Hash Object Operations ===
    std::cout << "\n--- Custom Hash (User Object) ---" << std::endl;
    auto &user_hash_ops = string_tpl.ops_for_hash();

    User user_to_save = {101, "Sandro", 99};
    user_hash_ops.hset("user:101", user_hash_mapper::to_hash(user_to_save));
    std::cout << "Saved user 'Sandro' to hash user:101" << std::endl;

    auto fetched_hash = user_hash_ops.hgetall("user:101");
    if (!fetched_hash.empty()) {
        User fetched_user = user_hash_mapper::from_hash(fetched_hash);
        std::cout << "Fetched user: id=" << fetched_user.id << ", username=" << fetched_user.username
                  << ", level=" << fetched_user.level << std::endl;
    }

    // === List Operations ===
    std::cout << "\n--- List Operations ---" << std::endl;
    auto &list_ops = string_tpl.ops_for_list();
    list_ops.rpush("my_list", {"A", "B", "C"});
    auto l_range = list_ops.lrange("my_list", 0, -1);
    std::cout << "LRANGE my_list: ";
    for (const auto &item: l_range) {
        std::cout << item << " ";
    }
    std::cout << std::endl;

    // === Set Operations ===
    std::cout << "\n--- Set Operations ---" << std::endl;
    auto &set_ops = string_tpl.ops_for_set();
    set_ops.sadd("my_set", {"member1", "member2", "member3"});
    if (set_ops.sismember("my_set", "member2")) {
        std::cout << "SISMEMBER: my_set contains 'member2'" << std::endl;
    }

    // === Sorted Set (ZSet) Operations ===
    std::cout << "\n--- Sorted Set Operations ---" << std::endl;
    auto &zset_ops = string_tpl.ops_for_zset();
    zset_ops.zadd("my_zset", {{"player1", 100.0}, {"player2", 250.5}, {"player3", 50.0}});
    auto z_range = zset_ops.zrange_withscores("my_zset", 0, -1);
    std::cout << "ZRANGE my_zset (with scores):" << std::endl;
    for (const auto &[member, score]: z_range) {
        std::cout << "  " << member << ": " << score << std::endl;
    }

    // === Lua Scripting ===
    std::cout << "\n--- Lua Scripting ---" << std::endl;
    const std::string lua_script = "return redis.call('HSET', KEYS[1], ARGV[1], ARGV[2])";
    std::string sha1;
    try {
        sha1 = conn->script_load(lua_script);
        std::cout << "Script loaded, SHA1: " << sha1 << std::endl;
    }
    catch (const std::exception &e) {
        std::cerr << "Failed to load script: " << e.what() << std::endl;
        return 1;
    }

    // Execute using EVALSHA for better performance
    try {
        conn->eval_sha1(sha1, {"my_hash"}, {"field3", "from_script"});
        std::cout << "EVALSHA successful." << std::endl;
    }
    catch (const redisdal::no_script_error &e) {
        // If Redis cleared its script cache, fall back to EVAL
        std::cerr << "NOSCRIPT error, falling back to EVAL: " << e.what() << std::endl;
        conn->eval(lua_script, {"my_hash"}, {"field3", "from_script"});
    }

    if (auto script_val = hash_ops.hget("my_hash", "field3")) {
        std::cout << "Value set by script: " << *script_val << std::endl;
    }

    // === Generic Key Operations ===
    std::cout << "\n--- Generic Key Operations ---" << std::endl;
    const std::string generic_key = "my_generic_key";
    value_ops.set(generic_key, "some value");

    // Check if a key exists
    if (string_tpl.exists(generic_key)) {
        std::cout << "EXISTS: Key '" << generic_key << "' exists." << std::endl;
    }

    // Set an expiration time (in seconds)
    string_tpl.expire(generic_key, 120);
    if (auto ttl = string_tpl.ttl(generic_key); ttl > 0) {
        std::cout << "TTL: Key '" << generic_key << "' will expire in " << ttl << " seconds." << std::endl;
    }

    // Check the data type of the key
    std::cout << "TYPE: The data type of key '" << generic_key << "' is " << string_tpl.type(generic_key) << "."
              << std::endl;

    // === Final Cleanup ===
    std::cout << "\n--- Cleaning up all keys ---" << std::endl;
    long long deleted_count = string_tpl.del(
        {"my_string", "my_counter", "my_hash", "user:101", "my_list", "my_set", "my_zset", "my_generic_key"});
    std::cout << "Deleted " << deleted_count << " keys." << std::endl;

    return 0;
}
