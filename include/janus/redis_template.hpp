#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "common.hpp"
#include "redis_operations.hpp"

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

		bool exists(const K &key) override {
			auto serialized_key = this->serialize_key(key);
			return connection.exists(serialized_key);
		}

		void keys(const std::string &pattern, std::unordered_set<K> &keys) override {
			std::unordered_set<std::string> s_keys;
			connection.keys(pattern, s_keys);
			for (const auto &s_key: s_keys) {
				keys.insert(this->deserialize_key(s_key));
			}
		}

		scan_result<K> scan(uint64_t cursor, const std::string &pattern, unsigned int count) override {
			auto s_result = connection.scan(cursor, pattern, count);
			scan_result<K> result;
			result.cursor = s_result.cursor;
			for (const auto &s_key: s_result.keys) {
				result.keys.insert(this->deserialize_key(s_key));
			}
			return result;
		}

		std::string type(const K &key) override {
			auto serialized_key = this->serialize_key(key);
			return connection.type(serialized_key);
		}

		bool expire(const K &key, long long seconds) override {
			auto serialized_key = this->serialize_key(key);
			return connection.expire(serialized_key, seconds);
		}

		bool pexpire(const K &key, int milliseconds) override {
			auto serialized_key = this->serialize_key(key);
			return connection.pexpire(serialized_key, milliseconds);
		}

		long long del(const K &key) override {
			auto serialized_key = this->serialize_key(key);
			return connection.del(serialized_key);
		}

		long long del(const std::vector<K> &keys) override {
			std::vector<std::string> s_keys;
			s_keys.reserve(keys.size());

			for (const auto &key: keys) {
				s_keys.push_back(this->serialize_key(key));
			}
			return connection.del(s_keys);
		}

		int64_t ttl(const K &key) override {
			auto serialized_key = this->serialize_key(key);
			return connection.ttl(serialized_key);
		}

		int64_t pttl(const K &key) override {
			auto serialized_key = this->serialize_key(key);
			return connection.pttl(serialized_key);
		}

		cmd_reply eval(const std::string &script, const std::vector<K> &keys, const std::vector<V> &args) override {
			std::vector<std::string> s_keys;
			for (const auto &key: keys) {
				s_keys.push_back(this->serialize_key(key));
			}
			std::vector<std::string> s_args;
			for (const auto &arg: args) {
				s_args.push_back(this->serialize_value(arg));
			}
			return connection.eval(script, s_keys, s_args);
		}

		std::string script_load(const std::string &script) override {
			std::string sha = connection.script_load(script);
			// cache mapping from sha -> script so we can reload on NOSCRIPT
			sha1_cache.emplace(sha, script);
			return sha;
		}

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

		cmd_reply exec_cmd(const std::string &cmd, const std::vector<std::string> &args) override {
			return connection.command(cmd, args);
		}

		// ==========================================================
		// Implementation of Operation Views (Returning References)
		// ==========================================================

		value_operations<K, V> &ops_for_value() override {
			return *value_ops;
		}

		hash_operations<K, V> &ops_for_hash() override {
			return *hash_ops;
		}

		list_operations<K, V> &ops_for_list() override {
			return *list_ops;
		}

		set_operations<K, V> &ops_for_set() override {
			return *set_ops;
		}

		zset_operations<K, V> &ops_for_zset() override {
			return *zset_ops;
		}

		[[nodiscard]] std::string serialize_key(const K &key) const {
			return key_serializer.serialize(key);
		}

		[[nodiscard]] K deserialize_key(const std::string &data) const {
			return key_serializer.deserialize(data);
		}

		[[nodiscard]] std::string serialize_value(const V &value) const {
			return value_serializer.serialize(value);
		}

		[[nodiscard]] V deserialize_value(const std::string &data) const {
			return value_serializer.deserialize(data);
		}

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
}
