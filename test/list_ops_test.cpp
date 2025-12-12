#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "janus/janus.hpp"

#include "test_env.hpp"

class list_operations_test: public testing::Test {
protected:
	// Type aliases (K and V are both std::string)
	using key_type = std::string;
	using value_type = std::string;

	const key_type test_key = "test_list_key";

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
		tpl->del(test_key);
	}

	// Helper function to get List operations interface
	[[nodiscard]] auto &list_ops() const {
		return tpl->ops_for_list();
	}
};

// --- Test Cases ---

TEST_F(list_operations_test, lpush_and_lpop) {
	// List state: []
	EXPECT_EQ(list_ops().llen(test_key), 0);

	// 1. Test LPUSH single (List: [B])
	long long len1 = list_ops().lpush(test_key, "B");
	EXPECT_EQ(len1, 1);

	// 2. Test LPUSH multiple (List: [A, B])
	std::vector<value_type> vals_to_push = {"A"};
	long long len2 = list_ops().lpush(test_key, vals_to_push);
	EXPECT_EQ(len2, 2);

	// 3. Test LPOP (pop A, List: [B])
	std::optional<value_type> pop_val1 = list_ops().lpop(test_key);
	ASSERT_TRUE(pop_val1);
	EXPECT_EQ(*pop_val1, "A");
	EXPECT_EQ(list_ops().llen(test_key), 1);

	// 4. Test LPOP last element (pop B, List: [])
	std::optional<value_type> pop_val2 = list_ops().lpop(test_key);
	ASSERT_TRUE(pop_val2);
	EXPECT_EQ(*pop_val2, "B");
	EXPECT_EQ(list_ops().llen(test_key), 0);

	// 5. Test LPOP on empty list
	EXPECT_FALSE(list_ops().lpop(test_key));
}

TEST_F(list_operations_test, rpush_and_rpop) {
	// List state: []

	// 1. Test RPUSH single (List: [X])
	long long len1 = list_ops().rpush(test_key, "X");
	EXPECT_EQ(len1, 1);

	// 2. Test RPUSH multiple (List: [X, Y, Z])
	std::vector<value_type> vals_to_push = {"Y", "Z"};
	long long len2 = list_ops().rpush(test_key, vals_to_push);
	EXPECT_EQ(len2, 3);

	// 3. Test RPOP (pop Z, List: [X, Y])
	std::optional<value_type> pop_val1 = list_ops().rpop(test_key);
	ASSERT_TRUE(pop_val1);
	EXPECT_EQ(*pop_val1, "Z");

	// 4. Test RPOP (pop Y, List: [X])
	std::optional<value_type> pop_val2 = list_ops().rpop(test_key);
	ASSERT_TRUE(pop_val2);
	EXPECT_EQ(*pop_val2, "Y");
}

TEST_F(list_operations_test, lrange_and_llen) {
	// Push elements: [1, 2, 3, 4, 5] (Left/Head is 1, Right/Tail is 5)
	std::vector<value_type> initial_data = {"5", "4", "3", "2", "1"};
	list_ops().lpush(test_key, initial_data);

	// 1. Test LLEN
	EXPECT_EQ(list_ops().llen(test_key), 5);

	// 2. Test full range (0 to -1)
	std::vector<value_type> full_range = list_ops().lrange(test_key, 0, -1);
	EXPECT_EQ(full_range.size(), 5);
	EXPECT_EQ(full_range[0], "1");
	EXPECT_EQ(full_range[4], "5");

	// 3. Test sub-range (1 to 3, inclusive) -> [2, 3, 4]
	std::vector<value_type> sub_range = list_ops().lrange(test_key, 1, 3);
	EXPECT_EQ(sub_range.size(), 3);
	EXPECT_EQ(sub_range[0], "2");
	EXPECT_EQ(sub_range[2], "4");
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
