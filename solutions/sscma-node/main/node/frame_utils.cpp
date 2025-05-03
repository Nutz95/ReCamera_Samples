#include <opencv2/opencv.hpp>
#include <string>

#include "frame_utils.h"
#include "logger.hpp"
#include "profiler.h"

static constexpr char TAG[] = "ma::node::FrameUtils";

namespace ma::node {


// Fonction pour convertir la frame d'entrée en Mat OpenCV
::cv::Mat FrameUtils::convertFrameToMat(videoFrame* frame) {
    ::cv::Mat raw_image;
    Profiler p("convertFrameToMat");
    // Ajouter un log pour voir quel format d'image est reçu
    MA_LOGI(TAG, "Converting frame: format=%d, size=%dx%d, physical=%d, channel=%d", frame->img.format, frame->img.width, frame->img.height, frame->img.physical, frame->chn);

    if (frame->img.format == MA_PIXEL_FORMAT_RGB888) {
        raw_image = ::cv::Mat(frame->img.height, frame->img.width, CV_8UC3, frame->img.data);
    } else if (frame->img.format == MA_PIXEL_FORMAT_YUV422) {
        // Conversion YUV422 vers BGR (directement pour OpenCV)
        ::cv::Mat yuv(frame->img.height, frame->img.width, CV_8UC2, frame->img.data);
        ::cv::cvtColor(yuv, raw_image, ::cv::COLOR_YUV2BGR_YUYV);  // Changement ici: YUV2BGR_YUYV
        MA_LOGI(TAG, "Converted YUV422 to BGR");
    } else if (frame->img.format == MA_PIXEL_FORMAT_JPEG) {
        // Décodage JPEG
        std::vector<uchar> buffer(frame->img.data, frame->img.data + frame->img.size);
        raw_image = ::cv::imdecode(buffer, ::cv::IMREAD_COLOR);
        MA_LOGI(TAG, "JPEG format Decoded size=%dx%d => %d", frame->img.width, frame->img.height, frame->img.size);
    } else if (frame->img.format == MA_PIXEL_FORMAT_H264) {
        // Le format H264 nécessite d'être décodé
        MA_LOGW(TAG, "H264 format detected - this format requires a decoder");
        // Pour H264, vous auriez besoin d'implémenter un décodeur H264
        // Ce n'est pas trivial et nécessiterait l'utilisation de FFmpeg ou d'une autre bibliothèque
        return ::cv::Mat();
    } else {
        // Format non supporté - retourne une image vide
        MA_LOGW(TAG, "Format d'image non supporté: %d", frame->img.format);
        return ::cv::Mat();
    }

    if (raw_image.empty()) {
        MA_LOGW(TAG, "Conversion résulte en une image vide");
    } else {
        MA_LOGI(TAG, "Image convertie avec succès: %dx%d", raw_image.cols, raw_image.rows);
    }

    return raw_image;
}

bool FrameUtils::prepareAndPublishOutputFrame(const ::cv::Mat& output_image, videoFrame* input_frame, MessageBox& output_frame, int output_width, int output_height) {
    // Créer une nouvelle frame pour l'image traitée
    videoFrame* output_frame_ptr = new videoFrame();
    output_frame_ptr->chn        = input_frame->chn;
    output_frame_ptr->timestamp  = input_frame->timestamp;
    output_frame_ptr->fps        = input_frame->fps;

    // Préparer l'image de sortie
    output_frame_ptr->img.width    = output_width;
    output_frame_ptr->img.height   = output_height;
    output_frame_ptr->img.format   = MA_PIXEL_FORMAT_RGB888;
    output_frame_ptr->img.size     = output_width * output_height * 3;  // RGB = 3 canaux
    output_frame_ptr->img.key      = true;
    output_frame_ptr->img.physical = false;
    output_frame_ptr->img.data     = new uint8_t[output_frame_ptr->img.size];

    // Copier les données de l'image traitée
    memcpy(output_frame_ptr->img.data, output_image.data, output_frame_ptr->img.size);

    // Ajouter le bloc de données
    output_frame_ptr->blocks.push_back({output_frame_ptr->img.data, output_frame_ptr->img.size});

    // Libérer la frame d'entrée
    input_frame->release();

    // Poster la frame traitée
    bool success = output_frame.post(output_frame_ptr, Tick::fromMilliseconds(50));
    if (!success) {
        output_frame_ptr->release();
        MA_LOGW(TAG, "Impossible de poster la frame traitée - dépassement de délai");
    }

    return success;
}
}  // namespace ma::node