#include <fstream>
#include <iostream>
#include <string>
#include <syslog.h>
#include <unistd.h>

#include <sscma.h>

#include <video.h>

#include "version.h"

#include "signal.h"

#include "flash_config.h"
#include "node/image_preprocessor.h"
#include "node/led.h"
#include "node/server.h"

// Inclure les en-têtes nécessaires pour la réinitialisation VPSS
extern "C" {
#include <cvi_sys.h>
#include <cvi_vb.h>
#include <cvi_vi.h>
#include <cvi_vpss.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
}

const std::string TAG = "sscma";

using namespace ma;
using namespace ma::node;

// Ajouter une fonction pour nettoyer et réinitialiser les ressources système
void resetSystemResources() {
    MA_LOGI(TAG, "Réinitialisation des ressources système...");

    for (int i = 0; i < 64; i++) {        // Supposant un maximum de 64 groupes VPSS
        for (int ch = 0; ch < 4; ch++) {  // Désactiver tous les canaux (0 à 3)
            CVI_VPSS_DisableChn(i, ch);
        }
        CVI_VPSS_StopGrp(i);
        CVI_VPSS_DestroyGrp(i);
    }

    for (int i = 0; i < 8; i++) {  // Supposant un maximum de 8 pipes VI
        CVI_VI_StopPipe(i);
        CVI_VI_DestroyPipe(i);
    }

    // Autres réinitialisations si nécessaires...
    MA_LOGI(TAG, "Réinitialisation des ressources système terminée.");
}

// Fonction pour fermer proprement l'application et libérer toutes les ressources
void shutdownApplication(NodeServer* server) {
    MA_LOGI(TAG, "Arrêt propre de l'application...");

    // Arrêter tous les noeuds
    NodeFactory::clear();

    // Arrêter le serveur
    if (server) {
        server->stop();
    }

    // Réinitialiser les ressources système
    resetSystemResources();

    MA_LOGI(TAG, "Arrêt de l'application terminé");

    // Fermer le journal système
    closelog();
}

void show_version() {
    std::cout << PROJECT_VERSION << std::endl;
}

void show_help() {
    std::cout << "Usage: sscma-node [options]\n"
              << "Options:\n"
              << "  -v, --version        Show version\n"
              << "  -h, --help           Show this help message\n"
              << "  -c, --config <file>  Configuration file, default is " << MA_NODE_CONFIG_FILE << "\n"
              << "  --start              Start the service\n"
              << "  --daemon             Run in daemon mode\n"
              << std::endl;
}

int main(int argc, char** argv) {

    openlog("sscma", LOG_CONS | LOG_PERROR, LOG_DAEMON);

    MA_LOGD("main", "version: %s build: %s", PROJECT_VERSION, __DATE__ " " __TIME__);

    Signal::install({SIGINT, SIGSEGV, SIGABRT, SIGTRAP, SIGTERM, SIGHUP, SIGQUIT, SIGPIPE}, [](int sig) {
        MA_LOGE(TAG, "received signal %d", sig);
        if (sig == SIGSEGV || sig == SIGABRT) {
            deinitVideo();
        } else {
            NodeFactory::clear();
        }
        closelog();
        exit(1);
    });


    std::string config_file = MA_NODE_CONFIG_FILE;
    bool start_service      = false;
    bool daemon             = false;

    if (argc < 2) {
        show_help();
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-v" || arg == "--version") {
            show_version();
            return 0;
        } else if (arg == "-h" || arg == "--help") {
            show_help();
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_file = argv[++i];
            } else {
                std::cerr << "Error: Missing argument for --config" << std::endl;
                return 1;
            }
        } else if (arg == "--start") {
            start_service = true;
        } else if (arg == "--daemon") {
            daemon        = true;
            start_service = true;
        } else {
            std::cerr << "Error: Unknown option " << arg << std::endl;
            return 1;
        }
    }


    if (start_service) {

        // Réinitialiser les ressources système au démarrage pour éviter les conflits
        resetSystemResources();

        // Lire les paramètres de configuration du flash depuis flow.json
        FlashConfig flashConfig = readFlashConfigFromFile();

        // Désactiver le clignotement de la LED rouge si configuré
        if (flashConfig.disable_red_led_blinking) {
            ma::disableRedLedBlinking();
        }

        StorageFile* config = new StorageFile();
        config->init(config_file.c_str());

        MA_LOGI(TAG, "starting the service...");
        MA_LOGI(TAG, "version: %s build: %s", PROJECT_VERSION, __DATE__ " " __TIME__);
        MA_LOGI(TAG, "config: %s", config_file.c_str());

        std::string client;
        std::string host;
        int port = 1883;
        std::string user;
        std::string password;

        MA_STORAGE_GET_STR(config, MA_STORAGE_KEY_MQTT_HOST, host, "localhost");
        MA_STORAGE_GET_STR(config, MA_STORAGE_KEY_MQTT_CLIENTID, client, "recamera");
        MA_STORAGE_GET_STR(config, MA_STORAGE_KEY_MQTT_USER, user, "");
        MA_STORAGE_GET_STR(config, MA_STORAGE_KEY_MQTT_PWD, password, "");
        MA_STORAGE_GET_POD(config, MA_STORAGE_KEY_MQTT_PORT, port, 1883);

        if (daemon) {
            char* err;
            pid_t pid;

            pid = fork();
            if (pid < 0) {
                err = strerror(errno);
                MA_LOGE(TAG, "Error in fork: %s", err);
                exit(1);
            }
            if (pid > 0) {
                exit(0);
            }
            if (setsid() < 0) {
                err = strerror(errno);
                MA_LOGE(TAG, "Error in setsid: %s", err);
                exit(1);
            }
        }

        NodeServer server(client);
        server.setStorage(config);
        server.start(host, port, user, password);

        // Auto-start nodes defined in flow.json
        {
            std::ifstream flowifs("/userdata/flow.json");
            if (flowifs.good()) {
                json flow;
                flowifs >> flow;
                if (flow.contains("nodes") && flow["nodes"].is_array()) {
                    for (auto& elem : flow["nodes"]) {
                        std::string id   = elem["id"].get<std::string>();
                        std::string type = elem["type"].get<std::string>();
                        json cdata;
                        cdata["type"] = type;
                        if (elem.contains("config"))
                            cdata["config"] = elem["config"];
                        if (elem.contains("deps"))
                            cdata["dependencies"] = elem["deps"];
                        MA_LOGI(TAG, "autostart create node: %s type %s", id.c_str(), type.c_str());
                        NodeFactory::create(id, type, cdata, &server);
                        if (auto nd = NodeFactory::find(id)) {
                            MA_LOGI(TAG, "autostart enabling node: %s", id.c_str());
                            nd->onControl("enabled", json(true));
                        }
                    }
                }
            }
        }

        // Attendre un moment pour que tous les noeuds s'initialisent correctement
        Thread::sleep(Tick::fromSeconds(2));

        // Récupérer une référence au noeud ImagePreProcessorNode
        Node* node                               = nullptr;
        ImagePreProcessorNode* imagePreProcessor = nullptr;

        // Essayer de trouver un noeud de type image_preprocessor
        // D'abord, vérifier dans flow.json pour trouver l'ID du noeud image_preprocessor
        std::string imageProcessorId = "";
        {
            std::ifstream flowifs("/userdata/flow.json");
            if (flowifs.good()) {
                json flow;
                flowifs >> flow;
                if (flow.contains("nodes") && flow["nodes"].is_array()) {
                    for (auto& elem : flow["nodes"]) {
                        if (elem.contains("type") && elem["type"].get<std::string>() == "image_preprocessor") {
                            imageProcessorId = elem["id"].get<std::string>();
                            MA_LOGI(TAG, "Found image_preprocessor node with ID: %s in flow.json", imageProcessorId.c_str());
                            break;
                        }
                    }
                }
            }
        }

        // Si on a trouvé l'ID, récupérer le noeud directement
        if (!imageProcessorId.empty()) {
            node = NodeFactory::find(imageProcessorId);
        }

        // Si on n'a pas trouvé l'ID ou le noeud, essayer avec un ID par défaut
        if (node == nullptr) {
            // Essayer avec quelques ID par défaut possibles
            const std::vector<std::string> possibleIds = {"image_preprocessor", "preprocessor", "img_proc"};
            for (const auto& id : possibleIds) {
                node = NodeFactory::find(id);
                if (node) {
                    MA_LOGI(TAG, "Found image processor node with default ID: %s", id.c_str());
                    break;
                }
            }
        }

        // Vérifier si on a trouvé le noeud et qu'il est du bon type
        if (node && node->type() == "image_preprocessor") {
            imagePreProcessor = static_cast<ImagePreProcessorNode*>(node);
            MA_LOGI(TAG, "ImagePreProcessorNode successfully accessed");
        }

        // Vérifier si le noeud image_preprocessor a été trouvé
        if (!imagePreProcessor) {
            MA_LOGE(TAG, "Erreur: Noeud ImagePreProcessorNode non trouvé!");
            shutdownApplication(&server);
            return 1;
        }

        // Configuration pour gérer l'entrée non bloquante
        struct termios old_tio, new_tio;
        tcgetattr(STDIN_FILENO, &old_tio);
        new_tio = old_tio;
        new_tio.c_lflag &= (~ICANON & ~ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

        // Mettre stdin en mode non-bloquant
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

        // Boucle principale avec invite utilisateur
        bool running = true;
        std::cout << "\n=== Application de capture d'images ===\n";
        std::cout << "Appuyez sur [Entrée] pour capturer une image\n";
        std::cout << "Appuyez sur 'q' pour quitter\n";

        while (running) {
            char c;
            int result = read(STDIN_FILENO, &c, 1);

            if (result > 0) {
                if (c == 'q' || c == 'Q') {
                    std::cout << "Demande de fermeture de l'application...\n";

                    // S'assurer que toutes les LEDs sont éteintes avant de quitter
                    std::cout << "Extinction de toutes les LEDs...\n";
                    // Éteindre d'abord la LED blanche (utilisée pour le flash)
                    Led::controlLed("white", false);
                    std::cout << "Toutes les LEDs ont été éteintes.\n";
                    running = false;

                } else {
                    // Lire les paramètres de configuration du flash à chaque capture
                    // pour permettre l'ajustement sans redémarrer l'application
                    FlashConfig flashConfig = readFlashConfigFromFile();

                    std::cout << "Activation du flash pour la capture...\n";

                    // Activer le flash (LED blanche) en utilisant directement la méthode statique de Led
                    Led::controlLed("white", true, flashConfig.flash_intensity);

                    // Attendre un court moment pour que la caméra s'adapte à l'éclairage
                    std::cout << "Attente de " << flashConfig.pre_capture_delay_ms << " ms avant la capture...\n";
                    Thread::sleep(Tick::fromMilliseconds(flashConfig.pre_capture_delay_ms));

                    std::cout << "Capture d'image demandée...\n";

                    // Demander une capture d'image
                    // Le flash sera éteint automatiquement après le traitement dans image_preprocessor.cpp
                    imagePreProcessor->requestCapture();

                    std::cout << "Capture en cours, le flash s'éteindra automatiquement après le traitement...\n";
                    std::cout << "Pour modifier les paramètres du flash, éditez le fichier /userdata/flow.json\n";

                    Thread::sleep(Tick::fromMilliseconds(500));
                    // Ajout des instructions après chaque capture pour rappeler à l'utilisateur
                    std::cout << "\n=== Application de capture d'images ===\n";
                    std::cout << "Appuyez sur [Entrée] pour capturer une image\n";
                    std::cout << "Appuyez sur 'q' pour quitter\n";
                }
            }

            // Dormir un court instant pour éviter de surcharger le CPU
            Thread::sleep(Tick::fromMilliseconds(10));
        }

        // Restaurer les paramètres du terminal
        tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
        fcntl(STDIN_FILENO, F_SETFL, flags);

        // Fermer proprement l'application
        shutdownApplication(&server);
    }

    closelog();
    return 0;
}
