#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "common.hpp"
#include "redis_operations.hpp"
#include "serialization.hpp"

namespace janus {
    class kv_connection;

    template<typename T>
    class serializer;

    template<typename K, typename V>
    class redis_operations {
    public:
        virtual ~redis_operations() = default;

        /* Check if a key exists */
        virtual bool exists(const K &key) = 0;
        /* Retrieve all keys matching the given pattern */
        virtual void keys(const std::string &pattern, std::unordered_set<K> &keys) = 0;
        /* Scan the keyspace starting from the given cursor */
        virtual scan_result<K> scan(uint64_t cursor, const std::string &pattern, unsigned int count) = 0;
        /* Set expiration for a key */
        virtual bool expire(const K &key, long long seconds) = 0;
        /* Set expiration for a key in milliseconds */
        virtual bool pexpire(const K &key, int milliseconds) = 0;
        /* Delete a single key */
        virtual long long del(const K &key) = 0;
        /* Delete multiple keys */
        virtual long long del(const std::vector<K> &keys) = 0;
        /* Get the time to live for a key in seconds */
        virtual int64_t ttl(const K &key) = 0;
        /* Get the time to live for a key in milliseconds */
        virtual int64_t pttl(const K &key) = 0;
        /* Remove the expiration from a key */
        virtual bool persist(const K &key) = 0;
        /* Ping the Redis server */
        virtual std::string ping() = 0;
        /* Ping the Redis server with a message */
        virtual std::string ping(const std::string &message) = 0;
        /* Get the data type of the value stored at key */
        virtual std::string type(const K &key) = 0;
        /* Evaluate lua script */
        virtual cmd_reply eval(const std::string &script, const std::vector<K> &keys, const std::vector<V> &args) = 0;
        /* Load script into script cache and return SHA1 */
        virtual std::string script_load(const std::string &script) = 0;
        /* Evaluate by SHA1 (EVALSHA) */
        virtual cmd_reply eval_sha1(const std::string &sha1, const std::vector<K> &keys,
                                    const std::vector<V> &args) = 0;
        /* Execute a raw Redis command */
        virtual cmd_reply exec_cmd(const std::string &cmd, const std::vector<std::string> &args) = 0;
        /* String operations view */
        virtual value_operations<K, V> &ops_for_value() = 0;
        /* Hash operations view */
        virtual hash_operations<K, V> &ops_for_hash() = 0;
        /* List operations view */
        virtual list_operations<K, V> &ops_for_list() = 0;
        /* Set operations view */
        virtual set_operations<K, V> &ops_for_set() = 0;
        /* Sorted Set operations view */
        virtual zset_operations<K, V> &ops_for_zset() = 0;
    };

    /**
     * @brief The central class for Redis interaction, managing connections and serialization.
     *
     * This class provides a high-level abstraction for Redis operations. It uses the Template
     * Method design pattern to delegate data-structure-specific logic to various `*operations`
     * interfaces, while handling the core logic of serialization and connection management.
     * @attention This class is NOT thread-safe. A single instance should not be shared across threads without external
     * locking.
     */
    template<typename K, typename V>
    class redis_template: public redis_operations<K, V> {
    public:
        /**
         * @brief Constructor: Injects all necessary dependencies.
         * @param conn Redis connection.
         * @param k_serializer Serializer for the key type K.
         * @param v_serializer Serializer for the value type V.
         */
        redis_template(kv_connection &conn, serializer<K> &k_serializer, serializer<V> &v_serializer) :
            connection(conn), key_serializer(k_serializer), value_serializer(v_serializer) {
            // references cannot be null, no runtime null check needed

            value_ops = std::make_unique<default_value_operations<K, V>>(*this);
            hash_ops = std::make_unique<default_hash_operations<K, V>>(*this);
            list_ops = std::make_unique<default_list_operations<K, V>>(*this);
            set_ops = std::make_unique<default_set_operations<K, V>>(*this);
            zset_ops = std::make_unique<default_zset_operations<K, V>>(*this);
        }

        /** @copydoc redis_operations::exists */
        bool exists(const K &key) override {
            auto serialized_key = this->serialize_key(key);
            return connection.exists(serialized_key);
        }

        /** @copydoc redis_operations::keys */
        void keys(const std::string &pattern, std::unordered_set<K> &keys) override {
            std::unordered_set<std::string> s_keys;
            connection.keys(pattern, s_keys);
            keys.reserve(s_keys.size());
            for (const auto &s_key: s_keys) {
                keys.insert(this->deserialize_key(s_key));
            }
        }

        /** @copydoc redis_operations::scan */
        scan_result<K> scan(uint64_t cursor, const std::string &pattern, unsigned int count) override {
            auto s_result = connection.scan(cursor, pattern, count);
            scan_result<K> result;
            result.cursor = s_result.cursor;
            result.keys.reserve(s_result.keys.size());
            for (const auto &s_key: s_result.keys) {
                result.keys.insert(this->deserialize_key(s_key));
            }
            return result;
        }

        /** @copydoc redis_operations::type */
        std::string type(const K &key) override {
            auto serialized_key = this->serialize_key(key);
            return connection.type(serialized_key);
        }

        /** @copydoc redis_operations::expire */
        bool expire(const K &key, long long seconds) override {
            auto serialized_key = this->serialize_key(key);
            return connection.expire(serialized_key, seconds);
        }

        /** @copydoc redis_operations::pexpire */
        bool pexpire(const K &key, int milliseconds) override {
            auto serialized_key = this->serialize_key(key);
            return connection.pexpire(serialized_key, milliseconds);
        }

        /** @copydoc redis_operations::del */
        long long del(const K &key) override {
            auto serialized_key = this->serialize_key(key);
            return connection.del(serialized_key);
        }

        /** @copydoc redis_operations::del */
        long long del(const std::vector<K> &keys) override {
            std::vector<std::string> s_keys;
            s_keys.reserve(keys.size());
            for (const auto &key: keys) {
                s_keys.push_back(this->serialize_key(key));
            }
            return connection.del(s_keys);
        }

        /** @copydoc redis_operations::ttl */
        int64_t ttl(const K &key) override {
            auto serialized_key = this->serialize_key(key);
            return connection.ttl(serialized_key);
        }

        /** @copydoc redis_operations::pttl */
        int64_t pttl(const K &key) override {
            auto serialized_key = this->serialize_key(key);
            return connection.pttl(serialized_key);
        }

        /** @copydoc redis_operations::persist */
        bool persist(const K &key) override {
            return connection.persist(this->serialize_key(key));
        }

        /** @copydoc redis_operations::ping */
        std::string ping() override {
            return connection.ping();
        }

        /** @copydoc redis_operations::ping */
        std::string ping(const std::string &message) override {
            return connection.ping(message);
        }

        cmd_reply eval(const std::string &script, const std::vector<K> &keys, const std::vector<V> &args) override {
            std::vector<std::string> s_keys;
            s_keys.reserve(keys.size());
            for (const auto &key: keys) {
                s_keys.push_back(this->serialize_key(key));
            }
            std::vector<std::string> s_args;
            s_args.reserve(args.size());
            for (const auto &arg: args) {
                s_args.push_back(this->serialize_value(arg));
            }
            return connection.eval(script, s_keys, s_args);
        }

        /** @copydoc redis_operations::script_load */
        std::string script_load(const std::string &script) override {
            std::string sha = connection.script_load(script);
            // cache mapping from sha -> script so we can reload on NOSCRIPT
            sha1_cache.emplace(sha, script);
            return sha;
        }

        /** @copydoc redis_operations::eval_sha1 */
        cmd_reply eval_sha1(const std::string &sha1, const std::vector<K> &keys, const std::vector<V> &args) override {
            std::vector<std::string> s_keys;
            s_keys.reserve(keys.size());
            for (const auto &key: keys) {
                s_keys.push_back(this->serialize_key(key));
            }
            std::vector<std::string> s_args;
            s_args.reserve(args.size());
            for (const auto &arg: args) {
                s_args.push_back(this->serialize_value(arg));
            }
            try {
                return connection.eval_sha1(sha1, s_keys, s_args);
            }
            catch (const no_script_error &) {
                // If we have the original script cached for this sha, reload and retry
                auto it = sha1_cache.find(sha1);
                if (it == sha1_cache.end()) {
                    throw; // unknown script, propagate
                }
                const std::string &script = it->second;
                std::string new_sha = connection.script_load(script);
                // update cache (in case sha changed)
                sha1_cache.emplace(new_sha, script);
                return connection.eval_sha1(new_sha, s_keys, s_args);
            }
        }

        /** @copydoc redis_operations::exec_cmd */
        cmd_reply exec_cmd(const std::string &cmd, const std::vector<std::string> &args) override {
            return connection.command(cmd, args);
        }

        // ==========================================================
        // Implementation of Operation Views (Returning References)
        // ==========================================================

        /** @copydoc redis_operations::ops_for_value */
        value_operations<K, V> &ops_for_value() override {
            return *value_ops;
        }

        /** @copydoc redis_operations::ops_for_hash */
        hash_operations<K, V> &ops_for_hash() override {
            return *hash_ops;
        }

        /** @copydoc redis_operations::ops_for_list */
        list_operations<K, V> &ops_for_list() override {
            return *list_ops;
        }

        /** @copydoc redis_operations::ops_for_set */
        set_operations<K, V> &ops_for_set() override {
            return *set_ops;
        }

        /** @copydoc redis_operations::ops_for_zset */
        zset_operations<K, V> &ops_for_zset() override {
            return *zset_ops;
        }

        /**
         * @brief Serializes a key of type K into a string.
         */
        [[nodiscard]] std::string serialize_key(const K &key) const {
            return key_serializer.serialize(key);
        }

        /**
         * @brief Deserializes a string into a key of type K.
         */
        [[nodiscard]] K deserialize_key(const std::string &data) const {
            return key_serializer.deserialize(data);
        }

        /**
         * @brief Serializes a value of type V into a string.
         */
        [[nodiscard]] std::string serialize_value(const V &value) const {
            return value_serializer.serialize(value);
        }

        /**
         * @brief Deserializes a string into a value of type V.
         */
        [[nodiscard]] V deserialize_value(const std::string &data) const {
            return value_serializer.deserialize(data);
        }

        /**
         * @brief Provides access to the underlying kv_connection.
         * @internal This is intended for internal use by operation classes.
         */
        [[nodiscard]] kv_connection &get_connection() const {
            return connection;
        }

    private:
        // --- Dependencies (Shared ownership) ---
        kv_connection &connection;
        serializer<K> &key_serializer;
        serializer<V> &value_serializer;

        // --- Operation Views Instances (Unique ownership) ---
        std::unique_ptr<value_operations<K, V>> value_ops;
        std::unique_ptr<hash_operations<K, V>> hash_ops;
        std::unique_ptr<list_operations<K, V>> list_ops;
        std::unique_ptr<set_operations<K, V>> set_ops;
        std::unique_ptr<zset_operations<K, V>> zset_ops;

        // cache mapping from sha1 -> script body for automatic reload on NOSCRIPT
        std::unordered_map<std::string, std::string> sha1_cache;
    };

    /**
     * @brief A convenience wrapper for `redis_template` for the common case of string keys and string values.
     *
     * This class inherits from `redis_template<std::string, std::string>` and automatically
     * manages its own `string_serializer` instances, simplifying its construction.
     */
    class string_redis_template: public redis_template<std::string, std::string> {
    public:
        /**
         * @brief Constructs a string_redis_template.
         * @param conn A reference to the underlying kv_connection.
         */
        explicit string_redis_template(kv_connection &conn) : redis_template(conn, kv_serializer, kv_serializer) {
        }

    private:
        // The serializer instance is owned by this class.
        string_serializer<std::string> kv_serializer;
    };
} // namespace janus
