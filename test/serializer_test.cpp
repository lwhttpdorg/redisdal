#include <gtest/gtest.h>
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
    // Note: std::to_string has a fixed precision for double.
    double deserialized = double_serializer.deserialize(serialized);
    EXPECT_DOUBLE_EQ(original, deserialized);
}

// Test case for boolean serialization.
TEST_F(SerializerTest, BoolSerialization) {
    // Serialization should consistently produce "1" or "0".
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
