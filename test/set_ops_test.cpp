#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"
#include "janus/janus.hpp"

#include "test_env.hpp"

class SetOpsTest: public testing::Test {
protected:
	// Type aliases (K and V are both std::string)
	using key_type = std::string;
	using value_type = std::string;

	std::unique_ptr<janus::redis_connection> conn;
	std::unique_ptr<janus::redis_template<key_type, value_type>> tpl;
	std::set<key_type> keys_to_clean;
	janus::string_serializer<key_type> key_serializer;
	janus::string_serializer<value_type> value_serializer;

	SetOpsTest() {
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

TEST_F(SetOpsTest, SaddSismemberScard) {
	const key_type test_key = "test_set_a";
	keys_to_clean.insert(test_key);
	std::vector<value_type> members = {"a", "b", "c", "b"};

	long long added_count = tpl->ops_for_set().sadd(test_key, members);
	EXPECT_EQ(added_count, 3) << "SADD should only count unique new members.";

	EXPECT_EQ(tpl->ops_for_set().scard(test_key), 3);

	EXPECT_TRUE(tpl->ops_for_set().sismember(test_key, "a"));
	EXPECT_FALSE(tpl->ops_for_set().sismember(test_key, "d"));
}

TEST_F(SetOpsTest, SremAndSmembers) {
	const key_type test_key = "test_set_a";
	keys_to_clean.insert(test_key);
	tpl->ops_for_set().sadd(test_key, {"1", "2", "3"});

	std::vector<value_type> to_remove = {"2", "4"};
	long long removed_count = tpl->ops_for_set().srem(test_key, to_remove);
	EXPECT_EQ(removed_count, 1) << "SREM should only count actually removed members.";

	std::vector<value_type> members = tpl->ops_for_set().smembers(test_key);

	std::unordered_set member_set(members.begin(), members.end());
	EXPECT_EQ(member_set.size(), 2);
	EXPECT_TRUE(member_set.count("1"));
	EXPECT_TRUE(member_set.count("3"));
	EXPECT_FALSE(member_set.count("2"));
}

TEST_F(SetOpsTest, Sinter) {
	const key_type test_key_1 = "test_set_a";
	const key_type test_key_2 = "test_set_b";
	keys_to_clean.insert({test_key_1, test_key_2});

	tpl->ops_for_set().sadd(test_key_1, {"1", "2", "3"});
	tpl->ops_for_set().sadd(test_key_2, {"2", "3", "4"});

	std::vector keys_to_intersect = {test_key_1, test_key_2};
	std::vector<value_type> intersection = tpl->ops_for_set().sinter(keys_to_intersect);

	std::unordered_set result_set(intersection.begin(), intersection.end());
	EXPECT_EQ(result_set.size(), 2);
	EXPECT_TRUE(result_set.count("2"));
	EXPECT_TRUE(result_set.count("3"));
	EXPECT_FALSE(result_set.count("1"));
}

TEST_F(SetOpsTest, Spop) {
	const key_type test_key = "test_set_a";
	keys_to_clean.insert(test_key);
	tpl->ops_for_set().sadd(test_key, {"x", "y", "z"});

	std::optional<value_type> popped_member = tpl->ops_for_set().spop(test_key);
	ASSERT_TRUE(popped_member);
	EXPECT_EQ(tpl->ops_for_set().scard(test_key), 2);

	tpl->ops_for_set().spop(test_key);
	tpl->ops_for_set().spop(test_key);

	EXPECT_FALSE(tpl->ops_for_set().spop(test_key));
	EXPECT_EQ(tpl->ops_for_set().scard(test_key), 0);
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
