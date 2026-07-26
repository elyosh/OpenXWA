if(NOT DEFINED XWA_BUNDLE_CONTENT_DIR
        OR NOT DEFINED XWA_RESOURCE_SOURCE_DIR
        OR NOT DEFINED XWA_SHADER_SOURCE_DIR)
    message(FATAL_ERROR "macOS bundle staging paths are incomplete")
endif()

set(bundle_resource_dir "${XWA_BUNDLE_CONTENT_DIR}/Resources")
set(bundle_shader_dir "${bundle_resource_dir}/shaders")

if(NOT IS_DIRECTORY "${XWA_RESOURCE_SOURCE_DIR}")
    message(FATAL_ERROR "OpenXWA resource directory does not exist: ${XWA_RESOURCE_SOURCE_DIR}")
endif()

file(GLOB shader_files "${XWA_SHADER_SOURCE_DIR}/*.msl")
if(NOT shader_files)
    message(FATAL_ERROR "No compiled MSL shaders found in ${XWA_SHADER_SOURCE_DIR}")
endif()

file(REMOVE_RECURSE
    "${bundle_resource_dir}/resources"
    "${bundle_shader_dir}"
)
file(MAKE_DIRECTORY "${bundle_shader_dir}")
file(COPY "${XWA_RESOURCE_SOURCE_DIR}" DESTINATION "${bundle_resource_dir}")
file(COPY ${shader_files} DESTINATION "${bundle_shader_dir}")
