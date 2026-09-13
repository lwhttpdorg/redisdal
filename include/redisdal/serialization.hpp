#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace redisdal {
    /**
     * @brief Strategy interface for converting typed values to and from Redis strings.
     *
     * @par Design pattern
     * Strategy: redis_template receives key and value serializers through constructor
     * injection, so conversion policies can vary independently of command execution.
     */
    template<typename T>
    class serializer {
    public:
        // Convert object of type T into a string
        virtual std::string serialize(const T &t) const = 0;

        // Convert string back into object of type T
        virtual T deserialize(const std::string &data) const = 0;

        virtual ~serializer() = default;
    };

    // Primary template declaration (specializations will follow)
    template<typename T, typename Enable = void>
    class string_serializable;

    // Specialization for arithmetic types (int, float, double, etc.)
    template<typename T>
    class string_serializable<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    public:
        // Floating point formatting preserves the precision of the actual type and is locale-independent.
        static std::string to_string(const T &t) {
            if constexpr (std::is_same_v<T, bool>) {
                return t ? "true" : "false";
            }
            else if constexpr (std::is_floating_point_v<T>) {
                std::array<char, std::numeric_limits<T>::max_digits10 + 32> buffer{};
                const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), t,
                                                  std::chars_format::general, std::numeric_limits<T>::max_digits10);
                if (result.ec != std::errc{}) {
                    throw std::runtime_error("Failed to serialize floating point value");
                }
                return std::string(buffer.data(), result.ptr);
            }
            else {
                return std::to_string(t);
            }
        }

        // Parse the entire input in the target type, without locale-dependent whitespace or narrowing.
        static T from_string(const std::string &s) {
            if constexpr (std::is_same_v<T, bool>) {
                std::string lower_s(s.size(), '\0');
                std::transform(s.begin(), s.end(), lower_s.begin(), [](unsigned char c) { return std::tolower(c); });
                if (lower_s == "true" || lower_s == "1") {
                    return true;
                }
                if (lower_s == "false" || lower_s == "0") {
                    return false;
                }
                throw std::invalid_argument(
                    "Invalid string format for boolean type. Expected 'true', 'false', '1', or '0'.");
            }
            else {
                if (s.empty()) {
                    throw std::invalid_argument("Empty numeric input");
                }
                const char *first = s.data();
                const char *last = first + s.size();
                // from_chars does not accept a leading '+', but the existing numeric serializers do.
                if (*first == '+') {
                    ++first;
                    if (first == last || *first == '-' || *first == '+') {
                        throw std::invalid_argument("Invalid numeric sign");
                    }
                }
                using parsed_type =
                    std::conditional_t<std::is_floating_point_v<T>, T,
                                       std::conditional_t<std::is_signed_v<T>, long long, unsigned long long>>;
                parsed_type value{};
                const auto result = std::from_chars(first, last, value);
                if (result.ec == std::errc::invalid_argument || result.ptr != last) {
                    throw std::invalid_argument("Invalid numeric input");
                }
                if (result.ec == std::errc::result_out_of_range) {
                    throw std::out_of_range("Numeric input is out of range for the target type");
                }
                if constexpr (std::is_integral_v<T>) {
                    if (value < std::numeric_limits<T>::lowest() || value > std::numeric_limits<T>::max()) {
                        throw std::out_of_range("Numeric input is out of range for the target type");
                    }
                }
                return static_cast<T>(value);
            }
        }
    };

    // Specialization for std::string
    template<>
    class string_serializable<std::string> {
    public:
        // For std::string, serialization is just identity
        static std::string to_string(const std::string &t) {
            return t;
        }

        // For std::string, deserialization is also identity
        static std::string from_string(const std::string &s) {
            return s;
        }
    };

    // Specialization for non-arithmetic types
    // Users must provide their own implementation of to_string and from_string
    template<typename T>
    class string_serializable<T, std::enable_if_t<!std::is_arithmetic_v<T>>> {
    public:
        static std::string to_string(const T &) {
            static_assert(
                sizeof(T) == 0,
                "Non-arithmetic types must provide their own specialization of string_serializable<T>::to_string");
            return {};
        }

        static T from_string(const std::string &) {
            static_assert(
                sizeof(T) == 0,
                "Non-arithmetic types must provide their own specialization of string_serializable<T>::from_string");
            return {};
        }
    };

    /**
     * @brief Default serialization strategy backed by string_serializable<T>.
     */
    template<typename T>
    class string_serializer: public serializer<T> {
    public:
        std::string serialize(const T &obj) const override {
            return string_serializable<T>::to_string(obj);
        }

        T deserialize(const std::string &bytes) const override {
            return string_serializable<T>::from_string(bytes);
        }
    };
} // namespace redisdal
