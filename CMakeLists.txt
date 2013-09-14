cmake_minimum_required(VERSION 2.8.11)
project(korelate)

set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "-std=c++11 -stdlib=libc++")

#libkorelate
include_directories(cxx/include)
set(korelate_srcs ${korelate_srcs} cxx/include/korelate/korelate.hpp)
set(korelate_srcs ${korelate_srcs} cxx/source/libkorelate.cpp)
set(korelate_libs ${korelate_libs} korelate)

#boost
find_package(Boost 1.54 COMPONENTS date_time REQUIRED )
message("Boost includes at ${Boost_INCLUDE_DIRS}")
message("Boost libs at ${Boost_LIBRARIES}")
include_directories(${Boost_INCLUDE_DIRS})
set(korelate_libs ${korelate_libs} ${Boost_LIBRARIES})

#sqllite3
find_path(SQLLite_INCLUDE_DIR sqlite3.h)
find_library(SQLLite_LIBRARY sqlite3)
message("SQLLite includes at ${SQLLite_INCLUDE_DIR}")
message("SQLLite lib at ${SQLLite_LIBRARY}")
include_directories(${SQLLite_INCLUDE_DIR})
set(korelate_libs ${korelate_libs} ${SQLLite_LIBRARY})

#Poco
find_path(Poco_INCLUDE_DIRS  Poco/Net/HTTPClientSession.h)
#find_library(PocoFoundation_LIBRARY PocoFoundation)
find_library(PocoNet_LIBRARY PocoNet)
message("Poco includes at ${Poco_INCLUDE_DIRS}")
#message("PocoFoundation lib at ${PocoFoundation_LIBRARY}")
message("PocoNet lib at ${PocoNet_LIBRARY}")

include_directories(${Poco_INCLUDE_DIRS})
set(korelate_libs ${korelate_libs} ${PocoFoundation_LIBRARY})
set(korelate_libs ${korelate_libs} ${PocoNet_LIBRARY})


add_library(korelate ${korelate_srcs})
add_executable(korelate_test cxx/source/korelate_test.cpp)
target_link_libraries(korelate_test ${korelate_libs})


