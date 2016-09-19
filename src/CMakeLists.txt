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

option(WITH_FLAC "Enables reading of FLAC (Free Lossless Audio Codec) audio files (like WAV but with compression)" OFF)
option(WITH_LIBSNDFILE "Enables reading/writing of WAV files" ON)
option(WITH_PORTAUDIO "Enables playing audio in GUI projects" ON)
option(WITH_JULIUS "Enables decoding speech audio using Julius library" OFF)
option(WITH_SPHINX "Enables decoding speech audio using CMU PocketSphinx library" ON)

# Either LibSndFile or Flac must be enabled to read wav or flac files
if (WITH_JULIUS)
	if (NOT WITH_LIBSNDFILE)
		message("Julius requires LibSndFile library")
	endif()
	if (NOT WITH_PORTAUDIO)
		message("Julius requires PortAudio library")
	endif()
endif()

if (NOT WITH_FLAC AND NOT WITH_LIBSNDFILE)
	message("Specify at least one of Flac or Wav to read audio")
endif()

# LibSndFile library
if (WITH_LIBSNDFILE)
	set(LibSndFile_INCLUDE_DIR "" CACHE PATH "")
	set(LibSndFile_LIB_DEBUG "" CACHE PATH "")
	set(LibSndFile_LIB_RELEASE "" CACHE PATH "")

	add_definitions(-DPG_HAS_LIBSNDFILE)

	add_library(LibSndFile SHARED IMPORTED)
	set_target_properties(LibSndFile PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${LibSndFile_INCLUDE_DIR}"
		IMPORTED_IMPLIB_DEBUG ${LibSndFile_LIB_DEBUG}
		IMPORTED_IMPLIB_RELEASE ${LibSndFile_LIB_RELEASE}
	)
endif()


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

# Flac library
if (WITH_FLAC)
	set(FlacC_INCLUDE_DIRS "" CACHE PATH "")
	set(FlacC_LIB_DEBUG "" CACHE FILEPATH "")
	set(FlacC_LIB_RELEASE "" CACHE FILEPATH "")
	set(FlacCpp_LIB_DEBUG "" CACHE FILEPATH "")
	set(FlacCpp_LIB_RELEASE "" CACHE FILEPATH "")

	#include_directories(${Flac_INCLUDE_DIRS})
	add_definitions(-DPG_HAS_FLAC)

	add_library(FlacC SHARED IMPORTED)
	set_target_properties(FlacC PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${FlacC_INCLUDE_DIRS}"
		#INTERFACE_COMPILE_DEFINITIONS PG_HAS_FLAC
		IMPORTED_IMPLIB_DEBUG "${FlacC_LIB_DEBUG}"
		IMPORTED_IMPLIB_RELEASE "${FlacC_LIB_RELEASE}"
	)
	add_library(FlacCpp SHARED IMPORTED)
	set_target_properties(FlacCpp PROPERTIES
		IMPORTED_IMPLIB_DEBUG "${FlacCpp_LIB_DEBUG}"
		IMPORTED_IMPLIB_RELEASE "${FlacCpp_LIB_RELEASE}"
	)
endif()

# PortAudio library
if (WITH_PORTAUDIO)
	set(PortAudio_INCLUDE_DIRS "" CACHE PATH "")
	set(PortAudio_LIB_DEBUG "" CACHE FILEPATH "")
	set(PortAudio_LIB_RELEASE "" CACHE FILEPATH "")

	add_definitions(-DPG_HAS_PORTAUDIO)

	add_library(PortAudio SHARED IMPORTED)
	set_target_properties(PortAudio PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${PortAudio_INCLUDE_DIRS}"
		IMPORTED_IMPLIB_DEBUG ${PortAudio_LIB_DEBUG}
		IMPORTED_IMPLIB_RELEASE ${PortAudio_LIB_RELEASE}
	)
endif()

# Sphinx library
if (WITH_SPHINX)
	set(Sphinx_SphinxBase_INCLUDE_DIRS "" CACHE PATH "")
	set(Sphinx_SphinxBase_LIB_DEBUG "" CACHE PATH "")
	set(Sphinx_SphinxBase_LIB_RELEASE "" CACHE PATH "")
	set(Sphinx_PocketSphinx_INCLUDE_DIRS "" CACHE PATHS "")
	set(Sphinx_PocketSphinx_LIB_DEBUG "" CACHE PATHS  "")
	set(Sphinx_PocketSphinx_LIB_RELEASE "" CACHE PATHS  "")

	add_definitions(-DPG_HAS_SPHINX)
	
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

# Julius4 library
if (WITH_JULIUS)
	set (Julius4_LibSent_INCLUDE_DIRS "" CACHE PATH "")
	set (Julius4_LibSent_LIB_DEBUG "" CACHE PATH "")
	set (Julius4_LibSent_LIB_RELEASE "" CACHE PATH "")
	set (Julius4_LibJulius_INCLUDE_DIRS "" CACHE PATH "")
	set (Julius4_LibJulius_LIB_DEBUG "" CACHE PATH "")
	set (Julius4_LibJulius_LIB_RELEASE "" CACHE PATH "")
	set (Julius4_Julius_LIB_DEBUG "" CACHE PATH "")
	set (Julius4_Julius_LIB_RELEASE "" CACHE PATH "")

	add_definitions(-DPG_HAS_JULIUS)

	add_library(Julius4_LibSent SHARED IMPORTED)
	set_target_properties(Julius4_LibSent PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${Julius4_LibSent_INCLUDE_DIRS}"
		INTERFACE_LINK_LIBRARIES "PortAudio;ZLib;ws2_32.lib" # TODO: ws2_32 is only for windows
		#LINK_LIBRARIES "PortAudio;ZLib;ws2_32.lib" # doesn't work
		#IMPORTED_LINK_DEPENDENT_LIBRARIES "PortAudio;ZLib;ws2_32.lib" # doesn't work
		IMPORTED_IMPLIB_DEBUG ${Julius4_LibSent_LIB_DEBUG}
		IMPORTED_IMPLIB_RELEASE ${Julius4_LibSent_LIB_RELEASE}
		)

	add_library(Julius4_LibJulius SHARED IMPORTED)
	set_target_properties(Julius4_LibJulius PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${Julius4_LibJulius_INCLUDE_DIRS}"
		IMPORTED_IMPLIB_DEBUG ${Julius4_LibJulius_LIB_DEBUG}
		IMPORTED_IMPLIB_RELEASE ${Julius4_LibJulius_LIB_RELEASE}
		)

	add_library(Julius4_Julius SHARED IMPORTED)
	set_target_properties(Julius4_Julius PROPERTIES
		IMPORTED_IMPLIB_DEBUG ${Julius4_Julius_LIB_DEBUG}
		IMPORTED_IMPLIB_RELEASE ${Julius4_Julius_LIB_RELEASE}
		)
endif()


add_subdirectory(PticaGovorunBackend)
add_subdirectory(TranscriberUI)