# 📚 Janus: C++ Redis Template Interface

Janus is a lightweight, modern C++ library designed to provide a high-level, template-based interface for interacting
with the Redis key-value store. It abstracts away the low-level details of connection handling and data serialization,
allowing developers to focus on application logic using native C++ types (`K` and `V`) for keys and values.

Janus is implemented as an Interface Library, meaning it primarily provides headers and API definitions for other
projects to link against.

## 🛠️ Prerequisites

To build and use Janus successfully, you must meet the following requirements:

1. **C++ Standard**: The compiler must support the C++17 standard (e.g., GCC, Clang, MSVC).
2. **CMake**: Version 3.11 or newer.
3. **hiredis**: The underlying C client library for Redis.
4. **Redis Server**: A running Redis instance is required for integration testing.

## ⚙️ Building the Library

Janus uses CMake for its build process. Please follow the standard out-of-source build steps.

### Step 1: Clone the Repository

```shell
git clone https://github.com/lwhttpdorg/janus.git
cd janus
```

### Step 2: Configure CMake

Create a separate build directory and execute CMake from within it.

Windows(mingw64):

```shell
cmake -S . -B build -G "MinGW Makefiles" -DENABLE_JANUS_TEST=ON -DCMAKE_PREFIX_PATH="D:/OpenCode/hiredis"
```

Linux:

```shell
cmake -S . -B build -G "Unix Makefiles" -DENABLE_JANUS_TEST=ON
```

⚠️ Note on Dependencies (`hiredis`):

Your `CMakeLists.txt` sets the `CMAKE_PREFIX_PATH` to help locate dependencies:

```cmake
set(CMAKE_PREFIX_PATH "D:/OpenCode/hiredis")
```

If your dependency libraries are installed in other locations, you **must** set the `CMAKE_PREFIX_PATH` variable during
configuration to specify the installation paths for `hiredis` and other required libraries.

```shell
cmake -DCMAKE_PREFIX_PATH="<Path/To/hiredis/install>;<Path/To/Other/Deps>"
```

### Step 3: Compile

Compile the project using your chosen build tool:

```shell
cmake --build build --config=Debug -j $(nproc)
```

## ✅ Running Tests

Tests are optional and require a running Redis instance. They are enabled by the CMake option `ENABLE_JANUS_TEST`.

### 1. Enabling Tests

To include the tests in your build, enable the option during CMake configuration:

```shell
cmake -S . -B build -DENABLE_JANUS_TEST=ON
```

### 2. Executing Tests with CTest

The tests are configured to read the Redis connection parameters from command-line environment variables. This allows
for flexible testing against various Redis instances. The default connection is `127.0.0.1:6379`.

📌 Using Default Redis (127.0.0.1:6379):

```shell
ctest --test-dir build --verbose
```

📌 Using Custom Redis Host/Port:
To use a remote Redis instance, set the `REDIS_HOST` environment variables before running `ctest`.

Linux / macOS (Bash/Zsh):

```shell
REDIS_HOST="tcp://172.17.57.112:6379" ctest --test-dir build --verbose
```

Windows (PowerShell):

```shell
$env:REDIS_HOST="tcp://172.17.57.112:6379"; ctest --test-dir build --verbose
```

## 🚀 Usage in Your Project

Janus is an `INTERFACE` library. You integrate it into your own CMake project by linking your targets against the
`janus` target.

### 1. Integration (Your `CMakeLists.txt`)

In your project's `CMakeLists.txt`:

```cmake
# Assume Janus is located in a known directory, or installed on the system
find_package(Janus REQUIRED)

add_executable(my_app src/main.cpp)

# Link your application against the Janus interface library
target_link_libraries(my_app PRIVATE janus)
```

By linking to `janus`, your project automatically inherits:

- The necessary include directories (`target_include_directories`).
- The required C++ standard (C++17).
- The underlying dependency (`hiredis::hiredis`).

### 2. High-Level OperationsJanus provides

the template layer (`redis_template`) and specialized operation classes (e.g., `list_operations`) that manage
serialization and connection handling, enabling clean, type-safe Redis interactions:

### 3. Working with Custom Types for Hashes

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

### 4. Comprehensive Example

The following example demonstrates operations for all major data types, including the custom `User` hash.

the template layer (`redis_template`) and specialized operation classes (e.g., `list_operations`) that manage
serialization and connection handling, enabling clean, type-safe Redis interactions:

```c++
#include <iostream>
#include <string>
#include <vector>

#include <janus/janus.hpp>

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

int main() {
	// 1. Create the underlying connection using a URL
	auto conn = std::make_shared<janus::redis_connection>("tcp://127.0.0.1:6379");

	// 2. Create serializers for different data types
	auto string_serializer = std::make_shared<janus::string_serializer<std::string>>();
	auto int_serializer = std::make_shared<janus::string_serializer<long long>>();

	// Create a template for string keys and string values
	janus::redis_template<std::string, std::string> string_tpl(*conn, *string_serializer, *string_serializer);
	// Create a template for string keys and integer values
	janus::redis_template<std::string, long long> int_tpl(*conn, *string_serializer, *int_serializer);

	// 3. Clean up keys from previous runs
	string_tpl.del({"my_string", "my_hash", "my_list", "my_set", "my_zset", "my_counter", "user:101"});
	std::cout << "Cleaned up old keys." << std::endl;

	// === String Operations ===
	std::cout << "\n--- String Operations ---" << std::endl;
	auto &value_ops = string_tpl.ops_for_value();
	value_ops.set("my_string", "hello");
	// Use the integer template for INCR operations
	int_tpl.ops_for_value().incr("my_counter", 10);
	if (auto val = value_ops.get("my_string")) {
		std::cout << "GET my_string: " << *val << std::endl;
	}

	// === Hash Operations ===
	std::cout << "\n--- Hash Operations ---" << std::endl;
	auto &hash_ops = string_tpl.ops_for_hash();
	hash_ops.hset("my_hash", {{"field1", "value1"}, {"field2", "value2"}});
	if (auto h_val = hash_ops.hget("my_hash", "field1")) {
		std::cout << "HGET my_hash field1: " << *h_val << std::endl;
	}

	// === Custom Hash Object Operations ===
	std::cout << "\n--- Custom Hash (User Object) ---" << std::endl;
	auto &user_hash_ops = string_tpl.ops_for_hash();

	User user_to_save = {101, "Sandro", 99};
	user_hash_ops.hset("user:101", user_hash_mapper::to_hash(user_to_save));
	std::cout << "Saved user 'Sandro' to hash user:101" << std::endl;

	auto fetched_hash = user_hash_ops.hgetall("user:101");
	if (!fetched_hash.empty()) {
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
	for (const auto &item: l_range) {
		std::cout << item << " ";
	}
	std::cout << std::endl;

	// === Set Operations ===
	std::cout << "\n--- Set Operations ---" << std::endl;
	auto &set_ops = string_tpl.ops_for_set();
	set_ops.sadd("my_set", {"member1", "member2", "member3"});
	if (set_ops.sismember("my_set", "member2")) {
		std::cout << "SISMEMBER: my_set contains 'member2'" << std::endl;
	}

	// === Sorted Set (ZSet) Operations ===
	std::cout << "\n--- Sorted Set Operations ---" << std::endl;
	auto &zset_ops = string_tpl.ops_for_zset();
	zset_ops.zadd("my_zset", {{"player1", 100.0}, {"player2", 250.5}, {"player3", 50.0}});
	auto z_range = zset_ops.zrange_withscores("my_zset", 0, -1);
	std::cout << "ZRANGE my_zset (with scores):" << std::endl;
	for (const auto &[member, score]: z_range) {
		std::cout << "  " << member << ": " << score << std::endl;
	}

	// === Lua Scripting ===
	std::cout << "\n--- Lua Scripting ---" << std::endl;
	const std::string lua_script = "return redis.call('HSET', KEYS[1], ARGV[1], ARGV[2])";
	std::string sha1;
	try {
		sha1 = conn->script_load(lua_script);
		std::cout << "Script loaded, SHA1: " << sha1 << std::endl;
	}
	catch (const std::exception &e) {
		std::cerr << "Failed to load script: " << e.what() << std::endl;
		return 1;
	}

	// Execute using EVALSHA for better performance
	try {
		conn->eval_sha1(sha1, {"my_hash"}, {"field3", "from_script"});
		std::cout << "EVALSHA successful." << std::endl;
	}
	catch (const janus::no_script_error &e) {
		// If Redis cleared its script cache, fall back to EVAL
		std::cerr << "NOSCRIPT error, falling back to EVAL: " << e.what() << std::endl;
		conn->eval(lua_script, {"my_hash"}, {"field3", "from_script"});
	}

	if (auto script_val = hash_ops.hget("my_hash", "field3")) {
		std::cout << "Value set by script: " << *script_val << std::endl;
	}

	return 0;
}
```

## 📜 License

This project uses the [Apache License 2.0](LICENSE) license
