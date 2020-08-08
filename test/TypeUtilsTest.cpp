//
// Created by Fanchao Liu on 8/08/20.
//

#include <catch2/catch.hpp>

#include <QObject>

#include <optional>
#include "TypeUtils.h"

class TestClass : public QObject {
Q_OBJECT
public:
    enum TestEnum {
        E1, E2, E3
    };

    Q_ENUM(TestEnum);
};


TEST_CASE("Enum to string") {
    auto[input, output] = GENERATE(table<TestClass::TestEnum, QString>(
            {
                    {TestClass::E1, QStringLiteral("E1")},
                    {TestClass::E2, QStringLiteral("E2")},
                    {TestClass::E3, QStringLiteral("E3")},
                    {static_cast<TestClass::TestEnum>(5), ""},
            }));

    REQUIRE(sqlx::enumToString(input) == output);
}

TEST_CASE("Enum from string") {
    auto[input, expected] = GENERATE(table<const char*, std::optional<TestClass::TestEnum>>(
            {
                    { "e1", std::nullopt },
                    { "E1", TestClass::E1 },
                    { "E2", TestClass::E2 },
                    { "E3", TestClass::E3 },
                    { "E4", std::nullopt},
            }));

    CHECK(sqlx::enumFromString<TestClass::TestEnum>(input) == expected);
}

#include "TypeUtilsTest.moc"