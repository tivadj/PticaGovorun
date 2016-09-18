# Qt5 requires CMake>2.8.9
cmake_minimum_required(VERSION 2.8.9)
project (TranscriberUISolution)

message(STATUS "CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}")
message(STATUS "CMAKE_CURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}")

cmake_policy(SET CMP0020 NEW) # OLD=do not automatically link to 'qtmain.lib' when referencing QtCore target
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# GSL library
set(GSL_INCLUDE_DIR "" CACHE PATH "")
include_directories(${GSL_INCLUDE_DIR})

# Boost
#set(Boost_DEBUG ON) # Prints diagnostics on CMake configuration
#set(Boost_LIB_DIAGNOSTIC_DEFINITIONS ON) # Prints diagnostics when compiling (linking)
set(Boost_USE_STATIC_LIBS ON)

# LibSndFile library
set(LibSndFile_INCLUDE_DIR "" CACHE PATH "")
set(LibSndFile_LIB_DEBUG "" CACHE PATH "")
set(LibSndFile_LIB_RELEASE "" CACHE PATH "")

add_library(LibSndFile SHARED IMPORTED)
set_target_properties(LibSndFile PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${LibSndFile_INCLUDE_DIR}"
	IMPORTED_IMPLIB_DEBUG ${LibSndFile_LIB_DEBUG}
	IMPORTED_IMPLIB_RELEASE ${LibSndFile_LIB_RELEASE}
)


# LibSampleRate library
set (LibSampleRate_INCLUDE_DIR "" CACHE PATH "")
set (LibSampleRate_LIB_DEBUG "" CACHE PATH "")
set (LibSampleRate_LIB_RELEASE "" CACHE PATH "")

add_library(LibSampleRate SHARED IMPORTED)
set_target_properties(LibSampleRate PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${LibSampleRate_INCLUDE_DIR}"
	IMPORTED_IMPLIB_DEBUG ${LibSampleRate_LIB_DEBUG}
	IMPORTED_IMPLIB_RELEASE ${LibSampleRate_LIB_RELEASE}
)

# Sphinx library
option(WITH_SPHINX "Enables decoding speech audio using CMU PocketSphinx library" ON)
if (WITH_SPHINX)
	set(Sphinx_SphinxBase_INCLUDE_DIRS "" CACHE PATH "")
	set(Sphinx_SphinxBase_LIB_DEBUG "" CACHE PATH "")
	set(Sphinx_SphinxBase_LIB_RELEASE "" CACHE PATH "")
	set(Sphinx_PocketSphinx_INCLUDE_DIRS "" CACHE PATHS "")
	set(Sphinx_PocketSphinx_LIB_DEBUG "" CACHE PATHS  "")
	set(Sphinx_PocketSphinx_LIB_RELEASE "" CACHE PATHS  "")

	add_library(Sphinx_SphinxBase SHARED IMPORTED)
	set_target_properties(Sphinx_SphinxBase PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${Sphinx_SphinxBase_INCLUDE_DIRS}"
		IMPORTED_IMPLIB_DEBUG ${Sphinx_SphinxBase_LIB_DEBUG}
		IMPORTED_IMPLIB_RELEASE ${Sphinx_SphinxBase_LIB_RELEASE}
		#IMPORTED_LOCATION_DEBUG ${Sphinx_SphinxBase_LIB_DEBUG} # NOTE: DLL location!
		#IMPORTED_LOCATION_RELEASE ${Sphinx_SphinxBase_LIB_RELEASE}
		)

	add_library(Sphinx_PocketSphinx SHARED IMPORTED)
	set_target_properties(Sphinx_PocketSphinx PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${Sphinx_PocketSphinx_INCLUDE_DIRS}"
		IMPORTED_IMPLIB_DEBUG ${Sphinx_PocketSphinx_LIB_DEBUG}
		IMPORTED_IMPLIB_RELEASE ${Sphinx_PocketSphinx_LIB_RELEASE}
	)
endif()

# PortAudio library
set(PortAudio_INCLUDE_DIRS "" CACHE PATH "")
set(PortAudio_LIB_DEBUG "" CACHE FILEPATH "")
set(PortAudio_LIB_RELEASE "" CACHE FILEPATH "")

add_library(PortAudio SHARED IMPORTED)
set_target_properties(PortAudio PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${PortAudio_INCLUDE_DIRS}"
	IMPORTED_IMPLIB_DEBUG ${PortAudio_LIB_DEBUG}
	IMPORTED_IMPLIB_RELEASE ${PortAudio_LIB_RELEASE}
)

add_subdirectory(PticaGovorunBackend)
add_subdirectory(TranscriberUI)