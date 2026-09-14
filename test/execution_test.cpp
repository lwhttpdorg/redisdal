#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "redisdal/redisdal.hpp"

namespace {
    using namespace redisdal;

    class scripted_connection final: public redis_connection {
    public:
        void expect(std::vector<std::string> argv, cmd_reply reply) {
            expect_action(std::move(argv), [reply = std::move(reply)] { return reply; });
        }

        void expect_action(std::vector<std::string> argv, std::function<cmd_reply()> action) {
            steps.push_back({std::move(argv), std::move(action)});
        }

        [[nodiscard]] int get_fd() const override {
            return -1;
        }

        [[nodiscard]] size_t remaining() const {
            return steps.size();
        }

        [[nodiscard]] size_t call_count() const {
            return calls;
        }

    protected:
        cmd_reply cmd_execv(const std::vector<std::string> &args) override {
            ++calls;
            if (steps.empty()) {
                throw std::runtime_error("Unexpected executor call");
            }
            auto next = std::move(steps.front());
            steps.pop_front();
            if (args != next.argv) {
                throw std::runtime_error("Unexpected command arguments");
            }
            return next.action();
        }

        cmd_reply cmd_exec(const char *, ...) override {
            throw std::runtime_error("Unexpected formatted executor call");
        }

    private:
        struct step {
            std::vector<std::string> argv;
            std::function<cmd_reply()> action;
        };

        std::deque<step> steps;
        size_t calls{0};
    };

    class pipeline_inheritance_contract: public redis_async_executor, public redis_connection {};

    static_assert(std::is_abstract_v<redis_executor>);
    static_assert(std::is_abstract_v<redis_async_executor>);
    static_assert(std::is_abstract_v<redis_cmd_ops>);
    static_assert(std::is_abstract_v<redis_connection>);
    static_assert(std::is_base_of_v<redis_executor, redis_cmd_ops>);
    static_assert(std::is_base_of_v<redis_cmd_ops, redis_connection>);
    static_assert(std::is_base_of_v<redis_connection, sync_connection>);
    static_assert(std::is_convertible_v<pipeline_inheritance_contract *, redis_executor *>);
    static_assert(std::is_base_of_v<redis_executor, sync_connection>);

    class ExecutionTest: public testing::Test {
    protected:
        void SetUp() override {
            auto scripted = std::make_unique<scripted_connection>();
            backend = scripted.get();
            connection = std::make_unique<redis_client>(std::move(scripted));
            conn = connection.get();
        }

        void TearDown() override {
            EXPECT_EQ(backend->remaining(), 0U);
        }

        scripted_connection *backend{nullptr};
        std::unique_ptr<redis_client> connection;
        redis_client *conn{nullptr};
    };

    TEST(ExecutorContractTest, SignedIntegerViewPreservesLegacyRepresentation) {
        for (const int64_t value: {std::numeric_limits<int64_t>::min(), int64_t{-2}, int64_t{-1}, int64_t{0},
                                   std::numeric_limits<int64_t>::max()}) {
            const auto reply = cmd_reply::make_signed_integer(value);
            EXPECT_EQ(reply.get_signed_integer(), value);
            EXPECT_EQ(reply.get_integer(), static_cast<uint64_t>(value));
        }
        EXPECT_FALSE(cmd_reply::make_nil().get_signed_integer());
        EXPECT_EQ(cmd_reply::make_integer(std::numeric_limits<uint64_t>::max()).get_signed_integer(), -1);
    }

    TEST_F(ExecutionTest, EncodesBinaryAndEmptyArgumentsWithoutSplitting) {
        const std::string key("key\0 %s", 7);
        const std::string value("\0x\0", 3);
        backend->expect({"SET", key, value}, cmd_reply::make_status("OK"));
        backend->expect({"GET", key}, cmd_reply::make_string(value));
        backend->expect({"PING", ""}, cmd_reply::make_string(""));
        backend->expect({"custom verb", key, "", value}, cmd_reply::make_nil());
        EXPECT_TRUE(conn->set(key, value));
        EXPECT_EQ(conn->get(key), value);
        EXPECT_EQ(conn->ping(""), "");
        EXPECT_TRUE(conn->command("custom verb", {key, "", value}).is_nil());
    }

    TEST_F(ExecutionTest, PreservesDurationsWiderThanInt) {
        constexpr long long duration = 2147483648LL;
        backend->expect({"EXPIRE", "key", "2147483648"}, cmd_reply::make_signed_integer(1));
        backend->expect({"PEXPIRE", "key", "2147483648"}, cmd_reply::make_signed_integer(1));
        backend->expect({"SET", "key", "value", "EX", "2147483648"}, cmd_reply::make_status("OK"));
        backend->expect({"SET", "key", "value", "PX", "2147483648"}, cmd_reply::make_status("OK"));
        EXPECT_TRUE(conn->expire("key", duration));
        EXPECT_TRUE(conn->pexpire("key", duration));
        EXPECT_TRUE(conn->set_ex("key", "value", duration));
        EXPECT_TRUE(conn->set_px("key", "value", duration));
    }

    TEST_F(ExecutionTest, DecodesNilStatusAndNegativeIntegers) {
        backend->expect({"GET", "missing"}, cmd_reply::make_nil());
        backend->expect({"SET", "key", "value", "NX"}, cmd_reply::make_nil());
        backend->expect({"TTL", "missing"}, cmd_reply::make_signed_integer(-2));
        backend->expect({"PTTL", "key"}, cmd_reply::make_signed_integer(-1));
        backend->expect({"DECRBY", "counter", "5"}, cmd_reply::make_signed_integer(-5));
        backend->expect({"TYPE", "key"}, cmd_reply::make_status("string"));
        EXPECT_FALSE(conn->get("missing"));
        EXPECT_FALSE(conn->set_not_exists("key", "value"));
        EXPECT_EQ(conn->ttl("missing"), -2);
        EXPECT_EQ(conn->pttl("key"), -1);
        EXPECT_EQ(conn->decr("counter", 5), -5);
        EXPECT_EQ(conn->type("key"), "string");
    }

    TEST_F(ExecutionTest, PreservesTopLevelAndNestedErrorContracts) {
        backend->expect({"GET", "wrongtype"}, cmd_reply::make_error("WRONGTYPE test"));
        backend->expect({"CUSTOM"}, cmd_reply::make_error("ERR test"));
        backend->expect({"EVAL", "script", "0"},
                        cmd_reply::make_array({cmd_reply::make_signed_integer(-7), cmd_reply::make_nil(),
                                               cmd_reply::make_array({cmd_reply::make_error("ERR nested")})}));
        EXPECT_THROW(conn->get("wrongtype"), redis_error);
        EXPECT_THROW(conn->command("CUSTOM", {}), redis_error);
        const auto reply = conn->eval("script", {}, {});
        const auto &values = reply.get_array().value();
        ASSERT_EQ(values.size(), 3U);
        EXPECT_EQ(values[0].get_signed_integer(), -7);
        EXPECT_TRUE(values[1].is_nil());
        EXPECT_TRUE(values[2].get_array()->at(0).is_error());
    }

    TEST_F(ExecutionTest, PropagatesTransportFailureWithoutRetryingWrites) {
        backend->expect_action({"INCRBY", "counter", "1"},
                               []() -> cmd_reply { throw std::runtime_error("transport disconnected after write"); });
        try {
            conn->incr("counter", 1);
            FAIL() << "Expected transport error";
        }
        catch (const std::runtime_error &error) {
            EXPECT_STREQ(error.what(), "transport disconnected after write");
        }
        EXPECT_EQ(backend->call_count(), 1U);
    }

    TEST_F(ExecutionTest, PreservesEmptyCollectionBehavior) {
        const std::vector<std::string> empty;
        EXPECT_EQ(conn->del(empty), 0);
        EXPECT_EQ(conn->hdel("key", empty), 0);
        EXPECT_EQ(conn->sadd("key", empty), 0);
        EXPECT_EQ(conn->srem("key", empty), 0);
        EXPECT_EQ(conn->zadd("key", {}), 0);
        EXPECT_EQ(conn->zrem("key", empty), 0);
        EXPECT_EQ(conn->xdel("key", empty), 0);
        EXPECT_EQ(conn->xack("key", "group", empty), 0);
        EXPECT_TRUE(conn->sinter(empty).empty());
        EXPECT_FALSE(conn->hset("key", {}));
        std::unordered_map<std::string, std::optional<std::string>> hash;
        conn->hget("key", hash);
        EXPECT_EQ(backend->call_count(), 0U);
        backend->expect({"LLEN", "key"}, cmd_reply::make_integer(3));
        backend->expect({"LLEN", "key"}, cmd_reply::make_integer(3));
        EXPECT_EQ(conn->lpush("key", empty), 3);
        EXPECT_EQ(conn->rpush("key", empty), 3);
    }

    TEST_F(ExecutionTest, DecodesHashValuesInRequestedFieldOrder) {
        std::unordered_map<std::string, std::optional<std::string>> hash{{"present", "old"}, {"missing", "old"}};
        std::vector<std::string> argv{"HMGET", "hash"};
        std::vector<cmd_reply> replies;
        for (const auto &field: hash) {
            argv.push_back(field.first);
            replies.push_back(field.first == "missing" ? cmd_reply::make_nil() : cmd_reply::make_string("value"));
        }
        backend->expect(std::move(argv), cmd_reply::make_array(std::move(replies)));
        conn->hget("hash", hash);
        EXPECT_EQ(hash.at("present"), "value");
        EXPECT_FALSE(hash.at("missing"));
    }

    TEST_F(ExecutionTest, HandlesEmptyScanPagesAndPreservesExistingHashFields) {
        backend->expect({"SCAN", "0", "MATCH", "*", "COUNT", "10"},
                        cmd_reply::make_array({cmd_reply::make_string("42"), cmd_reply::make_array({})}));
        const auto scan = conn->scan(0, "*", 10);
        EXPECT_EQ(scan.cursor, 42U);
        EXPECT_TRUE(scan.keys.empty());
        backend->expect(
            {"HSCAN", "hash", "42", "MATCH", "*", "COUNT", "1"},
            cmd_reply::make_array(
                {cmd_reply::make_string("0"),
                 cmd_reply::make_array({cmd_reply::make_string("field"), cmd_reply::make_string("new"),
                                        cmd_reply::make_string("another"), cmd_reply::make_string("value")})}));
        std::unordered_map<std::string, std::string> fields{{"field", "old"}};
        EXPECT_EQ(conn->hscan("hash", 42, "*", 1, fields), 0U);
        EXPECT_EQ(fields.at("field"), "old");
        EXPECT_EQ(fields.at("another"), "value");
    }

    TEST_F(ExecutionTest, PreservesScorePrecisionAndListOrdering) {
        const double score = 1.2345678901234567;
        const auto encoded = string_serializable<double>::to_string(score);
        backend->expect({"ZADD", "zset", encoded, "member"}, cmd_reply::make_integer(1));
        backend->expect({"ZRANGE", "zset", "0", "-1", "WITHSCORES"},
                        cmd_reply::make_array({cmd_reply::make_string("member"), cmd_reply::make_string(encoded)}));
        backend->expect({"RPUSH", "list", "second", "first"}, cmd_reply::make_integer(2));
        backend->expect({"LPOP", "list", "2"},
                        cmd_reply::make_array({cmd_reply::make_string("second"), cmd_reply::make_string("first")}));
        EXPECT_EQ(conn->zadd("zset", {{"member", score}}), 1);
        const auto scores = conn->zrange_withscores("zset", 0, -1);
        ASSERT_EQ(scores.size(), 1U);
        EXPECT_DOUBLE_EQ(scores[0].second, score);
        EXPECT_EQ(conn->rpush("list", std::vector<std::string>{"second", "first"}), 2);
        EXPECT_EQ(conn->lpop("list", 2), (std::vector<std::string>{"second", "first"}));
    }

    TEST_F(ExecutionTest, EncodesStreamOptionsAndGroupedReadKeys) {
        stream_add_options add;
        add.nomkstream = true;
        add.trim = stream_trim_options::maxlen(100, true, 20);
        backend->expect({"XADD", "stream", "NOMKSTREAM", "MAXLEN", "~", "100", "LIMIT", "20", "*", "field", "value"},
                        cmd_reply::make_nil());
        EXPECT_FALSE(conn->xadd("stream", {{"field", "value"}}, add));
        backend->expect({"XREADGROUP", "GROUP", "group", "consumer", "COUNT", "2", "BLOCK", "0", "NOACK", "STREAMS",
                         "one", "two", ">", "0"},
                        cmd_reply::make_nil());
        EXPECT_TRUE(conn->xreadgroup("group", "consumer", {{"one", ">"}, {"two", "0"}}, {2, 0, true}).empty());
        backend->expect({"XREAD", "COUNT", "1", "BLOCK", "5", "STREAMS", "one", "two", "0", "$"},
                        cmd_reply::make_nil());
        EXPECT_TRUE(conn->xread({{"one", "0"}, {"two", "$"}}, {1, 5}).empty());
    }

    TEST_F(ExecutionTest, PreservesDuplicateStreamFieldsAndDeletedPendingEntries) {
        const auto entries = cmd_reply::make_array(
            {cmd_reply::make_array(
                 {cmd_reply::make_string("1-0"),
                  cmd_reply::make_array({cmd_reply::make_string("field"), cmd_reply::make_string("first"),
                                         cmd_reply::make_string("field"), cmd_reply::make_string("second")})}),
             cmd_reply::make_array({cmd_reply::make_string("2-0"), cmd_reply::make_nil()})});
        backend->expect({"XREADGROUP", "GROUP", "g", "c", "STREAMS", "s", "0"},
                        cmd_reply::make_array({cmd_reply::make_array({cmd_reply::make_string("s"), entries})}));
        const auto batches = conn->xreadgroup("g", "c", {{"s", "0"}}, {});
        ASSERT_EQ(batches.size(), 1U);
        ASSERT_EQ(batches[0].entries.size(), 2U);
        const auto &first = batches[0].entries[0];
        EXPECT_EQ(first.fields,
                  (std::vector<std::pair<std::string, std::string>>{{"field", "first"}, {"field", "second"}}));
        EXPECT_EQ(batches[0].entries[1].id, "2-0");
        EXPECT_TRUE(batches[0].entries[1].fields.empty());
    }

    TEST_F(ExecutionTest, ValidatesStreamAndScriptArgumentsBeforeExecution) {
        EXPECT_THROW(conn->xadd("s", {}, {}), std::invalid_argument);
        EXPECT_THROW(conn->xadd("s", {{"f", "v"}}, {"", false, std::nullopt}), std::invalid_argument);
        EXPECT_THROW(conn->xread({}, {}), std::invalid_argument);
        EXPECT_THROW(conn->xread({{"s", ""}}, {}), std::invalid_argument);
        EXPECT_THROW(conn->xread({{"s", "$"}}, {0, std::nullopt}), std::invalid_argument);
        EXPECT_THROW(conn->xread({{"s", "$"}}, {std::nullopt, -1}), std::invalid_argument);
        EXPECT_THROW(conn->xtrim("s", stream_trim_options::maxlen(10, false, 1)), std::invalid_argument);
        EXPECT_THROW(conn->xtrim("s", stream_trim_options::maxlen(10, true, -1)), std::invalid_argument);
        EXPECT_THROW(conn->eval("script", std::vector<std::string>(65, "key"), {}), too_many_script_keys_error);
        EXPECT_THROW(conn->eval_sha1("sha", std::vector<std::string>(65, "key"), {}), too_many_script_keys_error);
        EXPECT_EQ(backend->call_count(), 0U);
    }

    TEST_F(ExecutionTest, RejectsMalformedStructuredReplies) {
        backend->expect({"HGETALL", "hash"}, cmd_reply::make_array({cmd_reply::make_string("unpaired")}));
        backend->expect({"ZRANGE", "zset", "0", "-1", "WITHSCORES"},
                        cmd_reply::make_array({cmd_reply::make_string("unpaired")}));
        backend->expect({"XRANGE", "stream", "-", "+"},
                        cmd_reply::make_array({cmd_reply::make_array({cmd_reply::make_string("1-0")})}));
        backend->expect({"TTL", "key"}, cmd_reply::make_string("-2"));
        EXPECT_THROW(conn->hgetall("hash"), std::runtime_error);
        EXPECT_THROW(conn->zrange_withscores("zset", 0, -1), std::runtime_error);
        EXPECT_THROW(conn->xrange("stream", "-", "+", std::nullopt), std::runtime_error);
        EXPECT_THROW(conn->ttl("key"), unexpected_reply_type_error);
    }

    TEST_F(ExecutionTest, DistinguishesNoScriptFromOtherServerErrors) {
        backend->expect({"SCRIPT", "LOAD", "bad script"}, cmd_reply::make_error("ERR compile failed"));
        backend->expect({"EVALSHA", "sha", "1", "key", "arg"}, cmd_reply::make_error("NOSCRIPT missing"));
        backend->expect({"EVALSHA", "sha", "0"}, cmd_reply::make_error("ERR script failed"));
        EXPECT_THROW(conn->script_load("bad script"), redis_error);
        EXPECT_THROW(conn->eval_sha1("sha", {"key"}, {"arg"}), no_script_error);
        EXPECT_THROW(conn->eval_sha1("sha", {}, {}), redis_error);
    }

    TEST_F(ExecutionTest, TypedFacadeReloadsMissingScriptThroughInjectedExecutor) {
        string_serializer<std::string> serializer;
        redis_template<std::string, std::string> tpl(*conn, serializer, serializer);
        backend->expect({"SCRIPT", "LOAD", "return ARGV[1]"}, cmd_reply::make_string("sha"));
        backend->expect({"EVALSHA", "sha", "1", "key", "arg"}, cmd_reply::make_error("NOSCRIPT missing"));
        backend->expect({"SCRIPT", "LOAD", "return ARGV[1]"}, cmd_reply::make_string("sha"));
        backend->expect({"EVALSHA", "sha", "1", "key", "arg"}, cmd_reply::make_string("arg"));
        EXPECT_EQ(tpl.script_load("return ARGV[1]"), "sha");
        EXPECT_EQ(tpl.eval_sha1("sha", {"key"}, {"arg"}).get_string(), "arg");
    }
} // namespace
