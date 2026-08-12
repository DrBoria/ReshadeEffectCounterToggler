cmake_minimum_required(VERSION 3.15)
project(ReshadeEffectCounterToggler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ReshadeEffectCounterToggler SHARED
	src/addon.cpp
	src/config.cpp
	src/effects.cpp
	src/input.cpp
)

target_include_directories(ReshadeEffectCounterToggler PRIVATE include)

target_compile_definitions(ReshadeEffectCounterToggler PRIVATE
	UNICODE
	_UNICODE
	NOMINMAX
	WIN32_LEAN_AND_MEAN
)

target_compile_options(ReshadeEffectCounterToggler PRIVATE /W3 /EHsc)

set_target_properties(ReshadeEffectCounterToggler PROPERTIES
	PREFIX ""
	SUFFIX ""
	OUTPUT_NAME "ReshadeEffectCounterToggler.addon64"
)
