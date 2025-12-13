#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "janus/janus.hpp"

#include "test_env.hpp"

class ListOpsTest: public testing::Test {
protected:
	// Type aliases (K and V are both std::string)
	using key_type = std::string;
	using value_type = std::string;

	std::unique_ptr<janus::redis_connection> conn;
	std::unique_ptr<janus::redis_template<key_type, value_type>> tpl;
	std::set<key_type> keys_to_clean;
	janus::string_serializer<key_type> key_serializer;
	janus::string_serializer<value_type> value_serializer;

	ListOpsTest() {
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

TEST_F(ListOpsTest, LpushAndLpop) {
	const key_type test_key = "test_list_key:pushpop";
	keys_to_clean.insert(test_key);

	// List state: []
	EXPECT_EQ(tpl->ops_for_list().llen(test_key), 0);

	long long len1 = tpl->ops_for_list().lpush(test_key, "B");
	EXPECT_EQ(len1, 1);

	std::vector<value_type> vals_to_push = {"A"};
	long long len2 = tpl->ops_for_list().lpush(test_key, vals_to_push);
	EXPECT_EQ(len2, 2);

	std::optional<value_type> pop_val1 = tpl->ops_for_list().lpop(test_key);
	ASSERT_TRUE(pop_val1);
	EXPECT_EQ(*pop_val1, "A");
	EXPECT_EQ(tpl->ops_for_list().llen(test_key), 1);

	std::optional<value_type> pop_val2 = tpl->ops_for_list().lpop(test_key);
	ASSERT_TRUE(pop_val2);
	EXPECT_EQ(*pop_val2, "B");
	EXPECT_EQ(tpl->ops_for_list().llen(test_key), 0);

	EXPECT_FALSE(tpl->ops_for_list().lpop(test_key));
}

TEST_F(ListOpsTest, RpushAndRpop) {
	const key_type test_key = "test_list_key:pushpop";
	keys_to_clean.insert(test_key);

	long long len1 = tpl->ops_for_list().rpush(test_key, "X");
	EXPECT_EQ(len1, 1);

	std::vector<value_type> vals_to_push = {"Y", "Z"};
	long long len2 = tpl->ops_for_list().rpush(test_key, vals_to_push);
	EXPECT_EQ(len2, 3);

	std::optional<value_type> pop_val1 = tpl->ops_for_list().rpop(test_key);
	ASSERT_TRUE(pop_val1);
	EXPECT_EQ(*pop_val1, "Z");

	std::optional<value_type> pop_val2 = tpl->ops_for_list().rpop(test_key);
	ASSERT_TRUE(pop_val2);
	EXPECT_EQ(*pop_val2, "Y");
}

TEST_F(ListOpsTest, LrangeAndLlen) {
	const key_type test_key = "test_list_key:range";
	keys_to_clean.insert(test_key);

	// After lpush, the list will be in reverse order of insertion: [1, 2, 3, 4, 5]
	std::vector<value_type> initial_data = {"5", "4", "3", "2", "1"};
	tpl->ops_for_list().lpush(test_key, initial_data);

	EXPECT_EQ(tpl->ops_for_list().llen(test_key), 5);

	std::vector<value_type> full_range = tpl->ops_for_list().lrange(test_key, 0, -1);
	EXPECT_EQ(full_range.size(), 5);
	EXPECT_EQ(full_range[0], "1");
	EXPECT_EQ(full_range[4], "5");

	std::vector<value_type> sub_range = tpl->ops_for_list().lrange(test_key, 1, 3);
	EXPECT_EQ(sub_range.size(), 3);
	EXPECT_EQ(sub_range[0], "2");
	EXPECT_EQ(sub_range[2], "4");
}

TEST_F(ListOpsTest, ListMultiPop) {
	std::string lpop_key = "mylist:lpop";
	std::string rpop_key = "mylist:rpop";
	std::string shortlist_key = "shortlist";
	keys_to_clean.insert(lpop_key);
	keys_to_clean.insert(rpop_key);
	keys_to_clean.insert(shortlist_key);

	tpl->ops_for_list().rpush(lpop_key, std::vector<value_type>{"a", "b", "c", "d", "e"});
	auto popped_l = tpl->ops_for_list().lpop(lpop_key, 3);
	ASSERT_EQ(popped_l.size(), 3);
	EXPECT_EQ(popped_l[0], "a");
	EXPECT_EQ(popped_l[1], "b");
	EXPECT_EQ(popped_l[2], "c");
	EXPECT_EQ(tpl->ops_for_list().llen(lpop_key), 2);

	tpl->ops_for_list().rpush(rpop_key, std::vector<value_type>{"a", "b", "c", "d", "e"});
	auto popped_r = tpl->ops_for_list().rpop(rpop_key, 3);
	ASSERT_EQ(popped_r.size(), 3);
	EXPECT_EQ(popped_r[0], "e");
	EXPECT_EQ(popped_r[1], "d");
	EXPECT_EQ(popped_r[2], "c");
	EXPECT_EQ(tpl->ops_for_list().llen(rpop_key), 2);

	auto empty_pop = tpl->ops_for_list().lpop("nonexistent:list", 5);
	EXPECT_TRUE(empty_pop.empty());

	tpl->ops_for_list().rpush(shortlist_key, std::vector<value_type>{"one", "two"});
	auto all_popped = tpl->ops_for_list().rpop("shortlist", 5);
	EXPECT_EQ(all_popped.size(), 2);
	EXPECT_EQ(tpl->ops_for_list().llen("shortlist"), 0);
}

TEST_F(ListOpsTest, ListIndex) {
	std::string key = "mylist:lindex";
	keys_to_clean.insert(key);

	tpl->ops_for_list().rpush(key, std::vector<value_type>{"one", "two", "three", "four"});

	auto val0 = tpl->ops_for_list().lindex(key, 0);
	ASSERT_TRUE(val0.has_value());
	EXPECT_EQ(*val0, "one");

	auto val_neg1 = tpl->ops_for_list().lindex(key, -1);
	ASSERT_TRUE(val_neg1.has_value());
	EXPECT_EQ(*val_neg1, "four");

	EXPECT_FALSE(tpl->ops_for_list().lindex(key, 10).has_value());
	EXPECT_FALSE(tpl->ops_for_list().lindex("nonexistent:key", 0).has_value());
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
