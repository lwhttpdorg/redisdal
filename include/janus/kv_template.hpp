#pragma once

namespace janus {
	// --- Forward declarations for all operation interfaces ---
	template<typename K, typename V>
	class value_operations;

	template<typename K, typename V>
	class list_operations;

	template<typename K, typename V>
	class set_operations;

	template<typename K, typename V>
	class zset_operations;

	template<typename K, typename V>
	class hash_operations;

	/**
	 * @brief Abstract interface that strictly mimics the Spring Data RedisTemplate.
	 * * This class acts as a Facade providing access points to all Redis data structure
	 * specific operations (like String, List, Hash, etc.). All command implementation
	 * logic is delegated to the respective *Operations interfaces.
	 * * @tparam K The type of the key (Key Type).
	 * @tparam V The default type of the value (Value Type).
	 */
	template<typename K, typename V>
	class kv_template {
	public:
		/**
		 * @brief Virtual destructor to ensure proper polymorphic destruction.
		 */
		virtual ~kv_template() = default;

		// ==========================================================
		// Operation Views (Accessors for specific data structure operations)
		// ==========================================================

		/**
		 * @brief Returns the ValueOperations interface (Redis String type).
		 * @return A non-null reference to ValueOperations<K, V> interface. The object is managed by the template.
		 */
		virtual value_operations<K, V> &ops_for_value() = 0;

		/**
		 * @brief Returns the HashOperations interface (Redis Hash type).
		 * @return A non-null reference to HashOperations<K, HK, HV> interface. The object is managed by the template.
		 */
		virtual hash_operations<K, V> &ops_for_hash() = 0;

		/**
		 * @brief Returns the ListOperations interface (Redis List type).
		 * @return A non-null reference to ListOperations<K, V> interface. The object is managed by the template.
		 */
		virtual list_operations<K, V> &ops_for_list() = 0;

		/**
		 * @brief Returns the SetOperations interface (Redis Set type).
		 * @return A non-null reference to SetOperations<K, V> interface. The object is managed by the template.
		 */
		virtual set_operations<K, V> &ops_for_set() = 0;

		/**
		 * @brief Returns the ZSetOperations interface (Redis Sorted Set type).
		 * @return A non-null reference to ZSetOperations<K, V> interface. The object is managed by the template.
		 */
		virtual zset_operations<K, V> &ops_for_zset() = 0;
	};
}
