#pragma once

#include "operations.hpp"

namespace janus {
	template<typename K, typename V>
	class redis_template;

	template<typename K, typename V>
	class default_value_operations: public value_operations<K, V> {
	public:
		explicit default_value_operations(redis_template<K, V> &ops) : tpl(ops) {
		}

		bool set(const K &key, const V &value) override {
			return tpl.get_connection().set(tpl.serialize_key(key), tpl.serialize_value(value));
		}

		std::optional<V> get(const K &key) override {
			auto val = tpl.get_connection().get(tpl.serialize_key(key));
			if (val) return tpl.deserialize_value(*val);
			return std::nullopt;
		}

		long long incr(const K &key, long long delta) override {
			return tpl.get_connection().incr(tpl.serialize_key(key), delta);
		}

		long long decr(const K &key, long long delta) override {
			return tpl.get_connection().decr(tpl.serialize_key(key), delta);
		}

		long long append(const K &key, const V &value) override {
			return tpl.get_connection().append(tpl.serialize_key(key), tpl.serialize_value(value));
		}

		std::optional<V> get_and_set(const K &key, const V &value) override {
			auto val = tpl.get_connection().getset(tpl.serialize_key(key), tpl.serialize_value(value));
			if (val) return tpl.deserialize_value(*val);
			return std::nullopt;
		}

	private:
		redis_template<K, V> &tpl;
	};

	template<typename K, typename V>
	class default_hash_operations: public hash_operations<K, V> {
	public:
		explicit default_hash_operations(redis_template<K, V> &tpl) : tpl(tpl) {
		}

		std::optional<V> hget(const K &key, const K &hash_key) override {
			auto val = tpl.get_connection().hget(tpl.serialize_key(key), tpl.serialize_key(hash_key));
			if (val) return tpl.deserialize_value(*val);
			return std::nullopt;
		}

		void hget(const K &key, std::unordered_map<K, std::optional<V>> &hash_map) override {
			std::unordered_map<std::string, std::optional<std::string>> serialized_map;
			for (const auto &pair: hash_map) {
				serialized_map.emplace(tpl.serialize_key(pair.first), std::nullopt);
			}

			tpl.get_connection().hget(tpl.serialize_key(key), serialized_map);

			hash_map.clear();
			for (const auto &pair: serialized_map) {
				std::optional<V> deserialized_value = std::nullopt;
				if (pair.second) {
					deserialized_value = tpl.deserialize_value(*pair.second);
				}
				hash_map.emplace(tpl.deserialize_key(pair.first), deserialized_value);
			}
		}

		std::unordered_map<K, V> hgetall(const K &key) override {
			auto raw_map = tpl.get_connection().hgetall(tpl.serialize_key(key));
			std::unordered_map<K, V> result;
			for (const auto &pair: raw_map) {
				result.emplace(tpl.deserialize_key(pair.first), tpl.deserialize_value(pair.second));
			}
			return result;
		}

		std::vector<K> hkeys(const K &key) override {
			auto raw_keys = tpl.get_connection().hkeys(tpl.serialize_key(key));
			std::vector<K> result;
			result.reserve(raw_keys.size());
			for (const auto &k: raw_keys) {
				result.push_back(tpl.deserialize_key(k));
			}
			return result;
		}

		std::vector<V> hvals(const K &key) override {
			auto raw_vals = tpl.get_connection().hvals(tpl.serialize_key(key));
			std::vector<V> result;
			result.reserve(raw_vals.size());
			for (const auto &v: raw_vals) {
				result.push_back(tpl.deserialize_value(v));
			}
			return result;
		}

		bool hset(const K &key, const K &field, const V &value) override {
			return tpl.get_connection().hset(tpl.serialize_key(key), tpl.serialize_key(field),
											 tpl.serialize_value(value));
		}

		bool hset(const K &key, const std::unordered_map<K, V> &hash_map) override {
			std::unordered_map<std::string, std::string> serialized_map;
			for (const auto &pair: hash_map) {
				serialized_map.emplace(tpl.serialize_key(pair.first), tpl.serialize_value(pair.second));
			}
			return tpl.get_connection().hset(tpl.serialize_key(key), serialized_map);
		}

		long long hdel(const K &key, const K &hash_key) override {
			return tpl.get_connection().hdel(tpl.serialize_key(key), tpl.serialize_key(hash_key));
		}

		long long hdel(const K &key, const std::vector<K> &hash_keys) override {
			std::vector<std::string> serialized_keys;
			serialized_keys.reserve(hash_keys.size());
			for (const auto &k: hash_keys) {
				serialized_keys.push_back(tpl.serialize_key(k));
			}
			return tpl.get_connection().hdel(tpl.serialize_key(key), serialized_keys);
		}

	private:
		redis_template<K, V> &tpl;
	};

	template<typename K, typename V>
	class default_list_operations: public list_operations<K, V> {
	public:
		explicit default_list_operations(redis_template<K, V> &ops) : tpl(ops) {
		}

		long long lpush(const K &key, const std::vector<V> &values) override {
			std::vector<std::string> serialized_values;
			serialized_values.reserve(values.size());
			for (const auto &v: values) {
				serialized_values.push_back(tpl.serialize_value(v));
			}
			return tpl.get_connection().lpush(tpl.serialize_key(key), serialized_values);
		}

		long long lpush(const K &key, const V &value) override {
			return tpl.get_connection().lpush(tpl.serialize_key(key), tpl.serialize_value(value));
		}

		long long rpush(const K &key, const V &value) override {
			return rpush(key, std::vector<V>{value});
		}

		long long rpush(const K &key, const std::vector<V> &values) override {
			std::vector<std::string> serialized_values;
			serialized_values.reserve(values.size());
			for (const auto &v: values) {
				serialized_values.push_back(tpl.serialize_value(v));
			}
			return tpl.get_connection().rpush(tpl.serialize_key(key), serialized_values);
		}

		std::optional<V> lpop(const K &key) override {
			auto val = tpl.get_connection().lpop(tpl.serialize_key(key));
			if (val) return tpl.deserialize_value(*val);
			return std::nullopt;
		}

		std::optional<V> rpop(const K &key) override {
			auto val = tpl.get_connection().rpop(tpl.serialize_key(key));
			if (val) return tpl.deserialize_value(*val);
			return std::nullopt;
		}

		std::vector<V> lrange(const K &key, long long start, long long stop) override {
			auto raw_list = tpl.get_connection().lrange(tpl.serialize_key(key), start, stop);
			std::vector<V> result;
			result.reserve(raw_list.size());
			for (const auto &v: raw_list) {
				result.push_back(tpl.deserialize_value(v));
			}
			return result;
		}

		long long llen(const K &key) override {
			return tpl.get_connection().llen(tpl.serialize_key(key));
		}

	private:
		redis_template<K, V> &tpl;
	};

	template<typename K, typename V>
	class default_set_operations: public set_operations<K, V> {
	public:
		explicit default_set_operations(redis_template<K, V> &ops) : tpl(ops) {
		}

		long long sadd(const K &key, const std::vector<V> &members) override {
			std::vector<std::string> serialized_members;
			serialized_members.reserve(members.size());
			for (const auto &m: members) {
				serialized_members.push_back(tpl.serialize_value(m));
			}
			return tpl.get_connection().sadd(tpl.serialize_key(key), serialized_members);
		}

		long long srem(const K &key, const std::vector<V> &members) override {
			std::vector<std::string> serialized_members;
			serialized_members.reserve(members.size());
			for (const auto &m: members) {
				serialized_members.push_back(tpl.serialize_value(m));
			}
			return tpl.get_connection().srem(tpl.serialize_key(key), serialized_members);
		}

		std::optional<V> spop(const K &key) override {
			auto val = tpl.get_connection().spop(tpl.serialize_key(key));
			if (val) return tpl.deserialize_value(*val);
			return std::nullopt;
		}

		std::vector<V> smembers(const K &key) override {
			auto raw_members = tpl.get_connection().smembers(tpl.serialize_key(key));
			std::vector<V> result;
			result.reserve(raw_members.size());
			for (const auto &m: raw_members) {
				result.push_back(tpl.deserialize_value(m));
			}
			return result;
		}

		long long scard(const K &key) override {
			return tpl.get_connection().scard(tpl.serialize_key(key));
		}

		bool sismember(const K &key, const V &member) override {
			return tpl.get_connection().sismember(tpl.serialize_key(key), tpl.serialize_value(member));
		}

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
		redis_template<K, V> &tpl;
	};

	template<typename K, typename V>
	class default_zset_operations: public zset_operations<K, V> {
	public:
		explicit default_zset_operations(redis_template<K, V> &ops) : tpl(ops) {
		}

		long long zadd(const K &key, const std::unordered_map<V, double> &members) override {
			std::unordered_map<std::string, double> serialized_members;
			for (const auto &pair: members) {
				serialized_members.emplace(tpl.serialize_value(pair.first), pair.second);
			}
			return tpl.get_connection().zadd(tpl.serialize_key(key), serialized_members);
		}

		long long zrem(const K &key, const std::vector<V> &members) override {
			std::vector<std::string> serialized_members;
			serialized_members.reserve(members.size());
			for (const auto &m: members) {
				serialized_members.push_back(tpl.serialize_value(m));
			}
			return tpl.get_connection().zrem(tpl.serialize_key(key), serialized_members);
		}

		double zincrby(const K &key, double increment, const V &member) override {
			return tpl.get_connection().zincrby(tpl.serialize_key(key), increment, tpl.serialize_value(member));
		}

		std::optional<double> zscore(const K &key, const V &member) override {
			return tpl.get_connection().zscore(tpl.serialize_key(key), tpl.serialize_value(member));
		}

		std::vector<V> zrange(const K &key, long long start, long long stop) override {
			auto raw_members = tpl.get_connection().zrange(tpl.serialize_key(key), start, stop);
			std::vector<V> result;
			result.reserve(raw_members.size());
			for (const auto &m: raw_members) {
				result.push_back(tpl.deserialize_value(m));
			}
			return result;
		}

		std::vector<V> zrevrange(const K &key, long long start, long long stop) override {
			auto raw_members = tpl.get_connection().zrevrange(tpl.serialize_key(key), start, stop);
			std::vector<V> result;
			result.reserve(raw_members.size());
			for (const auto &m: raw_members) {
				result.push_back(tpl.deserialize_value(m));
			}
			return result;
		}

		std::vector<std::pair<V, double>> zrange_withscores(const K &key, long long start, long long stop) override {
			auto raw_pairs = tpl.get_connection().zrange_withscores(tpl.serialize_key(key), start, stop);
			std::vector<std::pair<V, double>> result;
			result.reserve(raw_pairs.size());
			for (const auto &pair: raw_pairs) {
				result.emplace_back(tpl.deserialize_value(pair.first), pair.second);
			}
			return result;
		}

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
		redis_template<K, V> &tpl;
	};
}
