file(GLOB_RECURSE srcs ${CMAKE_CURRENT_LIST_DIR}/*.c ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

set(incs ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/node
)

# Ajout du header ZXing
include_directories(/home/nra/zxing-cpp-riscv64/include)

# Add paths to your custom OpenCV installation
set(OpenCV_DIR "/home/nra/opencv-riscv64/lib/cmake/opencv4")
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio objdetect)

# Link ZXing statiquement
set(ZXING_LIB /home/nra/zxing-cpp-riscv64/lib/libZXing.a)
set(OpenCV_STATIC_LIBS ${OpenCV_LIBS})
set(EXTRA_LIBS ${OpenCV_STATIC_LIBS} ${ZXING_LIB} ${EXTRA_LIBS})

component_register(
    COMPONENT_NAME main
    SRCS ${srcs}
    INCLUDE_DIRS ${incs}
    PRIVATE_REQUIREDS sscma-micro avformat avcodec avutil asound opencv_core opencv_imgcodecs opencv_imgproc
)


