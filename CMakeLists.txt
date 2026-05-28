cmake_minimum_required(VERSION 3.16)
project(CppQuizzer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(CppQuizzer src/main.cpp)

if(MINGW)
	get_filename_component(TOOLCHAIN_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
	foreach(RUNTIME_DLL libc++.dll libunwind.dll libwinpthread-1.dll)
		if(EXISTS "${TOOLCHAIN_BIN_DIR}/${RUNTIME_DLL}")
			add_custom_command(TARGET CppQuizzer POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
								"${TOOLCHAIN_BIN_DIR}/${RUNTIME_DLL}"
								"$<TARGET_FILE_DIR:CppQuizzer>/${RUNTIME_DLL}"
			)
		endif()
	endforeach()
endif()
