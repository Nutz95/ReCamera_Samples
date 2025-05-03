file(GLOB_RECURSE srcs ${CMAKE_CURRENT_LIST_DIR}/*.c ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

set(incs ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/node
)

# Add paths to your custom OpenCV installation
set(OpenCV_DIR "/home/nra/opencv-riscv64/lib/cmake/opencv4")
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio objdetect)

# Supprimer l'inclusion potentiellement polluante du SDK OpenCV
# include_directories(${OpenCV_INCLUDE_DIRS})

# Link OpenCV statically
set(OpenCV_STATIC_LIBS ${OpenCV_LIBS})
# Supprime le linkage statique global si présent
# set(CMAKE_EXE_LINKER_FLAGS "-static")

# Add OpenCV libraries to the project
set(EXTRA_LIBS ${OpenCV_STATIC_LIBS} ${EXTRA_LIBS})

component_register(
    COMPONENT_NAME main
    SRCS ${srcs}
    INCLUDE_DIRS ${incs}
    PRIVATE_REQUIREDS sscma-micro avformat avcodec avutil asound opencv_core opencv_imgcodecs opencv_imgproc
)


