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
ctest --test-dir build --output-on-failure
```

📌 Using Custom Redis Host/Port:
To use a remote Redis instance, set the `REDIS_HOST` and `REDIS_PORT` environment variables before running
`ctest`.

Linux / macOS (Bash/Zsh):

```shell
REDIS_HOST="172.17.57.112" REDIS_PORT=6379 ctest --test-dir build --verbose
```

Windows (PowerShell):

```shell
$env:REDIS_HOST="172.17.57.112"; $env:REDIS_PORT="6379"; ctest --test-dir build --verbose
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

```c++
#include <iostream>

#include <janus/janus.hpp>

int main(int argc, const char **argv) {
	// 1. Create the underlying connection
	std::shared_ptr<kv_connection> conn = std::make_shared<redis_connection>("172.16.0.2", 6379);

	// 2. Create serializers
	const std::shared_ptr<serializer<std::string>> k_serializer = std::make_shared<string_serializer<std::string>>();
	const std::shared_ptr<serializer<unsigned int>> v_serializer = std::make_shared<string_serializer<unsigned int>>();

	// 3. Construct redis_template
	redis_template<std::string, unsigned int> tpl(conn, k_serializer, v_serializer);

	// 4. Use ops_for_value() for string operations
	auto &value_ops = tpl.ops_for_value();

	value_ops.set("counter", 42);

	auto val = value_ops.get("counter");
	if (val) {
		std::cout << "counter = " << *val << "\n";
	}

	long long new_val = value_ops.incr("counter", 5);
	std::cout << "counter after incr = " << new_val << "\n";

	// 5. Use general key operations
	if (tpl.exists("counter")) {
		std::cout << "counter exists\n";
	}

	tpl.expire("counter", 60); // Set expiration time to 60 seconds

	return 0;
}
```

## 📜 License

This project uses the [Apache License 2.0](LICENSE) license
