//
// Created by Fanchao Liu on 8/08/20.
//

#include <QtTest/QtTest>

#include "TypeUtils.h"

class TypeUtilsTest : public QObject {
Q_OBJECT
public:
    enum TestEnum {
        E1, E2, E3
    };

    Q_ENUM(TestEnum);

private slots:

    void testEnumToString_data() {
        QTest::addColumn<TestEnum>("input");
        QTest::addColumn<QString>("output");

        QTest::newRow("E1") << E1 << "E1";
        QTest::newRow("E2") << E2 << "E2";
        QTest::newRow("E3") << E3 << "E3";
        QTest::newRow("No such element") << static_cast<TestEnum>(E3 + 1) << "";
    };

    void testEnumToString() {
        QFETCH(TestEnum, input);
        QFETCH(QString, output);
        QCOMPARE(sqlx::enumToString<TestEnum>(input), output);
    };

    void testEnumFromString_data() {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QVariant>("output");

        QTest::newRow("E1") << QStringLiteral("E1") << QVariant::fromValue(E1);
        QTest::newRow("E2") << QStringLiteral("E2") << QVariant::fromValue(E2);
        QTest::newRow("E3") << QStringLiteral("E3") << QVariant::fromValue(E3);
        QTest::newRow("No such element") << "nothing" << QVariant();
    }

    void testEnumFromString() {
        QFETCH(QString, input);
        QFETCH(QVariant, output);

        auto actual = sqlx::enumFromString<TestEnum>(input.toUtf8().data());
        QCOMPARE(actual ? QVariant::fromValue(*actual) : QVariant(), output);
    }
};

QTEST_MAIN(TypeUtilsTest)

#include "TypeUtilsTest.moc"