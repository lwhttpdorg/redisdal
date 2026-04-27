# Janus Project Design Document

<!-- TOC -->
- [1. Project Overview](#1.-project-overview)
- [2. Architecture Design](#2.-architecture-design)
  - [2.1. Class Diagram](#2.1.-class-diagram)
- [3. Usage Patterns](#3.-usage-patterns)
  - [3.1. Basic Usage](#3.1.-basic-usage)
  - [3.2. Template Method Pattern](#3.2.-template-method-pattern)
- [4. Build System](#4.-build-system)
  - [4.1. CMake vs Meson](#4.1.-cmake-vs-meson)
  - [4.2. Build Commands](#4.2.-build-commands)
- [5. Thread Safety](#5.-thread-safety)
- [6. Dependencies](#6.-dependencies)
  - [6.1. External Dependencies](#6.1.-external-dependencies)
<!-- /TOC -->

## 1. Project Overview

Janus is a C++ Redis client library based on hiredis, providing type-safe Redis operation interfaces.

## 2. Architecture Design

### 2.1. Class Diagram

```mermaid
classDiagram
    class redis_template~K, V~ {
        +exists(key)
        +keys(pattern, keys)
        +scan(cursor, pattern, count)
        +expire(key, seconds)
        +del(key)
        +ttl(key)
        +ping()
        +type(key)
        +eval(script, keys, args)
        +ops_for_value() value_operations~K, V~
        +ops_for_hash() hash_operations~K, V~
        +ops_for_list() list_operations~K, V~
        +ops_for_set() set_operations~K, V~
        +ops_for_zset() zset_operations~K, V~
    }

    class redis_operations~K, V~ {
        <<interface>>
        +exists(key)
        +keys(pattern, keys)
        +scan(cursor, pattern, count)
        +expire(key, seconds)
        +del(key)
        +ttl(key)
        +ping()
        +type(key)
        +eval(script, keys, args)
    }

    class kv_connection {
        <<interface>>
        +exists(key)
        +keys(pattern, keys)
        +scan(cursor, pattern, count)
        +type(key)
        +expire(key, seconds)
        +del(key)
        +ttl(key)
        +pexpire(key, ms)
        +pttl(key)
        +persist(key)
    }

    class value_operations~K, V~ {
        <<interface>>
        +set(key, value)
        +get(key)
        +incr(key, delta)
        +decr(key, delta)
        +append(key, value)
        +get_and_set(key, value)
    }

    class hash_operations~K, V~ {
        <<interface>>
        +hget(key, field)
        +hget(key, hash_map)
        +hgetall(key)
        +hkeys(key)
    }

    class list_operations~K, V~ {
        <<interface>>
        +lpush(key, value)
        +rpush(key, value)
        +lpop(key)
        +rpop(key)
    }

    class set_operations~K, V~ {
        <<interface>>
        +sadd(key, members)
        +srem(key, members)
        +smembers(key)
        +sis_member(key, member)
    }

    class zset_operations~K, V~ {
        <<interface>>
        +zadd(key, score, member)
        +zrem(key, member)
        +zrange(key, start, stop)
        +zrank(key, member)
        +zscore(key, member)
    }

    class redis_connection {
        +connect(config)
        +reconnect()
        +close()
        +execute(cmd)
    }

    class default_value_operations~K, V~ {
    }
    class default_hash_operations~K, V~ {
    }
    class default_list_operations~K, V~ {
    }
    class default_set_operations~K, V~ {
    }
    class default_zset_operations~K, V~ {
    }

    redis_connection --> kv_connection: implements
    redis_operations <|-- redis_template : implements
    value_operations <|-- default_value_operations : implements
    hash_operations <|-- default_hash_operations : implements
    list_operations <|-- default_list_operations : implements
    set_operations <|-- default_set_operations : implements
    zset_operations <|-- default_zset_operations : implements

    kv_connection --o redis_template : aggregation
    redis_template *-- value_operations : composition
    redis_template *-- hash_operations : composition
    redis_template *-- list_operations : composition
    redis_template *-- set_operations : composition
    redis_template *-- zset_operations : composition
```

---

## 3. Usage Patterns

### 3.1. Basic Usage

```cpp
#include <janus/janus.hpp>

int main() {
    janus::redis_connection conn;
    conn.connect("redis://127.0.0.1:6379");

    janus::redis_template<std::string, std::string> rt(conn);

    // String operations
    rt.ops_for_value().set("key", "value");
    auto val = rt.ops_for_value().get("key");

    // Hash operations
    rt.ops_for_hash().hset("user:1", "name", "Alice");

    conn.close();
    return 0;
}
```

### 3.2. Template Method Pattern

```mermaid
sequenceDiagram
    participant Client
    participant RT as redis_template
    participant VO as value_operations
    participant RC as redis_connection

    Client->>RT: set(key, value)
    RT->>RT: serialize(value)
    RT->>VO: set(key, serialized)
    VO->>RC: execute("SET", args)
    RC-->>VO: reply
    VO-->>RT: result
    RT-->>Client: success
```

---

## 4. Build System

### 4.1. CMake vs Meson

| Feature | CMake | Meson |
|---------|-------|-------|
| Minimum Version | 3.10 | - |
| Library Type | SHARED | library() |
| Dependency Finding | find_package | dependency() |
| Test Framework | FetchContent + GTest | subproject + system |
| Install Path | lib/ | lib/ |

### 4.2. Build Commands

```bash
# CMake
mkdir build && cd build
cmake .. -DENABLE_JANUS_TEST=ON
make

# Meson
meson setup build -Denable-test=true
meson compile -C build
meson test -C build
```

---

## 5. Thread Safety

> ⚠️ **Note**: `redis_template` instances are **NOT thread-safe**. In multi-threaded scenarios, each thread should hold an independent instance, or protect shared instances with external locks.

---

## 6. Dependencies

```mermaid
graph LR
    Janus[janus library] --> Hiredis[hiredis]
    Test[test] --> Janus
    Test --> GTest[GoogleTest]
    Test --> Threads[Threads]
```

### 6.1. External Dependencies

- **hiredis**: Redis C client library
- **GoogleTest**: Unit test framework (optional for testing)
- **Threads**: POSIX threads library