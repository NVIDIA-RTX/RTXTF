
set(dlss_sdk "${CMAKE_CURRENT_SOURCE_DIR}/external/DLSS")

if (WIN32)
	add_library(DLSS SHARED IMPORTED)

	set(dlss_platform "Windows_x86_64")
	set(dlss_lib_release "nvsdk_ngx_d.lib")
	set(dlss_lib_debug "nvsdk_ngx_d_dbg_iterator1.lib")

	set_target_properties(DLSS PROPERTIES
		IMPORTED_IMPLIB "${dlss_sdk}/lib/${dlss_platform}/x64/${dlss_lib_release}"
		IMPORTED_IMPLIB_DEBUG "${dlss_sdk}/lib/${dlss_platform}/x64/${dlss_lib_debug}"
		IMPORTED_LOCATION "${dlss_sdk}/lib/${dlss_platform}/rel/nvngx_dlss.dll"
		IMPORTED_LOCATION_DEBUG "${dlss_sdk}/lib/${dlss_platform}/dev/nvngx_dlss.dll"
	)

	set(DLSS_SHARED_LIBRARY_PATH "${dlss_sdk}/lib/${dlss_platform}/$<IF:$<CONFIG:Debug>,dev,rel>/nvngx_dlss.dll")

elseif (UNIX AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x64")
	add_library(DLSS STATIC IMPORTED)

	set(dlss_platform "Linux_x86_64")
	set(dlss_lib "libnvidia-ngx-dlss.so.310.1.0")

	set_target_properties(DLSS PROPERTIES
		IMPORTED_LOCATION "${dlss_sdk}/lib/${dlss_platform}/libnvsdk_ngx.a"
	)

	set(DLSS_SHARED_LIBRARY_PATH "${dlss_sdk}/lib/${dlss_platform}/$<IF:$<CONFIG:Debug>,dev,rel>/${dlss_lib}")

else()
	message("DLSS is not supported on the target platform.")
endif()

if (TARGET DLSS)
	target_include_directories(DLSS INTERFACE "${dlss_sdk}/include")
endif()
