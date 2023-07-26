cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(ZIP_STATIC True)
set(BUILD_SHARED_LIBS True)

project(bmexport VERSION 1.0.0)

add_library(bmexport 
	${PROJECT_SOURCE_DIR}/src/main.cpp
	${PROJECT_SOURCE_DIR}/src/AssetProvider.cpp
	${PROJECT_SOURCE_DIR}/src/main.cpp
	${PROJECT_SOURCE_DIR}/src/mati.cpp
	${PROJECT_SOURCE_DIR}/src/util.cpp
	${EDITOR_SOURCES})

target_include_directories(bmexport PUBLIC
	${PROJECT_SOURCE_DIR}/src
	${PROJECT_SOURCE_DIR}/vendor
	${PROJECT_SOURCE_DIR}/vendor/GameLib/Include
	${PROJECT_SOURCE_DIR}/vendor/json/include
	${PROJECT_SOURCE_DIR}/vendor/libzip
	${PROJECT_SOURCE_DIR}/vendor/libzip/lib
	${PROJECT_SOURCE_DIR}/vendor/zlib)
	
target_link_directories(bmexport PUBLIC
	${PROJECT_SOURCE_DIR}/vendor/GameLib
	${PROJECT_SOURCE_DIR}/vendor/libzip
	${PROJECT_SOURCE_DIR}/vendor/zlib)

target_link_libraries(bmexport PUBLIC GameLib)
target_link_libraries(bmexport PUBLIC libzip-static)
target_link_libraries(bmexport PUBLIC libz-static)