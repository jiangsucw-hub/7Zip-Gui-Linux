cmake_minimum_required(VERSION 3.16)
project(7zip-gui-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/SevenZipBackend.cpp
    src/ArchiveModel.cpp
    src/Worker.cpp
    src/utils/ArchiveUtils.cpp
    src/utils/AppLocale.cpp
    src/utils/DialogUtils.cpp
    src/utils/FallbackZhTranslator.cpp
    src/dialogs/PasswordDialog.cpp
    src/dialogs/ProgressDialog.cpp
    src/dialogs/ExtractDialog.cpp
    src/dialogs/AddDialog.cpp
    src/dialogs/OperationResultDialog.cpp
)

set(HEADERS
    src/MainWindow.h
    src/SevenZipBackend.h
    src/ArchiveModel.h
    src/Worker.h
    src/utils/ArchiveUtils.h
    src/utils/AppLocale.h
    src/utils/DialogUtils.h
    src/utils/FallbackZhTranslator.h
    src/dialogs/PasswordDialog.h
    src/dialogs/ProgressDialog.h
    src/dialogs/ExtractDialog.h
    src/dialogs/AddDialog.h
    src/dialogs/OperationResultDialog.h
)

set(TS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/translations/7zip-gui-cpp_zh_CN.ts")
set(QM_FILE "${CMAKE_CURRENT_BINARY_DIR}/7zip-gui-cpp_zh_CN.qm")

find_program(LRELEASE NAMES lrelease lrelease-qt6)
if(LRELEASE AND EXISTS "${TS_FILE}")
    add_custom_command(
        OUTPUT "${QM_FILE}"
        COMMAND "${LRELEASE}" "${TS_FILE}" -qm "${QM_FILE}"
        DEPENDS "${TS_FILE}"
        COMMENT "Generating Chinese translation"
    )
    set(I18N_QM "${QM_FILE}")
else()
    message(STATUS "lrelease not found — build without embedded zh_CN.qm (install qt6-tools)")
    set(I18N_QM "")
endif()

qt_add_executable(7zip-gui-cpp ${SOURCES} ${HEADERS})

if(I18N_QM)
    qt_add_resources(7zip-gui-cpp i18n_resources
        PREFIX "/i18n"
        BASE "${CMAKE_CURRENT_BINARY_DIR}"
        FILES "${QM_FILE}"
    )
endif()

target_link_libraries(7zip-gui-cpp PRIVATE Qt6::Widgets Qt6::Concurrent)

target_include_directories(7zip-gui-cpp PRIVATE src)

install(TARGETS 7zip-gui-cpp RUNTIME DESTINATION lib/7zip-gui-cpp)
install(PROGRAMS packaging/7zip-gui-cpp.wrapper.sh DESTINATION lib/7zip-gui-cpp)
install(PROGRAMS packaging/7zip-gui-cpp.wrapper.sh DESTINATION bin RENAME 7zip-gui-cpp)
install(PROGRAMS packaging/7zip-gui-cpp-launch DESTINATION bin)
install(PROGRAMS
    packaging/thunar-extract-here.sh
    packaging/thunar-test.sh
    packaging/thunar-add.sh
    DESTINATION lib/7zip-gui-cpp
)
install(FILES packaging/7zip-gui-cpp.desktop packaging/7zip-gui-cpp-open.desktop
        DESTINATION share/applications)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/7zip-gui-cpp.svg")
    install(FILES packaging/icons/7zip-gui-cpp.svg DESTINATION share/icons/hicolor/scalable/apps)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/icons-480.png")
    install(FILES packaging/icons/icons-480.png
            DESTINATION share/icons/hicolor/256x256/apps RENAME 7zip-gui-cpp.png)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/icons8-7-zip-480-whitebg-preview.png")
    install(FILES packaging/icons/icons8-7-zip-480-whitebg-preview.png
            DESTINATION share/icons/hicolor/256x256/apps RENAME 7zip-gui-cpp.png)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/icons8-7-zip-480.png")
    install(FILES packaging/icons/icons8-7-zip-480.png DESTINATION share/icons/hicolor/256x256/apps
            RENAME 7zip-gui-cpp.png)
endif()
install(FILES packaging/icons/NOTICE DESTINATION share/doc/7zip-gui-cpp)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/icons8-test-50.png")
    install(FILES packaging/icons/icons8-test-50.png DESTINATION share/7zip-gui-cpp/icons)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/icons8-extract-50.png")
    install(FILES packaging/icons/icons8-extract-50.png DESTINATION share/7zip-gui-cpp/icons)
endif()
