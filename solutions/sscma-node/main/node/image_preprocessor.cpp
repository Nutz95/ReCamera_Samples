#include <opencv2/opencv.hpp>
#include <sys/stat.h>
#include <unistd.h>
namespace cv2 = cv;

#include "image_preprocessor.h"

namespace ma::node {

#define CURRENT_CHANNEL CHN_JPEG  // CHN_JPEG, CHN_RAW, CHN_H264
#define RES_WIDTH       1920      // 2592 // 1920
#define RES_HEIGHT      1080      // 1944 //1080
#define JPEG_QUALITY    100
#define FPS             30

static constexpr char TAG[] = "ma::node::image_preprocessor";

// Fonction utilitaire pour sauvegarder une image au format JPEG
bool saveImageToJpeg(const cv2::Mat& image, const std::string& filepath, int quality = JPEG_QUALITY, bool create_dir = true) {
    std::vector<int> im_params = {cv2::IMWRITE_JPEG_QUALITY, quality};
    if (create_dir) {
        size_t last_slash = filepath.find_last_of('/');
        if (last_slash != std::string::npos) {
            std::string dir = filepath.substr(0, last_slash);
            struct stat st;
            if (stat(dir.c_str(), &st) != 0) {
                if (mkdir(dir.c_str(), 0777) != 0) {
                    MA_LOGE(TAG, "Failed to create directory: %s", dir.c_str());
                    return false;
                }
            }
        }
    }
    return cv2::imwrite(filepath, image, im_params);
}

// Fonction pour redimensionner une image à la taille cible avec des bandes noires
cv2::Mat resizeImage(const cv2::Mat& input_image, int target_width, int target_height) {
    // Calculer le ratio pour le redimensionnement
    float scale_width  = static_cast<float>(target_width) / input_image.cols;
    float scale_height = static_cast<float>(target_height) / input_image.rows;
    float scale        = std::min(scale_width, scale_height);

    // Calculer les nouvelles dimensions
    int new_width  = static_cast<int>(input_image.cols * scale);
    int new_height = static_cast<int>(input_image.rows * scale);

    // Créer une image noire de taille cible
    cv2::Mat output_image(target_height, target_width, CV_8UC3, cv2::Scalar(0, 0, 0));

    // Redimensionner l'image originale
    cv2::Mat resized_image;
    cv2::resize(input_image, resized_image, cv2::Size(new_width, new_height));

    // Calculer les offsets pour centrer l'image redimensionnée
    int offset_x = (target_width - new_width) / 2;
    int offset_y = (target_height - new_height) / 2;

    // Créer une ROI (Region Of Interest) sur l'image de sortie
    cv2::Mat roi = output_image(cv2::Rect(offset_x, offset_y, new_width, new_height));

    // Copier l'image redimensionnée dans la ROI
    resized_image.copyTo(roi);

    return output_image;
}

// Fonction pour gérer la sauvegarde des images selon les règles demandées
void saveProcessedImages(const cv2::Mat& input_image, const cv2::Mat& output_image, int& saved_count, ma_tick_t processing_time, ma_tick_t& last_debug) {
    double processing_time_ms = Tick::toMilliseconds(processing_time);

    // Obtenir le timestamp courant au format demandé (YYYY_MM_DD_HH_MM_SS_MS)
    time_t now = time(nullptr);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y_%m_%d_%H_%M_%S", &tm_info);

    // Ajouter les millisecondes
    struct timeval tv;
    gettimeofday(&tv, NULL);
    char timestamp_ms[80];
    snprintf(timestamp_ms, sizeof(timestamp_ms), "%s_%03ld", timestamp, tv.tv_usec / 1000);

    // Créer le dossier si nécessaire
    std::string output_dir = "/userdata/IMAGES/";
    struct stat st;
    if (stat(output_dir.c_str(), &st) != 0) {
        mkdir(output_dir.c_str(), 0777);
    }

    // Nom des fichiers avec préfixe et timestamp
    std::string input_filename  = output_dir + timestamp_ms + "_raw" + ".jpg";
    std::string output_filename = output_dir + timestamp_ms + ".jpg";

    // Sauvegarder les images
    bool input_saved  = saveImageToJpeg(input_image, input_filename, JPEG_QUALITY, false);
    bool output_saved = saveImageToJpeg(output_image, output_filename, JPEG_QUALITY, false);

    MA_LOGI(TAG,
            "Images sauvegardées - input: %s (%s), output: %s (%s) (processing time: %.2f ms)",
            input_filename.c_str(),
            input_saved ? "OK" : "ÉCHEC",
            output_filename.c_str(),
            output_saved ? "OK" : "ÉCHEC",
            processing_time_ms);

    saved_count++;
}

// Fonction pour convertir la frame d'entrée en Mat OpenCV
cv2::Mat convertFrameToMat(videoFrame* frame) {
    cv2::Mat raw_image;

    // Ajouter un log pour voir quel format d'image est reçu
    MA_LOGI(TAG, "Converting frame: format=%d, size=%dx%d, physical=%d", frame->img.format, frame->img.width, frame->img.height, frame->img.physical);

    if (frame->img.physical) {
        // Si l'image est en mémoire physique, nous devons d'abord la copier
        // Note: la gestion de la mémoire physique peut nécessiter une adaptation selon votre système
        raw_image = cv2::Mat(frame->img.height, frame->img.width, CV_8UC3);
        memcpy(raw_image.data, frame->img.data, frame->img.size);
        MA_LOGI(TAG, "Copied physical memory image to CPU memory");
    } else {
        // Si l'image est déjà en mémoire CPU
        if (frame->img.format == MA_PIXEL_FORMAT_RGB888) {
            raw_image = cv2::Mat(frame->img.height, frame->img.width, CV_8UC3, frame->img.data);
            MA_LOGI(TAG, "Created Mat from RGB888 frame");
        } else if (frame->img.format == MA_PIXEL_FORMAT_YUV422) {
            // Conversion YUV422 vers RGB
            cv2::Mat yuv(frame->img.height, frame->img.width, CV_8UC2, frame->img.data);
            cv2::cvtColor(yuv, raw_image, cv2::COLOR_YUV2RGB_YUYV);
            MA_LOGI(TAG, "Converted YUV422 to RGB");
        } else if (frame->img.format == MA_PIXEL_FORMAT_JPEG) {
            // Décodage JPEG
            std::vector<uchar> buffer(frame->img.data, frame->img.data + frame->img.size);
            raw_image = cv2::imdecode(buffer, cv2::IMREAD_COLOR);
            MA_LOGI(TAG, "JPEG format Decoded size=%dx%d => %d", frame->img.width, frame->img.height, frame->img.size);
        } else if (frame->img.format == MA_PIXEL_FORMAT_H264) {
            // Le format H264 nécessite d'être décodé
            MA_LOGW(TAG, "H264 format detected - this format requires a decoder");
            // Pour H264, vous auriez besoin d'implémenter un décodeur H264
            // Ce n'est pas trivial et nécessiterait l'utilisation de FFmpeg ou d'une autre bibliothèque
            return cv2::Mat();
        } else {
            // Format non supporté - retourne une image vide
            MA_LOGW(TAG, "Format d'image non supporté: %d", frame->img.format);
            return cv2::Mat();
        }
    }

    if (raw_image.empty()) {
        MA_LOGW(TAG, "Conversion résulte en une image vide");
    } else {
        MA_LOGI(TAG, "Image convertie avec succès: %dx%d", raw_image.cols, raw_image.rows);
    }

    return raw_image;
}

// Fonction pour créer et publier une frame de sortie à partir d'une Mat OpenCV
bool prepareAndPublishOutputFrame(const cv2::Mat& output_image, videoFrame* input_frame, MessageBox& output_frame, int output_width, int output_height) {
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

ImagePreProcessorNode::ImagePreProcessorNode(std::string id)
    : Node("image_preprocessor", std::move(id)),
      output_width_(640),
      output_height_(640),
      enabled_(true),
      thread_(nullptr),
      camera_(nullptr),
      input_frame_(10),
      output_frame_(60),
      debug_(false),
      saved_image_count_(0),
      capture_requested_(false) {}

ImagePreProcessorNode::~ImagePreProcessorNode() {
    onDestroy();
}

void ImagePreProcessorNode::threadEntry() {
    MA_LOGI(TAG, "ImagePreProcessorNode.threadEntry: started for node %s", id_.c_str());
    videoFrame* frame    = nullptr;
    ma_tick_t last_debug = Tick::current();

    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));

    MA_LOGI(TAG, "ImagePreProcessorNode.threadEntry: started_ = %s", started_ ? "true" : "false");

    while (started_) {
        // MA_LOGI(TAG, "ImagePreProcessorNode.threadEntry: waiting for frame");
        // Attendre une image d'entrée avec timeout de 1 seconde
        if (!input_frame_.fetch(reinterpret_cast<void**>(&frame), Tick::fromSeconds(1))) {
            MA_LOGI(TAG, "No frame received for 1 sec (timeout)");
            continue;
        }

        if (!enabled_) {
            MA_LOGI(TAG, "ImagePreProcessorNode.threadEntry: frame release because not enabled");
            frame->release();
            continue;
        }

        Thread::enterCritical();

        // Vérifier si l'utilisateur a demandé une capture d'image
        bool should_process = capture_requested_.load();

        if (should_process) {
            MA_LOGI(TAG, "ImagePreProcessorNode.threadEntry: User requested capture, processing frame");
            // Réinitialiser le flag après avoir détecté la demande
            capture_requested_.store(false);

            ma_tick_t start_time = Tick::current();  // Mesure du temps de traitement
            // MA_LOGI(TAG, "ImagePreProcessorNode.threadEntry: Processing frame with timestamp %lld", frame->timestamp);

            // Convertir la frame d'entrée en Mat OpenCV
            cv2::Mat raw_image = convertFrameToMat(frame);

            // Vérifier si la conversion a réussi
            if (raw_image.empty()) {
                MA_LOGW(TAG, "Échec de conversion de la frame en Mat OpenCV");
                frame->release();
                Thread::exitCritical();
                continue;
            }

            // Utiliser la fonction de redimensionnement pour traiter l'image
            cv2::Mat output_image = resizeImage(raw_image, output_width_, output_height_);

            // Calculer le temps de traitement
            ma_tick_t processing_time = Tick::current() - start_time;

            // Sauvegarder les images selon les nouvelles règles demandées
            saveProcessedImages(raw_image, output_image, saved_image_count_, processing_time, last_debug);

            // Préparer et publier la frame de sortie
            prepareAndPublishOutputFrame(output_image, frame, output_frame_, output_width_, output_height_);
        } else {
            // Si aucune capture n'est demandée, libérer l'image immédiatement
            // MA_LOGI(TAG, "ImagePreProcessorNode.threadEntry: No capture requested, releasing frame");
            frame->release();
        }

        Thread::exitCritical();
    }
}

void ImagePreProcessorNode::threadEntryStub(void* obj) {
    reinterpret_cast<ImagePreProcessorNode*>(obj)->threadEntry();
}

ma_err_t ImagePreProcessorNode::onCreate(const json& config) {
    Guard guard(mutex_);

    // Configuration de la résolution de sortie
    if (config.contains("width") && config["width"].is_number_integer()) {
        output_width_ = config["width"].get<int32_t>();
    }

    if (config.contains("height") && config["height"].is_number_integer()) {
        output_height_ = config["height"].get<int32_t>();
    }

    // Configuration du mode debug
    if (config.contains("debug") && config["debug"].is_boolean()) {
        debug_ = config["debug"].get<bool>();
    }

    // Création du thread de traitement
    thread_ = new Thread((type_ + "#" + id_).c_str(), &ImagePreProcessorNode::threadEntryStub, this);
    if (thread_ == nullptr) {
        MA_THROW(Exception(MA_ENOMEM, "Pas assez de mémoire"));
    }

    created_ = true;

    // Envoi d'une réponse indiquant que le nœud a été créé avec succès
    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", MA_OK}, {"data", {{"width", output_width_}, {"height", output_height_}}}}));

    // --- TRACE ---
    MA_LOGI(TAG, "ImagePreProcessorNode.onCreate: width=%d height=%d debug=%s", output_width_, output_height_, debug_ ? "true" : "false");

    return MA_OK;
}

ma_err_t ImagePreProcessorNode::onControl(const std::string& control, const json& data) {
    Guard guard(mutex_);

    if (control == "enabled" && data.is_boolean()) {
        bool enabled = data.get<bool>();
        MA_LOGI(TAG, "onControl: enabled=%s (previous=%s)", enabled ? "true" : "false", enabled_.load() ? "true" : "false");

        if (enabled_.load() != enabled) {
            enabled_.store(enabled);

            // Démarrer ou arrêter le nœud selon la valeur de enabled
            if (enabled) {
                MA_LOGI(TAG, "onControl: calling onStart() due to enabled=true");
                onStart();
            } else if (started_) {
                MA_LOGI(TAG, "onControl: calling onStop() due to enabled=false");
                onStop();
            }
        }
        json response = json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", enabled_.load()}});
        server_->response(id_, response);
    } else if (control == "resize" && data.is_object()) {
        if (data.contains("width") && data["width"].is_number_integer()) {
            output_width_ = data["width"].get<int32_t>();
        }
        if (data.contains("height") && data["height"].is_number_integer()) {
            output_height_ = data["height"].get<int32_t>();
        }
        json response = json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", json::object({{"width", output_width_}, {"height", output_height_}})}});
        server_->response(id_, response);
    } else if (control == "debug" && data.is_boolean()) {
        debug_        = data.get<bool>();
        json response = json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", debug_}});
        server_->response(id_, response);
    } else {
        json response = json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_ENOTSUP}, {"data", "Non supporté"}});
        server_->response(id_, response);
    }
    return MA_OK;
}

ma_err_t ImagePreProcessorNode::onDestroy() {
    Guard guard(mutex_);

    if (!created_) {
        return MA_OK;
    }

    // Arrêter le noeud s'il est en cours d'exécution
    onStop();

    // Nettoyer le thread
    if (thread_ != nullptr) {
        delete thread_;
        thread_ = nullptr;
    }

    // Détacher de la caméra si elle est connectée
    if (camera_ != nullptr) {
        camera_->detach(CURRENT_CHANNEL, &input_frame_);  // Utiliser CHN_JPEG (1) au lieu de CHN_RAW
    }

    created_ = false;

    return MA_OK;
}

ma_err_t ImagePreProcessorNode::onStart() {
    Guard guard(mutex_);

    if (started_) {
        return MA_OK;
    }

    // --- TRACE ---
    MA_LOGI(TAG, "ImagePreProcessorNode.onStart: attaching to camera node %s", id_.c_str());

    // Chercher le noeud caméra dans les dépendances
    for (auto& dep : dependencies_) {
        if (dep.second->type() == "camera") {
            camera_ = static_cast<CameraNode*>(dep.second);
            break;
        }
    }

    if (camera_ == nullptr) {
        MA_THROW(Exception(MA_ENOTSUP, "Noeud caméra non trouvé"));
        return MA_ENOTSUP;
    }

    // MODIFICATION: Essayer d'utiliser le canal JPEG (1) au lieu de RAW (0)
    if (CURRENT_CHANNEL == CHN_JPEG) {
        MA_LOGI(TAG, "ImagePreProcessorNode.onStart: Configuring camera channel CHN_JPEG=%d", CHN_JPEG);
        camera_->config(CURRENT_CHANNEL, RES_WIDTH, RES_HEIGHT, FPS, MA_PIXEL_FORMAT_JPEG);  // Canal 1 (JPEG)}
    } else if (CURRENT_CHANNEL == CHN_RAW) {
        MA_LOGI(TAG, "ImagePreProcessorNode.onStart: Configuring camera channel CHN_RAW=%d", CHN_RAW);
        camera_->config(CURRENT_CHANNEL, RES_WIDTH, RES_HEIGHT, FPS, MA_PIXEL_FORMAT_RGB888);  // Canal 0 (RAW)
    } else {
        MA_THROW(Exception(MA_ENOTSUP, "Canal non supporté"));
        return MA_ENOTSUP;
    }
    camera_->attach(CURRENT_CHANNEL, &input_frame_);  // Attachement au canal 1 (JPEG)

    MA_LOGI(TAG, "ImagePreProcessorNode.onStart: camera attached to channel %d, starting thread", CURRENT_CHANNEL);
    started_ = true;

    // Démarrer le thread de traitement
    thread_->start(this);

    MA_LOGI(TAG, "ImagePreProcessorNode.onStart: camera attached to channel %d, thread started", CURRENT_CHANNEL);
    return MA_OK;
}

ma_err_t ImagePreProcessorNode::onStop() {
    Guard guard(mutex_);

    if (!started_) {
        return MA_OK;
    }

    started_ = false;

    // Attendre la fin du thread
    if (thread_ != nullptr) {
        thread_->join();
    }

    // Détacher de la caméra
    if (camera_ != nullptr) {
        camera_->detach(CURRENT_CHANNEL, &input_frame_);  // Détachement du canal JPEG (1) au lieu de RAW
        camera_ = nullptr;
    }

    return MA_OK;
}

// Enregistrer le noeud dans la fabrique
REGISTER_NODE("image_preprocessor", ImagePreProcessorNode);

}  // namespace ma::node