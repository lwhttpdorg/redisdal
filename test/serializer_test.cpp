#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <locale>
#include <redisdal/serialization.hpp>
#include <string>

// Test fixture for serializer tests.
class SerializerTest: public ::testing::Test {
protected:
    redisdal::string_serializer<std::string> string_serializer;
    redisdal::string_serializer<int> int_serializer;
    redisdal::string_serializer<long long> long_long_serializer;
    redisdal::string_serializer<double> double_serializer;
    redisdal::string_serializer<bool> bool_serializer;
};

// Test case for std::string serialization.
TEST_F(SerializerTest, StringSerialization) {
    std::string original = "hello world";
    std::string serialized = string_serializer.serialize(original);
    EXPECT_EQ(original, serialized);
    std::string deserialized = string_serializer.deserialize(serialized);
    EXPECT_EQ(original, deserialized);
}

// Test case for integer serialization.
TEST_F(SerializerTest, IntSerialization) {
    int original = 42;
    std::string serialized = int_serializer.serialize(original);
    EXPECT_EQ("42", serialized);
    int deserialized = int_serializer.deserialize(serialized);
    EXPECT_EQ(original, deserialized);

    int negative_original = -123;
    serialized = int_serializer.serialize(negative_original);
    EXPECT_EQ("-123", serialized);
    deserialized = int_serializer.deserialize(serialized);
    EXPECT_EQ(negative_original, deserialized);
}

// Test case for long long serialization.
TEST_F(SerializerTest, LongLongSerialization) {
    long long original = 9876543210LL;
    std::string serialized = long_long_serializer.serialize(original);
    EXPECT_EQ("9876543210", serialized);
    long long deserialized = long_long_serializer.deserialize(serialized);
    EXPECT_EQ(original, deserialized);
}

// Test case for unsigned long long serialization.
TEST_F(SerializerTest, UnsignedLongLongSerialization) {
    redisdal::string_serializer<unsigned long long> ull_serializer;
    unsigned long long original = 18446744073709551615ULL; // ULLONG_MAX
    std::string serialized = ull_serializer.serialize(original);
    EXPECT_EQ("18446744073709551615", serialized);
    unsigned long long deserialized = ull_serializer.deserialize(serialized);
    EXPECT_EQ(original, deserialized);
}

// Test case for double serialization.
TEST_F(SerializerTest, DoubleSerialization) {
    double original = 3.14159;
    std::string serialized = double_serializer.serialize(original);
    double deserialized = double_serializer.deserialize(serialized);
    EXPECT_DOUBLE_EQ(original, deserialized);
}

// Test case for boolean serialization.
TEST_F(SerializerTest, BoolSerialization) {
    // Preserve the existing boolean encoding.
    EXPECT_EQ(bool_serializer.serialize(true), "true");
    EXPECT_EQ(bool_serializer.serialize(false), "false");

    // Deserialization should handle numeric and string representations.
    // Test true values.
    EXPECT_TRUE(bool_serializer.deserialize("1"));
    EXPECT_TRUE(bool_serializer.deserialize("true"));
    EXPECT_TRUE(bool_serializer.deserialize("TRUE"));

    // Test false values.
    EXPECT_FALSE(bool_serializer.deserialize("0"));
    EXPECT_FALSE(bool_serializer.deserialize("false"));
    EXPECT_FALSE(bool_serializer.deserialize("FALSE"));

    // Test invalid values.
    EXPECT_THROW(bool_serializer.deserialize("yes"), std::invalid_argument);
    EXPECT_THROW(bool_serializer.deserialize("no"), std::invalid_argument);
    EXPECT_THROW(bool_serializer.deserialize("2"), std::invalid_argument);
}

template<typename T>
class FloatingSerializerTest: public ::testing::Test {};

using FloatingTypes = ::testing::Types<float, double, long double>;
TYPED_TEST_SUITE(FloatingSerializerTest, FloatingTypes);

TYPED_TEST(FloatingSerializerTest, RoundTripsWithoutLosingPrecision) {
    redisdal::string_serializer<TypeParam> serializer;
    for (const TypeParam value:
         {TypeParam(0), TypeParam(-0.0), TypeParam(1e-8L), std::nextafter(TypeParam(1), TypeParam(2)),
          std::numeric_limits<TypeParam>::lowest(), std::numeric_limits<TypeParam>::max(),
          std::numeric_limits<TypeParam>::min(), std::numeric_limits<TypeParam>::denorm_min()}) {
        const auto text = serializer.serialize(value);
        const auto actual = serializer.deserialize(text);
        EXPECT_EQ(actual, value) << text;
        EXPECT_EQ(std::signbit(actual), std::signbit(value)) << text;
    }
}

TYPED_TEST(FloatingSerializerTest, ValidatesCompleteInputAndRange) {
    redisdal::string_serializer<TypeParam> serializer;
    for (const char *text: {"", "1.25junk", "1.2.3", "1e", " 1", "1 ", "1,5", "+-1"}) {
        EXPECT_THROW(serializer.deserialize(text), std::invalid_argument) << text;
    }
    EXPECT_THROW(serializer.deserialize(std::string("1.5\0tail", 8)), std::invalid_argument);
    EXPECT_THROW(serializer.deserialize("1e99999"), std::out_of_range);
    EXPECT_THROW(serializer.deserialize("1e-99999"), std::out_of_range);
    EXPECT_EQ(serializer.deserialize("+1.5"), TypeParam(1.5));
    EXPECT_TRUE(std::isnan(serializer.deserialize(serializer.serialize(std::numeric_limits<TypeParam>::quiet_NaN()))));
    EXPECT_EQ(serializer.deserialize("inf"), std::numeric_limits<TypeParam>::infinity());
    EXPECT_EQ(serializer.deserialize("-inf"), -std::numeric_limits<TypeParam>::infinity());
}

template<typename T>
class IntegerSerializerTest: public ::testing::Test {};

using IntegerTypes = ::testing::Types<signed char, unsigned char, char, short, unsigned short, int, unsigned int, long,
                                      unsigned long, long long, unsigned long long, wchar_t, char16_t, char32_t>;
TYPED_TEST_SUITE(IntegerSerializerTest, IntegerTypes);

TYPED_TEST(IntegerSerializerTest, RoundTripsBoundsAndRejectsPartialInput) {
    redisdal::string_serializer<TypeParam> serializer;
    for (const TypeParam value: {std::numeric_limits<TypeParam>::lowest(), std::numeric_limits<TypeParam>::max()}) {
        EXPECT_EQ(serializer.deserialize(serializer.serialize(value)), value);
    }
    for (const char *text: {"", "123junk", "1.0", " 1", "1 ", "+", "+-1", "++1"}) {
        EXPECT_THROW(serializer.deserialize(text), std::invalid_argument) << text;
    }
    EXPECT_THROW(serializer.deserialize(std::string("1\0tail", 6)), std::invalid_argument);
    EXPECT_THROW(serializer.deserialize("18446744073709551616"), std::out_of_range);
    EXPECT_EQ(serializer.deserialize("+42"), TypeParam(42));
    if constexpr (std::is_unsigned_v<TypeParam>) {
        EXPECT_THROW(serializer.deserialize("-1"), std::invalid_argument);
    }
}

TEST(SerializerBoundsTest, NarrowIntegerOverflowIsRejected) {
    redisdal::string_serializer<uint8_t> byte;
    redisdal::string_serializer<int8_t> signed_byte;
    EXPECT_THROW(byte.deserialize("256"), std::out_of_range);
    EXPECT_THROW(signed_byte.deserialize("128"), std::out_of_range);
    EXPECT_THROW(signed_byte.deserialize("-129"), std::out_of_range);
    EXPECT_EQ(redisdal::string_serializable<char>::to_string('A'), "65");
}

TEST(SerializerLocaleTest, DecimalPointDoesNotDependOnGlobalLocale) {
    class comma_punctuation: public std::numpunct<char> {
        char do_decimal_point() const override {
            return ',';
        }
    };
    struct locale_guard {
        std::locale previous;
        ~locale_guard() {
            std::locale::global(previous);
        }
    } guard;
    std::locale::global(std::locale(std::locale::classic(), new comma_punctuation));
    redisdal::string_serializer<double> serializer;
    EXPECT_EQ(serializer.serialize(1.5), "1.5");
    EXPECT_EQ(serializer.deserialize("1.5"), 1.5);
}
