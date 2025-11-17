#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"
#include "janus/janus.hpp"

#define DEFAULT_REDIS_HOST "127.0.0.1"
#define DEFAULT_REDIS_PORT 6379

// The Test Fixture class, implementing SetUp/TearDown logic
class redis_operations_test: public ::testing::Test {
protected:
	// Type aliases
	using key_type = std::string;
	using value_type = unsigned int;

	// Connection parameters
	std::string redis_host;
	unsigned short redis_port{DEFAULT_REDIS_PORT};

	std::shared_ptr<kv_connection> conn;
	std::shared_ptr<serializer<key_type>> k_serializer;
	std::shared_ptr<serializer<value_type>> v_serializer;
	std::unique_ptr<redis_template<key_type, value_type>> tpl;

	// Test Keys
	const key_type test_key_single = "ops_test_single_key";
	const key_type test_key_ttl = "ops_test_ttl_key";
	const key_type test_key_pttl = "ops_test_pttl_key";
	const key_type test_key_del_a = "ops_test_del_a";
	const key_type test_key_del_b = "ops_test_del_b";
	const key_type non_existent_key = "ops_test_non_existent_key_for_del";

	void SetUp() override {
		// 1. Retrieve connection parameters from environment variables
		if (const char *env_host = std::getenv("TEST_REDIS_HOST")) {
			redis_host = env_host;
		}
		else {
			redis_host = DEFAULT_REDIS_HOST;
			std::cerr << "Warning: TEST_REDIS_HOST not set. Using default: " << redis_host << std::endl;
		}

		if (const char *env_port = std::getenv("TEST_REDIS_PORT")) {
			try {
				int port_int = std::stoi(env_port);
				if (port_int > 0 && port_int < 65536) {
					redis_port = static_cast<unsigned short>(port_int);
				}
				else {
					throw std::runtime_error("Port out of range.");
				}
			}
			catch ([[maybe_unused]] const std::exception &e) {
				redis_port = DEFAULT_REDIS_PORT;
				std::cerr << "Warning: Invalid TEST_REDIS_PORT value. Using default: " << redis_port << std::endl;
			}
		}
		else {
			redis_port = DEFAULT_REDIS_PORT;
			std::cerr << "Warning: TEST_REDIS_PORT not set. Using default: " << redis_port << std::endl;
		}

		// 2. Create underlying connection
		try {
			conn = std::make_shared<redis_connection>(redis_host, redis_port);
		}
		catch (const std::runtime_error &e) {
			// If connection fails, skip all tests in this fixture
			GTEST_SKIP() << "Skipping test: Could not connect to Redis at " << redis_host << ":" << redis_port
						 << ". Error: " << e.what();
		}

		// 3. Create Serializers
		k_serializer = std::make_shared<string_serializer<key_type>>();
		v_serializer = std::make_shared<string_serializer<value_type>>();

		// 4. Construct redis_template
		tpl = std::make_unique<redis_template<key_type, value_type>>(conn, k_serializer, v_serializer);

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
		if (tpl) {
			tpl->del({test_key_single, test_key_ttl, test_key_pttl, test_key_del_a, test_key_del_b, non_existent_key});
		}
	}

	// Helper function to get String (Value) operations interface
	[[nodiscard]] auto &value_ops() const {
		return tpl->ops_for_value();
	}

	// Helper function to ensure a key exists (by setting a default value)
	void set_test_key(const key_type &key) const {
		// Set an arbitrary value to ensure key existence
		ASSERT_TRUE(value_ops().set(key, 0U)) << "Helper: Failed to set value for key: " << key;
	}
};

// -----------------------------------------------------------------------------
// Test Case 1: exists(K) and del(K/vector<K>)
// -----------------------------------------------------------------------------

// Test: set creates key, exists returns true, del removes key, exists returns false.
TEST_F(redis_operations_test, exists_set_del_single) {
	key_type test_key = test_key_single;

	// 1. Initial state: Key should not exist
	ASSERT_FALSE(tpl->exists(test_key)) << "Key should not exist initially.";

	// 2. Set value
	set_test_key(test_key);

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

// Test: deleting multiple keys, including non-existent ones, and verifying deletion.
TEST_F(redis_operations_test, del_multiple) {
	key_type key_a = test_key_del_a;
	key_type key_b = test_key_del_b;
	key_type key_c_non_existent = non_existent_key; // A key guaranteed not to exist

	// 1. Setup: Ensure A and B exist, C does not
	set_test_key(key_a);
	set_test_key(key_b);
	ASSERT_TRUE(tpl->exists(key_a) && tpl->exists(key_b)) << "Setup failed: Test keys A and B must exist.";
	ASSERT_FALSE(tpl->exists(key_c_non_existent)) << "Setup failed: Test key C must not exist.";

	// 2. DEL multiple keys (A, B exist; C does not)
	std::vector<key_type> keys_to_delete = {key_a, key_b, key_c_non_existent};
	long long deleted_count = tpl->del(keys_to_delete);

	// 3. Verify return value: Should be 2 (only A and B were deleted)
	EXPECT_EQ(deleted_count, 2)
		<< "DEL multiple should return the count of keys that actually existed and were deleted.";

	// 4. Verify all existing keys (A, B) are gone
	EXPECT_FALSE(tpl->exists(key_a)) << "Key A should be deleted after bulk DEL.";
	EXPECT_FALSE(tpl->exists(key_b)) << "Key B should be deleted after bulk DEL.";
}

// -----------------------------------------------------------------------------
// Test Case 2: expire/ttl
// -----------------------------------------------------------------------------

// Test: Set TTL, then verify that the value read by ttl() is <= the set value.
// Test: ttl() return values for non-existent key (-2), persistent key (-1), and expiring key (> 0).
TEST_F(redis_operations_test, ttl_and_expire) {
	key_type test_key = test_key_ttl;
	constexpr long long ttl_seconds = 5;

	// 1. Test TTL on non-existent key (should return -2)
	EXPECT_EQ(tpl->ttl(non_existent_key), -2) << "TTL on non-existent key must return -2.";

	// 2. Setup: Key exists but has no TTL
	set_test_key(test_key);

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

// -----------------------------------------------------------------------------
// Test Case 3: pexpire/pttl
// -----------------------------------------------------------------------------

// Test: Set PTTL, then verify that the value read by pttl() is <= the set value.
// Test: pttl() return values for non-existent key (-2), persistent key (-1), and expiring key (> 0).
TEST_F(redis_operations_test, pttl_and_pexpire) {
	key_type test_key = test_key_pttl;
	constexpr int pttl_milliseconds = 5000; // 5 seconds
	constexpr int sleep_milliseconds = 1000; // 1 second

	// 1. Test PTTL on non-existent key (should return -2)
	EXPECT_EQ(tpl->pttl(non_existent_key), -2) << "PTTL on non-existent key must return -2.";

	// 2. Setup: Key exists but has no TTL
	set_test_key(test_key);

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
	EXPECT_GE(remaining_pttl_after_delay, remaining_pttl - sleep_milliseconds)
		<< "PTTL must decrease by at least the sleep time.";
}

// -----------------------------------------------------------------------------
// Test Case 4: keys(pattern) - Testing redis_template implementation
// -----------------------------------------------------------------------------

// Test: Verify redis_template's keys(pattern) correctly finds and deserializes keys.
TEST_F(redis_operations_test, template_keys_pattern_matching) {
	const key_type base_key = "tpl_keys_test:";
	const std::string pattern_prefix = base_key + "*";

	// 1. Setup: Create keys with a specific pattern
	const key_type key_1 = base_key + "apple";
	const key_type key_2 = base_key + "banana";
	const key_type key_3 = "other_key:orange"; // Should NOT match

	set_test_key(key_1);
	set_test_key(key_2);
	set_test_key(key_3); // Set key that doesn't match the pattern

	// 2. Execute keys using the template instance
	std::unordered_set<key_type> found_keys;
	tpl->keys(pattern_prefix, found_keys);

	// 3. Verify results
	EXPECT_EQ(found_keys.size(), 2) << "Template KEYS should find exactly 2 matching keys.";
	EXPECT_TRUE(found_keys.count(key_1)) << "Template KEYS should include key_1.";
	EXPECT_TRUE(found_keys.count(key_2)) << "Template KEYS should include key_2.";
	EXPECT_FALSE(found_keys.count(key_3)) << "Template KEYS should NOT include key_3.";

	// 4. Cleanup
	tpl->del({key_1, key_2, key_3});
}

// -----------------------------------------------------------------------------
// Test Case 5: scan(cursor, pattern, count) - Testing redis_template implementation
// -----------------------------------------------------------------------------

// Test: Verify redis_template's scan() iterates through all matching keys, handling cursor and deserialization.
TEST_F(redis_operations_test, template_scan_iteration) {
	const key_type base_key = "tpl_scan_test:";
	const std::string pattern = base_key + "*";
	constexpr int total_keys = 25; // Keys to create
	constexpr int scan_count = 10; // Batch size per scan call

	std::unordered_set<key_type> expected_keys;

	// 1. Setup: Create the expected number of keys
	for (int i = 0; i < total_keys; ++i) {
		key_type key = base_key + std::to_string(i);
		set_test_key(key);
		expected_keys.insert(key);
	}

	// Set a key that should NOT be returned by the pattern
	set_test_key("non_matching_key");

	// 2. Perform SCAN iteration
	uint64_t cursor = 0;
	std::unordered_set<key_type> scanned_keys;
	int iteration_count = 0;

	do {
		auto result = tpl->scan(cursor, pattern, scan_count);
		cursor = result.cursor;

		// Accumulate scanned keys
		for (const auto &key: result.keys) {
			scanned_keys.insert(key);
		}

		iteration_count++;

	} while (cursor != 0 && iteration_count < 100); // Limit iterations to prevent endless loop

	// 3. Verify results

	// A. Check total count
	EXPECT_EQ(scanned_keys.size(), total_keys)
		<< "Template SCAN should find exactly " << total_keys << " matching keys.";

	// B. Check content integrity
	EXPECT_EQ(scanned_keys, expected_keys) << "The set of scanned keys does not match the expected set.";

	// C. Verify minimum iteration count
	int min_expected_iterations = (total_keys + scan_count - 1) / scan_count;
	EXPECT_GE(iteration_count, min_expected_iterations)
		<< "Template SCAN should take at least " << min_expected_iterations << " iterations.";

	// 4. Cleanup
	std::vector<key_type> keys_to_delete;
	keys_to_delete.reserve(expected_keys.size() + 1);
	for (const auto &key: expected_keys) {
		keys_to_delete.push_back(key);
	}
	keys_to_delete.emplace_back("non_matching_key");
	tpl->del(keys_to_delete);
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
