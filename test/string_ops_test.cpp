#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "janus/janus.hpp"

#include "test_env.hpp"

class string_operations_test: public testing::Test {
protected:
	// Type aliases
	using key_type = std::string;
	using value_type = unsigned int;

	std::shared_ptr<janus::kv_connection> connection;
	std::shared_ptr<janus::serializer<key_type>> k_serializer;
	std::shared_ptr<janus::serializer<value_type>> v_serializer;
	std::unique_ptr<janus::redis_template<key_type, value_type>> tpl;

	void SetUp() override {
		// 1. Retrieve connection parameters from environment variables
		std::string redis_url = get_redis_connection_url();

		// 2. Create underlying connection
		connection = std::make_shared<janus::redis_connection>(redis_url);

		// 3. Create Serializers
		k_serializer = std::make_shared<janus::string_serializer<key_type>>();
		v_serializer = std::make_shared<janus::string_serializer<value_type>>();

		// 4. Construct redis_template
		tpl = std::make_unique<janus::redis_template<key_type, value_type>>(*connection, *k_serializer, *v_serializer);

		// 5. Clean up test key
		clear_test_keys();
	}

	void TearDown() override {
		// 6. Clean up test key
		if (tpl) {
			clear_test_keys();
		}
	}

	// Helper to clean keys
	void clear_test_keys() const {
		tpl->del("test_string_set_get");
		tpl->del("test_string_counter");
		tpl->del("test_string_get_set");
		tpl->del("test_string_append");
	}

	// Helper function to get String (Value) operations interface
	[[nodiscard]] auto &value_ops() const {
		return tpl->ops_for_value();
	}
};

// --- Test Cases ---

TEST_F(string_operations_test, set_and_get) {
	key_type test_key = "test_string_set_get";
	value_type test_value = 45678U;

	// Test SET
	ASSERT_TRUE(value_ops().set(test_key, test_value)) << "SET operation failed.";

	// Test GET
	std::optional<value_type> retrieved_val = value_ops().get(test_key);
	ASSERT_TRUE(retrieved_val) << "GET operation failed, key not found.";
	EXPECT_EQ(*retrieved_val, test_value) << "Retrieved value mismatch.";

	// Test GET (key does not exist)
	std::optional<value_type> missing_val = value_ops().get("non_existent_key");
	EXPECT_FALSE(missing_val) << "GET returned value for non-existent key.";
}

TEST_F(string_operations_test, incr_and_decr) {
	key_type test_key = "test_string_counter";
	value_type initial_value = 100U;
	long long delta_incr = 15;
	long long delta_decr = 5;

	// Initialize counter
	ASSERT_TRUE(value_ops().set(test_key, initial_value)) << "Initial SET for counter failed.";

	// Test INCR
	long long new_val_incr = value_ops().incr(test_key, delta_incr);
	EXPECT_EQ(new_val_incr, initial_value + delta_incr) << "INCR operation result mismatch.";

	// Test DECR
	long long final_val = value_ops().decr(test_key, delta_decr);
	EXPECT_EQ(final_val, initial_value + delta_incr - delta_decr) << "DECR operation result mismatch.";
}

TEST_F(string_operations_test, get_and_set) {
	key_type test_key = "test_string_get_set";
	value_type initial_value = 500U;
	value_type new_value = 999U;

	// 1. Set initial value
	ASSERT_TRUE(value_ops().set(test_key, initial_value));

	// 2. GET_AND_SET: Retrieve old value and set new value
	std::optional<value_type> old_val = value_ops().get_and_set(test_key, new_value);
	ASSERT_TRUE(old_val) << "get_and_set failed to retrieve old value.";
	EXPECT_EQ(*old_val, initial_value) << "get_and_set retrieved incorrect old value.";

	// 3. Verify new value
	std::optional<value_type> current_val = value_ops().get(test_key);
	ASSERT_TRUE(current_val);
	EXPECT_EQ(*current_val, new_value) << "get_and_set failed to set new value.";
}

TEST_F(string_operations_test, append) {
	key_type test_key = "test_string_append";
	value_type val_a = 10; // Serialized to "10" (length 2)
	value_type val_b = 20; // Serialized to "20" (length 2)

	// 1. Set initial value ("10")
	ASSERT_TRUE(value_ops().set(test_key, val_a));

	// 2. APPEND ("10" + "20" -> "1020")
	unsigned long long expected_length = std::to_string(val_a).length() + std::to_string(val_b).length();
	unsigned long long new_length = value_ops().append(test_key, val_b);

	EXPECT_EQ(new_length, expected_length) << "append returned incorrect new length.";

	// 3. Optional: Verify the concatenated value (assuming "1020" deserializes to 1020U)
	if (std::optional<value_type> appended_val = value_ops().get(test_key)) {
		EXPECT_EQ(*appended_val, 1020U) << "Appended value did not deserialize as expected.";
	}
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
