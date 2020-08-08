cmake_minimum_required(VERSION 3.10)

project(QtSQLx)

find_package(Qt5 COMPONENTS Core Test Sql REQUIRED)
find_package(Boost 1.55.0 REQUIRED)

add_library(QtSQLx OBJECT src/TypeUtils.h src/DbUtils.h)
target_link_libraries(QtSQLx PUBLIC Qt5::Core Qt5::Sql)
target_include_directories(QtSQLx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${Boost_INCLUDE_DIR})
set_property(TARGET QtSQLx PROPERTY CXX_STANDARD 17)
set_property(TARGET QtSQLx PROPERTY CXX_STANDARD_REQUIRED ON)


set(TESTS DbUtilsTest TypeUtilsTest)

foreach (file ${TESTS})
    add_executable(${file} test/${file}.cpp)
    target_link_libraries(${file} Qt5::Test Qt5::Core QtSQLx)
    target_include_directories(${file} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    set_property(TARGET ${file} PROPERTY AUTOMOC ON)
    add_test(NAME ${file} COMMAND ${file})
    set_property(TARGET ${file} PROPERTY CXX_STANDARD 17)
    set_property(TARGET ${file} PROPERTY CXX_STANDARD_REQUIRED ON)
endforeach ()