cmake_minimum_required(VERSION 3.15)
project(PortalGame)

add_library(glad STATIC
	"src/glad.c"
)

target_include_directories(glad PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)
