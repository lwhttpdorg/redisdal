#pragma once

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common.hpp"

namespace janus {
	using string_scan_result = scan_result<std::string>;

	class kv_connection {
	public:
		virtual ~kv_connection() = default;

		virtual bool exists(const std::string &key) = 0;

		/**
		 * @brief Retrieves all keys matching the given pattern.
		 * @param pattern The pattern to match keys against (e.g., "*", "user:*").
		 * @param keys An unordered_set to store the matching keys.
		 */
		virtual void keys(const std::string &pattern, std::unordered_set<std::string> &keys) = 0;

		/**
		 * @brief Incrementally iterates the keys in the database.
		 * @param cursor The scan cursor.
		 * @param pattern The pattern to match keys against.
		 * @param count The number of elements to return. This is a hint to the server, not an upper limit.
		 * @return A scan_result containing the new cursor and the set of matching keys. (The cursor > 0 indicates more
		 * keys to scan.)
		 * @attention
		 *
		 * - the count is just a hint to the server, not an upper limit.
		 * - the returned cursor > 0 indicates more keys to scan, not that is an index of offset.
		 */
		virtual string_scan_result scan(uint64_t cursor, const std::string &pattern, unsigned int count) = 0;

		/**
		 * @brief Returns the data type of the value stored at key.
		 * @param key The key name to check.
		 * @return The data type as a string (e.g., "string", "list", "set", "zset", "hash", or "none" if the key does
		 * not exist).
		 * @throw std::runtime_error if the Redis reply is not of expected type.
		 */
		virtual std::string type(const std::string &key) = 0;

		virtual bool expire(const std::string &key, int seconds) = 0;

		virtual bool pexpire(const std::string &key, int milliseconds) = 0;

		virtual long long del(const std::string &keys) = 0;

		virtual long long del(const std::vector<std::string> &keys) = 0;

		/**
		 * @brief Returns the remaining time to live of a key that has an expiry.
		 * * @param key The key name to check.
		 * @return
		 * - >= 0: The remaining time to live in **seconds (s)**.
		 * - -1: The key exists but has no associated expiry (i.e., it is a persistent key).
		 * - -2: The key does not exist.
		 * @throw std::runtime_error if the Redis reply is not an integer.
		 */
		virtual int64_t ttl(const std::string &key) = 0;
		/**
		 * @brief Returns the remaining time to live of a key in milliseconds.
		 * * @param key The key name to check.
		 * @return
		 * - >= 0: The remaining time to live in **milliseconds (ms)**.
		 * - -1: The key exists but has no associated expiry (i.e., it is a persistent key).
		 * - -2: The key does not exist.
		 * @note PTTL offers higher precision than TTL.
		 * @throw std::runtime_error if the Redis reply is not an integer.
		 */
		virtual int64_t pttl(const std::string &key) = 0;

		// ============================================================================
		// For String
		// ============================================================================

		virtual bool set(const std::string &key, const std::string &value) = 0;
		/* SET if Not eXists, Only set the key if it does not already exist */
		virtual bool set_not_exists(const std::string &key, const std::string &value) = 0;
		/* Set the specified expire time, in seconds */
		virtual bool set_ex(const std::string &key, const std::string &value, int seconds) = 0;
		/* Set the specified expire time, in milliseconds */
		virtual bool set_px(const std::string &key, const std::string &value, int milliseconds) = 0;

		virtual std::optional<std::string> get(const std::string &key) = 0;

		virtual std::optional<std::string> getset(const std::string &key, const std::string &new_value) = 0;

		virtual long long incr(const std::string &key, long long delta) = 0;

		virtual long long decr(const std::string &key, long long delta) = 0;

		virtual long long append(const std::string &key, const std::string &value) = 0;

		// ============================================================================
		// For Hash
		// ============================================================================

		virtual std::optional<std::string> hget(const std::string &key, const std::string &hash_key) = 0;

		virtual void hget(const std::string &key,
						  std::unordered_map<std::string, std::optional<std::string>> &hash_map) = 0;

		virtual bool hset(const std::string &key, const std::string &field, const std::string &value) = 0;

		virtual bool hset(const std::string &key, const std::unordered_map<std::string, std::string> &hash_map) = 0;

		virtual std::unordered_map<std::string, std::string> hgetall(const std::string &key) = 0;

		virtual std::vector<std::string> hkeys(const std::string &key) = 0;

		virtual std::vector<std::string> hvals(const std::string &key) = 0;

		/**
		 * @brief Incrementally iterates the fields and values of a hash stored at key.
		 * @param key The hash key.
		 * @param cursor The scan cursor.
		 * @param pattern The pattern to match fields against.
		 * @param count The number of elements to return. This is a hint to the server, not an upper limit.
		 * @param hash_map An unordered_map to store the matching field-value pairs.
		 * @return The new cursor. (The cursor > 0 indicates more fields to scan.)
		 * @attention
		 *
		 * - the count is just a hint to the server, not an upper limit.
		 * - the returned cursor > 0 indicates more keys to scan, not that is an index of offset.
		 */
		virtual uint64_t hscan(const std::string &key, uint64_t &cursor, const std::string &pattern, unsigned int count,
							   std::unordered_map<std::string, std::string> &hash_map) = 0;

		virtual long long hdel(const std::string &key, const std::string &hash_key) = 0;

		virtual long long hdel(const std::string &key, const std::vector<std::string> &hash_keys) = 0;

		// ============================================================================
		// For list
		// ===========================================================================

		/**
		 * @brief Pushes one or multiple values onto the head (left) of a list.
		 * @param key The list key.
		 * @param values The values to push.
		 * @return The new length of the list after the push operation.
		 */
		virtual long long lpush(const std::string &key, const std::vector<std::string> &values) = 0;

		/**
		 * @brief Pushes a single value onto the head (left) of a list.
		 * @param key The list key.
		 * @param value The value to push.
		 * @return The new length of the list after the push operation.
		 */
		virtual long long lpush(const std::string &key, const std::string &value) = 0;

		/**
		 * @brief Pushes a single value onto the tail (right) of a list.
		 * @param key The list key.
		 * @param value The value to push.
		 * @return The new length of the list after the push operation.
		 */
		virtual long long rpush(const std::string &key, const std::string &value) = 0;

		/**
		 * @brief Pushes one or multiple values onto the tail (right) of a list.
		 * @param key The list key.
		 * @param values The values to push.
		 * @return The new length of the list after the push operation.
		 */
		virtual long long rpush(const std::string &key, const std::vector<std::string> &values) = 0;

		/**
		 * @brief Removes and gets the first element (head/left) of a list.
		 * @param key The list key.
		 * @return The element at the head of the list, or std::nullopt if the list is empty.
		 */
		virtual std::optional<std::string> lpop(const std::string &key) = 0;

		/**
		 * @brief Removes and gets the last element (tail/right) of a list.
		 * @param key The list key.
		 * @return The element at the tail of the list, or std::nullopt if the list is empty.
		 */
		virtual std::optional<std::string> rpop(const std::string &key) = 0;

		/**
		 * @brief Gets a range of elements from a list.
		 * @param key The list key.
		 * @param start The starting index (0 is the head).
		 * @param stop The stopping index (-1 is the last element).
		 * @return A vector of elements in the specified range.
		 */
		virtual std::vector<std::string> lrange(const std::string &key, long long start, long long stop) = 0;

		/**
		 * @brief Gets the length of a list.
		 * @param key The list key.
		 * @return The length of the list. Returns 0 if the key does not exist.
		 */
		virtual long long llen(const std::string &key) = 0;

		// ============================================================================
		// For Set
		// ============================================================================

		/**
		 * @brief Adds one or more members to a set. If a member is already a member of the set, it is ignored.
		 * @param key The set key.
		 * @param members The members to add.
		 * @return The number of members that were successfully added to the set (excluding existing members).
		 */
		virtual long long sadd(const std::string &key, const std::vector<std::string> &members) = 0;

		/**
		 * @brief Removes one or more members from a set.
		 * @param key The set key.
		 * @param members The members to remove.
		 * @return The number of members that were successfully removed from the set.
		 */
		virtual long long srem(const std::string &key, const std::vector<std::string> &members) = 0;

		/**
		 * @brief Returns all the members of the set.
		 * @param key The set key.
		 * @return A vector containing all members of the set. Returns an empty vector if the key does not exist.
		 */
		virtual std::vector<std::string> smembers(const std::string &key) = 0;

		/**
		 * @brief Returns the number of elements in the set.
		 * @param key The set key.
		 * @return The number of members in the set, or 0 if the key does not exist.
		 */
		virtual long long scard(const std::string &key) = 0;

		/**
		 * @brief Checks if a member is a member of the set.
		 * @param key The set key.
		 * @param member The member to check.
		 * @return True if the member is present, false otherwise.
		 */
		virtual bool sismember(const std::string &key, const std::string &member) = 0;

		/**
		 * @brief Removes and returns a random member from the set.
		 * @param key The set key.
		 * @return The removed member, or std::nullopt if the set is empty.
		 */
		virtual std::optional<std::string> spop(const std::string &key) = 0;

		/**
		 * @brief Returns the members resulting from the intersection of all the given sets.
		 * @param keys The keys of the sets to intersect.
		 * @return A vector of members resulting from the intersection.
		 */
		virtual std::vector<std::string> sinter(const std::vector<std::string> &keys) = 0;

		// ============================================================================
		// For ZSet
		// ============================================================================

		/**
		 * @brief Adds one or more members, or updates the score of existing members, in a sorted set.
		 * @param key The sorted set key.
		 * @param members A map of members and their scores (member -> score).
		 * @return The number of elements added to the sorted set (not including elements updated).
		 */
		virtual long long zadd(const std::string &key, const std::unordered_map<std::string, double> &members) = 0;

		/**
		 * @brief Removes one or more members from a sorted set.
		 * @param key The sorted set key.
		 * @param members The members to remove.
		 * @return The number of members removed from the sorted set.
		 */
		virtual long long zrem(const std::string &key, const std::vector<std::string> &members) = 0;

		/**
		 * @brief Returns the score associated with the given member in a sorted set.
		 * @param key The sorted set key.
		 * @param member The member whose score is requested.
		 * @return The score, or std::nullopt if the member or key does not exist.
		 */
		virtual std::optional<double> zscore(const std::string &key, const std::string &member) = 0;

		/**
		 * @brief Returns a range of members in a sorted set by index, ordered from lowest to highest score (WITHOUT
		 * SCORES).
		 * @param key The sorted set key.
		 * @param start The starting index (0 is the lowest score).
		 * @param stop The stopping index (-1 is the last element).
		 * @return A vector of members (excluding scores), maintaining order.
		 * @note Corresponds to Redis ZRANGE without WITHSCORES.
		 */
		virtual std::vector<std::string> zrange(const std::string &key, long long start, long long stop) = 0;

		/**
		 * @brief Returns a range of members in a sorted set by index, ordered from highest to lowest score (WITHOUT
		 * SCORES).
		 * @param key The sorted set key.
		 * @param start The starting index (0 is the highest score).
		 * @param stop The stopping index (-1 is the lowest element).
		 * @return A vector of members (excluding scores), maintaining order.
		 * @note Corresponds to Redis ZREVRANGE without WITHSCORES.
		 */
		virtual std::vector<std::string> zrevrange(const std::string &key, long long start, long long stop) = 0;

		/**
		 * @brief Returns a range of members and their scores in a sorted set by index, ordered from lowest to highest
		 * score (WITHSCORES).
		 * @param key The sorted set key.
		 * @param start The starting index (0 is the lowest score).
		 * @param stop The stopping index (-1 is the last element).
		 * @return A vector of (member, score) pairs, maintaining order.
		 * @note Corresponds to Redis ZRANGE ... WITHSCORES.
		 */
		virtual std::vector<std::pair<std::string, double>> zrange_withscores(const std::string &key, long long start,
																			  long long stop) = 0;

		/**
		 * @brief Returns a range of members and their scores in a sorted set by index, ordered from highest to lowest
		 * score (WITHSCORES).
		 * @param key The sorted set key.
		 * @param start The starting index (0 is the highest score).
		 * @param stop The stopping index (-1 is the lowest element).
		 * @return A vector of (member, score) pairs, maintaining order.
		 * @note Corresponds to Redis ZREVRANGE ... WITHSCORES.
		 */
		virtual std::vector<std::pair<std::string, double>> zrevrange_withscores(const std::string &key,
																				 long long start, long long stop) = 0;

		/**
		 * @brief Increments the score of a member in a sorted set by a specified value.
		 * @param key The sorted set key.
		 * @param increment The amount to increment the score by.
		 * @param member The member whose score is to be incremented.
		 * @return The new score of the member.
		 */
		virtual double zincrby(const std::string &key, double increment, const std::string &member) = 0;

		/**
		 * @brief Evaluate a Lua script server-side.
		 * @param script The Lua script to evaluate.
		 * @param keys The keys that the script will access.
		 * @param args The arguments to pass to the script.
		 * @return The result of the script execution as a cmd_result.
		 */
		virtual cmd_result eval(const std::string &script, const std::vector<std::string> &keys,
								const std::vector<std::string> &args) = 0;

		/**
		 * @brief Sends a raw command to the Redis server.
		 * @param cmd The command name.
		 * @param args The arguments for the command.
		 * @return The result of the command execution as a cmd_result.
		 */
		virtual cmd_result command(const std::string &cmd, const std::vector<std::string> &args) = 0;
	};
}
