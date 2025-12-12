#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"
#include "janus/janus.hpp"

#include "test_env.hpp"

class hash_operations_test: public testing::Test {
protected:
	// Type aliases (K and V are both std::string)
	using key_type = std::string;
	using value_type = std::string;
	using hash_map_type = std::unordered_map<key_type, value_type>;
	using optional_hash_map_type = std::unordered_map<key_type, std::optional<value_type>>;

	const key_type TEST_KEY = "test_hash_map";

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
		tpl->del(TEST_KEY);
	}

	// Helper function to get Hash operations interface
	[[nodiscard]] auto &hash_ops() const {
		return tpl->ops_for_hash();
	}
};

// --- Test Cases ---

TEST_F(hash_operations_test, hset_hget_single) {
	const key_type field_key = "field_name_1";
	const value_type field_value = "value_data_A";

	// 1. Test HSET
	// returns true if the field was newly created or updated successfully.
	EXPECT_TRUE(hash_ops().hset(TEST_KEY, field_key, field_value)) << "HSET failed for a new field.";

	// 2. Test HGET
	std::optional<value_type> retrieved_val = hash_ops().hget(TEST_KEY, field_key);
	ASSERT_TRUE(retrieved_val) << "HGET failed to retrieve the field.";
	EXPECT_EQ(*retrieved_val, field_value) << "Retrieved value mismatch.";

	// 3. Test HGET on non-existent field
	std::optional<value_type> missing_val = hash_ops().hget(TEST_KEY, "non_existent_field");
	EXPECT_FALSE(missing_val) << "HGET returned value for a non-existent field.";
}

TEST_F(hash_operations_test, hset_hget_multiple) {
	hash_map_type data_to_set = {{"f1", "v1"}, {"f2", "v2"}, {"f3", "v3"}};

	// 1. Test HSET (multiple fields)
	EXPECT_TRUE(hash_ops().hset(TEST_KEY, data_to_set)) << "HSET multiple fields failed.";

	// 2. Test HGETALL
	hash_map_type retrieved_data = hash_ops().hgetall(TEST_KEY);
	EXPECT_EQ(retrieved_data.size(), 3) << "HGETALL returned incorrect number of fields.";
	EXPECT_EQ(retrieved_data["f2"], "v2") << "HGETALL retrieved incorrect value.";
}

TEST_F(hash_operations_test, hget_batch_hmget) {
	// Setup initial data
	hash_map_type initial_data = {{"a", "1"}, {"b", "2"}, {"c", "3"}};
	hash_ops().hset(TEST_KEY, initial_data);

	// Prepare map for query (includes one existing and one non-existent field)
	optional_hash_map_type query_map = {
		{"a", std::nullopt}, {"b", std::nullopt}, {"d", std::nullopt} // Non-existent
	};

	// 1. Test HGET (batch/HMGET equivalent)
	hash_ops().hget(TEST_KEY, query_map);

	// 2. Verify results
	EXPECT_EQ(query_map.size(), 3) << "Batch HGET map size changed unexpectedly.";

	// Existing fields should have values
	ASSERT_TRUE(query_map["a"].has_value());
	EXPECT_EQ(*query_map["a"], "1");

	ASSERT_TRUE(query_map["b"].has_value());
	EXPECT_EQ(*query_map["b"], "2");

	// Non-existent field should be std::nullopt
	EXPECT_FALSE(query_map["d"].has_value()) << "Batch HGET returned value for non-existent field 'd'.";
}

TEST_F(hash_operations_test, hdel_single_and_multi) {
	// Setup initial data
	hash_map_type initial_data = {{"f1", "v1"}, {"f2", "v2"}, {"f3", "v3"}};
	hash_ops().hset(TEST_KEY, initial_data);

	// 1. Test HDEL (single field)
	long long deleted_count_1 = hash_ops().hdel(TEST_KEY, "f1");
	EXPECT_EQ(deleted_count_1, 1) << "HDEL single field did not return 1.";

	// Verify deletion
	EXPECT_FALSE(hash_ops().hget(TEST_KEY, "f1").has_value());

	// 2. Test HDEL (multiple fields)
	std::vector<key_type> keys_to_delete = {"f2", "f99"}; // f99 does not exist
	long long deleted_count_2 = hash_ops().hdel(TEST_KEY, keys_to_delete);
	EXPECT_EQ(deleted_count_2, 1) << "HDEL multiple fields returned incorrect count.";

	// Verify remaining count
	hash_map_type final_data = hash_ops().hgetall(TEST_KEY);
	EXPECT_EQ(final_data.size(), 1) << "HGETALL returned incorrect final size.";
	EXPECT_TRUE(final_data.count("f3"));
}

TEST_F(hash_operations_test, hkeys_and_hvals) {
	hash_map_type data = {{"k_apple", "red"}, {"k_banana", "yellow"}, {"k_grape", "purple"}};
	hash_ops().hset(TEST_KEY, data);

	// 1. Test HKEYS
	std::vector<key_type> keys = hash_ops().hkeys(TEST_KEY);
	EXPECT_EQ(keys.size(), 3);

	// Convert to set for order-independent comparison
	std::unordered_set key_set(keys.begin(), keys.end());
	EXPECT_TRUE(key_set.count("k_apple"));
	EXPECT_TRUE(key_set.count("k_banana"));
	EXPECT_TRUE(key_set.count("k_grape"));

	// 2. Test HVALS
	std::vector<value_type> values = hash_ops().hvals(TEST_KEY);
	EXPECT_EQ(values.size(), 3);

	// Convert to set for order-independent comparison
	std::unordered_set val_set(values.begin(), values.end());
	EXPECT_TRUE(val_set.count("red"));
	EXPECT_TRUE(val_set.count("yellow"));
	EXPECT_TRUE(val_set.count("purple"));
}

TEST_F(hash_operations_test, hscan) {
	// Test purpose: Insert several fields into a hash, scan them using hscan, and verify the existence of the fields.
	key_type test_key = "test_hash_hscan";

	// Insert fields
	std::vector<std::pair<std::string, value_type>> fields{
		{"f0", "0"}, {"f1", "1"}, {"f2", "2"}, {"f3", "3"}, {"f4", "4"}};

	for (const auto &p: fields) {
		ASSERT_TRUE(hash_ops().hset(test_key, p.first, p.second)) << "hset failed for field: " << p.first;
	}

	// Use hscan to scan all fields starting from cursor=0
	std::unordered_map<std::string, value_type> hash_map;
	// Note: The 'hscan' implementation needs to handle the iteration loop internally
	// or this call assumes all data is returned in one go (since COUNT is 100 and data set is small).
	auto cursor = hash_ops().hscan(test_key, 0, "*", 100, hash_map);

	// According to Redis HSCAN semantics, the cursor should be 0 if the scan is complete.
	EXPECT_EQ(cursor, 0u) << "Expected final cursor to be 0";

	// Verify that all inserted fields are present in the scan result
	std::unordered_set<std::string> found;
	for (const auto &kv: hash_map) {
		found.insert(kv.first);
	}
	for (const auto &p: fields) {
		EXPECT_TRUE(found.count(p.first)) << "Field missing from hscan result: " << p.first;
	}

	// Clean up
	tpl->del(test_key);
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
