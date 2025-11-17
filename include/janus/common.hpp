#pragma once

#include <cstdint>
#include <unordered_set>

namespace janus {
	template<typename K>
	class scan_result {
	public:
		uint64_t cursor;
		std::unordered_set<K> keys;
	};
}
