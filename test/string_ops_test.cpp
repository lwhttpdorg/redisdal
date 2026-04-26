#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "gtest/gtest.h"
#include "janus/janus.hpp"

#include "test_env.hpp"

class StringOpsTest: public testing::Test {
protected:
    // Type aliases
    using key_type = std::string;
    using value_type = unsigned int;

    std::unique_ptr<janus::redis_connection> conn;
    std::unique_ptr<janus::redis_template<key_type, value_type>> tpl;
    std::set<key_type> keys_to_clean;
    janus::string_serializer<key_type> key_serializer{};
    janus::string_serializer<value_type> value_serializer{};

    StringOpsTest() {
        std::string redis_url = get_redis_connection_url();
        conn = std::make_unique<janus::redis_connection>(redis_url);
        tpl = std::make_unique<janus::redis_template<key_type, value_type>>(*conn, key_serializer, value_serializer);
    }

    void TearDown() override {
        for (const auto &key: keys_to_clean) {
            tpl->del(key);
        }
    }
};

TEST_F(StringOpsTest, SetAndGet) {
    key_type test_key = "test_string_set_get";
    keys_to_clean.insert(test_key);
    value_type test_value = 45678U;

    ASSERT_TRUE(tpl->ops_for_value().set(test_key, test_value)) << "SET operation failed.";

    std::optional<value_type> retrieved_val = tpl->ops_for_value().get(test_key);
    ASSERT_TRUE(retrieved_val) << "GET operation failed, key not found.";
    EXPECT_EQ(*retrieved_val, test_value) << "Retrieved value mismatch.";

    std::optional<value_type> missing_val = tpl->ops_for_value().get("non_existent_key");
    EXPECT_FALSE(missing_val) << "GET returned value for non-existent key.";
}

TEST_F(StringOpsTest, IncrAndDecr) {
    key_type test_key = "test_string_counter";
    keys_to_clean.insert(test_key);
    value_type initial_value = 100U;
    long long delta_incr = 15;
    long long delta_decr = 5;

    ASSERT_TRUE(tpl->ops_for_value().set(test_key, initial_value)) << "Initial SET for counter failed.";

    long long new_val_incr = tpl->ops_for_value().incr(test_key, delta_incr);
    EXPECT_EQ(new_val_incr, initial_value + delta_incr) << "INCR operation result mismatch.";

    long long final_val = tpl->ops_for_value().decr(test_key, delta_decr);
    EXPECT_EQ(final_val, initial_value + delta_incr - delta_decr) << "DECR operation result mismatch.";
}

TEST_F(StringOpsTest, GetAndSet) {
    key_type test_key = "test_string_get_set";
    keys_to_clean.insert(test_key);
    value_type initial_value = 500U;
    value_type new_value = 999U;

    ASSERT_TRUE(tpl->ops_for_value().set(test_key, initial_value));

    std::optional<value_type> old_val = tpl->ops_for_value().get_and_set(test_key, new_value);
    ASSERT_TRUE(old_val) << "get_and_set failed to retrieve old value.";
    EXPECT_EQ(*old_val, initial_value) << "get_and_set retrieved incorrect old value.";

    std::optional<value_type> current_val = tpl->ops_for_value().get(test_key);
    ASSERT_TRUE(current_val);
    EXPECT_EQ(*current_val, new_value) << "get_and_set failed to set new value.";
}

TEST_F(StringOpsTest, Append) {
    key_type test_key = "test_string_append";
    keys_to_clean.insert(test_key);
    value_type val_a = 10; // Serialized to "10" (length 2)
    value_type val_b = 20; // Serialized to "20" (length 2)

    ASSERT_TRUE(tpl->ops_for_value().set(test_key, val_a));

    unsigned long long expected_length = std::to_string(val_a).length() + std::to_string(val_b).length();
    unsigned long long new_length = tpl->ops_for_value().append(test_key, val_b);

    EXPECT_EQ(new_length, expected_length) << "append returned incorrect new length.";

    if (std::optional<value_type> appended_val = tpl->ops_for_value().get(test_key)) {
        EXPECT_EQ(*appended_val, 1020U) << "Appended value did not deserialize as expected.";
    }
}
