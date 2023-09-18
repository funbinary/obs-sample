cmake_minimum_required(VERSION 3.10...3.25)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules")
include(VersionConfig)

project(obs-sample VERSION ${OBS_VERSION_CANONICAL})
