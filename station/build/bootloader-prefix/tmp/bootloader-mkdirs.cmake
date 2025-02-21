# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/esp/esp-idf-v5.3/components/bootloader/subproject"
  "D:/esp/WiFi/station/build/bootloader"
  "D:/esp/WiFi/station/build/bootloader-prefix"
  "D:/esp/WiFi/station/build/bootloader-prefix/tmp"
  "D:/esp/WiFi/station/build/bootloader-prefix/src/bootloader-stamp"
  "D:/esp/WiFi/station/build/bootloader-prefix/src"
  "D:/esp/WiFi/station/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/esp/WiFi/station/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/esp/WiFi/station/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
