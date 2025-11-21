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

	std::string redis_host;
	unsigned short redis_port{DEFAULT_REDIS_PORT};

	std::shared_ptr<janus::serializer<key_type>> k_serializer;
	std::shared_ptr<janus::serializer<value_type>> v_serializer;
	std::unique_ptr<janus::redis_template<key_type, value_type>> tpl;

	void SetUp() override {
		auto [h, p] = get_redis_connection_params();
		redis_host = h;
		redis_port = p;

		std::shared_ptr<janus::kv_connection> conn = std::make_shared<janus::redis_connection>(redis_host, redis_port);

		k_serializer = std::make_shared<janus::string_serializer<key_type>>();
		v_serializer = std::make_shared<janus::string_serializer<value_type>>();

		tpl = std::make_unique<janus::redis_template<key_type, value_type>>(conn, k_serializer, v_serializer);

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
		std::vector<key_type> keys = {key};
		std::vector<value_type> args = {7};

		janus::cmd_result reply = tpl->eval(script, keys, args);

		ASSERT_EQ(reply.get_type(), janus::cmd_result::result_type::INTEGER)
			<< "Lua script did not return an integer result";
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

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
