cmake_minimum_required(VERSION 3.11)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -fPIC")

set(ENV{PACKAGE_VERSION} "4.0")

# set(CMAKE_BUILD_TYPE Debug)

project(ztbackup)

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET glib-2.0 smbclient libssh2)

add_executable(ztbackup main.cpp BackupFolder.cpp ExcludeList.cpp LocalBackupFolder.cpp LogFile.cpp SmbBackupFolder.cpp Ssh2BackupFolder.cpp Util.cpp)
target_link_libraries(ztbackup PkgConfig::deps)
