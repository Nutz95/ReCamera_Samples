#!/bin/bash
set -e # Exit immediately if a command exits with a non-zero status.
# Script to build and deploy the sscma-node application with imagePreProcessor node to ReCamera

# Base home directory variable
HOME_DIR="/home/xxx"  # replace xxx with your user name on your Ubuntu 20.04
PROJECT_FOLDER="sscma-example-sg200x"
RECAMERA_OS_FOLDER="reCamera"
RECAMERA_OS_SDK_FOLDER="reCameraOS_SDK"

# Configuration
SOLUTION_DIR="${HOME_DIR}/${PROJECT_FOLDER}/solutions/sscma-node"
CAMERA_USER="recamera"
CAMERA_IP="192.168.42.1"
PASSWORD="XXXX" # Consider safer ways to handle passwords
DEB_PATTERN="sscma-node_*_riscv64.deb"
APP_NAME="sscma-node"
FORCE_REINSTALL=true # Set to true to uninstall before installing

# SDK library paths
SDK_TPU_LIB_DIR="${HOME_DIR}/${RECAMERA_OS_SDK_FOLDER}/sg2002_recamera_emmc/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_tpu_sdk/lib"
SDK_AUDIO_LIB_DIR="${HOME_DIR}/${RECAMERA_OS_FOLDER}/cvi_mpi/modules/audio/cv181x/musl_riscv64"
SDK_ISP_LIB_DIR="${HOME_DIR}/${RECAMERA_OS_FOLDER}/output/sg2002_recamera_emmc/buildroot-2021.05/output/cvitek_CV181X_musl_riscv64/target/usr/lib"
SDK_BASE_LIB_DIR="${HOME_DIR}/${RECAMERA_OS_FOLDER}/cvi_mpi/install/lib/riscv64"
SDK_MPI_LIB_DIR="${HOME_DIR}/${RECAMERA_OS_FOLDER}/cvi_mpi/modules/base/cv181x/musl_riscv64"
SDK_KERNEL_LIB_DIR="${HOME_DIR}/${RECAMERA_OS_FOLDER}/cvikernel/package/lib/riscv64"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    # No explicit exit 1 needed because of 'set -e'
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# --- Start Execution ---

# Setting up environment variables for compilation
log_info "Setting up environment variables"
export TOP_DIR="${HOME_DIR}/${RECAMERA_OS_FOLDER}"
export SG200X_SDK_PATH="${HOME_DIR}/${RECAMERA_OS_SDK_FOLDER}/sg2002_recamera_emmc/"
export PATH="${HOME_DIR}/${RECAMERA_OS_FOLDER}/host-tools/gcc/riscv64-linux-musl-x86_64/bin:$PATH"

# Verify that the compiler is available
if command -v riscv64-unknown-linux-musl-g++ &> /dev/null; then
    log_info "Compiler found in PATH: $(which riscv64-unknown-linux-musl-g++)"
else
    log_error "Compiler not found in PATH"
    exit 1
fi

# Navigate to solution directory
log_info "Navigating to solution directory: $SOLUTION_DIR"
cd "$SOLUTION_DIR"

# Cleaning and Building the project
log_info "Cleaning previous build..."
rm -rf build
log_info "Building the project..."
cmake -B build -DCMAKE_BUILD_TYPE=Release .
log_info "CMake configure done."
cmake --build build
log_info "Build complete."

# Packaging
log_info "Packaging the project..."
cd build
cpack
log_info "Packaging complete."

# Finding the generated .deb file
log_info "Finding .deb package..."
DEB_FILE=$(find . -maxdepth 1 -name "$DEB_PATTERN" | head -n 1) # Search only in current dir (build/)
if [ -z "$DEB_FILE" ]; then
    log_error "No .deb package found matching '$DEB_PATTERN' in $(pwd)"
    exit 1
fi
# Use relative path found by find
DEB_FILENAME=$(basename "$DEB_FILE")
log_info "Found package: $DEB_FILENAME"

# Deploying package to camera
log_info "Deploying package to ReCamera ($CAMERA_IP)..."
sshpass -p "$PASSWORD" scp "$DEB_FILE" "$CAMERA_USER@$CAMERA_IP:/tmp/"
log_success "Package copied to camera successfully"

# Installing the package
log_info "Installing package on ReCamera..."

# Build installation command
INSTALL_CMD=""

if [ "$FORCE_REINSTALL" = true ]; then
    log_info "Force reinstall enabled - will cleanly uninstall existing package first"
    # Uninstall and clean first if package is already installed
    INSTALL_CMD="echo '$PASSWORD' | sudo -S killall sscma-node >/dev/null 2>&1 || true; "
    INSTALL_CMD+="echo '$PASSWORD' | sudo -S opkg remove sscma-node 2>/dev/null || true; "
    INSTALL_CMD+="echo '$PASSWORD' | sudo -S rm -f /usr/DEBIAN/postinst 2>/dev/null || true; "
    INSTALL_CMD+="echo '$PASSWORD' | sudo -S rm -f /usr/local/bin/sscma-node_run 2>/dev/null || true; "
fi

# Add installation command (without launching the application yet)
INSTALL_CMD+="echo '$PASSWORD' | sudo -S opkg install /tmp/$DEB_FILENAME"

# Execute installation command on ReCamera
set +e # Temporarily disable immediate exit on error
sshpass -p "$PASSWORD" ssh "$CAMERA_USER@$CAMERA_IP" "$INSTALL_CMD"
INSTALL_RESULT=$?
set -e # Re-enable immediate exit on error

if [ $INSTALL_RESULT -ne 0 ]; then
    log_error "Package installation failed with exit code $INSTALL_RESULT"
    exit $INSTALL_RESULT
else
    log_success "Package installed successfully"
fi

# Create a temporary directory for libraries to copy
log_info "Creating temporary directory for libraries..."
TMP_LIB_DIR=$(mktemp -d)

# Search and copy necessary libraries to the temporary directory
log_info "Collecting necessary libraries..."

# List of required libraries
REQUIRED_LIBS=(
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
    "libcvikernel.so"
    "libopencv_core.so"
    "libopencv_imgproc.so"
    "libopencv_imgcodecs.so"
)

# Array of directories to search for libraries
LIB_SEARCH_PATHS=(
    "$SDK_AUDIO_LIB_DIR"
    "$SDK_TPU_LIB_DIR"
    "$SDK_ISP_LIB_DIR"
    "$SDK_BASE_LIB_DIR"
    "$SDK_MPI_LIB_DIR"
    "$SDK_KERNEL_LIB_DIR"
)

# Function to find a library in search paths
find_and_copy_lib() {
    local lib_name="$1"
    local found=0
    
    for search_dir in "${LIB_SEARCH_PATHS[@]}"; do
        if [ -f "${search_dir}/${lib_name}" ]; then
            cp "${search_dir}/${lib_name}" "$TMP_LIB_DIR/"
            echo "Copied ${lib_name} from ${search_dir}"
            found=1
            break
        fi
    done
    
    # Search for library recursively if not found in standard paths
    if [ $found -eq 0 ]; then
        echo "Searching for ${lib_name} recursively..."
        found_path=$(find ${HOME_DIR}/${RECAMERA_OS_FOLDER} -name "${lib_name}" -type f 2>/dev/null | head -n 1)
        if [ -n "$found_path" ]; then
            cp "$found_path" "$TMP_LIB_DIR/"
            echo "Copied ${lib_name} from ${found_path}"
            found=1
        fi
    fi
    
    if [ $found -eq 0 ]; then
        echo "Warning: ${lib_name} not found"
    fi
    
    # Only return status, but shouldn't stop the script
    return $found
}

# Creating wrapper script on camera
log_info "Creating wrapper script on camera..."
WRAPPER_SCRIPT="/tmp/sscma_node_wrapper.sh"
cat > "$WRAPPER_SCRIPT" << 'EOF'
#!/bin/sh
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
exec /usr/local/bin/sscma-node "$@"
EOF

sshpass -p "$PASSWORD" scp "$WRAPPER_SCRIPT" "$CAMERA_USER@$CAMERA_IP:/tmp/"
sshpass -p "$PASSWORD" ssh "$CAMERA_USER@$CAMERA_IP" "echo '$PASSWORD' | sudo -S cp /tmp/sscma_node_wrapper.sh /usr/local/bin/sscma-node-run && echo '$PASSWORD' | sudo -S chmod 755 /usr/local/bin/sscma-node-run"

# Creating test configuration for SSCMA-node
log_info "Creating test configuration for SSCMA-node..."
CONFIG_FILE="${HOME_DIR}/${PROJECT_FOLDER}/solutions/sscma-node/flow_test.json"

# Transfer configuration file directly to the correct location on camera
log_info "Copying flow.json configuration directly to /userdata/ on camera..."
sshpass -p "$PASSWORD" scp "$CONFIG_FILE" "$CAMERA_USER@$CAMERA_IP:/userdata/flow.json"
sshpass -p "$PASSWORD" ssh "$CAMERA_USER@$CAMERA_IP" "echo '$PASSWORD' | sudo -S chmod 666 /userdata/flow.json"

# Check if model exists, otherwise display a warning
sshpass -p "$PASSWORD" ssh "$CAMERA_USER@$CAMERA_IP" "if [ ! -f /userdata/MODEL/model.cvimodel ]; then echo 'WARNING: Model file not found at /userdata/MODEL/model.cvimodel! Make sure it exists.'; fi"

exit 0