cmake_minimum_required(VERSION 3.8)
project(CROWMerge)

set(CMAKE_MACOSX_RPATH NEW)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

if (NOT LLVM_BUILD_TYPE)
  set(LLVM_BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif()
set(LLVM_CONFIG_EXECUTABLE ${CMAKE_SOURCE_DIR}/third_party/llvm-${LLVM_BUILD_TYPE}-install/bin/llvm-config)
if(NOT EXISTS ${LLVM_CONFIG_EXECUTABLE})
  message(FATAL_ERROR "llvm-config could not be found!")
endif()

execute_process(
  COMMAND ${LLVM_CONFIG_EXECUTABLE} --includedir
  OUTPUT_VARIABLE LLVM_INCLUDEDIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND ${LLVM_CONFIG_EXECUTABLE} --cppflags
  OUTPUT_VARIABLE LLVM_CXXFLAGS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(LLVM_CXXFLAGS "${LLVM_CXXFLAGS} -fno-exceptions -fno-rtti")

execute_process(
  COMMAND ${LLVM_CONFIG_EXECUTABLE} --libs
  OUTPUT_VARIABLE LLVM_LIBS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND ${LLVM_CONFIG_EXECUTABLE} --system-libs
  OUTPUT_VARIABLE LLVM_SYSTEM_LIBS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(LLVM_LIBS "${LLVM_LIBS} ${LLVM_SYSTEM_LIBS}")
string(STRIP ${LLVM_LIBS} LLVM_LIBS)

execute_process(
  COMMAND ${LLVM_CONFIG_EXECUTABLE} --ldflags
  OUTPUT_VARIABLE LLVM_LDFLAGS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(PASS_LDFLAGS "${LLVM_LDFLAGS}")

execute_process(
  COMMAND ${LLVM_CONFIG_EXECUTABLE} --bindir
  OUTPUT_VARIABLE LLVM_BINDIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND ${LLVM_CONFIG_EXECUTABLE} --src-root
  OUTPUT_VARIABLE LLVM_SRC
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND ${LLVM_CONFIG_EXECUTABLE} --obj-root
  OUTPUT_VARIABLE LLVM_BUILD
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(CLANG_CXXFLAGS "")
set(CLANG_INCLUDEDIR "${LLVM_SRC}/../clang/include" "${LLVM_BUILD}/tools/clang/include")

set(ALIVE_SRCDIR "${CMAKE_SOURCE_DIR}/third_party/alive2")
set(ALIVE_BUILDDIR "${CMAKE_SOURCE_DIR}/third_party/alive2-build")
include_directories("${CMAKE_SOURCE_DIR}/third_party")
include_directories("${ALIVE_SRCDIR}")

set(CLANG_LIBS "-lclangCodeGen -lclangTooling -lclangRewrite -lclangFrontend -lclangAnalysis -lclangParse -lclangSerialization -lclangSema -lclangEdit -lclangAnalysis -lclangAST -lclangDriver -lclangLex -lclangBasic -lclangASTMatchers")

set(GTEST_CXXFLAGS "-DGTEST_HAS_RTTI=0")
set(GTEST_INCLUDEDIR "${LLVM_SRC}/utils/unittest/googletest/include")
set(GTEST_LIBS "-lgtest_main -lgtest")

set(LLVM_CXXFLAGS "${LLVM_CXXFLAGS} -fexceptions -std=c++17")

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++17")
  set(LLVM_CXXFLAGS "${LLVM_CXXFLAGS} -fvisibility-inlines-hidden")
  set(PASS_LDFLAGS "-Wl,-undefined,dynamic_lookup")
endif()

include_directories(include third_party/klee/include)
include_directories(include "${CMAKE_BINARY_DIR}/include")


find_library(ALIVE_LIBRARY alive2 PATHS "${ALIVE_BUILDDIR}" NO_DEFAULT_PATH)
if (ALIVE_LIBRARY)
  message(STATUS "Alive2: ${ALIVE_LIBRARY}")
else()
  message(SEND_ERROR "Alive2 not found")
endif()

set(Z3 "${CMAKE_SOURCE_DIR}/third_party/z3-install/bin/z3")
set(Z3_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/z3-install/include")
include_directories(${Z3_INCLUDE_DIR})

find_library(Z3_LIBRARY z3 PATHS "${CMAKE_SOURCE_DIR}/third_party/z3-install/lib" NO_DEFAULT_PATH)
if (Z3_LIBRARY)
  message(STATUS "Z3 shared lib: ${Z3_LIBRARY}")
else()
  message(SEND_ERROR "Z3 shared lib not found")
endif()


set(CROW_FILES
  #lib/Crow/Crow.cpp
  #include/souper/Crow/Crow.h
)

set(CROW_MERGE_SOURCES
  ${CROW_FILES}
  )

add_library(CROWMergePass SHARED
  ${CROW_MERGE_SOURCES}  
  lib/Pass/Pass.cpp
)


set(LLVM_LDFLAGS "${LLVM_LDFLAGS}")


foreach(target CROWMergePass)
  set_target_properties(${target} PROPERTIES COMPILE_FLAGS "${LLVM_CXXFLAGS}")
  target_include_directories(${target} PRIVATE "${LLVM_INCLUDEDIR}")
endforeach()



# dynamic

#target_link_libraries(souperCodegen ${PASS_LDFLAGS})
target_link_libraries(CROWMergePass ${PASS_LDFLAGS} ${ALIVE_LIBRARY} ${Z3_LIBRARY})
#target_link_libraries(souperPassProfileAll souperCodegen ${PASS_LDFLAGS} ${HIREDIS_LIBRARY} ${ALIVE_LIBRARY} ${Z3_LIBRARY})

# executables
#target_link_libraries(souper souperExtractor souperKVStore souperParser souperSMTLIB2 souperTool kleeExpr ${HIREDIS_LIBRARY} ${ALIVE_LIBRARY} ${Z3_LIBRARY})
#target_link_libraries(internal-solver-test souperSMTLIB2)
#target_link_libraries(lexer-test souperParser)
#target_link_libraries(parser-test souperParser)
#target_link_libraries(souper-check souperTool souperExtractor souperKVStore souperSMTLIB2 souperParser ${HIREDIS_LIBRARY} ${ALIVE_LIBRARY} ${Z3_LIBRARY})
#target_link_libraries(souper-interpret souperTool souperExtractor souperKVStore souperSMTLIB2 souperParser ${HIREDIS_LIBRARY} ${ALIVE_LIBRARY} ${Z3_LIBRARY})
#target_link_libraries(clang-souper souperClangTool souperExtractor souperKVStore souperParser souperSMTLIB2 souperTool kleeExpr ${CLANG_LIBS} ${LLVM_LIBS} ${LLVM_LDFLAGS} ${HIREDIS_LIBRARY} ${ALIVE_LIBRARY} ${Z3_LIBRARY})
#target_link_libraries(count-insts souperParser)
#target_link_libraries(souper2llvm souperParser souperCodegen)
#target_link_libraries(extractor_tests souperExtractor souperParser ${GTEST_LIBS} ${ALIVE_LIBRARY})
#target_link_libraries(inst_tests souperInfer souperPass souperInst souperExtractor ${GTEST_LIBS} ${ALIVE_LIBRARY})
#target_link_libraries(parser_tests souperParser ${GTEST_LIBS} ${ALIVE_LIBRARY})
#target_link_libraries(interpreter_tests souperInfer souperInst ${GTEST_LIBS} ${ALIVE_LIBRARY})
#target_link_libraries(bulk_tests souperInfer souperInst ${GTEST_LIBS} ${ALIVE_LIBRARY} ${Z3_LIBRARY})

set(CROW_MERGE_PASS ${CMAKE_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}crowmergePass${CMAKE_SHARED_LIBRARY_SUFFIX})

#set(SOUPER_PASS ${CMAKE_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}souperPass${CMAKE_SHARED_LIBRARY_SUFFIX})
#set(SOUPER_PASS_PROFILE_ALL ${CMAKE_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}souperPassProfileAll${CMAKE_SHARED_LIBRARY_SUFFIX})
#set(PROFILE_LIBRARY ${CMAKE_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}profileRuntime${CMAKE_STATIC_LIBRARY_SUFFIX})
#configure_file(${CMAKE_SOURCE_DIR}/utils/reduce.in ${CMAKE_BINARY_DIR}/reduce @ONLY)
#configure_file(${CMAKE_SOURCE_DIR}/utils/cache_dump.in ${CMAKE_BINARY_DIR}/cache_dump @ONLY)
##configure_file(${CMAKE_SOURCE_DIR}/utils/cache_dfa.in ${CMAKE_BINARY_DIR}/cache_dfa @ONLY)
#configure_file(${CMAKE_SOURCE_DIR}/utils/cache_import.in ${CMAKE_BINARY_DIR}/cache_import @ONLY)
#configure_file(${CMAKE_SOURCE_DIR}/utils/cache_infer.in ${CMAKE_BINARY_DIR}/cache_infer @ONLY)
#configure_file(${CMAKE_SOURCE_DIR}/utils/py_souper2llvm.in ${CMAKE_BINARY_DIR}/py_souper2llvm @ONLY)
#configure_file(${CMAKE_SOURCE_DIR}/include/souper/Tool/GetSolver.h.in ${CMAKE_BINARY_DIR}/include/souper/Tool/GetSolver.h @ONLY)

#if (BUILD_CLANG_TOOL)
#  configure_file(${CMAKE_SOURCE_DIR}/utils/sclang.in ${CMAKE_BINARY_DIR}/sclang @ONLY)
#  configure_file(${CMAKE_SOURCE_DIR}/utils/sclang.in ${CMAKE_BINARY_DIR}/sclang++ @ONLY)
#endif()

#add_subdirectory(docs)