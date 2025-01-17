# Copyright (C) 1995-2021, Rene Brun AND Fons Rademakers.
# All rights reserved.
#
# For the licensing terms see $ROOTSYS/LICENSE.
# For the list of contributors see $ROOTSYS/README/CREDITS.

############################################################################
# CMakeLists.txt file for building TMVA SOFIE tests.
# @author Federico Sossai, Sanjiban Sengupta
############################################################################


set(SOFIE_PARSERS_DIR ${CMAKE_SOURCE_DIR}/tmva/sofie_parsers)

if (NOT ONNX_MODELS_DIR)
  set(ONNX_MODELS_DIR input_models)
endif()

#Finding .onnx files to be parsed AND creating the appropriate code to
# parse all file. It is much faster to combine all parsing in a single executable
# which will avoid initialization time (especially when using ROOT)
set(CAPTURE_STR "EmitModel( \"@1\", \"@2\");")
set(ALL_CAPTURES "")
# Finding .onnx files to be parsed AND creating the appropriate command
file(GLOB ONNX_FILES "${ONNX_MODELS_DIR}/*.onnx")
foreach(onnx_file ${ONNX_FILES})
	get_filename_component(fname ${onnx_file} NAME_WE)
	get_filename_component(fdir ${onnx_file} DIRECTORY)
  string(REPLACE "@1" ${onnx_file} cap ${CAPTURE_STR})
  string(REPLACE "@2" ${fname} cap ${cap})
  list(APPEND ALL_CAPTURES ${cap})
endforeach()
string(REPLACE ";" ";\n" EMIT_CAPTURES "${ALL_CAPTURES}")
configure_file(EmitFromONNX.cxx.in EmitFromONNX_all.cxx @ONLY)
configure_file(EmitFromRoot.cxx.in EmitFromRoot_all.cxx @ONLY)
configure_file(EmitFromONNX_GPU.cxx.in EmitFromONNX_GPU_all.cxx @ONLY)

#Finding .onnx files to be parsed and creating the appropriate code to
# parse all file. It is much faster to combine all parsing in a single executable
# which will avoid initialization time (especially when using ROOT)
set(CAPTURE_STR "EmitModel( \"@1\", \"@2\");")
set(ALL_CAPTURES "")
# Finding .onnx files to be parsed and creating the appropriate command
file(GLOB ONNX_FILES "${ONNX_MODELS_DIR}/*.onnx")
foreach(onnx_file ${ONNX_FILES})
	get_filename_component(fname ${onnx_file} NAME_WE)
	get_filename_component(fdir ${onnx_file} DIRECTORY)
  string(REPLACE "@1" ${onnx_file} cap ${CAPTURE_STR})
  string(REPLACE "@2" ${fname} cap ${cap})
  list(APPEND ALL_CAPTURES ${cap})
endforeach()
string(REPLACE ";" ";\n" EMIT_CAPTURES "${ALL_CAPTURES}")
configure_file(EmitFromONNX.cxx.in EmitFromONNX_all.cxx @ONLY)
configure_file(EmitFromRoot.cxx.in EmitFromRoot_all.cxx @ONLY)

add_executable(emitFromONNX
   EmitFromONNX_all.cxx
)

if (sofie-sycl AND testing)
  add_executable(emitFromONNX_GPU
    EmitFromONNX_GPU_all.cxx  
  )
endif()

target_include_directories(emitFromONNX PRIVATE
   ${CMAKE_SOURCE_DIR}/tmva/sofie/inc
   ${SOFIE_PARSERS_DIR}/inc
   ${CMAKE_SOURCE_DIR}/tmva/inc
   ${CMAKE_CURRENT_BINARY_DIR}   # this is for the protobuf headerfile
)

if (sofie-sycl AND testing)
  target_include_directories(emitFromONNX_GPU PRIVATE
    ${CMAKE_SOURCE_DIR}/tmva/sofie/inc
    ${SOFIE_PARSERS_DIR}/inc
    ${CMAKE_SOURCE_DIR}/tmva/inc
    ${CMAKE_CURRENT_BINARY_DIR}   # this is for the protobuf headerfile
    ${CMAKE_SOURCE_DIR}/core/foundation/inc
    ${CMAKE_BINARY_DIR}/ginclude   # this is for RConfigure.h
  )
endif()

target_link_libraries(emitFromONNX ${Protobuf_LIBRARIES} ROOTTMVASofie ROOTTMVASofieParser)
set_target_properties(emitFromONNX PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
## silence protobuf warnings seen in version 3.0 AND 3.6. Not needed from protobuf version 3.17
target_compile_options(emitFromONNX PRIVATE -Wno-unused-parameter -Wno-array-bounds)

if (sofie-sycl AND testing)
target_link_libraries(emitFromONNX_GPU ${Protobuf_LIBRARIES} ROOTTMVASofie ROOTTMVASofieParser)
set_target_properties(emitFromONNX_GPU PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
## silence protobuf warnings seen in version 3.0 AND 3.6. Not needed from protobuf version 3.17
target_compile_options(emitFromONNX_GPU PRIVATE -Wno-unused-parameter -Wno-array-bounds)
endif()

# Automatic compilation of headers from onnx files
add_custom_target(SofieCompileModels_ONNX)
add_dependencies(SofieCompileModels_ONNX emitFromONNX)

if (sofie-sycl AND testing)
  add_custom_target(SofieCompileGPUModels_ONNX)
  add_dependencies(SofieCompileGPUModels_ONNX emitFromONNX_GPU)
endif()

add_custom_command(TARGET SofieCompileModels_ONNX POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E env ROOTIGNOREPREFIX=1 ./emitFromONNX ${onnx_file} ${CMAKE_CURRENT_BINARY_DIR}/${fname}
		USES_TERMINAL )

if (sofie-sycl AND testing)
  add_custom_command(TARGET SofieCompileGPUModels_ONNX POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E env ROOTIGNOREPREFIX=1 ./emitFromONNX_GPU ${onnx_file} ${CMAKE_CURRENT_BINARY_DIR}/${fname}
  USES_TERMINAL )
endif()

# Creating a Google Test
if (tmva-cpu)  # we need BLAS for compiling the models
ROOT_ADD_GTEST(TestCustomModelsFromONNX TestCustomModelsFromONNX.cxx
  LIBRARIES
    ROOTTMVASofie
    ${BLAS_LINKER_FLAGS}
    ${BLAS_LIBRARIES}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_dependencies(TestCustomModelsFromONNX SofieCompileModels_ONNX)
endif()

# Creating a Google Test
if (sofie-sycl AND testing)  # we need oneapi math kernel library to compile the models

if (${GPU_BLAS} STREQUAL "MKLBLAS")
  find_package(MKL REQUIRED)
  ROOT_ADD_GTEST(TestCustomGPUModelsFromONNX TestCustomGPUModelsFromONNX.cxx
    LIBRARIES
      ROOTTMVASofie
    INCLUDE_DIRS
      ${CMAKE_CURRENT_BINARY_DIR}
    COMPILE_OPTIONS
      -fsycl -DMKL_ILP64  -I${MKLROOT}/include
    LINK_OPTIONS
      -fsycl  -L${MKLROOT}/lib/intel64 -lmkl_sycl -lmkl_intel_ilp64 -lmkl_sequential -lmkl_core -lsycl -lpthread -lm -ldl
  )
else()
  find_package(PORTBLAS REQUIRED)
  if (${TARGET_GPU} STREQUAL "NVIDIA")
    ROOT_ADD_GTEST(TestCustomGPUModelsFromONNX TestCustomGPUModelsFromONNX.cxx
      LIBRARIES
        ROOTTMVASofie
      INCLUDE_DIRS
        ${CMAKE_CURRENT_BINARY_DIR}
      COMPILE_OPTIONS
        -fsycl -I${PORTBLAS_INCLUDE_DIR} -I${PORTBLAS_SRC_DIR} -fsycl-targets=nvptx64-nvidia-cuda
      LINK_OPTIONS
        # VERY IMPORTANT TO ADD THE TARGET AND IN THE LINKING STAGE
        # SEE https://intel.github.io/llvm-docs/GetStartedGuide.html#build-dpc-application-with-cmake
        -fsycl -fsycl-targets=nvptx64-nvidia-cuda 
      )
    elseif (${TARGET_GPU} STREQUAL "AMD")
    ROOT_ADD_GTEST(TestCustomGPUModelsFromONNX TestCustomGPUModelsFromONNX.cxx
      LIBRARIES
        ROOTTMVASofie
      INCLUDE_DIRS
        ${CMAKE_CURRENT_BINARY_DIR}
      COMPILE_OPTIONS
        -fsycl -I${PORTBLAS_INCLUDE_DIR} -I${PORTBLAS_SRC_DIR} -fsycl-targets=amdgcn-amd-amdhsa
      LINK_OPTIONS
        # VERY IMPORTANT TO ADD THE TARGET AND IN THE LINKING STAGE
        # SEE https://intel.github.io/llvm-docs/GetStartedGuide.html#build-dpc-application-with-cmake
        -fsycl -fsycl-targets=amdgcn-amd-amdhsa
      )
    else()
      ROOT_ADD_GTEST(TestCustomGPUModelsFromONNX TestCustomGPUModelsFromONNX.cxx
      LIBRARIES
        ROOTTMVASofie
      INCLUDE_DIRS
        ${CMAKE_CURRENT_BINARY_DIR}
      COMPILE_OPTIONS
        # default sycl-target is spirv
        -fsycl -I${PORTBLAS_INCLUDE_DIR} -I${PORTBLAS_SRC_DIR}
      LINK_OPTIONS
        -fsycl 
      ) 
    endif()
endif()
add_sycl_to_target(TARGET TestCustomGPUModelsFromONNX SOURCES TestCustomGPUModelsFromONNX.cxx)
add_dependencies(TestCustomGPUModelsFromONNX SofieCompileGPUModels_ONNX)

endif()

#For testing serialisation of RModel object

add_executable(emitFromROOT
   EmitFromRoot_all.cxx
)
target_include_directories(emitFromROOT PRIVATE
   ${CMAKE_SOURCE_DIR}/tmva/sofie/inc
   ${SOFIE_PARSERS_DIR}/inc
   ${CMAKE_SOURCE_DIR}/tmva/inc
   ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(emitFromROOT ${Protobuf_LIBRARIES} ROOTTMVASofie ROOTTMVASofieParser)
set_target_properties(emitFromROOT PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
## silence protobuf warnings seen in version 3.0 AND 3.6. Not needed from protobuf version 3.17
target_compile_options(emitFromROOT PRIVATE -Wno-unused-parameter -Wno-array-bounds)

# Automatic compilation of headers from root files
add_custom_target(SofieCompileModels_ROOT)
# onepcm or modules dependency is needed for using ROOT I/O when converting a model in a ROOT file
if (runtime_cxxmodules)
  add_dependencies(SofieCompileModels_ROOT emitFromROOT modules_idx)
else()
  add_dependencies(SofieCompileModels_ROOT emitFromROOT onepcm)
endif()

add_custom_command(TARGET SofieCompileModels_ROOT POST_BUILD
COMMAND ${CMAKE_COMMAND} -E env ROOTIGNOREPREFIX=1 ./emitFromROOT
		USES_TERMINAL )

# Creating a Google Test for Serialisation of RModel
if (tmva-cpu)
ROOT_ADD_GTEST(TestCustomModelsFromROOT TestCustomModelsFromROOT.cxx
  LIBRARIES
    ROOTTMVASofie
    ${BLAS_LINKER_FLAGS}
    ${BLAS_LIBRARIES}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
)
add_dependencies(TestCustomModelsFromROOT SofieCompileModels_ROOT)
endif()

# gtest
# Look for needed python modules
find_python_module(torch QUIET)
if(PY_TORCH_FOUND)
  configure_file(Conv1dModelGenerator.py  Conv1dModelGenerator.py COPYONLY)
  configure_file(Conv2dModelGenerator.py  Conv2dModelGenerator.py COPYONLY)
  configure_file(Conv3dModelGenerator.py  Conv3dModelGenerator.py COPYONLY)
  configure_file(ConvTrans2dModelGenerator.py  ConvTrans2dModelGenerator.py COPYONLY)
  configure_file(LinearModelGenerator.py  LinearModelGenerator.py COPYONLY)
  configure_file(RecurrentModelGenerator.py  RecurrentModelGenerator.py COPYONLY)

if (tmva-cpu)
  ROOT_ADD_GTEST(TestSofieModels TestSofieModels.cxx
    LIBRARIES
      ROOTTMVASofie
      ROOTTMVASofieParser
      ${BLAS_LINKER_FLAGS}
      ${BLAS_LIBRARIES}
    INCLUDE_DIRS
      ${CMAKE_CURRENT_BINARY_DIR}
  )
 endif()
endif()
