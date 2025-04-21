#include <iostream>

// #include "ma_transport_rtsp.h"
#include <sscma.h>
#include <video.h>

using namespace ma;


int main(int argc, char** argv) {

    Device* device = Device::getInstance();
    Camera* camera = nullptr;

    Signal::install({SIGINT, SIGSEGV, SIGABRT, SIGTRAP, SIGTERM, SIGHUP, SIGQUIT, SIGPIPE}, [device](int sig) {
        std::cout << "Caught signal " << sig << std::endl;
        for (auto& sensor : device->getSensors()) {
            sensor->deInit();
        }
        exit(0);
    });

    // TransportRTSP rtsp;
    // TransportRTSP::Config rtsp_config;
    // rtsp_config.port    = 554;
    // rtsp_config.format  = MA_PIXEL_FORMAT_H264;
    // rtsp_config.session = "test";
    // rtsp_config.user    = "admin";
    // rtsp_config.pass    = "admin";
    // rtsp.init(&rtsp_config);


    for (auto& sensor : device->getSensors()) {
        if (sensor->getType() == ma::Sensor::Type::kCamera) {
            camera = static_cast<Camera*>(sensor);
            camera->init(0);
            Camera::CtrlValue value;
            // Configuration du canal 0 (flux principal)
            value.i32 = 0;
            camera->commandCtrl(Camera::CtrlType::kChannel, Camera::CtrlMode::kWrite, value);
            value.u16s[0] = 1920;  // Résolution plus standard pour ce type de caméra
            value.u16s[1] = 1080;
            camera->commandCtrl(Camera::CtrlType::kWindow, Camera::CtrlMode::kWrite, value);

            // Configuration du canal 1 (flux secondaire)
            value.i32 = 1;
            camera->commandCtrl(Camera::CtrlType::kChannel, Camera::CtrlMode::kWrite, value);
            value.u16s[0] = 1280;
            value.u16s[1] = 720;  // Résolution plus petite pour le deuxième flux
            camera->commandCtrl(Camera::CtrlType::kWindow, Camera::CtrlMode::kWrite, value);

            // value.i32 = 2;
            // camera->commandCtrl(Camera::CtrlType::kChannel, Camera::CtrlMode::kWrite, value);
            break;
        }
    }


    camera->startStream(Camera::StreamMode::kRefreshOnReturn);
    static char buf[4 * 1024];
    uint32_t count = 0;

    // Assurez-vous que le répertoire existe
    system("mkdir -p /userdata/IMAGES");
    MA_LOGI(MA_TAG, "Waiting for image capture...");

    // Ajoutez une limite d'essais pour éviter une boucle infinie
    int attempts           = 0;
    const int max_attempts = 10;  // ~5 secondes à 100ms par tentative

    Camera::CtrlValue channelValue;
    channelValue.i32 = 0;  // Utilisez le canal principal
    camera->commandCtrl(Camera::CtrlType::kChannel, Camera::CtrlMode::kWrite, channelValue);

    // Configurez le format JPEG
    Camera::CtrlValue formatValue;
    formatValue.i32 = MA_PIXEL_FORMAT_AUTO;
    camera->commandCtrl(Camera::CtrlType::kFormat, Camera::CtrlMode::kWrite, formatValue);

    while (attempts < max_attempts) {
        ma_img_t jpeg;
        MA_LOGI(MA_TAG, "Attempting to retrieve frame %d/%d", attempts + 1, max_attempts);

        if (camera->retrieveFrame(jpeg, MA_PIXEL_FORMAT_AUTO) == MA_OK) {
            MA_LOGI(MA_TAG, "JPEG frame captured successfully, size: %d bytes", jpeg.size);

            if (jpeg.size <= 0) {
                MA_LOGE(MA_TAG, "Invalid JPEG size: %d", jpeg.size);
                camera->returnFrame(jpeg);
                attempts++;
                usleep(100000);  // Attendre 100ms
                continue;
            }

            // Enregistrer avec un nom de fichier unique basé sur l'horodatage
            char filename[128];
            time_t now = time(NULL);
            snprintf(filename, sizeof(filename), "/userdata/IMAGES/capture_%ld.jpg", now);

            FILE* file = fopen(filename, "wb");
            if (file != NULL) {
                size_t written = fwrite(jpeg.data, 1, jpeg.size, file);
                fclose(file);

                if (written == jpeg.size) {
                    MA_LOGI(MA_TAG, "Image successfully saved to %s", filename);
                    // Afficher les propriétés de l'image pour déboguer
                    MA_LOGI(MA_TAG, "Image properties: %dx%d, format: %d", jpeg.width, jpeg.height, jpeg.format);

                    camera->returnFrame(jpeg);
                    break;  // Sortir de la boucle
                } else {
                    MA_LOGE(MA_TAG, "Failed to write entire image: %zu/%d bytes written", written, jpeg.size);
                    camera->returnFrame(jpeg);
                    attempts++;
                }
            } else {
                MA_LOGE(MA_TAG, "Failed to open file for writing: %s (errno: %d: %s)", filename, errno, strerror(errno));
                camera->returnFrame(jpeg);
                attempts++;
            }
        } else {
            MA_LOGI(MA_TAG, "retrieveFrame failed, retrying...");
            attempts++;
            usleep(100000);  // Attendre 100ms entre les tentatives
        }
    }

    if (attempts >= max_attempts) {
        MA_LOGE(MA_TAG, "Failed to capture image after %d attempts", max_attempts);
    }

    MA_LOGI(MA_TAG, "Stopping camera stream");
    camera->stopStream();

    return 0;
}