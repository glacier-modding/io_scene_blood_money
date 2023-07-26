cmake_minimum_required(VERSION 3.17)
project(BMEdit)

set(CMAKE_CXX_STANDARD 20)

# --- Global dependencies
add_subdirectory(ThirdParty/fmt)

# --- Project modules
add_subdirectory(BMEdit/GameLib)
