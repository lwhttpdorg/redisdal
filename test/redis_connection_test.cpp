#include <utility>

#include "gtest/gtest.h"
#include "redisdal/redisdal.hpp"

#include "test_env.hpp"

class RedisConnectionTest: public testing::Test {
protected:
    std::unique_ptr<redisdal::redis_client> conn;
    std::unique_ptr<redisdal::string_redis_template> tpl;

    RedisConnectionTest() {
        std::string redis_url = get_redis_connection_url();
        conn = std::make_unique<redisdal::redis_client>(redis_url);
        tpl = std::make_unique<redisdal::string_redis_template>(*conn);
    }
};

TEST(ParseRedisUrlTest, TcpScheme) {
    // Basic TCP URL with host and port
    {
        auto config = redisdal::parse_redis_url("tcp://localhost:6380");
        EXPECT_EQ(config.scheme, redisdal::redis_scheme::TCP);
        EXPECT_EQ(config.host, "localhost");
        EXPECT_EQ(config.port, 6380);
        EXPECT_EQ(config.index, 0);
        EXPECT_TRUE(config.username.empty());
        EXPECT_TRUE(config.password.empty());
    }

    // TCP URL with just host, default port
    {
        auto config = redisdal::parse_redis_url("tcp://127.0.0.1");
        EXPECT_EQ(config.scheme, redisdal::redis_scheme::TCP);
        EXPECT_EQ(config.host, "127.0.0.1");
        EXPECT_EQ(config.port, 6379);
    }

    // TCP URL with IPv6
    {
        auto config = redisdal::parse_redis_url("tcp://[::1]:1234");
        EXPECT_EQ(config.scheme, redisdal::redis_scheme::TCP);
        EXPECT_EQ(config.host, "::1");
        EXPECT_EQ(config.port, 1234);
    }

    // TCP URL with username and password
    {
        auto config = redisdal::parse_redis_url("tcp://user:secret@my.redis.com:1234");
        EXPECT_EQ(config.scheme, redisdal::redis_scheme::TCP);
        EXPECT_EQ(config.host, "my.redis.com");
        EXPECT_EQ(config.port, 1234);
        EXPECT_EQ(config.username, "user");
        EXPECT_EQ(config.password, "secret");
    }

    // TCP URL with username and no password
    {
        auto config = redisdal::parse_redis_url("tcp://user@my.redis.com");
        EXPECT_EQ(config.host, "my.redis.com");
        EXPECT_EQ(config.port, 6379);
        EXPECT_EQ(config.username, "user");
        EXPECT_TRUE(config.password.empty());
    }

    // TCP URL with index parameter
    {
        auto config = redisdal::parse_redis_url("tcp://localhost?index=5");
        EXPECT_EQ(config.host, "localhost");
        EXPECT_EQ(config.port, 6379);
        EXPECT_EQ(config.index, 5);
    }

    // Full TCP URL
    {
        auto config = redisdal::parse_redis_url("tcp://user:secret@host:1234?index=2");
        EXPECT_EQ(config.scheme, redisdal::redis_scheme::TCP);
        EXPECT_EQ(config.host, "host");
        EXPECT_EQ(config.port, 1234);
        EXPECT_EQ(config.username, "user");
        EXPECT_EQ(config.password, "secret");
        EXPECT_EQ(config.index, 2);
    }
}

TEST(ParseRedisUrlTest, RedisScheme) {
    // 'redis://' is an alias for 'tcp://'
    auto config = redisdal::parse_redis_url("redis://user:secret@host:1234?index=2");
    EXPECT_EQ(config.scheme, redisdal::redis_scheme::REDIS);
    EXPECT_EQ(config.host, "host");
    EXPECT_EQ(config.port, 1234);
    EXPECT_EQ(config.username, "user");
    EXPECT_EQ(config.password, "secret");
    EXPECT_EQ(config.index, 2);
}

TEST(ParseRedisUrlTest, UnixScheme) {
    // Basic unix socket
    {
        auto config = redisdal::parse_redis_url("unix:///var/run/redis.sock");
        EXPECT_EQ(config.scheme, redisdal::redis_scheme::UNIX);
        EXPECT_EQ(config.host, "/var/run/redis.sock");
        EXPECT_EQ(config.port, 0); // Port is not applicable
    }

    // Unix socket with index and auth
    {
        auto config = redisdal::parse_redis_url("unix:///tmp/redis.sock?index=1&auth=secret");
        EXPECT_EQ(config.scheme, redisdal::redis_scheme::UNIX);
        EXPECT_EQ(config.host, "/tmp/redis.sock");
        EXPECT_EQ(config.index, 1);
        EXPECT_TRUE(config.username.empty());
        EXPECT_EQ(config.password, "secret");
    }

    // Unix socket with username and password
    {
        auto config = redisdal::parse_redis_url("unix:///tmp/redis.sock?username=user&password=pwd&index=2");
        EXPECT_EQ(config.scheme, redisdal::redis_scheme::UNIX);
        EXPECT_EQ(config.host, "/tmp/redis.sock");
        EXPECT_EQ(config.index, 2);
        EXPECT_EQ(config.username, "user");
        EXPECT_EQ(config.password, "pwd");
    }
}

TEST(ParseRedisUrlTest, InvalidUrls) {
    // Invalid scheme
    EXPECT_THROW(redisdal::parse_redis_url("http://localhost"), std::invalid_argument);

    // Missing host
    EXPECT_THROW(redisdal::parse_redis_url("tcp://"), std::invalid_argument);

    // Missing unix path
    EXPECT_THROW(redisdal::parse_redis_url("unix://"), std::invalid_argument); // Should fail as path is not absolute

    // Relative unix path (must be absolute)
    EXPECT_THROW(redisdal::parse_redis_url("unix://relative/path"), std::invalid_argument);
}

TEST_F(RedisConnectionTest, Ping) {
    // Test PING without a message
    EXPECT_EQ(tpl->ping(), "PONG");

    // Test PING with a message
    std::string message = "hello world";
    EXPECT_EQ(tpl->ping(message), message);
}

TEST(ParseRedisUrlTest, RejectsInvalidDatabaseIndices) {
    for (const char *index: {"", "-1", "1junk", "1.5", "4294967296", "18446744073709551616"}) {
        EXPECT_THROW(redisdal::parse_redis_url(std::string("tcp://localhost?index=") + index), std::invalid_argument)
            << index;
    }
}

TEST(ParseRedisUrlTest, RejectsLegacyDatabaseParameter) {
    EXPECT_THROW(redisdal::parse_redis_url("tcp://localhost?db=1"), std::invalid_argument);
}

TEST(RedisDatabaseTest, SelectsDatabaseBeforeExecutingCommands) {
    const std::string url = get_redis_connection_url();
    const std::string separator = url.find('?') == std::string::npos ? "?" : "&";
    redisdal::redis_client zero(url + separator + "index=0");
    redisdal::redis_client one(url + separator + "index=1");
    const std::string key = "redisdal:regression:database-selection";
    zero.set(key, "database-zero");
    one.set(key, "database-one");
    EXPECT_EQ(zero.get(key), "database-zero");
    EXPECT_EQ(one.get(key), "database-one");
    // Verify using an explicit SELECT as an independent oracle for the URL-selected DB.
    zero.command("SELECT", {"1"});
    EXPECT_EQ(zero.get(key), "database-one");
    zero.del(key);
    zero.command("SELECT", {"0"});
    zero.del(key);
}

TEST(RedisDatabaseTest, RejectsDatabaseSelectionFailureDuringConstruction) {
    const std::string url = get_redis_connection_url();
    const std::string separator = url.find('?') == std::string::npos ? "?" : "&";
    EXPECT_THROW(redisdal::redis_client connection(url + separator + "index=4294967295"), redisdal::redis_error);
}

TEST(RedisConnectionExecutionTest, ThrowsServerErrorsAndKeepsConnectionUsable) {
    redisdal::redis_client connection(get_redis_connection_url());
    EXPECT_THROW(connection.command("REDISDAL_NONEXISTENT_COMMAND", {}), redisdal::redis_error);
    EXPECT_EQ(connection.ping(), "PONG");
}

TEST(RedisConnectionExecutionTest, RepliesOutliveConnectionAndLaterCommands) {
    const std::string value("a\0b\0", 4);
    redisdal::cmd_reply reply;
    {
        redisdal::redis_client connection(get_redis_connection_url());
        reply =
            connection.eval("return {ARGV[1], -2, false, redis.pcall('REDISDAL_NONEXISTENT_COMMAND')}", {}, {value});
        // The reply must survive both subsequent hiredis calls and connection destruction.
        EXPECT_EQ(connection.ping(), "PONG");
    }
    const auto &values = reply.get_array().value();
    ASSERT_EQ(values.size(), 4U);
    EXPECT_EQ(values[0].get_string(), value);
    EXPECT_EQ(values[1].get_signed_integer(), -2);
    EXPECT_TRUE(values[2].is_nil());
    EXPECT_TRUE(values[3].is_error());
}

TEST(RedisConnectionExecutionTest, TransfersConnectionOwnershipOnMove) {
    redisdal::redis_client original(get_redis_connection_url());
    redisdal::redis_client moved(std::move(original));
    EXPECT_EQ(moved.ping(), "PONG");
    redisdal::redis_client target(get_redis_connection_url());
    target = std::move(moved);
    EXPECT_EQ(target.ping(), "PONG");
}
