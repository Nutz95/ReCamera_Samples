cmake_minimum_required(VERSION 3.5.0)

project(
    sscma-node
    VERSION 0.2.0
    LANGUAGES C CXX
)

get_filename_component(PROJECT_DIR ${CMAKE_CURRENT_LIST_DIR} ABSOLUTE)

message(STATUS "PROJECT_DIR: ${PROJECT_DIR}")

get_filename_component(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

include(${ROOT_DIR}/cmake/toolchain-riscv64-linux-musl-x86_64.cmake)

message(STATUS "ROOT_DIR: ${ROOT_DIR}")

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

# Install a wrapper script to easily launch sscma-node with correct LD_LIBRARY_PATH
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/sscma-node_run "#!/bin/sh
export LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:\$LD_LIBRARY_PATH
exec /usr/local/bin/sscma-node \"\$@\"")
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/sscma-node_run DESTINATION /usr/local/bin)

include(${ROOT_DIR}/cmake/project.cmake)





