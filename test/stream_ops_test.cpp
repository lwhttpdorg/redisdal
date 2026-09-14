#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "redisdal/redisdal.hpp"

#include "test_env.hpp"

class StreamOpsTest: public testing::Test {
protected:
    using key_type = std::string;
    using value_type = std::string;

    std::unique_ptr<redisdal::redis_client> conn;
    std::unique_ptr<redisdal::redis_template<key_type, value_type>> tpl;
    std::set<key_type> keys_to_clean;
    redisdal::string_serializer<key_type> key_serializer{};
    redisdal::string_serializer<value_type> value_serializer{};

    StreamOpsTest() {
        conn = std::make_unique<redisdal::redis_client>(get_redis_connection_url());
        tpl = std::make_unique<redisdal::redis_template<key_type, value_type>>(*conn, key_serializer, value_serializer);
    }

    void TearDown() override {
        for (const auto &key: keys_to_clean) {
            tpl->del(key);
        }
    }
};

TEST_F(StreamOpsTest, XaddXlenRangeAndDelete) {
    const key_type key = "test_stream_basic";
    keys_to_clean.insert(key);

    const std::vector<std::pair<key_type, value_type>> fields = {
        {"event", "created"},
        {"tag", "first"},
        {"tag", "second"},
        {std::string("binary\0field", 12), std::string("a\0b", 3)}};
    auto first_id = tpl->ops_for_stream().xadd(key, fields);
    ASSERT_TRUE(first_id);
    EXPECT_EQ(tpl->ops_for_stream().xlen(key), 1);

    auto second_id = tpl->ops_for_stream().xadd(key, {{"event", "updated"}});
    ASSERT_TRUE(second_id);

    auto entries = tpl->ops_for_stream().xrange(key, "-", "+");
    ASSERT_EQ(entries.size(), std::size_t{2});
    EXPECT_EQ(entries[0].id, *first_id);
    EXPECT_EQ(entries[0].fields, fields);
    EXPECT_EQ(entries[1].id, *second_id);

    auto reverse_entries = tpl->ops_for_stream().xrevrange(key, "+", "-", 1);
    ASSERT_EQ(reverse_entries.size(), std::size_t{1});
    EXPECT_EQ(reverse_entries[0].id, *second_id);

    EXPECT_EQ(tpl->ops_for_stream().xdel(key, {*first_id, "0-1"}), 1);
    EXPECT_EQ(tpl->ops_for_stream().xlen(key), 1);
    EXPECT_EQ(tpl->ops_for_stream().xdel(key, {}), 0);
}

TEST_F(StreamOpsTest, XaddAndXtrimOptions) {
    const key_type key = "test_stream_trim";
    keys_to_clean.insert(key);

    std::vector<std::string> ids;
    for (int i = 0; i < 5; ++i) {
        auto id = tpl->ops_for_stream().xadd(key, {{"index", std::to_string(i)}});
        ASSERT_TRUE(id);
        ids.push_back(*id);
    }

    auto removed = tpl->ops_for_stream().xtrim(key, redisdal::stream_trim_options::maxlen(2));
    EXPECT_EQ(removed, 3);
    EXPECT_EQ(tpl->ops_for_stream().xlen(key), 2);

    redisdal::stream_add_options add_options;
    add_options.trim = redisdal::stream_trim_options::maxlen(2);
    auto new_id = tpl->ops_for_stream().xadd(key, {{"index", "5"}}, add_options);
    ASSERT_TRUE(new_id);
    EXPECT_EQ(tpl->ops_for_stream().xlen(key), 2);

    EXPECT_EQ(tpl->ops_for_stream().xtrim(key, redisdal::stream_trim_options::minid(*new_id)), 1);
    auto entries = tpl->ops_for_stream().xrange(key, "-", "+");
    ASSERT_EQ(entries.size(), std::size_t{1});
    EXPECT_EQ(entries[0].id, *new_id);
}

TEST_F(StreamOpsTest, XreadMultipleStreamsAndTimeout) {
    const key_type first_key = "test_stream_read_first";
    const key_type second_key = "test_stream_read_second";
    const key_type empty_key = "test_stream_read_empty";
    keys_to_clean.insert(first_key);
    keys_to_clean.insert(second_key);
    keys_to_clean.insert(empty_key);

    ASSERT_TRUE(tpl->ops_for_stream().xadd(first_key, {{"source", "first"}}));
    ASSERT_TRUE(tpl->ops_for_stream().xadd(second_key, {{"source", "second"}}));

    redisdal::stream_read_options options;
    options.count = 1;
    auto batches = tpl->ops_for_stream().xread({{first_key, "0-0"}, {second_key, "0-0"}}, options);
    ASSERT_EQ(batches.size(), std::size_t{2});
    EXPECT_EQ(batches[0].key, first_key);
    ASSERT_EQ(batches[0].entries.size(), std::size_t{1});
    EXPECT_EQ(batches[1].key, second_key);
    ASSERT_EQ(batches[1].entries.size(), std::size_t{1});

    redisdal::stream_read_options blocking_options;
    blocking_options.block_ms = 10;
    auto no_entries = tpl->ops_for_stream().xread({{empty_key, "$"}}, blocking_options);
    EXPECT_TRUE(no_entries.empty());
}

TEST_F(StreamOpsTest, ConsumerGroupReadAndAck) {
    const key_type key = "test_stream_group_read";
    const std::string group = "workers";
    keys_to_clean.insert(key);

    EXPECT_TRUE(tpl->ops_for_stream().xgroup_create(key, group, "0-0", true));
    auto first_id = tpl->ops_for_stream().xadd(key, {{"job", "one"}});
    auto second_id = tpl->ops_for_stream().xadd(key, {{"job", "two"}});
    ASSERT_TRUE(first_id);
    ASSERT_TRUE(second_id);

    redisdal::stream_read_group_options options;
    options.count = 2;
    auto batches = tpl->ops_for_stream().xreadgroup(group, "consumer-1", {{key, ">"}}, options);
    ASSERT_EQ(batches.size(), std::size_t{1});
    ASSERT_EQ(batches[0].entries.size(), std::size_t{2});
    EXPECT_EQ(batches[0].entries[0].id, *first_id);
    EXPECT_EQ(batches[0].entries[1].id, *second_id);

    EXPECT_EQ(tpl->ops_for_stream().xack(key, group, {*first_id, *second_id}), 2);
    EXPECT_EQ(tpl->ops_for_stream().xack(key, group, {}), 0);

    redisdal::stream_read_group_options timeout_options;
    timeout_options.block_ms = 10;
    auto no_entries = tpl->ops_for_stream().xreadgroup(group, "consumer-1", {{key, ">"}}, timeout_options);
    EXPECT_TRUE(no_entries.empty());
}

TEST_F(StreamOpsTest, ConsumerGroupManagement) {
    const key_type key = "test_stream_group_management";
    const std::string group = "workers";
    keys_to_clean.insert(key);

    EXPECT_TRUE(tpl->ops_for_stream().xgroup_create(key, group, "$", true));
    EXPECT_TRUE(tpl->ops_for_stream().xgroup_createconsumer(key, group, "consumer-1"));
    EXPECT_FALSE(tpl->ops_for_stream().xgroup_createconsumer(key, group, "consumer-1"));
    EXPECT_EQ(tpl->ops_for_stream().xgroup_delconsumer(key, group, "consumer-1"), 0);
    EXPECT_TRUE(tpl->ops_for_stream().xgroup_setid(key, group, "0-0"));
    EXPECT_TRUE(tpl->ops_for_stream().xgroup_destroy(key, group));
    EXPECT_FALSE(tpl->ops_for_stream().xgroup_destroy(key, group));
}

TEST_F(StreamOpsTest, ValidatesInvalidArguments) {
    const key_type key = "test_stream_validation";
    keys_to_clean.insert(key);

    EXPECT_THROW(tpl->ops_for_stream().xadd(key, {}), std::invalid_argument);
    EXPECT_THROW(tpl->ops_for_stream().xread({}), std::invalid_argument);

    redisdal::stream_read_options read_options;
    read_options.count = 0;
    EXPECT_THROW(tpl->ops_for_stream().xread({{key, "0-0"}}, read_options), std::invalid_argument);

    auto trim_options = redisdal::stream_trim_options::maxlen(10, false, 1);
    EXPECT_THROW(tpl->ops_for_stream().xtrim(key, trim_options), std::invalid_argument);
}

TEST_F(StreamOpsTest, SerializesTypedValues) {
    const key_type key = "test_stream_typed_values";
    keys_to_clean.insert(key);

    redisdal::string_serializer<int> int_serializer;
    redisdal::redis_template<std::string, int> int_tpl(*conn, key_serializer, int_serializer);
    auto id = int_tpl.ops_for_stream().xadd(key, {{"answer", 42}});
    ASSERT_TRUE(id);

    auto entries = int_tpl.ops_for_stream().xrange(key, "-", "+");
    ASSERT_EQ(entries.size(), std::size_t{1});
    ASSERT_EQ(entries[0].fields.size(), std::size_t{1});
    EXPECT_EQ(entries[0].fields[0].first, "answer");
    EXPECT_EQ(entries[0].fields[0].second, 42);
}
