cmake_minimum_required(VERSION 3.16)

project(INGCsrv VERSION 1.2 LANGUAGES CXX)

#---------------Добавляем версию проекта и номер сборки#---------------
set(SOURCE_NUMBER_FILE "${CMAKE_SOURCE_DIR}/version/build_number.txt")
if(EXISTS ${SOURCE_NUMBER_FILE})
    file(READ ${SOURCE_NUMBER_FILE} BUILD_NUMBER)
    math(EXPR BUILD_NUMBER "${BUILD_NUMBER} + 1")
else()
    set(BUILD_NUMBER 1)
endif()
file(WRITE ${SOURCE_NUMBER_FILE} "${BUILD_NUMBER}")

# Установка версии проекта
set(PROJECT_VERSION "${PROJECT_VERSION}")
set(PROJECT_VERSION_BUILD "${PROJECT_VERSION}.${BUILD_NUMBER}")
set(PROJECT_VERSION_FULL "${PROJECT_NAME}.${PROJECT_VERSION}.${BUILD_NUMBER}")
# Создаем конфигурационный файл версии
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version/version.in.h
    ${CMAKE_CURRENT_SOURCE_DIR}/version/version.h
)
#----------------------------------------------------------------------

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.h
    mainwindow.cpp
    mainwindow.ui
)

# Просто создаем исполняемый файл
add_executable(INGCsrv ${PROJECT_SOURCES}
    systemlog/uisystemlog.h systemlog/uisystemlog.cpp systemlog/uisystemlog.ui
    systemlog/modelsystemlog.h systemlog/modelsystemlog.cpp
    opcua/uiopcua.h opcua/uiopcua.cpp opcua/uiopcua.ui
    opcua/uigencertificate.h opcua/uigencertificate.cpp opcua/uigencertificate.ui
    opcua/uibrowsenode.h opcua/uibrowsenode.cpp opcua/uibrowsenode.ui
    opcua/opcuaclient.h opcua/opcuaclient.cpp
    opcua/modelnodes.h opcua/modelnodes.cpp
    opcua/modelattributes.h opcua/modelattributes.cpp
    tcpserver/tcpserver.h tcpserver/tcpserver.cpp
    tcpserver/processdata.h tcpserver/processdata.cpp
    tcpserver/parsingdata.h tcpserver/parsingdata.cpp
    tcpserver/uitcp.h tcpserver/uitcp.cpp tcpserver/uitcp.ui
    tcpserver/tcpthread.h tcpserver/tcpthread.cpp
    tcpserver/modelclients.h tcpserver/modelclients.cpp
    tcpserver/buttondelagate.h tcpserver/buttondelagate.cpp
    viewmapping/uiviewmapping.h viewmapping/uiviewmapping.cpp viewmapping/uiviewmapping.ui
    viewmapping/modelmapping.h viewmapping/modelmapping.cpp
    viewmapping/modelevent.h viewmapping/modelevent.cpp
    viewmapping/jsonmodelio.h viewmapping/jsonmodelio.cpp
    engine.h engine.cpp
    dialog.h dialog.cpp
    applib.h applib.cpp
    fileio.h fileio.cpp
)
# Иконка приложения
if(WIN32)
    # Создаем файл ресурсов с иконкой
    set(RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
    file(WRITE ${RC_FILE} "IDI_ICON1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/img/logo.ico\"\n")
    # Добавляем RC файл к исполняемому файлу
    target_sources(INGCsrv PRIVATE ${RC_FILE})
    # Указываем CMake, что это файл ресурсов
    set_source_files_properties(${RC_FILE} PROPERTIES LANGUAGE RC)
endif()

# Добавляем ресурсы (если используете .qrc)
qt_add_resources(INGCsrv Resources
    PREFIX "/"
    FILES
    "img/logo.png"
    "img/open62541.png"
    "img/icon/update.svg"
    "img/icon/dragclick.svg"
    "img/icon/mainmenu/close.svg"
    "img/icon/mainmenu/openfolder.svg"
    "img/icon/mainmenu/systemlog.svg"
    "img/icon/mainmenu/generate.svg"
    "img/icon/mainmenu/browsenodes.svg"
    "img/icon/mainmenu/connection.svg"
    "img/icon/mainmenu/server.svg"
    "img/icon/mainmenu/map.svg"
    "img/icon/mainmenu/about.svg"
    "img/icon/mainmenu/aboutqt.svg"
    "img/icon/tray/close.svg"
    "img/icon/tray/hide.svg"
    "img/icon/tray/restore.svg"
    "img/icon/tray/status.svg"
    "img/icon/tray/stplay.svg"
    "img/icon/tray/ststop.svg"
    "img/icon/clean.svg"
    "img/icon/plugconnected.svg"
    "img/icon/plugdisconnected.svg"
    "img/icon/stop.svg"
    "img/icon/play.svg"
    "img/icon/load.svg"
    "img/icon/expandAll.svg"
    "img/icon/collapseAll.svg"
    # другие файлы, если нужны
)

target_link_libraries(INGCsrv PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(INGCsrv PRIVATE Qt6::Core)

# for QTcpSocket
find_package(Qt6 REQUIRED COMPONENTS Network)
find_package(Qt6 REQUIRED COMPONENTS Core)
target_link_libraries(INGCsrv PRIVATE Qt6::Network)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.INGCsrv)
endif()
set_target_properties(INGCsrv PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS INGCsrv
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(INGCsrv)
endif()



#------------------Импортируем статическую библиотеку open62541------------------

# # Создание импортируемой цели
# add_library(open62541 STATIC IMPORTED)
# # Указание пути к библиотеке
# set_target_properties(open62541 PROPERTIES
#     IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/opcua/open62541/libopen62541.a
#     INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/opcua/open62541/
# )
# # Для Windows добавляем все необходимые системные библиотеки
# if(WIN32)
#     set(EXTRA_LIBS ws2_32 iphlpapi)
#     if(MINGW)
#         # Для MinGW могут потребоваться дополнительные библиотеки
#         list(APPEND EXTRA_LIBS advapi32 user32)
#         # Подавить предупреждения при линковке (open62541 выдает предупрждения)
#         set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wno-attributes")
#     endif()
#     target_link_libraries(open62541 INTERFACE ${EXTRA_LIBS})
# endif()

# # включаем шифрование для библиотеки open62541
# target_compile_definitions(INGCsrv PRIVATE UA_ENABLE_ENCRYPTION=1)

# # Найти OpenSSL
# set(OPENSSL_ROOT_DIR "C:/Qt/Tools/OpenSSLv3/Win_x64")
# find_package(OpenSSL REQUIRED)
# set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
# set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libcrypto.a")
# set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libssl.a")
# set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})

# # Связываем OpenSSL с open62541
# target_link_libraries(open62541 INTERFACE OpenSSL::SSL OpenSSL::Crypto)
# # Связываем с основной целью
# target_link_libraries(INGCsrv PRIVATE open62541)


#------------------Импортируем динамическую библиотеку open62541-----------------

# 1. Включаем директорию с заголовками
target_include_directories(INGCsrv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/opcua/open62541
)
# 2. Линкуем с библиотекой и всеми зависимостями
target_link_libraries(INGCsrv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/opcua/open62541/open62541.lib
    ws2_32
    crypt32
    iphlpapi
    dbghelp
    advapi32
    user32
)
# 3. Найти OpenSSL
set(OPENSSL_ROOT_DIR "C:/Qt/Tools/OpenSSLv3/Win_x64")
find_package(OpenSSL REQUIRED)
# 4. Добавляем OpenSSL
target_link_libraries(INGCsrv PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
)
target_link_libraries(INGCsrv PRIVATE Qt6::Core)
# 5. Определения компилятора
target_compile_definitions(INGCsrv PRIVATE
    UA_ENABLE_ENCRYPTION=1
    UA_DYNAMIC_LINKING=1
)
# 6. Копируем DLL в выходную директорию
add_custom_command(TARGET INGCsrv POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/opcua/open62541/open62541.dll"
    "$<TARGET_FILE_DIR:INGCsrv>"
    COMMENT "Copying open62541 DLL to output directory"
)
#--------------------------------------------------------------------------------

