#include "janus/redis_connection.hpp"
#include "gtest/gtest.h"
#include "janus/janus.hpp"

#include "test_env.hpp"

class RedisConnectionTest: public testing::Test {
protected:
	std::unique_ptr<janus::redis_connection> conn;
	std::unique_ptr<janus::string_redis_template> tpl;

	RedisConnectionTest() {
		std::string redis_url = get_redis_connection_url();
		conn = std::make_unique<janus::redis_connection>(redis_url);
		tpl = std::make_unique<janus::string_redis_template>(*conn);
	}
};

TEST(ParseRedisUrlTest, TcpScheme) {
	// Basic TCP URL with host and port
	{
		auto config = janus::parse_redis_url("tcp://localhost:6380");
		EXPECT_EQ(config.scheme, janus::redis_scheme::TCP);
		EXPECT_EQ(config.host, "localhost");
		EXPECT_EQ(config.port, 6380);
		EXPECT_EQ(config.db, 0);
		EXPECT_TRUE(config.username.empty());
		EXPECT_TRUE(config.password.empty());
	}

	// TCP URL with just host, default port
	{
		auto config = janus::parse_redis_url("tcp://127.0.0.1");
		EXPECT_EQ(config.scheme, janus::redis_scheme::TCP);
		EXPECT_EQ(config.host, "127.0.0.1");
		EXPECT_EQ(config.port, 6379);
	}

	// TCP URL with IPv6
	{
		auto config = janus::parse_redis_url("tcp://[::1]:1234");
		EXPECT_EQ(config.scheme, janus::redis_scheme::TCP);
		EXPECT_EQ(config.host, "::1");
		EXPECT_EQ(config.port, 1234);
	}

	// TCP URL with username and password
	{
		auto config = janus::parse_redis_url("tcp://user:secret@my.redis.com:1234");
		EXPECT_EQ(config.scheme, janus::redis_scheme::TCP);
		EXPECT_EQ(config.host, "my.redis.com");
		EXPECT_EQ(config.port, 1234);
		EXPECT_EQ(config.username, "user");
		EXPECT_EQ(config.password, "secret");
	}

	// TCP URL with username and no password
	{
		auto config = janus::parse_redis_url("tcp://user@my.redis.com");
		EXPECT_EQ(config.host, "my.redis.com");
		EXPECT_EQ(config.port, 6379);
		EXPECT_EQ(config.username, "user");
		EXPECT_TRUE(config.password.empty());
	}

	// TCP URL with db parameter
	{
		auto config = janus::parse_redis_url("tcp://localhost?db=5");
		EXPECT_EQ(config.host, "localhost");
		EXPECT_EQ(config.port, 6379);
		EXPECT_EQ(config.db, 5);
	}

	// Full TCP URL
	{
		auto config = janus::parse_redis_url("tcp://user:secret@host:1234?db=2");
		EXPECT_EQ(config.scheme, janus::redis_scheme::TCP);
		EXPECT_EQ(config.host, "host");
		EXPECT_EQ(config.port, 1234);
		EXPECT_EQ(config.username, "user");
		EXPECT_EQ(config.password, "secret");
		EXPECT_EQ(config.db, 2);
	}
}

TEST(ParseRedisUrlTest, RedisScheme) {
	// 'redis://' is an alias for 'tcp://'
	auto config = janus::parse_redis_url("redis://user:secret@host:1234?db=2");
	EXPECT_EQ(config.scheme, janus::redis_scheme::REDIS);
	EXPECT_EQ(config.host, "host");
	EXPECT_EQ(config.port, 1234);
	EXPECT_EQ(config.username, "user");
	EXPECT_EQ(config.password, "secret");
	EXPECT_EQ(config.db, 2);
}

TEST(ParseRedisUrlTest, UnixScheme) {
	// Basic unix socket
	{
		auto config = janus::parse_redis_url("unix:///var/run/redis.sock");
		EXPECT_EQ(config.scheme, janus::redis_scheme::UNIX);
		EXPECT_EQ(config.host, "/var/run/redis.sock");
		EXPECT_EQ(config.port, 0); // Port is not applicable
	}

	// Unix socket with db and auth
	{
		auto config = janus::parse_redis_url("unix:///tmp/redis.sock?db=1&auth=secret");
		EXPECT_EQ(config.scheme, janus::redis_scheme::UNIX);
		EXPECT_EQ(config.host, "/tmp/redis.sock");
		EXPECT_EQ(config.db, 1);
		EXPECT_TRUE(config.username.empty());
		EXPECT_EQ(config.password, "secret");
	}

	// Unix socket with username and password
	{
		auto config = janus::parse_redis_url("unix:///tmp/redis.sock?username=user&password=pwd&db=2");
		EXPECT_EQ(config.scheme, janus::redis_scheme::UNIX);
		EXPECT_EQ(config.host, "/tmp/redis.sock");
		EXPECT_EQ(config.db, 2);
		EXPECT_EQ(config.username, "user");
		EXPECT_EQ(config.password, "pwd");
	}
}

TEST(ParseRedisUrlTest, InvalidUrls) {
	// Invalid scheme
	EXPECT_THROW(janus::parse_redis_url("http://localhost"), std::invalid_argument);

	// Missing host
	EXPECT_THROW(janus::parse_redis_url("tcp://"), std::invalid_argument);

	// Missing unix path
	EXPECT_THROW(janus::parse_redis_url("unix://"), std::invalid_argument); // Should fail as path is not absolute

	// Relative unix path (must be absolute)
	EXPECT_THROW(janus::parse_redis_url("unix://relative/path"), std::invalid_argument);
}

TEST_F(RedisConnectionTest, Ping) {
	// Test PING without a message
	EXPECT_EQ(tpl->ping(), "PONG");

	// Test PING with a message
	std::string message = "hello world";
	EXPECT_EQ(tpl->ping(message), message);
}
