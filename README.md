# SSCMA Simple Image capture Example for SG200X with OpenCv processing.

This repository provides a sample project that will help you to easily take some pictures from the ReCamera.

For that the project you are looking for is `sscma-node`.

To build it you will want to follow the next guidelines in order to properly setup the different dependencies and download the **ReCamera-OS** and the prebuilt SDK (see chapter 1 and 2 of this guide)


## Notes about compilation helpers:
### deploy_sscma_node.sh
A script that will help you build and deploy your project to your ReCamera.

You will need to edit the begining of the file to setup your environement folders:

```bash 
HOME_DIR="/home/xxx"  # replace xxx with your user name on your Ubuntu 20.04
PROJECT_FOLDER="sscma-example-sg200x"
RECAMERA_OS_FOLDER="reCamera"
RECAMERA_OS_SDK_FOLDER="reCameraOS_SDK"

# Configuration
CAMERA_USER="recamera"
CAMERA_IP="192.168.42.1"
PASSWORD="XXXX" # Consider safer ways to handle passwords
```

- on my Ubuntu 20.04 LTS (wsl), i have this repo files downloaded inside **/home/[USERNAME]/sscma-example-sg200x**
- I have my ReCamera OS downloaded into **/home/[USERNAME]/reCamera**
- I have mu ReCamera OS SDK downloaded into **/home/[USERNAME]/reCameraOS_SDK**
So adjust the variables above to fit your environement.

- Of course replace the "XXXX" to fit your ssh camera password.

Execute the script on your vsCode terminal:
```bash
/home/xxx/deploy_sscma_node.sh
```

The script will then build your project, make a static includes of all the dependancies and libs, then create a .deb package, then will try to deploy it using ***sshpass*** (you may need to install it in your ubuntu system for it to work.)

* Once deployed, connect to your camera using another terminal inside VsCode:
```bash
ssh recamera@192.168.42.1
```

* once logged in, copy the helper scripts (using winscp or scp command) **start_custom_node.sh** and **stop_all.sh** to your home folder.

make them executable if needed:
```bash
sudo chmod +x start_custom_node.sh
sudo chmod +x stop_all.sh
```

* Run the **stop_all.sh** to stop NodeRed service (it will cause you trouble if it's still runnin, plus it will free you a lot of memory for your application!!!!)
```bash
sudo stop_all.sh
```

* Start you application (it will also close existing instances and stop NodeRed)
```bash
sudo start_custom_node.sh
```

If everything is fine you should have your **sscma-node** application running.

* To deploy a new version of your application, on you vscode ubuntu system, call again the **deploy_sscma_node.sh** script and it will deploy the new version to your camera.

### start_custom_node.sh

### stop_all.sh


## Project Directory Structure  

```bash
.
├── cmake         # Build scripts
├── components    # Functional components
├── docs          # Documentation
├── images        # Images
├── scripts       # Scripts
├── solutions     # Applications
├── test          # Tests
└── tools         # Tools
```

## Prerequisites  

### 1. Clone and Set Up **ReCamera-OS**  

This project depends on **ReCamera-OS**, which provides the necessary toolchain, SDK, and runtime environment. Ensure you have cloned and set up **ReCamera-OS** from the following repository:  

🔗 [ReCamera-OS GitHub Repository](https://github.com/Seeed-Studio/reCamera-OS)  

```bash
git clone https://github.com/Seeed-Studio/reCamera-OS.git
cd reCamera-OS
# Follow the setup instructions in the repository
```  

### 2. Use a Prebuilt SDK (Optional)  

If you do not wish to build **ReCamera-OS** manually, you can download a prebuilt SDK package:  

1. Visit [ReCamera-OS Releases](https://github.com/Seeed-Studio/reCamera-OS/releases).  
2. Download the latest **reCamera_OS_SDK_x.x.x.tar.gz** package.  
3. Extract the package and set the SDK path:  

   ```bash
   export SG200X_SDK_PATH=<PATH_TO_RECAMERA-OS-SDK>/sg2002_recamera_emmc/
   ```  

## Compilation Guide  

Follow these steps to set up the environment, compile the project, and generate the necessary application package.  

### 1. Clone This Repository  

```bash
git clone https://github.com/Seeed-Studio/sscma-example-sg200x
cd sscma-example-sg200x
git submodule update --init
```  

### 2. Configure Environment  

Set up the required paths before building the application:  

```bash
export SG200X_SDK_PATH=<PATH_TO_RECAMERA-OS>/output/sg2002_recamera_emmc/
export PATH=<PATH_TO_RECAMERA-OS>/host-tools/gcc/riscv64-linux-musl-x86_64/bin:$PATH
```  

### 3. Build the Application  

Navigate to the project directory and compile:  

```bash
cd solutions/helloworld
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```  

If the build process completes successfully, the executable binary should be available in the `build` directory.  

### 4. Package the Application  

To prepare the application for distribution, package it using `cpack`:  

```bash
cd build && cpack
```  

This will generate a **.deb** package, which can be installed on the device.  

## Deploying the Application  

### 1. Transfer the Package to the Device  

Use **scp** or other file transfer methods to copy the package to the ReCamera device:  

```bash
scp build/helloworld-1.0.0-1.deb recamera@192.168.42.1:/tmp/
```  

Replace `recamera@192.168.42.1` with the actual username and IP address of your device.  

### 2. Install the Application  

Log into the device via SSH and install the package using `opkg`:  

```bash
ssh recamera@192.168.42.1
sudo opkg install /tmp/helloworld-1.0.0-1.deb
```  

**Note**: sudo password is the same as the WEB UI password. default is `recamera`.

### 3. Run the Application  

Once installed, you can run the application:  

```bash
helloworld
Hello, ReCamera!
```  

For more information, go to the specific solution's README.