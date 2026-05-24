#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

namespace redisdal {
    template<typename K, typename V>
    class value_operations {
    public:
        virtual ~value_operations() = default;

        virtual bool set(const K &key, const V &value) = 0;

        virtual std::optional<V> get(const K &key) = 0;

        virtual long long incr(const K &key, long long delta) = 0;

        virtual long long decr(const K &key, long long delta) = 0;

        virtual long long append(const K &key, const V &value) = 0;

        virtual std::optional<V> get_and_set(const K &key, const V &value) = 0;
    };

    template<typename K, typename V>
    class hash_operations {
    public:
        virtual ~hash_operations() = default;

        /**
         * @brief Gets the value associated with the field in the hash stored at key.
         * @param key The hash key (K).
         * @param hash_key The field name (K).
         * @return The value, or std::optional<V> if the key or field does not exist.
         */
        virtual std::optional<V> hget(const K &key, const K &hash_key) = 0;

        /**
         * @brief Gets the values associated with the specified fields in the hash stored at key
         * The results are written back into the passed hash_map.
         * @param key The hash key (K).
         * @param hash_map Contains fields (K) to query on input; updated with fields and their corresponding values
         * (std::optional<V>) on output.
         */
        virtual void hget(const K &key, std::unordered_map<K, std::optional<V>> &hash_map) = 0;

        /**
         * @brief Gets all the fields and values in the hash.
         * @param key The hash key (K).
         * @return An unordered map containing all fields and values.
         */
        virtual std::unordered_map<K, V> hgetall(const K &key) = 0;

        /**
         * @brief Gets all the field names in the hash.
         * @param key The hash key (K).
         * @return A vector of all field keys.
         */
        virtual std::vector<K> hkeys(const K &key) = 0;

        /**
         * @brief Gets all the values in the hash.
         * @param key The hash key (K).
         * @return A vector of all values.
         */
        virtual std::vector<V> hvals(const K &key) = 0;

        /**
         * @brief Incrementally iterates the fields and values of a hash.
         * @param key The hash key (K).
         * @param cursor The scan cursor.
         * @param pattern The pattern to match fields against.
         * @param count The number of elements to return. This is a hint to the server, not an upper limit.
         * @param hash_map An unordered_map to store the resulting fields and values.
         * @return The new cursor position after the scan. (The cursor > 0 indicates more fields to scan.)
         * @attention
         *
         * - the count is just a hint to the server, not an upper limit.
         * - the returned cursor > 0 indicates more keys to scan, not that is an index of offset.
         */
        virtual uint64_t hscan(const K &key, uint64_t cursor, const K &pattern, unsigned int count,
                               std::unordered_map<K, V> &hash_map) = 0;

        /* Write/Set Operations */

        /**
         * @brief Sets the value of a hash field.
         * @param key The hash key (K).
         * @param field The field name (K).
         * @param value The value (V).
         * @return True on success (field was new or value was updated).
         */
        virtual bool hset(const K &key, const K &field, const V &value) = 0;

        /**
         * @brief Sets multiple hash fields to multiple values.
         * @param key The hash key (K).
         * @param hash_map A map of fields (K) and their values (V) to set.
         * @return True on success.
         */
        virtual bool hset(const K &key, const std::unordered_map<K, V> &hash_map) = 0;

        /* Deletion Operations */

        /**
         * @brief Deletes the specified field from a hash.
         * @param key The hash key (K).
         * @param hash_key The field name to delete (K).
         * @return The number of fields removed from the hash (1 or 0).
         */
        virtual long long hdel(const K &key, const K &hash_key) = 0;

        /**
         * @brief Deletes the specified fields from a hash (multiple fields).
         * @param key The hash key (K).
         * @param hash_keys A vector of field names to delete (vector<K>).
         * @return The number of fields that were removed from the hash.
         */
        virtual long long hdel(const K &key, const std::vector<K> &hash_keys) = 0;
    };

    template<typename K, typename V>
    class list_operations {
    public:
        virtual ~list_operations() = default;

        /**
         * @brief Pushes one or multiple values onto the head (left) of a list. (Corresponds to LPUSH)
         * @param key The list key (K).
         * @param values The values (V) to push.
         * @return The new length of the list after the push operation.
         */
        virtual long long lpush(const K &key, const std::vector<V> &values) = 0;

        /**
         * @brief Pushes a single value onto the head (left) of a list. (Corresponds to LPUSH)
         * @param key The list key (K).
         * @param value The value (V) to push.
         * @return The new length of the list after the push operation.
         */
        virtual long long lpush(const K &key, const V &value) = 0;

        /**
         * @brief Pushes a single value onto the tail (right) of a list. (Corresponds to RPUSH)
         * @param key The list key (K).
         * @param value The value (V) to push.
         * @return The new length of the list after the push operation.
         */
        virtual long long rpush(const K &key, const V &value) = 0;

        /**
         * @brief Pushes one or multiple values onto the tail (right) of a list. (Corresponds to RPUSH)
         * @param key The list key (K).
         * @param values The values (V) to push.
         * @return The new length of the list after the push operation.
         */
        virtual long long rpush(const K &key, const std::vector<V> &values) = 0;

        /* Pop/Removal Operations (LPOP/RPOP) */

        /**
         * @brief Removes and gets the first element (head/left) of a list. (Corresponds to LPOP)
         * @param key The list key (K).
         * @return The element at the head of the list, or std::optional<V> if the list is empty.
         */
        virtual std::optional<V> lpop(const K &key) = 0;

        /**
         * @brief Removes and gets the last element (tail/right) of a list. (Corresponds to RPOP)
         * @param key The list key (K).
         * @return The element at the tail of the list, or std::optional<V> if the list is empty.
         */
        virtual std::optional<V> rpop(const K &key) = 0;

        /**
         * @brief Removes and gets up to `count` elements from the head (left) of a list. (Corresponds to LPOP with
         * count)
         * @param key The list key (K).
         * @param count The maximum number of elements to pop.
         * @return A vector of popped elements. Returns an empty vector if the key does not exist or the list is empty.
         */
        virtual std::vector<V> lpop(const K &key, int count) = 0;

        /**
         * @brief Removes and gets up to `count` elements from the tail (right) of a list. (Corresponds to RPOP with
         * count)
         * @param key The list key (K).
         * @param count The maximum number of elements to pop.
         * @return A vector of popped elements. Returns an empty vector if the key does not exist or the list is empty.
         */
        virtual std::vector<V> rpop(const K &key, int count) = 0;

        /* Read/Range Operations (LRANGE/LLEN) */

        /**
         * @brief Gets a range of elements from a list. (Corresponds to LRANGE)
         * @param key The list key (K).
         * @param start The starting index (0 is the head).
         * @param stop The stopping index (-1 is the last element).
         * @return A vector of elements (V) in the specified range.
         */
        virtual std::vector<V> lrange(const K &key, long long start, long long stop) = 0;

        /**
         * @brief Gets the length of a list. (Corresponds to LLEN)
         * @param key The list key (K).
         * @return The length of the list. Returns 0 if the key does not exist.
         */
        virtual long long llen(const K &key) = 0;

        /**
         * @brief Gets an element from a list by its index. (Corresponds to LINDEX)
         * @param key The list key (K).
         * @param index The index of the element to get (0 is the first, -1 is the last).
         * @return The element (V) at the specified index, or std::optional<V> if the index is out of range.
         */
        virtual std::optional<V> lindex(const K &key, long long index) = 0;
    };

    template<typename K, typename V>
    class set_operations {
    public:
        virtual ~set_operations() = default;

        /**
         * @brief Adds one or more members to a set. If a member is already a member of the set, it is ignored.
         * (Corresponds to SADD)
         * @param key The set key (K).
         * @param members The members (V) to add.
         * @return The number of members that were successfully added to the set (excluding existing members).
         */
        virtual long long sadd(const K &key, const std::vector<V> &members) = 0;

        /**
         * @brief Removes one or more members from a set. (Corresponds to SREM)
         * @param key The set key (K).
         * @param members The members (V) to remove.
         * @return The number of members that were successfully removed from the set.
         */
        virtual long long srem(const K &key, const std::vector<V> &members) = 0;

        /**
         * @brief Removes and returns a random member from the set. (Corresponds to SPOP)
         * @param key The set key (K).
         * @return The removed member (V), or std::optional<V> if the set is empty.
         */
        virtual std::optional<V> spop(const K &key) = 0;

        /**
         * @brief Returns all the members of the set. (Corresponds to SMEMBERS)
         * @param key The set key (K).
         * @return A vector containing all members (V) of the set. Returns an empty vector if the key does not exist.
         */
        virtual std::vector<V> smembers(const K &key) = 0;

        /**
         * @brief Returns the number of elements in the set. (Corresponds to SCARD)
         * @param key The set key (K).
         * @return The number of members in the set, or 0 if the key does not exist.
         */
        virtual long long scard(const K &key) = 0;

        /**
         * @brief Checks if a member is a member of the set. (Corresponds to SISMEMBER)
         * @param key The set key (K).
         * @param member The member (V) to check.
         * @return True if the member is present, false otherwise.
         */
        virtual bool sismember(const K &key, const V &member) = 0;

        /**
         * @brief Returns the members resulting from the intersection of all the given sets. (Corresponds to SINTER)
         * @param keys The keys (K) of the sets to intersect.
         * @return A vector of members (V) resulting from the intersection.
         */
        virtual std::vector<V> sinter(const std::vector<K> &keys) = 0;
    };

    template<typename K, typename V>
    class zset_operations {
    public:
        virtual ~zset_operations() = default;

        /**
         * @brief Adds one or more members, or updates the score of existing members, in a sorted set. (Corresponds to
         * ZADD)
         * @param key The sorted set key (K).
         * @param members A map of members (V) and their scores (double).
         * @return The number of elements added to the sorted set (not including elements updated).
         */
        virtual long long zadd(const K &key, const std::unordered_map<V, double> &members) = 0;

        /**
         * @brief Removes one or more members from a sorted set. (Corresponds to ZREM)
         * @param key The sorted set key (K).
         * @param members The members (V) to remove.
         * @return The number of members removed from the sorted set.
         */
        virtual long long zrem(const K &key, const std::vector<V> &members) = 0;

        /**
         * @brief Increments the score of a member in a sorted set by a specified value. (Corresponds to ZINCRBY)
         * @param key The sorted set key (K).
         * @param increment The amount to increment the score by (double).
         * @param member The member (V) whose score is to be incremented.
         * @return The new score (double) of the member.
         */
        virtual double zincrby(const K &key, double increment, const V &member) = 0;

        /**
         * @brief Returns the score associated with the given member in a sorted set. (Corresponds to ZSCORE)
         * @param key The sorted set key (K).
         * @param member The member (V) whose score is requested.
         * @return The score (double), or std::optional<double> if the member or key does not exist.
         */
        virtual std::optional<double> zscore(const K &key, const V &member) = 0;

        /**
         * @brief Returns a range of members in a sorted set by index, ordered from lowest to highest score (WITHOUT
         * SCORES). (Corresponds to ZRANGE)
         * @param key The sorted set key (K).
         * @param start The starting index (0 is the lowest score).
         * @param stop The stopping index (-1 is the last element).
         * @return A vector of members (V) (excluding scores), maintaining order.
         */
        virtual std::vector<V> zrange(const K &key, long long start, long long stop) = 0;

        /**
         * @brief Returns a range of members in a sorted set by index, ordered from highest to lowest score (WITHOUT
         * SCORES). (Corresponds to ZREVRANGE)
         * @param key The sorted set key (K).
         * @param start The starting index (0 is the highest score).
         * @param stop The stopping index (-1 is the lowest element).
         * @return A vector of members (V) (excluding scores), maintaining order.
         */
        virtual std::vector<V> zrevrange(const K &key, long long start, long long stop) = 0;

        /**
         * @brief Returns a range of members and their scores in a sorted set by index, ordered from lowest to highest
         * score (WITHSCORES). (Corresponds to ZRANGE WITHSCORES)
         * @param key The sorted set key (K).
         * @param start The starting index (0 is the lowest score).
         * @param stop The stopping index (-1 is the last element).
         * @return A vector of (member, score) pairs, maintaining order.
         */
        virtual std::vector<std::pair<V, double>> zrange_withscores(const K &key, long long start, long long stop) = 0;

        /**
         * @brief Returns a range of members and their scores in a sorted set by index, ordered from highest to lowest
         * score (WITHSCORES). (Corresponds to ZREVRANGE WITHSCORES)
         * @param key The sorted set key (K).
         * @param start The starting index (0 is the highest score).
         * @param stop The stopping index (-1 is the lowest element).
         * @return A vector of (member, score) pairs, maintaining order.
         */
        virtual std::vector<std::pair<V, double>> zrevrange_withscores(const K &key, long long start,
                                                                       long long stop) = 0;
    };
} // namespace redisdal
