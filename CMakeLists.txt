cmake_minimum_required(VERSION 3.24)
project(FolderJunction VERSION 1.0 LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "FolderJunction is a Windows-only application. Build with Visual Studio 2022 on Windows.")
endif()

# Main Application
add_executable(FolderJunction WIN32
  src/main.cpp
  src/app_theme.cpp
  src/link_ops.cpp
  src/scanner.cpp
  src/ui_layout.cpp
  resources/FolderJunction.rc
)
target_include_directories(FolderJunction PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(FolderJunction PRIVATE cxx_std_20)
target_compile_definitions(FolderJunction PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_link_libraries(FolderJunction PRIVATE comctl32 comdlg32 shell32 shlwapi ole32 uuid advapi32 dwmapi uxtheme)
if(MSVC)
  target_compile_options(FolderJunction PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_options(FolderJunction PRIVATE /MANIFEST:EMBED /MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/resources/app.manifest /MANIFESTUAC:NO)
  set_property(TARGET FolderJunction PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(MINGW)
  target_link_options(FolderJunction PRIVATE -municode)
endif()
set_target_properties(FolderJunction PROPERTIES OUTPUT_NAME "FolderJunction")

add_custom_command(TARGET FolderJunction POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:FolderJunction> ${CMAKE_CURRENT_SOURCE_DIR}/bin/FolderJunction.exe
)

# Release Installer Executable
add_executable(FolderJunction-Setup WIN32 src/installer.cpp resources/FolderJunctionInstaller.rc)
target_compile_features(FolderJunction-Setup PRIVATE cxx_std_20)
target_compile_definitions(FolderJunction-Setup PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_link_libraries(FolderJunction-Setup PRIVATE comctl32 shell32 shlwapi ole32 uuid advapi32)
if(MSVC)
  target_compile_options(FolderJunction-Setup PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_options(FolderJunction-Setup PRIVATE /MANIFEST:EMBED /MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/resources/app.manifest /MANIFESTUAC:NO)
  set_property(TARGET FolderJunction-Setup PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(MINGW)
  target_link_options(FolderJunction-Setup PRIVATE -municode)
endif()
set_target_properties(FolderJunction-Setup PROPERTIES OUTPUT_NAME "FolderJunction-Setup")
add_dependencies(FolderJunction-Setup FolderJunction)

add_custom_command(TARGET FolderJunction-Setup POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:FolderJunction-Setup> ${CMAKE_CURRENT_SOURCE_DIR}/bin/FolderJunction-Setup.exe
)

# Installation and Packaging (CPack)
install(TARGETS FolderJunction DESTINATION .)
install(FILES LICENSE.txt README.md DESTINATION .)

set(CPACK_PACKAGE_NAME "FolderJunction")
set(CPACK_PACKAGE_VENDOR "Abhishek Chaurasia")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lightweight native Windows link manager")
set(CPACK_PACKAGE_VERSION "1.0")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/folder-junction.ico")
set(CPACK_GENERATOR "ZIP;7Z")
include(CPack)
