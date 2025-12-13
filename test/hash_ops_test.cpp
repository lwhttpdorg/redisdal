#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"
#include "janus/janus.hpp"

#include "test_env.hpp"

class HashOpsTest: public testing::Test {
protected:
	// Type aliases (K and V are both std::string)
	using key_type = std::string;
	using value_type = std::string;
	using hash_map_type = std::unordered_map<key_type, value_type>;
	using optional_hash_map_type = std::unordered_map<key_type, std::optional<value_type>>;

	std::unique_ptr<janus::redis_connection> conn;
	std::unique_ptr<janus::redis_template<key_type, value_type>> tpl;
	std::set<key_type> keys_to_clean;
	janus::string_serializer<key_type> key_serializer;
	janus::string_serializer<value_type> value_serializer;

	HashOpsTest() {
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

TEST_F(HashOpsTest, HsetHgetSingle) {
	const key_type test_key = "test_hash_map";
	keys_to_clean.insert(test_key);
	const key_type field_key = "field_name_1";
	const value_type field_value = "value_data_A";

	EXPECT_TRUE(tpl->ops_for_hash().hset(test_key, field_key, field_value)) << "HSET failed for a new field.";

	std::optional<value_type> retrieved_val = tpl->ops_for_hash().hget(test_key, field_key);
	ASSERT_TRUE(retrieved_val) << "HGET failed to retrieve the field.";
	EXPECT_EQ(*retrieved_val, field_value) << "Retrieved value mismatch.";

	std::optional<value_type> missing_val = tpl->ops_for_hash().hget(test_key, "non_existent_field");
	EXPECT_FALSE(missing_val) << "HGET returned value for a non-existent field.";
}

TEST_F(HashOpsTest, HsetHgetMultiple) {
	const key_type test_key = "test_hash_map";
	keys_to_clean.insert(test_key);
	hash_map_type data_to_set = {{"f1", "v1"}, {"f2", "v2"}, {"f3", "v3"}};

	EXPECT_TRUE(tpl->ops_for_hash().hset(test_key, data_to_set)) << "HSET multiple fields failed.";

	hash_map_type retrieved_data = tpl->ops_for_hash().hgetall(test_key);
	EXPECT_EQ(retrieved_data.size(), 3) << "HGETALL returned incorrect number of fields.";
	EXPECT_EQ(retrieved_data["f2"], "v2") << "HGETALL retrieved incorrect value.";
}

TEST_F(HashOpsTest, HgetBatchHmget) {
	const key_type test_key = "test_hash_map";
	keys_to_clean.insert(test_key);
	hash_map_type initial_data = {{"a", "1"}, {"b", "2"}, {"c", "3"}};
	tpl->ops_for_hash().hset(test_key, initial_data);

	optional_hash_map_type query_map = {
		{"a", std::nullopt}, {"b", std::nullopt}, {"d", std::nullopt} // Non-existent
	};

	tpl->ops_for_hash().hget(test_key, query_map);

	EXPECT_EQ(query_map.size(), 3) << "Batch HGET map size changed unexpectedly.";

	ASSERT_TRUE(query_map["a"].has_value());
	EXPECT_EQ(*query_map["a"], "1");

	ASSERT_TRUE(query_map["b"].has_value());
	EXPECT_EQ(*query_map["b"], "2");

	EXPECT_FALSE(query_map["d"].has_value()) << "Batch HGET returned value for non-existent field 'd'.";
}

TEST_F(HashOpsTest, HdelSingleAndMulti) {
	const key_type test_key = "test_hash_map";
	keys_to_clean.insert(test_key);
	hash_map_type initial_data = {{"f1", "v1"}, {"f2", "v2"}, {"f3", "v3"}};
	tpl->ops_for_hash().hset(test_key, initial_data);

	long long deleted_count_1 = tpl->ops_for_hash().hdel(test_key, "f1");
	EXPECT_EQ(deleted_count_1, 1) << "HDEL single field did not return 1.";

	EXPECT_FALSE(tpl->ops_for_hash().hget(test_key, "f1").has_value());

	std::vector<key_type> keys_to_delete = {"f2", "f99"}; // f99 does not exist
	long long deleted_count_2 = tpl->ops_for_hash().hdel(test_key, keys_to_delete);
	EXPECT_EQ(deleted_count_2, 1) << "HDEL multiple fields returned incorrect count.";

	hash_map_type final_data = tpl->ops_for_hash().hgetall(test_key);
	EXPECT_EQ(final_data.size(), 1) << "HGETALL returned incorrect final size.";
	EXPECT_TRUE(final_data.count("f3"));
}

TEST_F(HashOpsTest, HkeysAndHvals) {
	const key_type test_key = "test_hash_map";
	keys_to_clean.insert(test_key);
	hash_map_type data = {{"k_apple", "red"}, {"k_banana", "yellow"}, {"k_grape", "purple"}};
	tpl->ops_for_hash().hset(test_key, data);

	std::vector<key_type> keys = tpl->ops_for_hash().hkeys(test_key);
	EXPECT_EQ(keys.size(), 3);

	std::unordered_set key_set(keys.begin(), keys.end());
	EXPECT_TRUE(key_set.count("k_apple"));
	EXPECT_TRUE(key_set.count("k_banana"));
	EXPECT_TRUE(key_set.count("k_grape"));

	std::vector<value_type> values = tpl->ops_for_hash().hvals(test_key);
	EXPECT_EQ(values.size(), 3);

	std::unordered_set val_set(values.begin(), values.end());
	EXPECT_TRUE(val_set.count("red"));
	EXPECT_TRUE(val_set.count("yellow"));
	EXPECT_TRUE(val_set.count("purple"));
}

TEST_F(HashOpsTest, Hscan) {
	key_type test_key = "test_hash_hscan";
	keys_to_clean.insert(test_key);

	std::vector<std::pair<std::string, value_type>> fields{
		{"f0", "0"}, {"f1", "1"}, {"f2", "2"}, {"f3", "3"}, {"f4", "4"}};

	for (const auto &p: fields) {
		ASSERT_TRUE(tpl->ops_for_hash().hset(test_key, p.first, p.second)) << "hset failed for field: " << p.first;
	}

	std::unordered_map<std::string, value_type> hash_map;
	auto cursor = tpl->ops_for_hash().hscan(test_key, 0, "*", 100, hash_map);

	EXPECT_EQ(cursor, 0u) << "Expected final cursor to be 0";

	std::unordered_set<std::string> found;
	for (const auto &kv: hash_map) {
		found.insert(kv.first);
	}
	for (const auto &p: fields) {
		EXPECT_TRUE(found.count(p.first)) << "Field missing from hscan result: " << p.first;
	}
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
