# Force l'utilisation de la version custom d'OpenCV 4.11
include_directories(BEFORE /home/nra/opencv-riscv64/include/opencv4)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(OpenCV_DIR "/home/nra/opencv-riscv64/lib/cmake/opencv4")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_minimum_required(VERSION 3.10)

# Récupération de la version depuis version.h pour garantir la cohérence
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.h" version_h)
string(REGEX MATCH "PROJECT_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)\"" _ ${version_h})
set(VERSION_FROM_HEADER ${CMAKE_MATCH_1})
message(STATUS "Version from version.h: ${VERSION_FROM_HEADER}")

project(sscma-node VERSION ${VERSION_FROM_HEADER} LANGUAGES C CXX)
message(STATUS "Project version set to: ${PROJECT_VERSION}")

set(RISCV_SDK_ROOT $ENV{RISCV_SDK_ROOT})
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain-riscv64-linux.cmake CACHE FILEPATH "toolchain file")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "$ENV{RISCV_SDK_ROOT}/cmake")

get_filename_component(PROJECT_DIR ${CMAKE_CURRENT_LIST_DIR} ABSOLUTE)

message(STATUS "PROJECT_DIR: ${PROJECT_DIR}")

get_filename_component(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

include(${ROOT_DIR}/cmake/toolchain-riscv64-linux-musl-x86_64.cmake)

message(STATUS "ROOT_DIR: ${ROOT_DIR}")

message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_INCLUDE_PATH: ${CMAKE_INCLUDE_PATH}")
message(STATUS "CMAKE_SYSTEM_INCLUDE_PATH: ${CMAKE_SYSTEM_INCLUDE_PATH}")

# Force l'utilisation de la version custom d'OpenCV 4.11
set(OpenCV_DIR "/home/nra/opencv-riscv64/lib/cmake/opencv4" CACHE PATH "Path to OpenCV 4.11.0 config" FORCE)
set(OpenCV_STATIC ON)
set(OpenCV_INCLUDE_DIRS "/home/nra/opencv-riscv64/include/opencv4/3rdparty")
set(OpenCV_LIB_DIR "/home/nra/opencv-riscv64/lib")
include_directories(BEFORE /home/nra/opencv-riscv64/include/opencv4)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/main/node/config)
link_directories(
    /home/nra/opencv-riscv64/lib
    /home/nra/reCamera/output/sg2002_recamera_emmc/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_tpu_sdk/lib
    /home/nra/reCamera/output/sg2002_recamera_emmc/cvi_mpi/modules/audio/cv181x/musl_riscv64
    /home/nra/reCameraOS_SDK/sg2002_recamera_emmc/buildroot-2021.05/output/cvitek_CV181X_musl_riscv64/host/riscv64-buildroot-linux-musl/sysroot/usr/lib
    /home/nra/reCamera/output/sg2002_recamera_emmc/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_ai_sdk/lib
    /home/nra/reCamera/output/sg2002_recamera_emmc/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_tpu_sdk/lib
    /home/nra/reCamera/output/sg2002_recamera_emmc/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_ai_sdk/sample/3rd/rtsp/lib
)

# Pour un linkage mixte : OpenCV statique, autres libs dynamiques
set(OpenCV_STATIC ON)
# Ne pas forcer le linkage statique global
# set(CMAKE_EXE_LINKER_FLAGS "-static")

find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio objdetect photo)

message(STATUS "OpenCV_INCLUDE_DIRS: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV_LIBS: ${OpenCV_LIBS}")
message(STATUS "OpenCV_VERSION: ${OpenCV_VERSION}")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message(STATUS "CMAKE_FIND_ROOT_PATH: ${CMAKE_FIND_ROOT_PATH}")

# Link OpenCV statically
set(OpenCV_STATIC_LIBS ${OpenCV_LIBS})

# Configurer les options d'installation et packaging
set(CPACK_DEBIAN_PACKAGE_DEPENDS "device")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_GENERATOR "DEB")

# Désactiver les composants séparés pour avoir un seul package
set(CPACK_DEB_COMPONENT_INSTALL OFF)
set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE ON)

# Script postinst amélioré (sera exécuté après l'installation du package)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/postinst "#!/bin/sh
echo 'Configuring environment for ${CMAKE_PROJECT_NAME}'

# Créer le répertoire lib s'il n'existe pas
mkdir -p /usr/local/lib

# Créer les liens symboliques pour les versions spécifiques des bibliothèques OpenCV et FFmpeg
cd /usr/lib
for lib in libopencv_core.so libopencv_imgproc.so libopencv_imgcodecs.so; do
    if [ -f \"\$lib\" ] && [ ! -f \"\${lib}.3.2\" ]; then
        ln -sf \"/usr/lib/\$lib\" \"/usr/lib/\${lib}.3.2\"
    fi
done

for lib in libavformat.so libavcodec.so; do
    if [ -f \"\$lib\" ] && [ ! -f \"\${lib}.58\" ]; then
        ln -sf \"/usr/lib/\$lib\" \"/usr/lib/\${lib}.58\"
    fi
done

if [ -f \"libavutil.so\" ] && [ ! -f \"libavutil.so.56\" ]; then
    ln -sf \"/usr/lib/libavutil.so\" \"/usr/lib/libavutil.so.56\"
fi

# Mise à jour du cache de bibliothèques
ldconfig

# Créer un script wrapper pour l'application
cat > /usr/local/bin/${CMAKE_PROJECT_NAME}_run << 'EOL'
#!/bin/sh
# Wrapper script for ${CMAKE_PROJECT_NAME}
export LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:\$LD_LIBRARY_PATH
/usr/local/bin/${CMAKE_PROJECT_NAME} \"\$@\"
EOL

# Rendre le script exécutable
chmod 755 /usr/local/bin/${CMAKE_PROJECT_NAME}_run

echo 'Installation completed successfully!'
exit 0
")

# Installer le script post-installation
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/postinst
        DESTINATION DEBIAN
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# Script preinst pour s'assurer que les anciens fichiers sont supprimés
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/preinst "#!/bin/sh
echo 'Preparing for installation of ${CMAKE_PROJECT_NAME} ${PROJECT_VERSION}'

# Supprimer l'ancien exécutable s'il existe
if [ -f /usr/local/bin/${CMAKE_PROJECT_NAME} ]; then
    echo 'Removing old executable...'
    rm -f /usr/local/bin/${CMAKE_PROJECT_NAME}
fi

exit 0
")

# Installer le script pre-installation
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/preinst
        DESTINATION DEBIAN
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# Install a wrapper script to easily launch sscma-node with correct LD_LIBRARY_PATH
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/sscma-node_run "#!/bin/sh
export LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:\$LD_LIBRARY_PATH
exec /usr/local/bin/sscma-node \"\$@\"")

# Installer le wrapper script
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/sscma-node_run DESTINATION /usr/local/bin)

# Important : Pas besoin d'inclure directement le fichier main/CMakeLists.txt,
# il sera inclus par project.cmake 

# ==== Configuration système originale (pour résoudre les problèmes de dépendances) ====
include(${ROOT_DIR}/cmake/project.cmake)

# ==== Après l'inclusion de project.cmake, nous modifions target_link_libraries pour résoudre notre problème ====
if(TARGET sscma-node)
    target_link_libraries(sscma-node PRIVATE -lopencv_objdetect -lopencv_core /home/nra/zxing-cpp-riscv64/lib/libZXing.a)
    
    # CRUCIAL: INSTALLATION EXPLICITE DE L'EXÉCUTABLE
    install(TARGETS sscma-node 
            RUNTIME DESTINATION /usr/local/bin
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    
    # Ajouter des dépendances explicites à l'installation
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E echo \"Installing ${PROJECT_NAME} ${PROJECT_VERSION} to package...\")")
endif()

# Informations de packaging pour CPack
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Admin")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SSCMA Node for ReCameraOS")
set(CPACK_PACKAGE_VENDOR "Sophgo")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# S'assurer que la directive d'installation sera respectée
set(CPACK_SET_DESTDIR true)
set(CPACK_INSTALL_PREFIX "/")

# Activer CPack pour la création de packages
include(CPack)




