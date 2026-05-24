#pragma once

#include "operations.hpp"

namespace redisdal {
    template<typename K, typename V>
    class redis_template;

    /**
     * @brief Default implementation of value_operations.
     *
     * This class delegates all Redis String operations to a redis_template,
     * handling the serialization and deserialization of keys and values.
     * @tparam K The key type.
     * @tparam V The value type.
     */
    template<typename K, typename V>
    class default_value_operations: public value_operations<K, V> {
    public:
        /**
         * @brief Constructs a default_value_operations instance.
         * @param ops A reference to the redis_template that provides connection and serialization services.
         */
        explicit default_value_operations(redis_template<K, V> &ops) : tpl(ops) {
        }

        /** @copydoc value_operations::set */
        bool set(const K &key, const V &value) override {
            return tpl.get_connection().set(tpl.serialize_key(key), tpl.serialize_value(value));
        }

        /** @copydoc value_operations::get */
        std::optional<V> get(const K &key) override {
            auto val = tpl.get_connection().get(tpl.serialize_key(key));
            if (val) {
                return tpl.deserialize_value(*val);
            }
            return std::nullopt;
        }

        /** @copydoc value_operations::incr */
        long long incr(const K &key, long long delta) override {
            return tpl.get_connection().incr(tpl.serialize_key(key), delta);
        }

        /** @copydoc value_operations::decr */
        long long decr(const K &key, long long delta) override {
            return tpl.get_connection().decr(tpl.serialize_key(key), delta);
        }

        /** @copydoc value_operations::append */
        long long append(const K &key, const V &value) override {
            return tpl.get_connection().append(tpl.serialize_key(key), tpl.serialize_value(value));
        }

        /** @copydoc value_operations::get_and_set */
        std::optional<V> get_and_set(const K &key, const V &value) override {
            auto val = tpl.get_connection().getset(tpl.serialize_key(key), tpl.serialize_value(value));
            if (val) {
                return tpl.deserialize_value(*val);
            }
            return std::nullopt;
        }

    private:
        // A reference to the main template for accessing connection and serializers.
        redis_template<K, V> &tpl;
    };

    /**
     * @brief Default implementation of hash_operations.
     *
     * This class delegates all Redis Hash operations to a redis_template,
     * handling the serialization and deserialization of keys, hash fields, and hash values.
     * @tparam K The key and hash field type.
     * @tparam V The hash value type.
     */
    template<typename K, typename V>
    class default_hash_operations: public hash_operations<K, V> {
    public:
        /**
         * @brief Constructs a default_hash_operations instance.
         * @param tpl A reference to the redis_template that provides connection and serialization services.
         */
        explicit default_hash_operations(redis_template<K, V> &tpl) : tpl(tpl) {
        }

        /** @copydoc hash_operations::hget */
        std::optional<V> hget(const K &key, const K &hash_key) override {
            auto val = tpl.get_connection().hget(tpl.serialize_key(key), tpl.serialize_key(hash_key));
            if (val) {
                return tpl.deserialize_value(*val);
            }
            return std::nullopt;
        }

        /** @copydoc hash_operations::hget */
        void hget(const K &key, std::unordered_map<K, std::optional<V>> &hash_map) override {
            if (hash_map.empty()) {
                return;
            }

            std::unordered_map<std::string, std::optional<std::string>> serialized_map;
            // Prepare the map for the underlying connection. The values don't matter yet.
            for (const auto &pair: hash_map) {
                serialized_map.emplace(tpl.serialize_key(pair.first), std::nullopt);
            }

            // Delegate to the connection layer, which will populate `serialized_map` with results.
            tpl.get_connection().hget(tpl.serialize_key(key), serialized_map);

            // Update the original user-provided map with deserialized results. This is an in-place update.
            for (const auto &pair: serialized_map) {
                // Find the corresponding key in the original map and update its value.
                auto it = hash_map.find(tpl.deserialize_key(pair.first));
                if (it != hash_map.end()) {
                    if (pair.second.has_value()) {
                        it->second = tpl.deserialize_value(*pair.second);
                    }
                    else {
                        it->second = std::nullopt;
                    }
                }
            }
        }

        /** @copydoc hash_operations::hgetall */
        std::unordered_map<K, V> hgetall(const K &key) override {
            auto raw_map = tpl.get_connection().hgetall(tpl.serialize_key(key));
            std::unordered_map<K, V> result;
            result.reserve(raw_map.size());
            for (const auto &pair: raw_map) {
                result.emplace(tpl.deserialize_key(pair.first), tpl.deserialize_value(pair.second));
            }
            return result;
        }

        /** @copydoc hash_operations::hkeys */
        std::vector<K> hkeys(const K &key) override {
            auto raw_keys = tpl.get_connection().hkeys(tpl.serialize_key(key));
            std::vector<K> result;
            result.reserve(raw_keys.size());
            for (const auto &k: raw_keys) {
                result.push_back(tpl.deserialize_key(k));
            }
            return result;
        }

        /** @copydoc hash_operations::hvals */
        std::vector<V> hvals(const K &key) override {
            auto raw_vals = tpl.get_connection().hvals(tpl.serialize_key(key));
            std::vector<V> result;
            result.reserve(raw_vals.size());
            for (const auto &v: raw_vals) {
                result.push_back(tpl.deserialize_value(v));
            }
            return result;
        }

        /** @copydoc hash_operations::hscan */
        uint64_t hscan(const K &key, uint64_t cursor, const K &pattern, unsigned int count,
                       std::unordered_map<K, V> &hash_map) override {
            std::unordered_map<std::string, std::string> serialized_map;
            auto new_cursor = tpl.get_connection().hscan(tpl.serialize_key(key), cursor, tpl.serialize_key(pattern),
                                                         count, serialized_map);
            hash_map.clear();
            hash_map.reserve(serialized_map.size());
            for (const auto &pair: serialized_map) {
                hash_map.emplace(tpl.deserialize_key(pair.first), tpl.deserialize_value(pair.second));
            }
            return new_cursor;
        }

        /** @copydoc hash_operations::hset */
        bool hset(const K &key, const K &field, const V &value) override {
            return tpl.get_connection().hset(tpl.serialize_key(key), tpl.serialize_key(field),
                                             tpl.serialize_value(value));
        }

        /** @copydoc hash_operations::hset */
        bool hset(const K &key, const std::unordered_map<K, V> &hash_map) override {
            std::unordered_map<std::string, std::string> serialized_map;
            serialized_map.reserve(hash_map.size());
            for (const auto &pair: hash_map) {
                serialized_map.emplace(tpl.serialize_key(pair.first), tpl.serialize_value(pair.second));
            }
            return tpl.get_connection().hset(tpl.serialize_key(key), serialized_map);
        }

        /** @copydoc hash_operations::hdel */
        long long hdel(const K &key, const K &hash_key) override {
            return tpl.get_connection().hdel(tpl.serialize_key(key), tpl.serialize_key(hash_key));
        }

        /** @copydoc hash_operations::hdel */
        long long hdel(const K &key, const std::vector<K> &hash_keys) override {
            std::vector<std::string> serialized_keys;
            serialized_keys.reserve(hash_keys.size());
            for (const auto &k: hash_keys) {
                serialized_keys.push_back(tpl.serialize_key(k));
            }
            return tpl.get_connection().hdel(tpl.serialize_key(key), serialized_keys);
        }

    private:
        // A reference to the main template for accessing connection and serializers.
        redis_template<K, V> &tpl;
    };

    /**
     * @brief Default implementation of list_operations.
     *
     * This class delegates all Redis List operations to a redis_template,
     * handling the serialization and deserialization of keys and values.
     * @tparam K The key type.
     * @tparam V The value type.
     */
    template<typename K, typename V>
    class default_list_operations: public list_operations<K, V> {
    public:
        /**
         * @brief Constructs a default_list_operations instance.
         * @param ops A reference to the redis_template that provides connection and serialization services.
         */
        explicit default_list_operations(redis_template<K, V> &ops) : tpl(ops) {
        }

        /** @copydoc list_operations::lpush */
        long long lpush(const K &key, const std::vector<V> &values) override {
            std::vector<std::string> serialized_values;
            serialized_values.reserve(values.size());
            for (const auto &v: values) {
                serialized_values.push_back(tpl.serialize_value(v));
            }
            return tpl.get_connection().lpush(tpl.serialize_key(key), serialized_values);
        }

        /** @copydoc list_operations::lpush */
        long long lpush(const K &key, const V &value) override {
            return tpl.get_connection().lpush(tpl.serialize_key(key), tpl.serialize_value(value));
        }

        /** @copydoc list_operations::rpush */
        long long rpush(const K &key, const V &value) override {
            return tpl.get_connection().rpush(tpl.serialize_key(key), tpl.serialize_value(value));
        }

        /** @copydoc list_operations::rpush */
        long long rpush(const K &key, const std::vector<V> &values) override {
            std::vector<std::string> serialized_values;
            serialized_values.reserve(values.size());
            for (const auto &v: values) {
                serialized_values.push_back(tpl.serialize_value(v));
            }
            return tpl.get_connection().rpush(tpl.serialize_key(key), serialized_values);
        }

        /** @copydoc list_operations::lpop */
        std::optional<V> lpop(const K &key) override {
            auto val = tpl.get_connection().lpop(tpl.serialize_key(key));
            if (val) {
                return tpl.deserialize_value(*val);
            }
            return std::nullopt;
        }

        /** @copydoc list_operations::rpop */
        std::optional<V> rpop(const K &key) override {
            auto val = tpl.get_connection().rpop(tpl.serialize_key(key));
            if (val) {
                return tpl.deserialize_value(*val);
            }
            return std::nullopt;
        }

        /** @copydoc list_operations::lpop */
        std::vector<V> lpop(const K &key, int count) override {
            auto raw_list = tpl.get_connection().lpop(tpl.serialize_key(key), count);
            std::vector<V> result;
            result.reserve(raw_list.size());
            for (const auto &v: raw_list) {
                result.push_back(tpl.deserialize_value(v));
            }
            return result;
        }

        /** @copydoc list_operations::rpop */
        std::vector<V> rpop(const K &key, int count) override {
            auto raw_list = tpl.get_connection().rpop(tpl.serialize_key(key), count);
            std::vector<V> result;
            result.reserve(raw_list.size());
            for (const auto &v: raw_list) {
                result.push_back(tpl.deserialize_value(v));
            }
            return result;
        }

        /** @copydoc list_operations::lrange */
        std::vector<V> lrange(const K &key, long long start, long long stop) override {
            auto raw_list = tpl.get_connection().lrange(tpl.serialize_key(key), start, stop);
            std::vector<V> result;
            result.reserve(raw_list.size());
            for (const auto &v: raw_list) {
                result.push_back(tpl.deserialize_value(v));
            }
            return result;
        }

        /** @copydoc list_operations::llen */
        long long llen(const K &key) override {
            return tpl.get_connection().llen(tpl.serialize_key(key));
        }

        /** @copydoc list_operations::lindex */
        std::optional<V> lindex(const K &key, long long index) override {
            auto val = tpl.get_connection().lindex(tpl.serialize_key(key), index);
            if (val) {
                return tpl.deserialize_value(*val);
            }
            return std::nullopt;
        }

    private:
        // A reference to the main template for accessing connection and serializers.
        redis_template<K, V> &tpl;
    };

    /**
     * @brief Default implementation of set_operations.
     *
     * This class delegates all Redis Set operations to a redis_template,
     * handling the serialization and deserialization of keys and members.
     * @tparam K The key type.
     * @tparam V The member type.
     */
    template<typename K, typename V>
    class default_set_operations: public set_operations<K, V> {
    public:
        /**
         * @brief Constructs a default_set_operations instance.
         * @param ops A reference to the redis_template that provides connection and serialization services.
         */
        explicit default_set_operations(redis_template<K, V> &ops) : tpl(ops) {
        }

        /** @copydoc set_operations::sadd */
        long long sadd(const K &key, const std::vector<V> &members) override {
            std::vector<std::string> serialized_members;
            serialized_members.reserve(members.size());
            for (const auto &m: members) {
                serialized_members.push_back(tpl.serialize_value(m));
            }
            return tpl.get_connection().sadd(tpl.serialize_key(key), serialized_members);
        }

        /** @copydoc set_operations::srem */
        long long srem(const K &key, const std::vector<V> &members) override {
            std::vector<std::string> serialized_members;
            serialized_members.reserve(members.size());
            for (const auto &m: members) {
                serialized_members.push_back(tpl.serialize_value(m));
            }
            return tpl.get_connection().srem(tpl.serialize_key(key), serialized_members);
        }

        /** @copydoc set_operations::spop */
        std::optional<V> spop(const K &key) override {
            auto val = tpl.get_connection().spop(tpl.serialize_key(key));
            if (val) {
                return tpl.deserialize_value(*val);
            }
            return std::nullopt;
        }

        /** @copydoc set_operations::smembers */
        std::vector<V> smembers(const K &key) override {
            auto raw_members = tpl.get_connection().smembers(tpl.serialize_key(key));
            std::vector<V> result;
            result.reserve(raw_members.size());
            for (const auto &m: raw_members) {
                result.push_back(tpl.deserialize_value(m));
            }
            return result;
        }

        /** @copydoc set_operations::scard */
        long long scard(const K &key) override {
            return tpl.get_connection().scard(tpl.serialize_key(key));
        }

        /** @copydoc set_operations::sismember */
        bool sismember(const K &key, const V &member) override {
            return tpl.get_connection().sismember(tpl.serialize_key(key), tpl.serialize_value(member));
        }

        /** @copydoc set_operations::sinter */
        std::vector<V> sinter(const std::vector<K> &keys) override {
            std::vector<std::string> serialized_keys;
            serialized_keys.reserve(keys.size());
            for (const auto &k: keys) {
                serialized_keys.push_back(tpl.serialize_key(k));
            }

            auto raw_members = tpl.get_connection().sinter(serialized_keys);
            std::vector<V> result;
            result.reserve(raw_members.size());
            for (const auto &m: raw_members) {
                result.push_back(tpl.deserialize_value(m));
            }
            return result;
        }

    private:
        // A reference to the main template for accessing connection and serializers.
        redis_template<K, V> &tpl;
    };

    /**
     * @brief Default implementation of zset_operations (Sorted Set).
     *
     * This class delegates all Redis Sorted Set operations to a redis_template,
     * handling the serialization and deserialization of keys and members.
     * @tparam K The key type.
     * @tparam V The member type.
     */
    template<typename K, typename V>
    class default_zset_operations: public zset_operations<K, V> {
    public:
        /**
         * @brief Constructs a default_zset_operations instance.
         * @param ops A reference to the redis_template that provides connection and serialization services.
         */
        explicit default_zset_operations(redis_template<K, V> &ops) : tpl(ops) {
        }

        /** @copydoc zset_operations::zadd */
        long long zadd(const K &key, const std::unordered_map<V, double> &members) override {
            std::unordered_map<std::string, double> serialized_members;
            serialized_members.reserve(members.size());
            for (const auto &pair: members) {
                serialized_members.emplace(tpl.serialize_value(pair.first), pair.second);
            }
            return tpl.get_connection().zadd(tpl.serialize_key(key), serialized_members);
        }

        /** @copydoc zset_operations::zrem */
        long long zrem(const K &key, const std::vector<V> &members) override {
            std::vector<std::string> serialized_members;
            serialized_members.reserve(members.size());
            for (const auto &m: members) {
                serialized_members.push_back(tpl.serialize_value(m));
            }
            return tpl.get_connection().zrem(tpl.serialize_key(key), serialized_members);
        }

        /** @copydoc zset_operations::zincrby */
        double zincrby(const K &key, double increment, const V &member) override {
            return tpl.get_connection().zincrby(tpl.serialize_key(key), increment, tpl.serialize_value(member));
        }

        /** @copydoc zset_operations::zscore */
        std::optional<double> zscore(const K &key, const V &member) override {
            return tpl.get_connection().zscore(tpl.serialize_key(key), tpl.serialize_value(member));
        }

        /** @copydoc zset_operations::zrange */
        std::vector<V> zrange(const K &key, long long start, long long stop) override {
            auto raw_members = tpl.get_connection().zrange(tpl.serialize_key(key), start, stop);
            std::vector<V> result;
            result.reserve(raw_members.size());
            for (const auto &m: raw_members) {
                result.push_back(tpl.deserialize_value(m));
            }
            return result;
        }

        /** @copydoc zset_operations::zrevrange */
        std::vector<V> zrevrange(const K &key, long long start, long long stop) override {
            auto raw_members = tpl.get_connection().zrevrange(tpl.serialize_key(key), start, stop);
            std::vector<V> result;
            result.reserve(raw_members.size());
            for (const auto &m: raw_members) {
                result.push_back(tpl.deserialize_value(m));
            }
            return result;
        }

        /** @copydoc zset_operations::zrange_withscores */
        std::vector<std::pair<V, double>> zrange_withscores(const K &key, long long start, long long stop) override {
            auto raw_pairs = tpl.get_connection().zrange_withscores(tpl.serialize_key(key), start, stop);
            std::vector<std::pair<V, double>> result;
            result.reserve(raw_pairs.size());
            for (const auto &pair: raw_pairs) {
                result.emplace_back(tpl.deserialize_value(pair.first), pair.second);
            }
            return result;
        }

        /** @copydoc zset_operations::zrevrange_withscores */
        std::vector<std::pair<V, double>> zrevrange_withscores(const K &key, long long start, long long stop) override {
            auto raw_pairs = tpl.get_connection().zrevrange_withscores(tpl.serialize_key(key), start, stop);
            std::vector<std::pair<V, double>> result;
            result.reserve(raw_pairs.size());
            for (const auto &pair: raw_pairs) {
                result.emplace_back(tpl.deserialize_value(pair.first), pair.second);
            }
            return result;
        }

    private:
        // A reference to the main template for accessing connection and serializers.
        redis_template<K, V> &tpl;
    };
} // namespace redisdal
