# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/bsmeh/esp/esp-idf/components/bootloader/subproject"
  "C:/Users/bsmeh/OneDrive/Dokumenti/Ljubljana/Senlab/azure/sampleMqttEsp/esp-azure/examples/prov_dev_client_ll_sample/build/bootloader"
  "C:/Users/bsmeh/OneDrive/Dokumenti/Ljubljana/Senlab/azure/sampleMqttEsp/esp-azure/examples/prov_dev_client_ll_sample/build/bootloader-prefix"
  "C:/Users/bsmeh/OneDrive/Dokumenti/Ljubljana/Senlab/azure/sampleMqttEsp/esp-azure/examples/prov_dev_client_ll_sample/build/bootloader-prefix/tmp"
  "C:/Users/bsmeh/OneDrive/Dokumenti/Ljubljana/Senlab/azure/sampleMqttEsp/esp-azure/examples/prov_dev_client_ll_sample/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/bsmeh/OneDrive/Dokumenti/Ljubljana/Senlab/azure/sampleMqttEsp/esp-azure/examples/prov_dev_client_ll_sample/build/bootloader-prefix/src"
  "C:/Users/bsmeh/OneDrive/Dokumenti/Ljubljana/Senlab/azure/sampleMqttEsp/esp-azure/examples/prov_dev_client_ll_sample/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/bsmeh/OneDrive/Dokumenti/Ljubljana/Senlab/azure/sampleMqttEsp/esp-azure/examples/prov_dev_client_ll_sample/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/bsmeh/OneDrive/Dokumenti/Ljubljana/Senlab/azure/sampleMqttEsp/esp-azure/examples/prov_dev_client_ll_sample/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
