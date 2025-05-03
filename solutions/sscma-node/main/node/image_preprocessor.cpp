#include <fstream>
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>  // Include for gettimeofday
#include <unistd.h>

#include "FlowConfigReader.h"
#include "ai_config.h"
#include "crop_config.h"
#include "flash_config.h"
#include "frame_utils.h"
#include "image_preprocessor.h"
#include "image_utils.h"
#include "label_mapper.h"
#include "led.h"
#include "preprocessor_config.h"  // Nouveau fichier d'en-tête
#include "profiler.h"
#include "white_balance_config.h"  // Ajout de l'include pour la nouvelle classe

namespace ma::node {

// Suppression ou remplacement de la définition de CURRENT_CHANNEL
#define RES_WIDTH  1920  // Configuration pour résolution 1080p
#define RES_HEIGHT 1080  // Configuration pour résolution 1080p
#define FPS        10    // FPS standard pour 1080p

static constexpr char TAG[] = "ma::node::image_preprocessor";


// Fonction pour gérer la sauvegarde des images selon les règles demandées
void saveProcessedImages(const ::cv::Mat& input_image,
                         const ::cv::Mat& output_image,
                         int& saved_count,
                         ma_tick_t processing_time,
                         ma_tick_t& last_debug,
                         std::string file_extension = ".jpg",
                         std::string tube_type      = "OTHER",
                         bool save_resized          = false,
                         bool save_raw              = false) {
    double processing_time_ms = Tick::toMilliseconds(processing_time);

    // Obtenir le timestamp courant au format demandé (YYYY_MM_DD_HH_MM_SS_MS)
    time_t now = time(nullptr);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y_%m_%d_%H_%M_%S", &tm_info);

    // Ajouter les millisecondes
    struct timeval tv;
    ::gettimeofday(&tv, NULL);  // Use global namespace
    char timestamp_ms[80];
    snprintf(timestamp_ms, sizeof(timestamp_ms), "%s_%03ld", timestamp, tv.tv_usec / 1000);

    // Créer le dossier si nécessaire
    std::string output_dir = "/userdata/IMAGES/" + tube_type + "/";
    struct stat st;
    if (stat(output_dir.c_str(), &st) != 0) {
        mkdir(output_dir.c_str(), 0777);
    }

    // Nom des fichiers avec préfixe et timestamp
    std::string raw_filename    = output_dir + timestamp_ms + "_raw" + file_extension;
    std::string output_filename = output_dir + timestamp_ms + file_extension;
    if (file_extension == ".bmp") {
        // Utilisation de la nouvelle classe WhiteBalanceConfig pour lire les paramètres
        // On n'a plus besoin de passer l'ID du nœud car les paramètres sont au niveau racine
        ma::WhiteBalanceConfig wbConfig = ma::readWhiteBalanceConfigFromFile();
        float red_balance_factor        = wbConfig.red_balance_factor;
        float green_balance_factor      = wbConfig.green_balance_factor;
        float blue_balance_factor       = wbConfig.blue_balance_factor;

        if (save_raw) {
            bool input_saved = ImageUtils::saveImageToBmp(input_image, raw_filename, red_balance_factor, green_balance_factor, blue_balance_factor);
            MA_LOGI(TAG, "Image brute sauvegardée: %s (%s) (Mapping de l'image en mémoire: %.2f ms)", raw_filename.c_str(), input_saved ? "OK" : "ÉCHEC", Tick::toMilliseconds(processing_time));
        }

        if (!output_image.empty() && save_resized) {
            ImageUtils::saveImageToBmp(output_image, output_filename, red_balance_factor, green_balance_factor, blue_balance_factor);
            MA_LOGI(TAG, "Applied white balance with red factor: %.2f, green factor: %.2f, blue factor: %.2f", red_balance_factor, green_balance_factor, blue_balance_factor);
        }

    } else {
        bool raw_saved    = false;
        bool output_saved = false;
        if (save_raw) {
            raw_saved = ImageUtils::saveImageToJpeg(input_image, raw_filename, JPEG_QUALITY, false);
        }
        if (!output_image.empty() && save_resized) {
            output_saved = ImageUtils::saveImageToJpeg(output_image, output_filename, JPEG_QUALITY, false);
        }

        MA_LOGI(TAG,
                "Images sauvegardées - input: %s (%s), output: %s (%s) (processing time: %.2f ms)",
                raw_filename.c_str(),
                raw_saved ? "OK" : "ÉCHEC",
                output_filename.c_str(),
                output_saved ? "OK" : "ÉCHEC",
                processing_time_ms);
    }

    saved_count++;
}

// Fonction pour créer et publier une frame de sortie à partir d'une Mat OpenCV
ImagePreProcessorNode::ImagePreProcessorNode(std::string id)
    : Node("image_preprocessor", id),  // Pass type and id to base constructor
      output_width_(640),
      output_height_(640),
      enabled_(false),
      thread_(nullptr),
      camera_(nullptr),
      debug_(false),
      saved_image_count_(0),
      capture_requested_(false),
      tube_type_("OTHER"),
      flash_active_(false),
      flash_intensity_(-1),
      enable_resize_(true),
      enable_denoising_(false),
      flash_duration_ms_(200),
      pre_capture_delay_ms_(100),
      disable_red_led_blinking_(false),
      channel_(0),
      ai_processor_(nullptr),
      ai_model_path_(""),
      enable_ai_detection_(false) {
    // Initialisation des paramètres de la caméra
    // camera_->config(...);
}

ImagePreProcessorNode::~ImagePreProcessorNode() {
    onDestroy();

    // Nettoyer le processeur IA
    if (ai_processor_ != nullptr) {
        delete ai_processor_;
        ai_processor_ = nullptr;
    }
    // Nettoyer le label_mapper_
    if (label_mapper_ != nullptr) {
        delete label_mapper_;
        label_mapper_ = nullptr;
    }
}

void ImagePreProcessorNode::threadEntry() {
    MA_LOGI(TAG, "ImagePreProcessorNode.threadEntry: started for node %s", id_.c_str());
    videoFrame* frame    = nullptr;
    ma_tick_t last_debug = Tick::current();

    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));

    MA_LOGI(TAG, "ImagePreProcessorNode.threadEntry: started_ = %s", started_ ? "true" : "false");

    while (started_) {
        if (!fetchAndValidateFrame(frame)) {
            continue;
        }

        bool should_process = capture_requested_.load();

        if (should_process) {
            processCaptureRequest(frame, last_debug, tube_type_);
        } else {
            handleNoCaptureRequested(frame);
        }
        thread_->sleep(Tick::fromMilliseconds(2));
    };
}

// Nouvelle méthode privée : fetchAndValidateFrame
bool ImagePreProcessorNode::fetchAndValidateFrame(videoFrame*& frame) {
    // Attendre une image d'entrée avec timeout de 1 seconde
    // Profiler p("fetchAndValidateFrame");
    if (!input_frame_.fetch(reinterpret_cast<void**>(&frame), Tick::fromMicroseconds(500))) {
        // MA_LOGI(TAG, "No frame received for 1 sec (timeout)");
        return false;
    }
    if (!enabled_) {
        // MA_LOGI(TAG, "ImagePreProcessorNode.threadEntry: frame release because not enabled");
        frame->release();
        return false;
    }
    return true;
}

// Nouvelle méthode privée : processCaptureRequest
void ImagePreProcessorNode::processCaptureRequest(videoFrame* frame, ma_tick_t& last_debug, std::string tubeType) {
    Profiler p("processCaptureRequest");
    MA_LOGI(TAG, "ImagePreProcessorNode.threadEntry: User requested capture into %s, processing frame", tubeType.c_str());
    capture_requested_.store(false);
    ma_tick_t start_time = Tick::current();

    Thread::enterCritical();
    ::cv::Mat raw_image = FrameUtils::convertFrameToMat(frame);
    Thread::exitCritical();

    Led::controlLed("white", false);

    if (raw_image.empty()) {
        MA_LOGW(TAG, "Échec de conversion de la frame en Mat OpenCV");
        frame->release();
        return;
    }

    // Charger la configuration du préprocesseur depuis le fichier flow.json
    ma::PreprocessorConfig preprocessorCfg = ma::readPreprocessorConfigFromFile(id_);

    // Utiliser les paramètres de configuration lus du fichier
    bool save_raw     = preprocessorCfg.save_raw;
    enable_resize_    = preprocessorCfg.enable_resize;
    enable_denoising_ = preprocessorCfg.enable_denoising;
    debug_            = preprocessorCfg.debug;

    MA_LOGI(TAG, "Configuration chargée: save_raw=%s, enable_resize=%s, enable_denoising=%s", save_raw ? "true" : "false", enable_resize_ ? "true" : "false", enable_denoising_ ? "true" : "false");

    if (enable_denoising_) {
        MA_LOGI(TAG, "Denoising enabled - Application du filtre de débruitage");
        raw_image = ImageUtils::denoiseImage(raw_image);
    } else {
        MA_LOGI(TAG, "Denoising disabled - Utilisation de l'image brute");
    }

    // Lecture dynamique des paramètres de crop
    ma::CropConfig cropCfg = ma::readCropConfigFromFile();
    if (cropCfg.enabled) {
        MA_LOGI(TAG, "Cropping enabled - Region [%d,%d] à [%d,%d]", cropCfg.xmin, cropCfg.ymin, cropCfg.xmax, cropCfg.ymax);
        raw_image = ImageUtils::cropImage(raw_image, cropCfg.xmin, cropCfg.ymin, cropCfg.xmax, cropCfg.ymax);
    }

    if (preprocessorCfg.enable_ccw_rotation) {
        MA_LOGI(TAG, "Rotation CCW enabled - Rotating image 90° counter clockwise");
        raw_image = ImageUtils::rotate90CCW(raw_image);
    }

    ::cv::Mat output_image;
    if (enable_resize_) {
        MA_LOGI(TAG, "Resizing enabled - applying resizing to %dx%d", output_width_, output_height_);
        output_image = ImageUtils::resizeImage(raw_image, output_width_, output_height_);
    } else {
        MA_LOGI(TAG, "Resizing disabled");
        output_image = raw_image.clone();
    }

    MA_LOGI(TAG, "Checking to perform AI detection: %s, model loaded: %s", enable_ai_detection_ ? "true" : "false", ai_processor_->isModelLoaded() ? "true" : "false");
    // Exécuter la détection IA si elle est activée
    if (enable_ai_detection_ && ai_processor_ != nullptr && ai_processor_->isModelLoaded() && output_image.empty() == false) {
        performAIDetection(output_image);
    }

    ma_tick_t processing_time = Tick::current() - start_time;

    if (enable_resize_ || debug_) {
        // Passer le paramètre save_raw à la fonction saveProcessedImages
        saveProcessedImages(raw_image, output_image, saved_image_count_, processing_time, last_debug, save_raw ? ".bmp" : ".jpg", tubeType.c_str(), enable_resize_, debug_);
        if (!output_image.empty()) {
            FrameUtils::prepareAndPublishOutputFrame(output_image, frame, output_frame_, output_width_, output_height_);
        } else {
            frame->release();
            return;
        }
    } else {
        MA_LOGI(TAG, "Redimensionnement désactivé - Pas d'image de sortie");
        std::string output_dir = "/userdata/IMAGES/";
        struct stat st;
        if (stat(output_dir.c_str(), &st) != 0) {
            mkdir(output_dir.c_str(), 0777);
        }
        time_t now = time(nullptr);
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y_%m_%d_%H_%M_%S", &tm_info);
        struct timeval tv;
        ::gettimeofday(&tv, NULL);
        char timestamp_ms[80];
        snprintf(timestamp_ms, sizeof(timestamp_ms), "%s_%03ld", timestamp, tv.tv_usec / 1000);
        std::string input_filename = output_dir + timestamp_ms + "_raw" + ".bmp";


        // Ne sauvegarder l'image brute que si save_raw est activé
        if (save_raw) {
            ma::WhiteBalanceConfig wbConfig = ma::readWhiteBalanceConfigFromFile();
            float red_balance_factor        = wbConfig.red_balance_factor;
            float green_balance_factor      = wbConfig.green_balance_factor;
            float blue_balance_factor       = wbConfig.blue_balance_factor;

            MA_LOGI(TAG, "Applied white balance with red factor: %.2f, green factor: %.2f, blue factor: %.2f", red_balance_factor, green_balance_factor, blue_balance_factor);
            bool input_saved = ImageUtils::saveImageToBmp(raw_image, input_filename, red_balance_factor, green_balance_factor, blue_balance_factor);
            MA_LOGI(TAG, "Image brute sauvegardée: %s (%s) (Mapping de l'image en mémoire: %.2f ms)", input_filename.c_str(), input_saved ? "OK" : "ÉCHEC", Tick::toMilliseconds(processing_time));
        } else {
            MA_LOGI(TAG, "Sauvegarde de l'image brute désactivée (save_raw=false)");
        }
        frame->release();
    }
    saved_image_count_++;
    Led::flashLed("blue", flash_intensity_, 20);
}

// Nouvelle méthode privée : handleNoCaptureRequested
void ImagePreProcessorNode::handleNoCaptureRequested(videoFrame* frame) {
    frame->release();
}

void ImagePreProcessorNode::threadEntryStub(void* obj) {
    reinterpret_cast<ImagePreProcessorNode*>(obj)->threadEntry();
}

ma_err_t ImagePreProcessorNode::onCreate(const json& config) {
    Guard guard(mutex_);

    PreprocessorConfig preprocessorCfg = ma::readPreprocessorConfigFromFile(id_);
    FlashConfig flashCfg               = ma::readFlashConfigFromFile();

    output_width_  = preprocessorCfg.width;
    output_height_ = preprocessorCfg.height;
    debug_         = preprocessorCfg.debug;

    flash_intensity_          = flashCfg.flash_intensity;
    flash_duration_ms_        = flashCfg.flash_duration_ms;
    pre_capture_delay_ms_     = flashCfg.pre_capture_delay_ms;
    disable_red_led_blinking_ = flashCfg.disable_red_led_blinking;

    AIConfig aiCfg          = ma::readAIConfigFromFile();
    enable_ai_detection_    = aiCfg.enabled;
    ai_model_path_          = aiCfg.model_path;
    ai_model_labels_path_   = aiCfg.model_labels_path;
    ai_detection_threshold_ = aiCfg.threshold;

    if (enable_ai_detection_ && !ai_model_path_.empty()) {
        // Initialiser le processeur IA
        if (ai_processor_ == nullptr) {
            label_mapper_ = new LabelMapper(ai_model_labels_path_.c_str());
            ai_processor_ = new AIModelProcessor(label_mapper_);
            MA_LOGI(TAG, "AI processor created sucessfuly");
        }

        if (ai_processor_) {
            // Initialiser le moteur et charger le modèle
            ma_err_t ret = ai_processor_->initEngine();
            if (ret == MA_OK) {
                ret = ai_processor_->loadModel(ai_model_path_);
                if (ret != MA_OK) {
                    MA_LOGE(TAG, "Error when loading AI model: %s", ai_model_path_.c_str());
                    delete ai_processor_;
                    ai_processor_        = nullptr;
                    enable_ai_detection_ = false;
                } else {
                    // Configurer le seuil de détection si spécifié
                    ai_processor_->setDetectionThreshold(ai_detection_threshold_);
                    MA_LOGI(TAG, "Configured detection Threashold: %.2f", ai_detection_threshold_);
                }
            } else {
                MA_LOGE(TAG, "Error initializing AI engine");
                delete ai_processor_;
                ai_processor_        = nullptr;
                enable_ai_detection_ = false;
            }
        }
    }

    // Lire le canal à utiliser depuis la configuration (raw ou jpeg)
    // Par défaut, utiliser JPEG (canal 1)
    channel_ = CHN_JPEG;  // Par défaut: CHN_JPEG (1)

    if (config.contains("attach_channel") && config["attach_channel"].is_string()) {
        std::string channel_str = config["attach_channel"].get<std::string>();
        if (channel_str == "raw") {
            channel_ = CHN_RAW;  // CHN_RAW (0)
            MA_LOGI(TAG, "Canal configuré: RAW (0)");
        } else if (channel_str == "jpeg" || channel_str == "jpg") {
            channel_ = CHN_JPEG;  // CHN_JPEG (1)
            MA_LOGI(TAG, "Canal configuré: JPEG (1)");
        } else if (channel_str == "h264") {
            channel_ = CHN_H264;  // CHN_H264 (2)
            MA_LOGI(TAG, "Canal configuré: H264 (2)");
        } else {
            MA_LOGW(TAG, "Canal inconnu '%s', utilisation du canal JPEG (1) par défaut", channel_str.c_str());
        }
    } else {
        MA_LOGW(TAG, "attach_channel non spécifié dans la configuration, utilisation du canal JPEG (1) par défaut");
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
    MA_LOGI(TAG,
            "ImagePreProcessorNode.onCreate: width=%d height=%d debug=%s channel=%d ai_enabled=%s",
            output_width_,
            output_height_,
            debug_ ? "true" : "false",
            channel_,
            enable_ai_detection_ ? "true" : "false");

    return MA_OK;
}

ma_err_t ImagePreProcessorNode::onControl(const std::string& control, const json& data) {
    Guard guard(mutex_);
    json response;  // Declare response variable

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
    }
    // Nouvelle commande pour activer/désactiver le contrôle global des LEDs
    else if (control == "lights" && data.is_boolean()) {
        // This control is deprecated as LED control is now static
        MA_LOGW(NODE_TAG, "Control 'lights' is deprecated.");
        response = json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_ENOTSUP}, {"data", "Control 'lights' is deprecated"}});  // Use MA_ENOTSUP
        server_->response(id_, response);                                                                                                            // Send the response
    } else if (control == "capture") {
        // Cette commande est utilisée pour demander la capture d'une image
        // Elle peut être appelée par un client pour déclencher la capture
        // d'image à distance
        if (data.is_boolean()) {
            capture_requested_.store(data.get<bool>());
            json response = json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", capture_requested_.load()}});
            server_->response(id_, response);
        } else {
            json response = json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_EINVAL}, {"data", "Invalid data for capture"}});
            server_->response(id_, response);
        }
    } else {
        // Commande non reconnue
        json response = json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_ENOTSUP}, {"data", "Commande non reconnue"}});
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
        camera_->detach(channel_, &input_frame_);  // Utiliser le canal configuré
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

    // Utiliser le canal configuré dans flow.json (channel_) au lieu de CURRENT_CHANNEL
    if (channel_ == CHN_JPEG) {
        MA_LOGI(TAG, "ImagePreProcessorNode.onStart: Configuring camera channel CHN_JPEG=%d", CHN_JPEG);
        camera_->config(channel_, RES_WIDTH, RES_HEIGHT, FPS, MA_PIXEL_FORMAT_JPEG);  // Canal 1 (JPEG)
    } else if (channel_ == CHN_RAW) {
        MA_LOGI(TAG, "ImagePreProcessorNode.onStart: Configuring camera channel CHN_RAW=%d", CHN_RAW);
        camera_->config(channel_, RES_WIDTH, RES_HEIGHT, FPS, MA_PIXEL_FORMAT_RGB888);  // Canal 0 (RAW)
    } else if (channel_ == CHN_H264) {
        MA_LOGI(TAG, "ImagePreProcessorNode.onStart: Configuring camera channel CHN_H264=%d", CHN_H264);
        camera_->config(channel_, RES_WIDTH, RES_HEIGHT, FPS, MA_PIXEL_FORMAT_H264);  // Canal 2 (H264)
    } else {
        MA_THROW(Exception(MA_ENOTSUP, "Canal non supporté"));
        return MA_ENOTSUP;
    }
    camera_->attach(channel_, &input_frame_);  // Attachement au canal configuré

    MA_LOGI(TAG, "ImagePreProcessorNode.onStart: camera attached to channel %d, starting thread", channel_);
    started_ = true;

    // Démarrer le thread de traitement
    thread_->start(this);

    MA_LOGI(TAG, "ImagePreProcessorNode.onStart: camera attached to channel %d, thread started", channel_);
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
        camera_->detach(channel_, &input_frame_);  // Utiliser le canal configuré
        camera_ = nullptr;
    }

    return MA_OK;
}

// Méthode pour la détection IA
void ImagePreProcessorNode::performAIDetection(::cv::Mat& output_image) {
    Profiler p("ImagePreProcessorNode: performAIDetection");
    MA_LOGI(TAG, "AI detection enabled. Performing detection...");

    // Convertir l'image en RGB si elle est en BGR (OpenCV utilise BGR par défaut)
    ::cv::Mat rgb_image;

    rgb_image = output_image.clone();
    //}

    MA_LOGI(TAG, "Running AI detection on image of size: %dx%d", rgb_image.cols, rgb_image.rows);
    // Exécuter la détection d'objets
    ma_err_t ret = ai_processor_->runDetection(rgb_image);
    if (ret == MA_OK) {
        MA_LOGI(TAG, "AI detection completed successfully");
        // Dessiner les résultats de détection sur l'image
        ai_processor_->drawDetectionResults(output_image);

        // Afficher les statistiques de performance
        ma_perf_t perf = ai_processor_->getPerformanceStats();
        MA_LOGI(TAG, "IA Performance: prétraitement=%ldms, inférence=%ldms, post-traitement=%ldms", perf.preprocess, perf.inference, perf.postprocess);

        // Afficher les résultats de détection
        auto results = ai_processor_->getDetectionResults();
        MA_LOGI(TAG, "IA Detections: %zu objets detected", results.size());
        for (const auto& result : results) {
            std::string label = label_mapper_ ? label_mapper_->getLabel(result.target) : std::to_string(result.target);
            MA_LOGI(TAG, "  - Objet: classe=%d (%s), score=%.3f, position=[%.2f, %.2f, %.2f, %.2f]", result.target, label.c_str(), result.score, result.x, result.y, result.w, result.h);
        }
    } else {
        MA_LOGE(TAG, "Error when performing detection: code=%d", ret);
    }
}

// Enregistrer le noeud dans la fabrique
REGISTER_NODE("image_preprocessor", ImagePreProcessorNode);

}  // namespace ma::node