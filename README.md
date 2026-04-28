# 📚 Janus: C++ Redis Template Interface

<!-- TOC -->
- [1. 🛠️ Prerequisites](#1.-%F0%9F%9B%A0%EF%B8%8F-prerequisites)
- [2. ⚙️ Building the Library](#2.-%E2%9A%99%EF%B8%8F-building-the-library)
  - [2.1. Step 1: Clone the Repository](#2.1.-step-1%3A-clone-the-repository)
  - [2.2. Step 2: Configure](#2.2.-step-2%3A-configure)
  - [2.3. Step 3: Compile](#2.3.-step-3%3A-compile)
- [3. ✅ Running Tests](#3.-%E2%9C%85-running-tests)
  - [3.1. Enabling Tests](#3.1.-enabling-tests)
  - [3.2. Executing Tests with](#3.2.-executing-tests-with)
- [4. 🚀 Usage](#4.-%F0%9F%9A%80-usage)
  - [4.1. Project Integration](#4.1.-project-integration)
  - [4.2. Quick Start](#4.2.-quick-start)
  - [4.3. Custom Type Serialization](#4.3.-custom-type-serialization)
  - [4.4. Full Example](#4.4.-full-example)
- [5. 📜 License](#5.-%F0%9F%93%9C-license)
<!-- /TOC -->

Janus is a lightweight, modern C++ library designed to provide a high-level, template-based interface for interacting
with the Redis key-value store. It abstracts away the low-level details of connection handling and data serialization,
allowing developers to focus on application logic using native C++ types (`K` and `V`) for keys and values.

Janus is built as a shared library that provides both headers and a compiled component for other projects to link against.

## 1. 🛠️ Prerequisites

To build and use Janus successfully, you must meet the following requirements:

1. **C++ Standard**: The compiler must support the C++17 standard (e.g., GCC, Clang, MSVC).
2. **CMake/Meson**: CMake 3.10+ or Meson 1.1.0+.
3. **hiredis**: The underlying C client library for Redis.
4. **Redis Server**: A running Redis instance is required for integration testing.

## 2. ⚙️ Building the Library

Janus uses `CMake` or `Meson` for its build process. Please follow the standard out-of-source build steps.

### 2.1. Step 1: Clone the Repository

```shell
git clone https://github.com/lwhttpdorg/janus.git
cd janus
```

### 2.2. Step 2: Configure

Create a separate build directory and execute CMake from within it.

**Windows(mingw64)**

```shell
cmake -S . -B build -G "MinGW Makefiles" -DENABLE_JANUS_TEST=ON -DCMAKE_PREFIX_PATH="D:/OpenCode/hiredis"
```

or:

```shell
meson setup build . -Denable_janus_test=true --pkg-config-path="D:/OpenCode/hiredis/lib/pkgconfig"
```

**Linux**

```shell
cmake -S . -B build -G "Unix Makefiles" -DENABLE_JANUS_TEST=ON
```

or:

```shell
meson setup build . -Denable_janus_test=true
```

### 2.3. Step 3: Compile

Compile the project using your chosen build tool:

```shell
cmake --build build --config=Debug -j $(nproc)
```

or:

```shell
meson compile -C build -j $(nproc)
```

## 3. ✅ Running Tests

Tests are optional and require a running Redis instance. They are enabled by the CMake option `ENABLE_JANUS_TEST`.

### 3.1. Enabling Tests

To include the tests in your build, enable the option during CMake configuration:

```shell
cmake -S . -B build -DENABLE_JANUS_TEST=ON
```

or:

```shell
meson setup build . -Denable_janus_test=true
```

### 3.2. Executing Tests with

The tests are configured to read the Redis connection parameters from command-line environment variables. This allows
for flexible testing against various Redis instances. The default connection is `127.0.0.1:6379`.

📌 Using Default Redis (127.0.0.1:6379):

```shell
ctest --test-dir build --verbose
```

or:

```shell
meson test -C build --verbose --print-errorlogs
```

📌 Using Custom Redis Host/Port:
To use a remote Redis instance, set the `REDIS_HOST` environment variables before running `ctest`.

**Linux**

```shell
REDIS_HOST="tcp://172.17.57.112:6379" ctest --test-dir build --verbose
```

or:

```shell
REDIS_HOST="tcp://172.17.57.112:6379" meson test -C build
```

**Windows (PowerShell)**

```shell
$env:REDIS_HOST="tcp://172.17.57.112:6379"; ctest --test-dir build --verbose
```

or:

```shell
$env:REDIS_HOST="tcp://172.17.57.112:6379"; meson test -C build
```

## 4. 🚀 Usage

You integrate Janus into your own CMake or Meson project by linking your targets against the `janus` library.

### 4.1. Project Integration

**CMake (FetchContent)**

```cmake
cmake_minimum_required(VERSION 3.11)
project(my_app)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(janus
  GIT_REPOSITORY https://github.com/lwhttpdorg/janus.git
  GIT_TAG v1.2  # Or main for the latest
)
FetchContent_MakeAvailable(janus)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE janus)
```

**Meson (subproject / wrap)**

Place a `janus.wrap` file in your `subprojects/` directory:

```ini
[wrap-git]
directory = janus
url = https://github.com/lwhttpdorg/janus.git
revision = v1.2
```

Then in your `meson.build`:

```meson
project('my_app', 'cpp', default_options: ['cpp_std=c++17'])

janus_proj = subproject('janus')
janus_dep = janus_proj.get_variable('janus_dep')

executable('my_app', 'main.cpp', dependencies: janus_dep)
```

By depending on `janus`, your project automatically inherits:

- The necessary include directories
- The hiredis dependency

### 4.2. Quick Start

Janus provides the template layer (`redis_template`) and specialized operation classes (e.g., `list_operations`) that manage
serialization and connection handling, enabling clean, type-safe Redis interactions:

```cpp
#include "janus/janus.hpp"

int main() {
    // Connect to Redis (connection is established in the constructor)
    janus::redis_connection conn("tcp://127.0.0.1:6379");

    // string_redis_template is a convenience alias for redis_template<string, string>
    // that manages its own serializer instances internally
    janus::string_redis_template tpl(conn);

    // String operations
    tpl.ops_for_value().set("greeting", "hello world");
    auto val = tpl.ops_for_value().get("greeting");  // std::optional<std::string>

    // Hash operations
    tpl.ops_for_hash().hset("user:1", {{"name", "Alice"}, {"role", "admin"}});
    auto name = tpl.ops_for_hash().hget("user:1", "name");

    // List operations
    tpl.ops_for_list().rpush("queue", {"task1", "task2", "task3"});
    auto task = tpl.ops_for_list().lpop("queue");

    // Set operations
    tpl.ops_for_set().sadd("tags", {"cpp", "redis", "janus"});
    bool has_cpp = tpl.ops_for_set().sismember("tags", "cpp");

    // Sorted set operations
    tpl.ops_for_zset().zadd("leaderboard", {{"alice", 100.0}, {"bob", 85.5}});
    auto top = tpl.ops_for_zset().zrevrange_withscores("leaderboard", 0, -1);

    // Key-level operations live on redis_template directly
    tpl.expire("greeting", 60);
    tpl.del("greeting");

    return 0;
}
```

### 4.3. Custom Type Serialization

A powerful feature of Janus is its ability to map custom C++ objects to Redis Hashes. Here’s how you can define a `User`
object and a mapper to automatically handle its serialization.

```cpp
// Define your custom object
struct User {
	long long id{};
	std::string username;
	int level{};
};

// Create a hash mapper for your custom type
class user_hash_mapper {
public:
	static std::unordered_map<std::string, std::string> to_hash(const User &user) {
		return {{"id", std::to_string(user.id)}, {"username", user.username}, {"level", std::to_string(user.level)}};
	}

	static User from_hash(const std::unordered_map<std::string, std::string> &hash) {
		User user;
		if (hash.count("id")) user.id = std::stoll(hash.at("id"));
		if (hash.count("username")) user.username = hash.at("username");
		if (hash.count("level")) user.level = std::stoi(hash.at("level"));
		return user;
	}
};
```

Now you can use this mapper with `redis_template` to work with `User` objects directly.

### 4.4. Full Example

The following example demonstrates operations for all major data types, including the custom `User` hash.

```c++
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "janus/janus.hpp"

// Define a custom object for hash mapping demonstration
struct User
{
	long long id{};
	std::string username;
	int level{};
};

// Create a hash mapper for your custom type
class user_hash_mapper
{
public:
	static std::unordered_map<std::string, std::string> to_hash(const User &user)
	{
		return {{"id", std::to_string(user.id)}, {"username", user.username}, {"level", std::to_string(user.level)}};
	}

	static User from_hash(const std::unordered_map<std::string, std::string> &hash)
	{
		User user;
		if (hash.count("id"))
			user.id = std::stoll(hash.at("id"));
		if (hash.count("username"))
			user.username = hash.at("username");
		if (hash.count("level"))
			user.level = std::stoi(hash.at("level"));
		return user;
	}
};

int main()
{
	// 1. Create the underlying connection using a URL from the environment
	std::string redis_url = "unix:///run/valkey/valkey.sock";
	auto conn = std::make_shared<janus::redis_connection>(redis_url);

	// 2. Create serializers for different data types
	auto string_serializer = std::make_shared<janus::string_serializer<std::string>>();
	auto int_serializer = std::make_shared<janus::string_serializer<long long>>();

	// Create a template for string keys and string values
	janus::redis_template<std::string, std::string> string_tpl(*conn, *string_serializer, *string_serializer);
	// Create a template for string keys and integer values
	janus::redis_template<std::string, long long> int_tpl(*conn, *string_serializer, *int_serializer);

	// === String Operations ===
	std::cout << "\n--- String Operations ---" << std::endl;
	auto &value_ops = string_tpl.ops_for_value();
	value_ops.set("my_string", "hello");
	// Use the integer template for INCR operations
	int_tpl.ops_for_value().incr("my_counter", 10);
	if (auto val = value_ops.get("my_string"))
	{
		std::cout << "GET my_string: " << *val << std::endl;
	}

	// === Hash Operations ===
	std::cout << "\n--- Hash Operations ---" << std::endl;
	auto &hash_ops = string_tpl.ops_for_hash();
	hash_ops.hset("my_hash", {{"field1", "value1"}, {"field2", "value2"}});
	if (auto h_val = hash_ops.hget("my_hash", "field1"))
	{
		std::cout << "HGET my_hash field1: " << *h_val << std::endl;
	}

	// === Custom Hash Object Operations ===
	std::cout << "\n--- Custom Hash (User Object) ---" << std::endl;
	auto &user_hash_ops = string_tpl.ops_for_hash();

	User user_to_save = {101, "Sandro", 99};
	user_hash_ops.hset("user:101", user_hash_mapper::to_hash(user_to_save));
	std::cout << "Saved user 'Sandro' to hash user:101" << std::endl;

	auto fetched_hash = user_hash_ops.hgetall("user:101");
	if (!fetched_hash.empty())
	{
		User fetched_user = user_hash_mapper::from_hash(fetched_hash);
		std::cout << "Fetched user: id=" << fetched_user.id << ", username=" << fetched_user.username
				  << ", level=" << fetched_user.level << std::endl;
	}

	// === List Operations ===
	std::cout << "\n--- List Operations ---" << std::endl;
	auto &list_ops = string_tpl.ops_for_list();
	list_ops.rpush("my_list", {"A", "B", "C"});
	auto l_range = list_ops.lrange("my_list", 0, -1);
	std::cout << "LRANGE my_list: ";
	for (const auto &item : l_range)
	{
		std::cout << item << " ";
	}
	std::cout << std::endl;

	// === Set Operations ===
	std::cout << "\n--- Set Operations ---" << std::endl;
	auto &set_ops = string_tpl.ops_for_set();
	set_ops.sadd("my_set", {"member1", "member2", "member3"});
	if (set_ops.sismember("my_set", "member2"))
	{
		std::cout << "SISMEMBER: my_set contains 'member2'" << std::endl;
	}

	// === Sorted Set (ZSet) Operations ===
	std::cout << "\n--- Sorted Set Operations ---" << std::endl;
	auto &zset_ops = string_tpl.ops_for_zset();
	zset_ops.zadd("my_zset", {{"player1", 100.0}, {"player2", 250.5}, {"player3", 50.0}});
	auto z_range = zset_ops.zrange_withscores("my_zset", 0, -1);
	std::cout << "ZRANGE my_zset (with scores):" << std::endl;
	for (const auto &[member, score] : z_range)
	{
		std::cout << "  " << member << ": " << score << std::endl;
	}

	// === Lua Scripting ===
	std::cout << "\n--- Lua Scripting ---" << std::endl;
	const std::string lua_script = "return redis.call('HSET', KEYS[1], ARGV[1], ARGV[2])";
	std::string sha1;
	try
	{
		sha1 = conn->script_load(lua_script);
		std::cout << "Script loaded, SHA1: " << sha1 << std::endl;
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to load script: " << e.what() << std::endl;
		return 1;
	}

	// Execute using EVALSHA for better performance
	try
	{
		conn->eval_sha1(sha1, {"my_hash"}, {"field3", "from_script"});
		std::cout << "EVALSHA successful." << std::endl;
	}
	catch (const janus::no_script_error &e)
	{
		// If Redis cleared its script cache, fall back to EVAL
		std::cerr << "NOSCRIPT error, falling back to EVAL: " << e.what() << std::endl;
		conn->eval(lua_script, {"my_hash"}, {"field3", "from_script"});
	}

	if (auto script_val = hash_ops.hget("my_hash", "field3"))
	{
		std::cout << "Value set by script: " << *script_val << std::endl;
	}

	// === Generic Key Operations ===
	std::cout << "\n--- Generic Key Operations ---" << std::endl;
	const std::string generic_key = "my_generic_key";
	value_ops.set(generic_key, "some value");

	// Check if a key exists
	if (string_tpl.exists(generic_key))
	{
		std::cout << "EXISTS: Key '" << generic_key << "' exists." << std::endl;
	}

	// Set an expiration time (in seconds)
	string_tpl.expire(generic_key, 120);
	if (auto ttl = string_tpl.ttl(generic_key); ttl > 0)
	{
		std::cout << "TTL: Key '" << generic_key << "' will expire in " << ttl << " seconds." << std::endl;
	}

	// Check the data type of the key
	std::cout << "TYPE: The data type of key '" << generic_key << "' is " << string_tpl.type(generic_key) << "."
			  << std::endl;

	// === Final Cleanup ===
	std::cout << "\n--- Cleaning up all keys ---" << std::endl;
	long long deleted_count = string_tpl.del(
		{"my_string", "my_counter", "my_hash", "user:101", "my_list", "my_set", "my_zset", "my_generic_key"});
	std::cout << "Deleted " << deleted_count << " keys." << std::endl;

	return 0;
}
```

## 5. 📜 License

This project uses the [Apache License 2.0](LICENSE) license
