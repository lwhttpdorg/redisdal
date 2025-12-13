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

class LuaAndRawCmdTest: public testing::Test {
protected:
	using key_type = std::string;
	using value_type = unsigned long long;

	std::unique_ptr<janus::redis_connection> conn;
	std::unique_ptr<janus::redis_template<key_type, value_type>> tpl;
	std::set<key_type> keys_to_clean;
	janus::string_serializer<key_type> key_serializer{};
	janus::string_serializer<value_type> value_serializer{};

	LuaAndRawCmdTest() {
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

TEST_F(LuaAndRawCmdTest, exec_command_set_get) {
	const key_type key = "test_exec_cmd_key";
	keys_to_clean.insert(key);

	try {
		std::vector<std::string> args = {key, "4242"};
		tpl->exec_cmd("SET", args);

		auto val = tpl->ops_for_value().get(key);
		ASSERT_TRUE(val) << "GET after raw SET returned no value";
		EXPECT_EQ(*val, 4242ULL);
	}
	catch (const std::exception &e) {
		FAIL() << "exec_command_set_get failed with exception: " << e.what();
	}
}

TEST_F(LuaAndRawCmdTest, eval_lua_incr_script) {
	const key_type key = "test_lua_incr_key";
	keys_to_clean.insert(key);

	ASSERT_TRUE(tpl->ops_for_value().set(key, 10ULL));

	// Lua script: increment KEYS[1] by ARGV[1] and return the new value
	const std::string script = "return redis.call('INCRBY', KEYS[1], ARGV[1])";

	try {
		std::vector<key_type> keys = {key};
		std::vector<value_type> args = {7};

		janus::cmd_reply reply = tpl->eval(script, keys, args);

		ASSERT_EQ(reply.get_type(), janus::reply_type::INTEGER) << "Lua script did not return an integer result";
		std::optional<uint64_t> new_val = reply.get_integer();
		ASSERT_TRUE(new_val.has_value()) << "Lua script returned no integer value";

		EXPECT_EQ(new_val.value(), 17ULL) << "Lua script did not return expected incremented value";

		auto current = tpl->ops_for_value().get(key);
		ASSERT_TRUE(current);
		EXPECT_EQ(*current, 17ULL);
	}
	catch (const std::exception &e) {
		FAIL() << "eval_lua_incr_script failed with exception: " << e.what();
	}
}

TEST_F(LuaAndRawCmdTest, eval_sha1_autoreload) {
	const key_type key = "test_lua_incr_key";
	keys_to_clean.insert(key);

	ASSERT_TRUE(tpl->ops_for_value().set(key, 100ULL));

	const std::string script = "return redis.call('INCRBY', KEYS[1], ARGV[1])";

	try {
		// Load script and get sha1
		std::string sha = tpl->script_load(script);

		std::vector<key_type> keys = {key};
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
