//
// Created by Fanchao Liu on 9/08/20.
//

#define CATCH_CONFIG_RUNNER

#include <catch2/catch.hpp>
#include <QCoreApplication>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    return Catch::Session().run( argc, argv );
}