cmake_minimum_required(VERSION 3.20)

# =============================================================================
#   SHIFT: Secure Hardware Interface for File Transfer
#   Authors: ISFCR, PES University RR Campus
# =============================================================================

project(shift LANGUAGES C ASM)

# Define Colors for Pretty Output
string(ASCII 27 Esc)
set(Reset "${Esc}[m")
set(Bold "${Esc}[1m")
set(Red "${Esc}[31m")
set(Green "${Esc}[32m")
set(Yellow "${Esc}[33m")
set(Blue "${Esc}[34m")
set(Cyan "${Esc}[36m")
set(White "${Esc}[37m")
set(BoldRed "${Esc}[1;31m")
set(BoldGreen "${Esc}[1;32m")
set(BoldYellow "${Esc}[1;33m")
set(BoldCyan "${Esc}[1;36m")

message(
    STATUS
    "${BoldCyan}[Init]${Reset}       Starting SHIFT Build Configuration..."
)

# -----------------------------------------------------------------------------
# 1. Environment & Toolchain Discovery
# -----------------------------------------------------------------------------

# Mandatory Environment Variables
if(NOT DEFINED ENV{SDK_SOURCE_DIR})
    message(
        FATAL_ERROR
        "${BoldRed}[Error]      SDK_SOURCE_DIR is not set. TI SDK path is required.${Reset}"
    )
endif()
set(SDK_SOURCE_DIR $ENV{SDK_SOURCE_DIR})

if(NOT DEFINED ENV{PROJECT_DIR})
    message(
        FATAL_ERROR
        "${BoldRed}[Error]      PROJECT_DIR is not set. Please export it.${Reset}"
    )
endif()
set(PROJECT_DIR $ENV{PROJECT_DIR})

# Toolchains
# Main: TI Clang
find_program(TI_CC tiarmclang REQUIRED DOC "Path to TI ARM Clang Compiler")
find_program(TI_OBJCOPY tiarmobjcopy DOC "Path to TI ARM Objcopy")
find_program(TI_SIZE tiarmsize DOC "Path to TI ARM Size utility")

if(TI_CC)
    message(
        STATUS
        "   ${BoldGreen}+${Reset} TI Clang:     ${BoldYellow}${TI_CC}${Reset}"
    )
endif()

# Secondary: GCC (for Crypto Libraries)
find_program(GCC_CC arm-none-eabi-gcc REQUIRED DOC "Path to ARM GCC Compiler")
find_program(GCC_AR arm-none-eabi-ar REQUIRED DOC "Path to ARM GCC Archiver")

if(GCC_CC)
    message(
        STATUS
        "   ${BoldGreen}+${Reset} ARM GCC:      ${BoldYellow}${GCC_CC}${Reset}"
    )
endif()

# Default Compiler Settings (TI Clang)
set(CMAKE_C_COMPILER ${TI_CC})
set(CMAKE_ASM_COMPILER ${TI_CC})
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# 2. Centralized Configuration (Flags)
# -----------------------------------------------------------------------------

# Architecture Flags (Cortex-M0+) for consistency across compilers
set(ARCH_FLAGS -mcpu=cortex-m0plus -mfloat-abi=soft -mthumb)

# Optimization Flags: Full (-O3) for Crypto, Balanced (-O2) for Firmware
set(FIRMWARE_OPT -O2)
set(CRYPTO_OPT -O3)
set(SECTION_FLAGS -ffunction-sections -fdata-sections)

# Warning & Diagnostics Flags
# Strict Code Quality & MISRA-C Alignment (Warnings Only - Non-Blocking)
set(WARN_FLAGS
    -Wall
    -Wextra
    -Wshadow # Standard strictness
    -Wundef # Catch undefined macros
    -Wformat=2 # Strict format string checks
    -Wmissing-prototypes # Enforce headers/static
    -Wstrict-prototypes # Enforce (void) in empty args
    -Wredundant-decls # Clean headers
    -Wcast-align # Critical for M0+ (HardFault on unaligned access)
    -Wvla # No VLA (Stack safety / MISRA)
    -Wpointer-arith # No void pointer math
    -Wconversion # Catch implicit truncations
    -Wsign-conversion # Catch signed/unsigned mixups
)

# Debug Flags
set(DBG_FLAGS -gdwarf-3 -g)

# Aggregated Flags (Base)
set(COMMON_CFLAGS ${ARCH_FLAGS} ${SECTION_FLAGS} ${WARN_FLAGS} ${DBG_FLAGS})

# TI/SDK Definitions
add_compile_definitions(__MSPM0L2228__ DeviceFamily_MSPM0L222X)

# -----------------------------------------------------------------------------
# 3. Artifact: uECC (GCC Build)
# -----------------------------------------------------------------------------
set(UECC_SRC "${PROJECT_DIR}/src/uECC.c")
set(UECC_LIB "${CMAKE_CURRENT_BINARY_DIR}/libuECC.a")

# GCC Flags List: Include Common Flags + GCC-specifics
# -fdiagnostics-color=always: Force colored output for GCC
# -w: Suppress ALL warnings for external libraries (uECC & Ascon)
set(GCC_FLAGS_LIST
    ${COMMON_CFLAGS}
    ${CRYPTO_OPT}
    -fomit-frame-pointer
    -march=armv6-m
    -I${PROJECT_DIR}/include
    -fdiagnostics-color=always
    -w
)

add_custom_command(
    OUTPUT ${UECC_LIB}
    COMMAND
        ${GCC_CC} -c ${UECC_SRC} -o "${CMAKE_CURRENT_BINARY_DIR}/uECC.o"
        ${GCC_FLAGS_LIST}
    COMMAND ${GCC_AR} rcs ${UECC_LIB} "${CMAKE_CURRENT_BINARY_DIR}/uECC.o"
    DEPENDS
        ${UECC_SRC}
        "${PROJECT_DIR}/include/uECC.h"
        "${PROJECT_DIR}/src/asm_arm.inc"
    COMMENT
        "${BoldGreen}➜ [Build]${Reset}   Compiling uECC (${BoldYellow}GCC${Reset})"
    VERBATIM
)
add_custom_target(uecc_build DEPENDS ${UECC_LIB})
add_library(uecc STATIC IMPORTED)
set_target_properties(uecc PROPERTIES IMPORTED_LOCATION ${UECC_LIB})
add_dependencies(uecc uecc_build)

# -----------------------------------------------------------------------------
# 4. Artifact: Masked Ascon-AEAD (GCC External Project)
# -----------------------------------------------------------------------------
include(ExternalProject)

set(ASCON_MASKED_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/ambld")
set(ASCON_MASKED_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/aminst")

set(ASCON_MASKED_FLAGS_LIST
    ${GCC_FLAGS_LIST}
    -DNUM_SHARES_M=2
    -DNUM_SHARES_C=2
    -DNUM_SHARES_AD=2
    -DNUM_SHARES_NPUB=2
)
list(JOIN ASCON_MASKED_FLAGS_LIST " " ASCON_MASKED_FLAGS_STRING_SPACE)
list(JOIN ASCON_MASKED_FLAGS_LIST "|" ASCON_MASKED_FLAGS_STRING_PIPE)

ExternalProject_Add(
    ascon_masked_ext
    PREFIX ${ASCON_MASKED_BUILD_DIR}
    SOURCE_DIR "${PROJECT_DIR}/external/ascon"
    LIST_SEPARATOR |
    CMAKE_ARGS
        -DCMAKE_C_COMPILER=${GCC_CC}
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
        "-DCMAKE_C_FLAGS=${ASCON_MASKED_FLAGS_STRING_SPACE}"
        "-DREL_FLAGS=${ASCON_MASKED_FLAGS_STRING_PIPE}" -DALG_LIST=asconaead128
        -DIMPL_LIST=protected_bi32_armv6_leveled -DTEST_LIST=
        -DCMAKE_INSTALL_PREFIX=${ASCON_MASKED_INSTALL_DIR}
    BUILD_COMMAND ${CMAKE_COMMAND} --build .
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS
        ${ASCON_MASKED_BUILD_DIR}/src/ascon_masked_ext-build/libcrypto_aead_asconaead128_protected_bi32_armv6_leveled.a
)

add_library(ascon_aead_masked STATIC IMPORTED)
set_target_properties(
    ascon_aead_masked
    PROPERTIES
        IMPORTED_LOCATION
            ${ASCON_MASKED_BUILD_DIR}/src/ascon_masked_ext-build/libcrypto_aead_asconaead128_protected_bi32_armv6_leveled.a
        INTERFACE_INCLUDE_DIRECTORIES
            "${PROJECT_DIR}/external/ascon/crypto_aead/asconaead128/protected_bi32_armv6_leveled"
)
add_dependencies(ascon_aead_masked ascon_masked_ext)

# -----------------------------------------------------------------------------
# 5. Main Application (TI Clang)
# -----------------------------------------------------------------------------

# Gather Sources (Exclude uECC)
file(
    GLOB_RECURSE APP_SOURCES
    "${PROJECT_DIR}/src/*.c"
    "${PROJECT_DIR}/secrets/*.c"
)
list(FILTER APP_SOURCES EXCLUDE REGEX ".*uECC\\.c$")

# Include SDK source for RAM-resident operations
set(SDK_DRIVERS
    "${SDK_SOURCE_DIR}/source/ti/driverlib/dl_flashctl.c"
    "${SDK_SOURCE_DIR}/source/ti/driverlib/dl_common.c"
)
list(APPEND APP_SOURCES ${SDK_DRIVERS})

add_executable(${PROJECT_NAME} ${APP_SOURCES})

# Include Paths
# SDK/System includes -> SYSTEM (Suppress warnings)
target_include_directories(
    ${PROJECT_NAME}
    SYSTEM
    PRIVATE
        "${SDK_SOURCE_DIR}/source"
        "${SDK_SOURCE_DIR}/source/third_party/CMSIS/Core/Include"
    # "${PROJECT_DIR}/external/ascon/crypto_aead_hash/asconaeadxof128/bi32_armv6m"
)
# User includes -> PRIVATE (Strict warnings)
target_include_directories(
    ${PROJECT_NAME}
    PRIVATE "${PROJECT_DIR}/include" "${PROJECT_DIR}/secrets"
)

# Apply Flags
# -fcolor-diagnostics: Force colored output for Clang
target_compile_options(
    ${PROJECT_NAME}
    PRIVATE ${COMMON_CFLAGS} ${FIRMWARE_OPT} -fcolor-diagnostics
)

# Linker Configuration
target_link_options(
    ${PROJECT_NAME}
    PRIVATE
        "-Wl,${PROJECT_DIR}/linker.cmd"
        "-Wl,-m,${PROJECT_NAME}.map"
        "-Wl,--rom_model"
        "-Wl,--warn_sections"
        "-Wl,-u,_c_int00"
        "-Wl,--unused_section_elimination=on" # TI Clang GC sections
)

# Link Libraries
target_link_libraries(
    ${PROJECT_NAME}
    "${SDK_SOURCE_DIR}/source/ti/driverlib/lib/ticlang/m0p/mspm0l122x_l222x/driverlib.a"
    "${SDK_SOURCE_DIR}/source/ti/drivers/lib/ticlang/m0p/drivers_mspm0l122x_l222x.a"
    uecc
    ascon_aead_masked
)

# Dependency on linker script
set_property(
    TARGET ${PROJECT_NAME}
    PROPERTY LINK_DEPENDS "${PROJECT_DIR}/linker.cmd"
)

# -----------------------------------------------------------------------------
# 6. Post-Build
# -----------------------------------------------------------------------------
if(TI_OBJCOPY)
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND
            ${TI_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}>
            ${PROJECT_NAME}.bin
        COMMAND
            ${TI_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}>
            ${PROJECT_NAME}.hex
        COMMENT
            "${BoldGreen}➜ [Post-Build]${Reset} Generated binary and hex files"
    )
endif()

if(TI_SIZE)
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${TI_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "${BoldGreen}➜ [Post-Build]${Reset} Size Analysis:"
    )
endif()

message(
    STATUS
    "${BoldCyan}[Config]${Reset}     Target:           ${BoldGreen}MSPM0L2228 (Cortex-M0+)${Reset}"
)
message(
    STATUS
    "${BoldCyan}[Config]${Reset}     Main Toolchain:   ${BoldYellow}TI Clang${Reset}"
)
message(
    STATUS
    "${BoldCyan}[Config]${Reset}     Crypto Toolchain: ${BoldYellow}GCC${Reset}"
)
