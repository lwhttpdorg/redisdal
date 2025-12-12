#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "janus/janus.hpp"

#include "test_env.hpp"

class execute_cmd_lua_test: public testing::Test {
protected:
	using key_type = std::string;
	using value_type = unsigned long long;

	std::shared_ptr<janus::serializer<key_type>> k_serializer;
	std::shared_ptr<janus::serializer<value_type>> v_serializer;
	std::unique_ptr<janus::redis_template<key_type, value_type>> tpl;
	std::shared_ptr<janus::kv_connection> connection;

	void SetUp() override {
		// 1. Retrieve connection parameters from environment variables
		std::string redis_url = get_redis_connection_url();

		// 2. Create underlying connection
		connection = std::make_shared<janus::redis_connection>(redis_url);

		k_serializer = std::make_shared<janus::string_serializer<key_type>>();
		v_serializer = std::make_shared<janus::string_serializer<value_type>>();

		tpl = std::make_unique<janus::redis_template<key_type, value_type>>(*connection, *k_serializer, *v_serializer);

		clear_keys();
	}

	void TearDown() override {
		if (tpl) {
			clear_keys();
		}
	}

	void clear_keys() const {
		tpl->del("test_exec_cmd_key");
		tpl->del("test_lua_incr_key");
	}

	[[nodiscard]] auto &value_ops() const {
		return tpl->ops_for_value();
	}
};

// Execute a raw Redis command (SET + GET) via low-level connection
TEST_F(execute_cmd_lua_test, exec_command_set_get) {
	const key_type key = "test_exec_cmd_key";

	try {
		std::vector<std::string> args = {key, "4242"};
		tpl->exec_cmd("SET", args);

		// Verify using high-level value operations
		auto val = value_ops().get(key);
		ASSERT_TRUE(val) << "GET after raw SET returned no value";
		EXPECT_EQ(*val, 4242ULL);
	}
	catch (const std::exception &e) {
		FAIL() << "exec_command_set_get failed with exception: " << e.what();
	}
}

// Evaluate a small Lua script that increments a numeric key by ARGV[1]
TEST_F(execute_cmd_lua_test, eval_lua_incr_script) {
	const key_type key = "test_lua_incr_key";

	// Initialize key to 10
	ASSERT_TRUE(value_ops().set(key, 10ULL));

	// Lua script: increment KEYS[1] by ARGV[1] and return the new value
	const std::string script = "return redis.call('INCRBY', KEYS[1], ARGV[1])";

	try {
		std::vector keys = {key};
		std::vector<value_type> args = {7};

		janus::cmd_reply reply = tpl->eval(script, keys, args);

		ASSERT_EQ(reply.get_type(), janus::reply_type::INTEGER) << "Lua script did not return an integer result";
		std::optional<uint64_t> new_val = reply.get_integer();
		ASSERT_TRUE(new_val.has_value()) << "Lua script returned no integer value";

		EXPECT_EQ(new_val.value(), 17ULL) << "Lua script did not return expected incremented value";

		// Also verify via high-level get
		auto current = value_ops().get(key);
		ASSERT_TRUE(current);
		EXPECT_EQ(*current, 17ULL);
	}
	catch (const std::exception &e) {
		FAIL() << "eval_lua_incr_script failed with exception: " << e.what();
	}
}

// Test EVALSHA path and automatic reload when server reports NOSCRIPT
TEST_F(execute_cmd_lua_test, eval_sha1_autoreload) {
	const key_type key = "test_lua_incr_key";

	// Initialize key to 100
	ASSERT_TRUE(value_ops().set(key, 100ULL));

	const std::string script = "return redis.call('INCRBY', KEYS[1], ARGV[1])";

	try {
		// Load script and get sha1
		std::string sha = tpl->script_load(script);

		std::vector keys = {key};
		std::vector<value_type> args = {5};

		// First call using EVALSHA should work
		janus::cmd_reply r1 = tpl->eval_sha1(sha, keys, args);
		ASSERT_EQ(r1.get_type(), janus::reply_type::INTEGER);
		EXPECT_EQ(r1.get_integer().value_or(0), 105ULL);

		// Flush server script cache to force NOSCRIPT on next EVALSHA
		tpl->exec_cmd("SCRIPT", {"FLUSH"});

		// Second call should trigger automatic reload and succeed
		janus::cmd_reply r2 = tpl->eval_sha1(sha, keys, args);
		ASSERT_EQ(r2.get_type(), janus::reply_type::INTEGER);
		EXPECT_EQ(r2.get_integer().value_or(0), 110ULL);
	}
	catch (const std::exception &e) {
		FAIL() << "eval_sha1_autoreload failed with exception: " << e.what();
	}
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
