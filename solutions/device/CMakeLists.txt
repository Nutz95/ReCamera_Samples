cmake_minimum_required(VERSION 3.5.0)

project(device C CXX)

get_filename_component(PROJECT_DIR ${CMAKE_CURRENT_LIST_DIR} ABSOLUTE)

message(STATUS "PROJECT_DIR: ${PROJECT_DIR}")

get_filename_component(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

include(${ROOT_DIR}/cmake/toolchain-riscv64-linux-musl-x86_64.cmake)

message(STATUS "ROOT_DIR: ${ROOT_DIR}")

# Configurer les options d'installation et packaging
set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_GENERATOR "DEB")

# Désactiver les composants séparés pour avoir un seul package
set(CPACK_DEB_COMPONENT_INSTALL OFF)
set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE ON)

# Définir les chemins des bibliothèques SDK (mise à jour avec les bons chemins)
set(SDK_TPU_LIB_DIR "/home/nra/reCameraOS_SDK/sg2002_recamera_emmc/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_tpu_sdk/lib")
set(SDK_AUDIO_LIB_DIR "/home/nra/reCamera/cvi_mpi/modules/audio/cv181x/musl_riscv64")
set(SDK_ISP_LIB_DIR "/home/nra/reCamera/output/sg2002_recamera_emmc/buildroot-2021.05/output/cvitek_CV181X_musl_riscv64/target/usr/lib")
set(SDK_BASE_LIB_DIR "/home/nra/reCamera/cvi_mpi/install/lib/riscv64")
set(SDK_MPI_LIB_DIR "/home/nra/reCamera/cvi_mpi/modules/base/cv181x/musl_riscv64")

# Créer un répertoire pour les bibliothèques dans le package
install(DIRECTORY DESTINATION lib)

# Liste des bibliothèques nécessaires au fonctionnement
set(REQUIRED_LIBS
    "libcviruntime.so" 
    "libtinyalsa.so"
    "libcvi_audio.so"
    "libcvi_dnvqe.so" 
    "libcvi_ssp.so"
    "libcvi_ssp2.so"
    "libcvi_vqe.so"
    "libcvi_VoiceEngine.so"
    "libcvi_RES1.so"
    "libaacdec2.so"
    "libaacenc2.so"
    "libaaccomm2.so"
    "libaacsbrdec2.so"
    "libaacsbrenc2.so"
    "libae.so"
    "libaf.so"
    "libawb.so"
    "libcvi_bin.so"
    "libcvi_bin_isp.so"
    "libgdc.so"
    "libisp.so"
    "libisp_algo.so"
    "libsys.so"
    "libvenc.so"
    "libvpss.so"
    "libvi.so"
    "libvo.so"
    "libcvi_ispd2.so"
    "libsns_ov5647.so"
    "libsns_sc530ai_2l.so"
    "libcvi_rtsp.so"
)

# Tableau des répertoires à chercher dans l'ordre
set(LIB_SEARCH_PATHS
    ${SDK_AUDIO_LIB_DIR}
    ${SDK_TPU_LIB_DIR}
    ${SDK_ISP_LIB_DIR}
    ${SDK_BASE_LIB_DIR}
    ${SDK_MPI_LIB_DIR}
)

# Rechercher et installer les bibliothèques requises
foreach(lib ${REQUIRED_LIBS})
    set(LIB_FOUND FALSE)
    foreach(search_dir ${LIB_SEARCH_PATHS})
        if(EXISTS "${search_dir}/${lib}")
            message(STATUS "Installing library from ${search_dir}: ${lib}")
            install(FILES "${search_dir}/${lib}" 
                    DESTINATION lib
                    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
            set(LIB_FOUND TRUE)
            break()
        endif()
    endforeach()
    
    if(NOT LIB_FOUND)
        message(WARNING "Library not found: ${lib}")
    endif()
endforeach()

# Script postinst amélioré (sera exécuté après l'installation du package)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/postinst "#!/bin/sh
echo 'Configuring environment for ${CMAKE_PROJECT_NAME}'

# Créer le répertoire lib s'il n'existe pas
mkdir -p /usr/local/lib

# Copier les bibliothèques au bon endroit
cp -f /usr/share/device/lib/*.so /usr/local/lib/ 2>/dev/null || true

# Créer un script wrapper pour l'application
cat > /usr/local/bin/${CMAKE_PROJECT_NAME}_run << 'EOL'
#!/bin/sh
# Wrapper script for ${CMAKE_PROJECT_NAME}
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
/usr/local/bin/${CMAKE_PROJECT_NAME} \"$@\"
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

include(${ROOT_DIR}/cmake/project.cmake)




