#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

#include <hiredis/hiredis.h>

#include "kv_connection.hpp"

namespace janus {
	class redis_connection: public kv_connection {
	public:
		redis_connection(const std::string &host, const unsigned short port) {
			context = redisConnect(host.c_str(), port);
			if (!context || context->err) {
				throw std::runtime_error("Redis connect failed");
			}
		}

		~redis_connection() override {
			redisFree(context);
		}

		bool exists(const std::string &key) override {
			auto r = exec("EXISTS %s", key.c_str());
			return r->type == REDIS_REPLY_INTEGER && r->integer == 1;
		}

		void keys(const std::string &pattern, std::unordered_set<std::string> &keys) override {
			auto r = exec("KEYS %s", pattern.c_str());
			if (r->type == REDIS_REPLY_ARRAY) {
				for (size_t i = 0; i < r->elements; ++i) {
					if (r->element[i]->type == REDIS_REPLY_STRING) {
						keys.emplace(r->element[i]->str, r->element[i]->len);
					}
				}
			}
		}

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
		string_scan_result scan(uint64_t cursor, const std::string &pattern, unsigned int count) override {
			string_scan_result result{0, {}};
			const std::string cursor_str = std::to_string(cursor);
			auto r = exec("SCAN %s MATCH %s COUNT %u", cursor_str.c_str(), pattern.c_str(), count);
			if (r->type == REDIS_REPLY_ARRAY && r->elements == 2) {
				// First element is the new cursor
				if (r->element[0]->type == REDIS_REPLY_STRING) {
					try {
						result.cursor = std::stoull(std::string(r->element[0]->str, r->element[0]->len));
					}
					catch (const std::exception &e) {
						throw std::runtime_error("SCAN: failed to convert cursor to uint64_t: "
												 + std::string(e.what()));
					}
				}
				else {
					throw std::runtime_error("SCAN: unexpected cursor reply type or missing cursor.");
				}
				// Second element is the array of keys
				if (r->element[1]->type == REDIS_REPLY_ARRAY) {
					for (size_t i = 0; i < r->element[1]->elements; ++i) {
						if (r->element[1]->element[i]->type == REDIS_REPLY_STRING) {
							std::string key(r->element[1]->element[i]->str, r->element[1]->element[i]->len);
							result.keys.insert(std::move(key));
						}
					}
				}
			}
			return result;
		}

		std::string type(const std::string &key) override {
			auto r = exec("TYPE %s", key.c_str());
			if (r->type == REDIS_REPLY_STATUS) {
				return std::string(r->str, r->len);
			}
			throw std::runtime_error("TYPE: unexpected reply type");
		}

		bool expire(const std::string &key, int seconds) override {
			auto r = exec("EXPIRE %s %d", key.c_str(), seconds);
			return r->type == REDIS_REPLY_INTEGER && r->integer == 1;
		}

		bool pexpire(const std::string &key, int milliseconds) override {
			auto r = exec("PEXPIRE %s %d", key.c_str(), milliseconds);
			return r->type == REDIS_REPLY_INTEGER && r->integer == 1;
		}

		int64_t ttl(const std::string &key) override {
			auto r = exec("TTL %s", key.c_str());

			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("TTL: unexpected reply type");
			}
			return r->integer;
		}

		int64_t pttl(const std::string &key) override {
			auto r = exec("PTTL %s", key.c_str());

			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("PTTL: unexpected reply type");
			}
			return r->integer;
		}

		long long del(const std::string &key) override {
			auto r = exec("DEL %s", key.c_str());
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("DEL: unexpected reply type");
			}
			return r->integer;
		}

		long long del(const std::vector<std::string> &keys) override {
			if (keys.empty()) return 0;
			std::vector<const char *> argv;
			std::vector<size_t> argvlen;
			argv.push_back("DEL");
			argvlen.push_back(3);
			for (auto &k: keys) {
				argv.push_back(k.c_str());
				argvlen.push_back(k.size());
			}
			auto r = execv(argv, argvlen);
			return (r->type == REDIS_REPLY_INTEGER) ? r->integer : 0;
		}

		// ============================================================================
		// For String
		// ============================================================================

		bool set(const std::string &key, const std::string &value) override {
			auto r = exec("SET %s %s", key.c_str(), value.c_str());

			if (r->type == REDIS_REPLY_STATUS) {
				if (std::string(r->str, r->len) == "OK") {
					return true;
				}
			}

#ifdef DEBUG
			std::cerr << "Warning: Redis SET returned non-'OK' status." << std::endl;
#endif
			return false;
		}

		/* SET if Not eXists, Only set the key if it does not already exist */
		bool set_not_exists(const std::string &key, const std::string &value) override {
			auto r = exec("SET %s %s NX", key.c_str(), value.c_str());

			if (r->type == REDIS_REPLY_STATUS) {
				return std::string(r->str, r->len) == "OK";
			}
			if (r->type == REDIS_REPLY_NIL) {
				return false;
			}
			return false;
		}

		/* Set the specified expire time, in seconds */
		bool set_ex(const std::string &key, const std::string &value, int seconds) override {
			auto r = exec("SET %s %s EX %d", key.c_str(), value.c_str(), seconds);
			return (r->type == REDIS_REPLY_STATUS) && (std::string(r->str, r->len) == "OK");
		}

		/* Set the specified expire time, in milliseconds */
		bool set_px(const std::string &key, const std::string &value, int milliseconds) override {
			auto r = exec("SET %s %s PX %d", key.c_str(), value.c_str(), milliseconds);
			return (r->type == REDIS_REPLY_STATUS) && (std::string(r->str, r->len) == "OK");
		}

		std::optional<std::string> get(const std::string &key) override {
			auto r = exec("GET %s", key.c_str());
			if (r->type == REDIS_REPLY_NIL) return std::nullopt;
			if (r->type == REDIS_REPLY_STRING) return std::string(r->str, r->len);
			return std::nullopt;
		}

		std::optional<std::string> getset(const std::string &key, const std::string &new_value) override {
			auto r = exec("GETSET %s %s", key.c_str(), new_value.c_str());
			if (r->type == REDIS_REPLY_NIL) return std::nullopt;
			if (r->type == REDIS_REPLY_STRING) return std::string(r->str, r->len);
			return std::nullopt;
		}

		long long incr(const std::string &key, long long delta) override {
			auto r = exec("INCRBY %s %lld", key.c_str(), delta);
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("INCRBY: unexpected reply type");
			}
			return r->integer;
		}

		long long decr(const std::string &key, long long delta) override {
			auto r = exec("DECRBY %s %lld", key.c_str(), delta);
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("DECRBY: unexpected reply type");
			}
			return r->integer;
		}

		long long append(const std::string &key, const std::string &value) override {
			auto r = exec("APPEND %s %s", key.c_str(), value.c_str());
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("APPEND: unexpected reply type");
			}
			return r->integer;
		}

		// ============================================================================
		// For Hash
		// ============================================================================

		std::optional<std::string> hget(const std::string &key, const std::string &hash_key) override {
			auto r = exec("HGET %s %s", key.c_str(), hash_key.c_str());
			if (r->type == REDIS_REPLY_NIL) {
				return std::nullopt;
			}
			if (r->type == REDIS_REPLY_STRING) {
				return std::string(r->str, r->len);
			}
			throw std::runtime_error("Unexpected reply type in HGET");
		}

		void hget(const std::string &key,
				  std::unordered_map<std::string, std::optional<std::string>> &hash_map) override {
			if (hash_map.empty()) return;

			std::vector<const char *> argv;
			std::vector<size_t> argvlen;

			argv.push_back("HMGET");
			argvlen.push_back(5);
			argv.push_back(key.c_str());
			argvlen.push_back(key.size());

			std::vector<std::string> fields;
			fields.reserve(hash_map.size());
			for (auto &kv: hash_map) {
				argv.push_back(kv.first.c_str());
				argvlen.push_back(kv.first.size());
				fields.push_back(kv.first);
			}

			auto r = execv(argv, argvlen);
			if (r->type != REDIS_REPLY_ARRAY) return;

			for (size_t i = 0; i < r->elements && i < fields.size(); ++i) {
				redisReply *elem = r->element[i];
				if (elem->type == REDIS_REPLY_STRING) {
					hash_map[fields[i]] = std::string(elem->str, elem->len);
				}
				else if (elem->type == REDIS_REPLY_NIL) {
					hash_map[fields[i]] = std::nullopt;
				}
				else {
					throw std::runtime_error("HMGET: unexpected element type");
				}
			}
		}

		bool hset(const std::string &key, const std::string &field, const std::string &value) override {
			auto r = exec("HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
			return r->type == REDIS_REPLY_INTEGER && r->integer >= 0;
		}

		bool hset(const std::string &key, const std::unordered_map<std::string, std::string> &hash_map) override {
			if (hash_map.empty()) return false;

			std::vector<const char *> argv;
			std::vector<size_t> argvlen;

			argv.push_back("HSET");
			argvlen.push_back(4);
			argv.push_back(key.c_str());
			argvlen.push_back(key.size());

			for (auto &kv: hash_map) {
				argv.push_back(kv.first.c_str());
				argvlen.push_back(kv.first.size());
				argv.push_back(kv.second.c_str());
				argvlen.push_back(kv.second.size());
			}

			redisReply *raw = static_cast<redisReply *>(
				redisCommandArgv(context, static_cast<int>(argv.size()), argv.data(), argvlen.data()));
			if (!raw) throw std::runtime_error("HSET command failed");
			std::unique_ptr<redisReply, reply_deleter> r(raw);

			return r->type == REDIS_REPLY_INTEGER && r->integer >= 0;
		}

		std::unordered_map<std::string, std::string> hgetall(const std::string &key) override {
			std::unordered_map<std::string, std::string> result;
			auto r = exec("HGETALL %s", key.c_str());
			if (r->type == REDIS_REPLY_ARRAY) {
				for (size_t i = 0; i + 1 < r->elements; i += 2) {
					result.emplace(r->element[i]->str, r->element[i + 1]->str);
				}
			}
			return result;
		}

		std::vector<std::string> hkeys(const std::string &key) override {
			std::vector<std::string> result;
			auto r = exec("HKEYS %s", key.c_str());
			if (r->type == REDIS_REPLY_ARRAY) {
				for (size_t i = 0; i < r->elements; ++i) {
					result.emplace_back(r->element[i]->str, r->element[i]->len);
				}
			}
			return result;
		}

		std::vector<std::string> hvals(const std::string &key) override {
			std::vector<std::string> result;
			auto r = exec("HVALS %s", key.c_str());
			if (r->type == REDIS_REPLY_ARRAY) {
				for (size_t i = 0; i < r->elements; ++i) {
					result.emplace_back(r->element[i]->str, r->element[i]->len);
				}
			}
			return result;
		}

		/* @brief Incrementally iterates the fields and values of a hash stored at key.
		 * @param key The hash key.
		 * @param cursor The scan cursor.
		 * @param pattern The pattern to match fields against.
		 * @param count The number of elements to return. This is a hint to the server, not an limit.
		 * @param hash_map An unordered_map to store the matching field-value pairs.
		 * @return The new cursor. (The cursor > 0 indicates more fields to scan.)
		 * @attention
		 *
		 * - the count is just a hint to the server, not an upper limit.
		 * - the returned cursor > 0 indicates more keys to scan, not that is an index of offset.
		 */
		uint64_t hscan(const std::string &key, uint64_t &cursor, const std::string &pattern, unsigned int count,
					   std::unordered_map<std::string, std::string> &hash_map) override {
			const std::string cursor_str = std::to_string(cursor);
			auto r = exec("HSCAN %s %s MATCH %s COUNT %u", key.c_str(), cursor_str.c_str(), pattern.c_str(), count);
			if (r->type == REDIS_REPLY_ARRAY && r->elements == 2) {
				// First element is the new cursor
				if (r->element[0]->type == REDIS_REPLY_STRING) {
					try {
						std::string cursor_reply(r->element[0]->str, r->element[0]->len);
						cursor = std::stoull(cursor_reply);
					}
					catch (const std::exception &e) {
						throw std::runtime_error("HSCAN: failed to convert cursor to uint64_t: "
												 + std::string(e.what()));
					}
				}
				else {
					throw std::runtime_error("HSCAN: unexpected cursor reply type or missing cursor.");
				}
				// Second element is the array of field-value pairs
				if (r->element[1]->type == REDIS_REPLY_ARRAY) {
					for (size_t i = 0; i + 1 < r->element[1]->elements; i += 2) {
						if (r->element[1]->element[i]->type == REDIS_REPLY_STRING
							&& r->element[1]->element[i + 1]->type == REDIS_REPLY_STRING) {
							std::string field(r->element[1]->element[i]->str, r->element[1]->element[i]->len);
							std::string value(r->element[1]->element[i + 1]->str, r->element[1]->element[i + 1]->len);
							hash_map.emplace(std::move(field), std::move(value));
						}
					}
				}
			}
			return cursor;
		}

		long long hdel(const std::string &key, const std::string &hash_key) override {
			auto r = exec("HDEL %s %s", key.c_str(), hash_key.c_str());
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("HDEL: unexpected reply type");
			}
			return r->integer;
		}

		long long hdel(const std::string &key, const std::vector<std::string> &hash_keys) override {
			if (hash_keys.empty()) return 0;

			std::vector<const char *> argv;
			std::vector<size_t> argvlen;

			argv.push_back("HDEL");
			argvlen.push_back(4);
			argv.push_back(key.c_str());
			argvlen.push_back(key.size());

			for (auto &f: hash_keys) {
				argv.push_back(f.c_str());
				argvlen.push_back(f.size());
			}

			auto r = execv(argv, argvlen);
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("HDEL: unexpected reply type");
			}
			return r->integer;
		}

		// ============================================================================
		// For list
		// ============================================================================

		long long lpush(const std::string &key, const std::vector<std::string> &values) override {
			if (values.empty()) return llen(key);

			std::vector<const char *> argv;
			std::vector<size_t> argvlen;

			argv.push_back("LPUSH");
			argvlen.push_back(5);
			argv.push_back(key.c_str());
			argvlen.push_back(key.size());

			for (const auto &v: values) {
				argv.push_back(v.c_str());
				argvlen.push_back(v.size());
			}

			auto r = execv(argv, argvlen);
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("LPUSH: unexpected reply type");
			}
			return r->integer;
		}

		long long lpush(const std::string &key, const std::string &value) override {
			auto r = exec("LPUSH %s %s", key.c_str(), value.c_str());
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("LPUSH: unexpected reply type");
			}
			return r->integer;
		}

		long long rpush(const std::string &key, const std::string &value) override {
			auto r = exec("RPUSH %s %s", key.c_str(), value.c_str());
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("RPUSH: unexpected reply type");
			}
			return r->integer;
		}

		long long rpush(const std::string &key, const std::vector<std::string> &values) override {
			if (values.empty()) return llen(key);

			std::vector<const char *> argv;
			std::vector<size_t> argvlen;

			argv.push_back("RPUSH");
			argvlen.push_back(5);
			argv.push_back(key.c_str());
			argvlen.push_back(key.size());

			for (const auto &v: values) {
				argv.push_back(v.c_str());
				argvlen.push_back(v.size());
			}

			auto r = execv(argv, argvlen);
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("RPUSH: unexpected reply type");
			}
			return r->integer;
		}

		std::optional<std::string> lpop(const std::string &key) override {
			auto r = exec("LPOP %s", key.c_str());
			if (r->type == REDIS_REPLY_NIL) return std::nullopt;
			if (r->type == REDIS_REPLY_STRING) return std::string(r->str, r->len);
			throw std::runtime_error("LPOP: unexpected reply type");
		}

		std::optional<std::string> rpop(const std::string &key) override {
			auto r = exec("RPOP %s", key.c_str());
			if (r->type == REDIS_REPLY_NIL) return std::nullopt;
			if (r->type == REDIS_REPLY_STRING) return std::string(r->str, r->len);
			throw std::runtime_error("RPOP: unexpected reply type");
		}

		std::vector<std::string> lrange(const std::string &key, long long start, long long stop) override {
			std::vector<std::string> result;
			auto r = exec("LRANGE %s %lld %lld", key.c_str(), start, stop);
			if (r->type == REDIS_REPLY_ARRAY) {
				result.reserve(r->elements);
				for (size_t i = 0; i < r->elements; ++i) {
					if (r->element[i]->type == REDIS_REPLY_STRING) {
						result.emplace_back(r->element[i]->str, r->element[i]->len);
					}
				}
			}
			else if (r->type != REDIS_REPLY_NIL) {
				throw std::runtime_error("LRANGE: unexpected reply type");
			}
			return result;
		}

		long long llen(const std::string &key) override {
			auto r = exec("LLEN %s", key.c_str());
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("LLEN: unexpected reply type");
			}
			return r->integer;
		}

		// ============================================================================
		// For Set
		// ============================================================================

		long long sadd(const std::string &key, const std::vector<std::string> &members) override {
			if (members.empty()) return 0;

			std::vector<const char *> argv;
			std::vector<size_t> argvlen;

			argv.push_back("SADD");
			argvlen.push_back(4);
			argv.push_back(key.c_str());
			argvlen.push_back(key.size());

			for (const auto &m: members) {
				argv.push_back(m.c_str());
				argvlen.push_back(m.size());
			}

			auto r = execv(argv, argvlen);
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("SADD: unexpected reply type");
			}
			return r->integer;
		}

		long long srem(const std::string &key, const std::vector<std::string> &members) override {
			if (members.empty()) return 0;

			std::vector<const char *> argv;
			std::vector<size_t> argvlen;

			argv.push_back("SREM");
			argvlen.push_back(4);
			argv.push_back(key.c_str());
			argvlen.push_back(key.size());

			for (const auto &m: members) {
				argv.push_back(m.c_str());
				argvlen.push_back(m.size());
			}

			auto r = execv(argv, argvlen);
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("SREM: unexpected reply type");
			}
			return r->integer;
		}

		std::vector<std::string> smembers(const std::string &key) override {
			std::vector<std::string> result;
			auto r = exec("SMEMBERS %s", key.c_str());
			if (r->type == REDIS_REPLY_ARRAY) {
				result.reserve(r->elements);
				for (size_t i = 0; i < r->elements; ++i) {
					if (r->element[i]->type == REDIS_REPLY_STRING) {
						result.emplace_back(r->element[i]->str, r->element[i]->len);
					}
				}
			}
			else if (r->type != REDIS_REPLY_NIL) {
				throw std::runtime_error("SMEMBERS: unexpected reply type");
			}
			return result;
		}

		long long scard(const std::string &key) override {
			auto r = exec("SCARD %s", key.c_str());
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("SCARD: unexpected reply type");
			}
			return r->integer;
		}

		bool sismember(const std::string &key, const std::string &member) override {
			auto r = exec("SISMEMBER %s %s", key.c_str(), member.c_str());
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("SISMEMBER: unexpected reply type");
			}
			return r->integer == 1;
		}

		std::optional<std::string> spop(const std::string &key) override {
			auto r = exec("SPOP %s", key.c_str());
			if (r->type == REDIS_REPLY_NIL) return std::nullopt;
			if (r->type == REDIS_REPLY_STRING) return std::string(r->str, r->len);
			throw std::runtime_error("SPOP: unexpected reply type");
		}

		std::vector<std::string> sinter(const std::vector<std::string> &keys) override {
			if (keys.empty()) return {};

			std::vector<const char *> argv;
			std::vector<size_t> argvlen;

			argv.push_back("SINTER");
			argvlen.push_back(6);
			for (const auto &k: keys) {
				argv.push_back(k.c_str());
				argvlen.push_back(k.size());
			}

			std::vector<std::string> result;
			auto r = execv(argv, argvlen);

			if (r->type == REDIS_REPLY_ARRAY) {
				result.reserve(r->elements);
				for (size_t i = 0; i < r->elements; ++i) {
					if (r->element[i]->type == REDIS_REPLY_STRING) {
						result.emplace_back(r->element[i]->str, r->element[i]->len);
					}
				}
			}
			else if (r->type != REDIS_REPLY_NIL) {
				throw std::runtime_error("SINTER: unexpected reply type");
			}
			return result;
		}

		// ============================================================================
		// For ZSet
		// ============================================================================

		long long zadd(const std::string &key, const std::unordered_map<std::string, double> &members) override {
			if (members.empty()) return 0;

			std::vector<const char *> argv;
			std::vector<size_t> argvlen;

			argv.push_back("ZADD");
			argvlen.push_back(4);
			argv.push_back(key.c_str());
			argvlen.push_back(key.size());

			std::vector<std::string> score_strings;
			score_strings.reserve(members.size());

			for (const auto &member: members) {
				score_strings.emplace_back(std::to_string(member.second));
				argv.push_back(score_strings.back().c_str());
				argvlen.push_back(score_strings.back().size());
				argv.push_back(member.first.c_str());
				argvlen.push_back(member.first.size());
			}

			auto r = execv(argv, argvlen);
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("ZADD: unexpected reply type");
			}
			return r->integer;
		}

		long long zrem(const std::string &key, const std::vector<std::string> &members) override {
			if (members.empty()) return 0;

			std::vector<const char *> argv;
			std::vector<size_t> argvlen;

			argv.push_back("ZREM");
			argvlen.push_back(4);
			argv.push_back(key.c_str());
			argvlen.push_back(key.size());

			for (const auto &m: members) {
				argv.push_back(m.c_str());
				argvlen.push_back(m.size());
			}

			auto r = execv(argv, argvlen);
			if (r->type != REDIS_REPLY_INTEGER) {
				throw std::runtime_error("ZREM: unexpected reply type");
			}
			return r->integer;
		}

		std::optional<double> zscore(const std::string &key, const std::string &member) override {
			auto r = exec("ZSCORE %s %s", key.c_str(), member.c_str());
			if (r->type == REDIS_REPLY_NIL) return std::nullopt;

			if (r->type == REDIS_REPLY_STRING) {
				try {
					return std::stod(std::string(r->str, r->len));
				}
				catch (const std::exception &) {
					throw std::runtime_error("ZSCORE: failed to convert score to double");
				}
			}
			throw std::runtime_error("ZSCORE: unexpected reply type");
		}

		std::vector<std::string> zrange(const std::string &key, long long start, long long stop) override {
			// ZRANGE key start stop
			std::vector<std::string> result;
			auto r = exec("ZRANGE %s %lld %lld", key.c_str(), start, stop);

			if (r->type == REDIS_REPLY_ARRAY) {
				result.reserve(r->elements);
				for (size_t i = 0; i < r->elements; ++i) {
					if (r->element[i]->type == REDIS_REPLY_STRING) {
						result.emplace_back(r->element[i]->str, r->element[i]->len);
					}
				}
			}
			else if (r->type != REDIS_REPLY_NIL) {
				throw std::runtime_error("ZRANGE: unexpected reply type");
			}
			return result;
		}

		std::vector<std::string> zrevrange(const std::string &key, long long start, long long stop) override {
			// ZREVRANGE key start stop
			std::vector<std::string> result;
			auto r = exec("ZREVRANGE %s %lld %lld", key.c_str(), start, stop);

			if (r->type == REDIS_REPLY_ARRAY) {
				result.reserve(r->elements);
				for (size_t i = 0; i < r->elements; ++i) {
					if (r->element[i]->type == REDIS_REPLY_STRING) {
						result.emplace_back(r->element[i]->str, r->element[i]->len);
					}
				}
			}
			else if (r->type != REDIS_REPLY_NIL) {
				throw std::runtime_error("ZREVRANGE: unexpected reply type");
			}
			return result;
		}

		std::vector<std::pair<std::string, double>> zrange_withscores(const std::string &key, long long start,
																	  long long stop) override {
			// ZRANGE key start stop WITHSCORES
			std::vector<std::pair<std::string, double>> result;
			auto r = exec("ZRANGE %s %lld %lld WITHSCORES", key.c_str(), start, stop);

			if (r->type == REDIS_REPLY_ARRAY) {
				if (r->elements % 2 != 0) {
					throw std::runtime_error("ZRANGE WITHSCORES: expected even number of elements");
				}
				result.reserve(r->elements / 2);

				for (size_t i = 0; i + 1 < r->elements; i += 2) {
					redisReply *member_reply = r->element[i];
					redisReply *score_reply = r->element[i + 1];

					if (member_reply->type == REDIS_REPLY_STRING && score_reply->type == REDIS_REPLY_STRING) {
						try {
							double score = std::stod(std::string(score_reply->str, score_reply->len));
							result.emplace_back(std::string(member_reply->str, member_reply->len), score);
						}
						catch (const std::exception &) {
							throw std::runtime_error("ZRANGE WITHSCORES: score conversion failed");
						}
					}
					else {
						throw std::runtime_error("ZRANGE WITHSCORES: unexpected element type");
					}
				}
			}
			else if (r->type != REDIS_REPLY_NIL) {
				throw std::runtime_error("ZRANGE WITHSCORES: unexpected reply type");
			}
			return result;
		}

		std::vector<std::pair<std::string, double>> zrevrange_withscores(const std::string &key, long long start,
																		 long long stop) override {
			// ZREVRANGE key start stop WITHSCORES
			std::vector<std::pair<std::string, double>> result;
			auto r = exec("ZREVRANGE %s %lld %lld WITHSCORES", key.c_str(), start, stop);

			if (r->type == REDIS_REPLY_ARRAY) {
				if (r->elements % 2 != 0) {
					throw std::runtime_error("ZREVRANGE WITHSCORES: expected even number of elements");
				}
				result.reserve(r->elements / 2);

				for (size_t i = 0; i + 1 < r->elements; i += 2) {
					redisReply *member_reply = r->element[i];
					redisReply *score_reply = r->element[i + 1];

					if (member_reply->type == REDIS_REPLY_STRING && score_reply->type == REDIS_REPLY_STRING) {
						try {
							double score = std::stod(std::string(score_reply->str, score_reply->len));
							result.emplace_back(std::string(member_reply->str, member_reply->len), score);
						}
						catch (const std::exception &) {
							throw std::runtime_error("ZREVRANGE WITHSCORES: score conversion failed");
						}
					}
					else {
						throw std::runtime_error("ZREVRANGE WITHSCORES: unexpected element type");
					}
				}
			}
			else if (r->type != REDIS_REPLY_NIL) {
				throw std::runtime_error("ZREVRANGE WITHSCORES: unexpected reply type");
			}
			return result;
		}

		double zincrby(const std::string &key, double increment, const std::string &member) override {
			std::string increment_str = std::to_string(increment);

			// ZINCRBY key increment member
			auto r = exec("ZINCRBY %s %s %s", key.c_str(), increment_str.c_str(), member.c_str());

			if (r->type == REDIS_REPLY_STRING) {
				try {
					return std::stod(std::string(r->str, r->len));
				}
				catch (const std::exception &) {
					throw std::runtime_error("ZINCRBY: failed to convert returned score to double");
				}
			}
			throw std::runtime_error("ZINCRBY: unexpected reply type");
		}

	protected:
		struct reply_deleter {
			void operator()(redisReply *r) const noexcept {
				if (r) freeReplyObject(r);
			}
		};
		using reply_ptr = std::unique_ptr<redisReply, reply_deleter>;

		reply_ptr exec(const char *fmt, ...) const {
			va_list ap;
			va_start(ap, fmt);
			redisReply *r = static_cast<redisReply *>(redisvCommand(context, fmt, ap));
			va_end(ap);
			if (!r) throw std::runtime_error("Command failed");
			if (r->type == REDIS_REPLY_ERROR) {
				std::string err(r->str, r->len);
				freeReplyObject(r);
				throw std::runtime_error("Redis error: " + err);
			}
			return reply_ptr(r);
		}

		[[nodiscard]] reply_ptr execv(const std::vector<const char *> &argv, const std::vector<size_t> &argvlen) const {
			redisReply *r = static_cast<redisReply *>(redisCommandArgv(
				context, static_cast<int>(argv.size()), const_cast<const char **>(argv.data()), argvlen.data()));
			if (!r) throw std::runtime_error("CommandArgv failed");
			if (r->type == REDIS_REPLY_ERROR) {
				std::string err(r->str, r->len);
				freeReplyObject(r);
				throw std::runtime_error("Redis error: " + err);
			}
			return reply_ptr(r);
		}

	private:
		redisContext *context;
	};
}
