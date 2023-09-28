# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set (EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_BINARY_DIR}/../../../port"
                        "${CMAKE_CURRENT_SOURCE_DIR}/components"
                        "$ENV{IDF_PATH}/examples/common_components/protocol_examples_common")
project(prov_dev_client_ll_sample)
