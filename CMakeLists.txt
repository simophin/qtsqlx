cmake_minimum_required(VERSION 3.10)

project(QtSQLx)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Core Test Sql REQUIRED)
find_package(Boost 1.55.0 REQUIRED)

add_library(QtSQLx OBJECT src/TypeUtils.h src/DbUtils.h)
target_link_libraries(QtSQLx PUBLIC Qt5::Core Qt5::Sql)
target_include_directories(QtSQLx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${Boost_INCLUDE_DIR})
set_property(TARGET QtSQLx PROPERTY LANGUAGE CXX)
set_property(TARGET QtSQLx PROPERTY LINKER_LANGUAGE CXX)


option(BUILD_TESTS "Skip buildling tests" ON)

if (BUILD_TESTS)
find_package(Catch2 REQUIRED)
add_executable(QtSQLx_test
        test/TypeUtilsTest.cpp
        test/DbUtilsTest.cpp
        test/main.cpp)
target_link_libraries(QtSQLx_test Catch2::Catch2 QtSQLx)
target_compile_definitions(QtSQLx_test PRIVATE CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS)
set_property(TARGET QtSQLx_test PROPERTY AUTOMOC ON)
endif()