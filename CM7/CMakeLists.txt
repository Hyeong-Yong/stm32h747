cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Compiler options
set(STM32_MCU_FLAGS  "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard " )

#Linker options
set(STM32_LINKER_SCRIPT stm32h747xx_flash_CM7.ld)
set(STM32_LINKER_OPTION  )

# Set the project name
set(CMAKE_PROJECT_NAME stm32h747_CM7)
project(${CMAKE_PROJECT_NAME})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Include mx-generated
include("mx-generated.cmake")


# In order to use CMake for cross-compiling
set(CMAKE_CXX_COMPILER_FORCED "true")

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    
    App/ap
    App/common
    App/common/include
    App/hw
    App/hw/include
    App/hw/include/qspi
    App/hw/include/ovtoct
    App/hw/include/galvano

    # USB
    App/hw/src/usb
    App/hw/src/usb/usb_cdc
    App/Middlewares/ST/STM32_USB_Device_Library/Core/Inc

    #ETH
    LWIP/App
    LWIP/Target
)

file(GLOB SRC_FILES CONFIGURE_DEPENDS
    App/ap/*.c
    App/common/src/*.c
    App/hw/*.c
    App/hw/src/*.c
    
    # galvano
    App/hw/src/galvano/*.c

    # ovtoct
    App/hw/src/ovtoct/*.c

    # USB
    App/hw/src/usb/*.c
    App/hw/src/usb/usb_cdc/*.c
    App/Middlewares/ST/STM32_USB_Device_Library/Core/Src/*.c

    # ETH
    LWIP/App/*.c
    LWIP/Target/*.c
)


# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${SRC_FILES}

)


# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined libraries
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()




#
# Core MCU flags, CPU, instruction set and FPU setup
# Needs to be set properly for your MCU
#
set(CPU_PARAMETERS
    -mthumb

    # This needs attention to properly set for used MCU
    -mcpu=cortex-m7                 # Modified
    -mfpu=fpv5-d16                  # Modified
    -mfloat-abi=hard                # Modified
)


#linkder_script_SRC는 비활성화 : 어디선가 호출되어서, 여기서 정의하면 두번 호출되어서 오류남
set(PROJ_PATH                       ${CMAKE_CURRENT_SOURCE_DIR})
#set(linker_script_SRC               ${PROJ_PATH}/STM32H750XBHx_FLASH.ld) # Modified
set(EXECUTABLE                      ${CMAKE_PROJECT_NAME})

#Linker options
target_link_options(${EXECUTABLE} PRIVATE
    #-T${linker_script_SRC}
    ${CPU_PARAMETERS}
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
    --specs=nosys.specs
    -u _printf_float                # STDIO float formatting support
    -Wl,--start-group
    -lc
    -lm
    -lstdc++
    -lsupc++
    -Wl,--end-group
    -Wl,--print-memory-usage
)










