#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility> // For std::pair
#include <vector>

#include "gtest/gtest.h"
#include "janus/janus.hpp"

#include "test_env.hpp"

class zset_operations_test: public testing::Test {
protected:
	// Type aliases (K is string, V is member string)
	using key_type = std::string;
	using member_type = std::string; // V
	using score_type = double;

	const key_type test_key = "test_zset_leaderboard";

	// Connection parameters
	std::string redis_host;
	unsigned short redis_port{DEFAULT_REDIS_PORT};

	std::shared_ptr<janus::kv_connection> conn;
	std::shared_ptr<janus::serializer<key_type>> k_serializer;
	std::shared_ptr<janus::serializer<member_type>> v_serializer; // V is the member type
	std::unique_ptr<janus::redis_template<key_type, member_type>> tpl;

	void SetUp() override {
		// 1. Retrieve connection parameters from environment variables
		auto [redis_host, redis_port] = get_redis_connection_params();

		// 2. Create underlying connection
		conn = std::make_shared<janus::redis_connection>(redis_host, redis_port);

		// 3. Create Serializers
		k_serializer = std::make_shared<janus::string_serializer<key_type>>();
		v_serializer = std::make_shared<janus::string_serializer<member_type>>();

		// 4. Construct redis_template
		tpl = std::make_unique<janus::redis_template<key_type, member_type>>(conn, k_serializer, v_serializer);

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
		tpl->del(test_key);
	}

	// Helper function to get ZSet operations interface
	[[nodiscard]] auto &zset_ops() const {
		return tpl->ops_for_zset();
	}

	// Helper function to setup basic ZSet data
	void setup_zset() const {
		std::unordered_map<member_type, score_type> initial_data = {
			{"alice", 10.0}, {"bob", 50.5}, {"charlie", 30.0}, {"diana", 20.0}};
		zset_ops().zadd(test_key, initial_data);
	}
};

// --- Test Cases ---

TEST_F(zset_operations_test, zadd_zscore_zrem) {
	// 1. Test ZADD
	std::unordered_map<member_type, score_type> data = {{"a", 1.0}, {"b", 2.0}};
	long long added_count = zset_ops().zadd(test_key, data);
	EXPECT_EQ(added_count, 2);

	// 2. Test ZSCORE
	std::optional<score_type> score_a = zset_ops().zscore(test_key, "a");
	ASSERT_TRUE(score_a);
	EXPECT_EQ(*score_a, 1.0);

	EXPECT_FALSE(zset_ops().zscore(test_key, "c")) << "ZSCORE on non-existent member should return nullopt.";

	// 3. Test ZREM
	long long removed_count = zset_ops().zrem(test_key, {"a", "c"}); // 'c' non-existent
	EXPECT_EQ(removed_count, 1);
	EXPECT_FALSE(zset_ops().zscore(test_key, "a"));
}

TEST_F(zset_operations_test, zincrby) {
	// Setup initial score
	zset_ops().zadd(test_key, {{"player", 100.0}});

	// 1. Test ZINCRBY
	score_type new_score = zset_ops().zincrby(test_key, 15.5, "player");
	EXPECT_DOUBLE_EQ(new_score, 115.5);

	// 2. Verify score via ZSCORE
	std::optional<score_type> final_score = zset_ops().zscore(test_key, "player");
	ASSERT_TRUE(final_score);
	EXPECT_DOUBLE_EQ(*final_score, 115.5);
}

TEST_F(zset_operations_test, zrange_and_zrevrange) {
	setup_zset(); // Data: {alice: 10, diana: 20, charlie: 30, bob: 50.5}

	// 1. Test ZRANGE (Low to High Index 0 to 2) -> {alice, diana, charlie}
	std::vector<member_type> range_asc = zset_ops().zrange(test_key, 0, 2);
	ASSERT_EQ(range_asc.size(), 3);
	EXPECT_EQ(range_asc[0], "alice");
	EXPECT_EQ(range_asc[2], "charlie");

	// 2. Test ZREVRANGE (High to Low Index 0 to 2) -> {bob, charlie, diana}
	std::vector<member_type> range_desc = zset_ops().zrevrange(test_key, 0, 2);
	ASSERT_EQ(range_desc.size(), 3);
	EXPECT_EQ(range_desc[0], "bob");
	EXPECT_EQ(range_desc[2], "diana");
}

TEST_F(zset_operations_test, zrange_withscores) {
	setup_zset(); // Data: {alice: 10, diana: 20, charlie: 30, bob: 50.5}

	// 1. Test ZRANGE_WITHSCORES (Index 0 to 1) -> {alice: 10, diana: 20}
	auto pairs_asc = zset_ops().zrange_withscores(test_key, 0, 1);
	ASSERT_EQ(pairs_asc.size(), 2);
	EXPECT_EQ(pairs_asc[0].first, "alice");
	EXPECT_DOUBLE_EQ(pairs_asc[1].second, 20.0);

	// 2. Test ZREVRANGE_WITHSCORES (Index 0 to 1) -> {bob: 50.5, charlie: 30}
	auto pairs_desc = zset_ops().zrevrange_withscores(test_key, 0, 1);
	ASSERT_EQ(pairs_desc.size(), 2);
	EXPECT_EQ(pairs_desc[0].first, "bob");
	EXPECT_DOUBLE_EQ(pairs_desc[1].second, 30.0);
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
