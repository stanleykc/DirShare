cmake_minimum_required(VERSION 3.3...4.0)
project(opendds_devguide_dirshare CXX)
enable_testing()

find_package(OpenDDS REQUIRED)
include(opendds_testing)

# Make sure the MPC-generated headers are gone so the CMake build will use the
# right ones. This is not needed in a real project.
file(GLOB headers "*.h")
file(GLOB listener_headers
  "SnapshotListenerImpl.h"
  "FileContentListenerImpl.h"
  "FileChunkListenerImpl.h"
  "FileEventListenerImpl.h"
  "FileMonitor.h"
  "Checksum.h"
  "FileUtils.h"
)
list(REMOVE_ITEM headers ${listener_headers})
list(LENGTH headers header_count)
if(header_count GREATER 0)
  file(REMOVE ${headers})
endif()

# IDL TypeSupport Library
add_library(dirshare_idl)
opendds_target_sources(dirshare_idl PUBLIC "DirShare.idl")
target_link_libraries(dirshare_idl PUBLIC OpenDDS::Dcps)

set(opendds_libs
  OpenDDS::Dcps # Core OpenDDS Library
  OpenDDS::InfoRepoDiscovery OpenDDS::Tcp # For run_test.pl
  OpenDDS::Rtps OpenDDS::Rtps_Udp # For run_test.pl --rtps
  dirshare_idl
)

# DirShare application (hybrid pub/sub)
add_executable(dirshare
  DirShare.cpp
  FileMonitor.cpp
  Checksum.cpp
  FileUtils.cpp
  SnapshotListenerImpl.cpp
  FileContentListenerImpl.cpp
  FileChunkListenerImpl.cpp
  FileEventListenerImpl.cpp
)
target_link_libraries(dirshare ${opendds_libs})

# Testing
configure_file(rtps.ini . COPYONLY)
opendds_add_test(NAME info_repo)
opendds_add_test(NAME rtps ARGS --rtps)
