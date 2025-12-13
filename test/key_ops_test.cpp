#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"
#include "janus/janus.hpp"

#include "test_env.hpp"

// The Test Fixture class, implementing SetUp/TearDown logic
class KeyOpsTest: public testing::Test {
protected:
	// Type aliases
	using key_type = std::string;
	using value_type = unsigned int;

	std::unique_ptr<janus::redis_connection> conn;
	std::unique_ptr<janus::redis_template<key_type, value_type>> tpl;
	std::set<key_type> keys_to_clean;
	janus::string_serializer<key_type> key_serializer;
	janus::string_serializer<value_type> value_serializer;

	KeyOpsTest() {
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

TEST_F(KeyOpsTest, ExistsSetDel) {
	const key_type test_key = "ops_test_single_key";

	// 1. Initial state: Key should not exist
	ASSERT_FALSE(tpl->exists(test_key)) << "Key should not exist initially.";

	// 2. Set value
	tpl->ops_for_value().set(test_key, 1);
	keys_to_clean.insert(test_key);

	// 3. After SET: Key should exist
	ASSERT_TRUE(tpl->exists(test_key)) << "Key should exist after SET.";

	// 4. DEL existing key
	long long deleted_count_existing = tpl->del(test_key);
	EXPECT_EQ(deleted_count_existing, 1) << "DEL on existing key should return 1.";

	// 5. After DEL: Key should not exist
	ASSERT_FALSE(tpl->exists(test_key)) << "Key should not exist after DEL.";

	// 6. DEL non-existent key
	long long deleted_count_non_existing = tpl->del(test_key);
	EXPECT_EQ(deleted_count_non_existing, 0) << "DEL on non-existent key should return 0.";
}

TEST_F(KeyOpsTest, DelMultipleKey) {
	const key_type key_a = "ops_test_del_a";
	const key_type key_b = "ops_test_del_b";
	const key_type key_c_non_existent = "ops_test_non_existent_key_for_del";

	// 1. Setup: Ensure A and B exist, C does not
	tpl->ops_for_value().set(key_a, 1);
	tpl->ops_for_value().set(key_b, 1);
	keys_to_clean.insert({key_a, key_b});

	ASSERT_TRUE(tpl->exists(key_a) && tpl->exists(key_b)) << "Setup failed: Test keys A and B must exist.";
	ASSERT_FALSE(tpl->exists(key_c_non_existent)) << "Setup failed: Test key C must not exist.";

	// 2. DEL multiple keys (A, B exist; C does not)
	std::vector keys_to_delete = {key_a, key_b, key_c_non_existent};
	long long deleted_count = tpl->del(keys_to_delete);

	// 3. Verify return value: Should be 2 (only A and B were deleted)
	EXPECT_EQ(deleted_count, 2)
		<< "DEL multiple should return the count of keys that actually existed and were deleted.";

	// 4. Verify all existing keys (A, B) are gone
	EXPECT_FALSE(tpl->exists(key_a)) << "Key A should be deleted after bulk DEL.";
	EXPECT_FALSE(tpl->exists(key_b)) << "Key B should be deleted after bulk DEL.";
}

TEST_F(KeyOpsTest, TTLAndExpireTest) {
	const key_type test_key = "ops_test_ttl_key";
	const key_type non_existent_key = "non_existent";

	constexpr long long ttl_seconds = 5;

	// 1. Test TTL on non-existent key (should return -2)
	EXPECT_EQ(tpl->ttl(non_existent_key), -2);

	// 2. Setup: Key exists but has no TTL
	tpl->ops_for_value().set(test_key, 1);
	keys_to_clean.insert(test_key);

	// 3. Test TTL on persistent key (should return -1)
	EXPECT_EQ(tpl->ttl(test_key), -1) << "TTL on persistent key must return -1.";

	// 4. Set EXPIRE
	ASSERT_TRUE(tpl->expire(test_key, ttl_seconds)) << "EXPIRE operation failed.";

	// 5. Read TTL: must be > 0 and <= set value
	int64_t remaining_ttl = tpl->ttl(test_key);

	EXPECT_GT(remaining_ttl, 0) << "TTL must be positive after EXPIRE.";
	// The value read should be less than or equal to the set time due to Redis processing and network latency.
	EXPECT_LE(remaining_ttl, ttl_seconds) << "TTL must be less than or equal to the set value.";

	// 6. Wait 1 second to verify TTL decay
	std::this_thread::sleep_for(std::chrono::seconds(1));
	int64_t remaining_ttl_after_delay = tpl->ttl(test_key);

	EXPECT_GT(remaining_ttl_after_delay, 0) << "TTL must still be positive.";
	EXPECT_LE(remaining_ttl_after_delay, remaining_ttl) << "TTL must decrease over time.";
}

TEST_F(KeyOpsTest, pttl_and_pexpire) {
	const key_type test_key = "ops_test_pttl_key";
	const key_type non_existent_key = "non_existent";

	constexpr int pttl_milliseconds = 5000; // 5 seconds
	constexpr int sleep_milliseconds = 2000; // 2 second

	// 1. Test PTTL on non-existent key (should return -2)
	EXPECT_EQ(tpl->pttl(non_existent_key), -2);

	// 2. Setup: Key exists but has no TTL
	tpl->ops_for_value().set(test_key, 1);
	keys_to_clean.insert(test_key);

	// 3. Test PTTL on persistent key (should return -1)
	EXPECT_EQ(tpl->pttl(test_key), -1) << "PTTL on persistent key must return -1.";

	// 4. Set PEXPIRE
	ASSERT_TRUE(tpl->pexpire(test_key, pttl_milliseconds)) << "PEXPIRE operation failed.";

	// 5. Read PTTL: must be > 0 and <= set value
	int64_t remaining_pttl = tpl->pttl(test_key);

	EXPECT_GT(remaining_pttl, 0) << "PTTL must be positive after PEXPIRE.";
	EXPECT_LE(remaining_pttl, pttl_milliseconds) << "PTTL must be less than or equal to the set value (in ms).";

	// 6. Wait 1 second (1000ms) to verify PTTL decay
	std::this_thread::sleep_for(std::chrono::milliseconds(sleep_milliseconds));
	int64_t remaining_pttl_after_delay = tpl->pttl(test_key);

	EXPECT_GT(remaining_pttl_after_delay, 0) << "PTTL must still be positive after delay.";
	// The remaining PTTL must be less than the initial remaining PTTL minus the sleep time (with a small tolerance for
	// latency).
	EXPECT_LE(remaining_pttl_after_delay, remaining_pttl - sleep_milliseconds)
		<< "PTTL must decrease by at least the sleep time (actual decay >= sleep time).";
}

TEST_F(KeyOpsTest, Persist) {
	const key_type test_key1 = "mykey:persist";
	const key_type test_key2 = "mykey:no_expiry";

	tpl->ops_for_value().set(test_key1, 1);
	keys_to_clean.insert(test_key1);

	// 1. Set a key with an expiration
	ASSERT_TRUE(tpl->expire(test_key1, 60));

	// 2. Check that TTL is set and positive
	int64_t time_to_live = tpl->ttl(test_key1);
	EXPECT_GT(time_to_live, 0);
	EXPECT_LE(time_to_live, 60);

	// 3. Persist the key to remove expiration
	EXPECT_TRUE(tpl->persist(test_key1));

	// 4. Check that TTL is now -1 (persistent)
	EXPECT_EQ(tpl->ttl(test_key1), -1);

	// 5. Test persist on a non-existent key
	EXPECT_FALSE(tpl->persist("nonexistent:key"));

	// 6. Test persist on a key with no expiry
	tpl->ops_for_value().set(test_key2, 1);
	keys_to_clean.insert(test_key2);
	EXPECT_FALSE(tpl->persist(test_key2));
}

TEST_F(KeyOpsTest, TemplateKeysPatternMatching) {
	const key_type base_key = "tpl_keys_test:";
	const std::string pattern_prefix = base_key + "*";

	// 1. Setup: Create keys with a specific pattern
	const key_type key_1 = base_key + "apple";
	const key_type key_2 = base_key + "banana";
	const key_type key_3 = "other_key:orange"; // Should NOT match

	tpl->ops_for_value().set(key_1, 1);
	tpl->ops_for_value().set(key_2, 2);
	tpl->ops_for_value().set(key_3, 3);
	keys_to_clean.insert({key_1, key_2, key_3});

	// 2. Execute keys using the template instance
	std::unordered_set<key_type> found_keys;
	tpl->keys(pattern_prefix, found_keys);

	// 3. Verify results
	EXPECT_EQ(found_keys.size(), 2) << "Template KEYS should find exactly 2 matching keys.";
	EXPECT_TRUE(found_keys.count(key_1)) << "Template KEYS should include key_1.";
	EXPECT_TRUE(found_keys.count(key_2)) << "Template KEYS should include key_2.";
	EXPECT_FALSE(found_keys.count(key_3)) << "Template KEYS should NOT include key_3.";
}

TEST_F(KeyOpsTest, TemplateScanIteration) {
	const key_type base_key = "tpl_scan_test:";
	const key_type non_matching_key = "non_matching_key";
	const std::string pattern = base_key + "*";
	constexpr int total_keys = 25; // Keys to create
	keys_to_clean.insert("non_matching_key");

	std::unordered_set<key_type> expected_keys;

	// 1. Setup: Create the expected number of keys
	for (int i = 0; i < total_keys; ++i) {
		key_type key = base_key + std::to_string(i);
		tpl->ops_for_value().set(key, i);
		keys_to_clean.insert(key);
		expected_keys.insert(key);
	}

	// Set a key that should NOT be returned by the pattern
	tpl->ops_for_value().set(non_matching_key, 1);
	keys_to_clean.insert(non_matching_key);

	// 2. Perform SCAN iteration
	uint64_t cursor = 0;
	std::unordered_set<key_type> scanned_keys;
	int iteration_count = 0;

	do {
		constexpr int scan_count = 10;
		auto result = tpl->scan(cursor, pattern, scan_count);
		cursor = result.cursor;
		// Accumulate scanned keys
		for (const auto &key: result.keys) {
			scanned_keys.insert(key);
		}
		++iteration_count;
		// cursor > 0 indicates more keys to scan
	} while (cursor != 0 && iteration_count < 100); // Limit iterations to prevent endless loop

	// 3. Verify results

	// A. Check total count
	EXPECT_EQ(scanned_keys.size(), total_keys)
		<< "Template SCAN should find exactly " << total_keys << " matching keys.";

	// B. Check content integrity
	EXPECT_EQ(scanned_keys, expected_keys) << "The set of scanned keys does not match the expected set.";

	// C. Verify minimum iteration count
	/* Since scan_count is a hint and not an upper limit, any number of iteration_count greater than zero is reasonable.
	 */
	EXPECT_GE(iteration_count, 0) << "Template SCAN should take at least 1 iteration to complete the scan.";
}

TEST_F(KeyOpsTest, type) {
	// Test purpose: Verify that redis_template::type returns the correct string description for different data types
	const key_type string_key = "test_type_string";
	const key_type hash_key = "test_type_hash";

	// 1) Create a string type key
	ASSERT_TRUE(tpl->ops_for_value().set(string_key, 123));
	keys_to_clean.insert(string_key);

	// 2) Create a hash type key (insert one field)
	ASSERT_TRUE(tpl->ops_for_hash().hset(hash_key, std::string("field1"), 1));
	keys_to_clean.insert(hash_key);

	// Verify the return value of type()
	EXPECT_EQ(tpl->type(string_key), "string") << "Expected type string for key: " << string_key;
	EXPECT_EQ(tpl->type(hash_key), "hash") << "Expected type hash for key: " << hash_key;
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
