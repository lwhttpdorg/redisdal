#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility> // For std::pair
#include <vector>

#include "gtest/gtest.h"
#include "janus/janus.hpp"

#include "test_env.hpp"

class ZSetOpsTest: public testing::Test {
protected:
	// Type aliases (K is string, V is member string)
	using key_type = std::string;
	using member_type = std::string; // V
	using score_type = double;

	std::unique_ptr<janus::redis_connection> conn;
	std::unique_ptr<janus::redis_template<key_type, member_type>> tpl;
	std::set<key_type> keys_to_clean;
	janus::string_serializer<key_type> key_serializer;
	janus::string_serializer<member_type> value_serializer;

	ZSetOpsTest() {
		std::string redis_url = get_redis_connection_url();
		conn = std::make_unique<janus::redis_connection>(redis_url);
		tpl = std::make_unique<janus::redis_template<key_type, member_type>>(*conn, key_serializer, value_serializer);
	}

	void TearDown() override {
		for (const auto &key: keys_to_clean) {
			tpl->del(key);
		}
	}

	// Helper function to setup basic ZSet data
	void setup_zset(const key_type &key) const {
		std::unordered_map<member_type, score_type> initial_data = {
			{"alice", 10.0}, {"bob", 50.5}, {"charlie", 30.0}, {"diana", 20.0}};
		tpl->ops_for_zset().zadd(key, initial_data);
	}
};

TEST_F(ZSetOpsTest, ZaddZscoreZrem) {
	const key_type test_key = "test_zset_leaderboard";
	keys_to_clean.insert(test_key);
	std::unordered_map<member_type, score_type> data = {{"a", 1.0}, {"b", 2.0}};
	long long added_count = tpl->ops_for_zset().zadd(test_key, data);
	EXPECT_EQ(added_count, 2);

	std::optional<score_type> score_a = tpl->ops_for_zset().zscore(test_key, "a");
	ASSERT_TRUE(score_a);
	EXPECT_EQ(*score_a, 1.0);

	EXPECT_FALSE(tpl->ops_for_zset().zscore(test_key, "c")) << "ZSCORE on non-existent member should return nullopt.";

	long long removed_count = tpl->ops_for_zset().zrem(test_key, {"a", "c"}); // 'c' non-existent
	EXPECT_EQ(removed_count, 1);
	EXPECT_FALSE(tpl->ops_for_zset().zscore(test_key, "a"));
}

TEST_F(ZSetOpsTest, Zincrby) {
	const key_type test_key = "test_zset_leaderboard";
	keys_to_clean.insert(test_key);
	tpl->ops_for_zset().zadd(test_key, {{"player", 100.0}});

	score_type new_score = tpl->ops_for_zset().zincrby(test_key, 15.5, "player");
	EXPECT_DOUBLE_EQ(new_score, 115.5);

	std::optional<score_type> final_score = tpl->ops_for_zset().zscore(test_key, "player");
	ASSERT_TRUE(final_score);
	EXPECT_DOUBLE_EQ(*final_score, 115.5);
}

TEST_F(ZSetOpsTest, ZrangeAndZrevrange) {
	const key_type test_key = "test_zset_leaderboard";
	keys_to_clean.insert(test_key);
	setup_zset(test_key); // Data: {alice: 10, diana: 20, charlie: 30, bob: 50.5}

	std::vector<member_type> range_asc = tpl->ops_for_zset().zrange(test_key, 0, 2);
	ASSERT_EQ(range_asc.size(), 3);
	EXPECT_EQ(range_asc[0], "alice");
	EXPECT_EQ(range_asc[2], "charlie");

	std::vector<member_type> range_desc = tpl->ops_for_zset().zrevrange(test_key, 0, 2);
	ASSERT_EQ(range_desc.size(), 3);
	EXPECT_EQ(range_desc[0], "bob");
	EXPECT_EQ(range_desc[2], "diana");
}

TEST_F(ZSetOpsTest, ZrangeWithscores) {
	const key_type test_key = "test_zset_leaderboard";
	keys_to_clean.insert(test_key);
	setup_zset(test_key); // Data: {alice: 10, diana: 20, charlie: 30, bob: 50.5}

	auto pairs_asc = tpl->ops_for_zset().zrange_withscores(test_key, 0, 1);
	ASSERT_EQ(pairs_asc.size(), 2);
	EXPECT_EQ(pairs_asc[0].first, "alice");
	EXPECT_DOUBLE_EQ(pairs_asc[1].second, 20.0);

	auto pairs_desc = tpl->ops_for_zset().zrevrange_withscores(test_key, 0, 1);
	ASSERT_EQ(pairs_desc.size(), 2);
	EXPECT_EQ(pairs_desc[0].first, "bob");
	EXPECT_DOUBLE_EQ(pairs_desc[1].second, 30.0);
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
