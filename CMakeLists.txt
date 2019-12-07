
cmake_minimum_required(VERSION 3.10)

# set the project name
project(SkypeCacheViewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(chromium)
include_directories(chromium)

# add the executable
add_executable(${PROJECT_NAME}
	src/chromium_leveldb_comparator_provider.cpp
	src/skype_leveldb_scanner.cpp)

target_link_libraries(${PROJECT_NAME}
	leveldb_chromium_comparator
	leveldb
	pthread
	)

target_include_directories(${PROJECT_NAME} PUBLIC
	chromium
	)
