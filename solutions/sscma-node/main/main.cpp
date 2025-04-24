#include <fstream>
#include <iostream>
#include <optional>  // Ajout pour std::optional
#include <string>
#include <syslog.h>
#include <unistd.h>
#include <vector>  // Ajout pour std::vector

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

// Structure pour stocker les arguments parsés
struct AppArguments {
    std::string config_file = MA_NODE_CONFIG_FILE;
    bool start_service      = false;
    bool daemon             = false;
    bool show_help          = false;
    bool show_version       = false;
    bool error              = false;
    std::string error_message;
};

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
    std::cout << "Usage: sscma-node [options]\\n"
              << "Options:\\n"
              << "  -v, --version        Show version\\n"
              << "  -h, --help           Show this help message\\n"
              << "  -c, --config <file>  Configuration file, default is " << MA_NODE_CONFIG_FILE << "\\n"
              << "  --start              Start the service\\n"
              << "  --daemon             Run in daemon mode\\n"
              << std::endl;
}

// Fonction pour parser les arguments de la ligne de commande
AppArguments parseArguments(int argc, char** argv) {
    AppArguments args;

    // Si aucun argument n'est fourni (sauf le nom du programme), afficher l'aide
    if (argc < 2) {
        args.show_help = true;
        return args;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-v" || arg == "--version") {
            args.show_version = true;
            return args;  // Pas besoin de parser plus loin
        } else if (arg == "-h" || arg == "--help") {
            args.show_help = true;
            return args;  // Pas besoin de parser plus loin
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                args.config_file = argv[++i];
            } else {
                args.error         = true;
                args.error_message = "Error: Missing argument for --config";
                return args;
            }
        } else if (arg == "--start") {
            args.start_service = true;
        } else if (arg == "--daemon") {
            args.daemon        = true;
            args.start_service = true;  // --daemon implique --start
        } else {
            args.error         = true;
            args.error_message = "Error: Unknown option " + arg;
            return args;
        }
    }

    // Si aucune action n'est spécifiée (start, help, version), afficher l'aide par défaut
    if (!args.start_service && !args.show_help && !args.show_version) {
        args.show_help = true;
    }


    return args;
}

// Fonction pour démarrer le service principal
int startService(const std::string& config_file, bool daemon) {
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
            delete config;  // Libérer la mémoire avant de quitter
            return 1;
        }
        if (pid > 0) {
            exit(0);  // Parent exits successfully
        }
        if (setsid() < 0) {
            err = strerror(errno);
            MA_LOGE(TAG, "Error in setsid: %s", err);
            delete config;  // Libérer la mémoire avant de quitter
            return 1;
        }
    }

    NodeServer server(client);
    server.setStorage(config);  // Le serveur prend possession du pointeur config
    server.start(host, port, user, password);

    // Auto-start nodes defined in flow.json
    {
        std::ifstream flowifs("/userdata/flow.json");
        if (flowifs.good()) {
            json flow;
            try {  // Ajouter une gestion d'erreur pour le parsing JSON
                flowifs >> flow;
                if (flow.contains("nodes") && flow["nodes"].is_array()) {
                    for (auto& elem : flow["nodes"]) {
                        // Vérifier si les clés existent avant de les utiliser
                        if (!elem.contains("id") || !elem.contains("type")) {
                            MA_LOGW(TAG, "Skipping node due to missing 'id' or 'type' in flow.json");
                            continue;
                        }
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
                        } else {
                            MA_LOGW(TAG, "Failed to find node %s after creation for autostart enable.", id.c_str());
                        }
                    }
                }
            } catch (const json::parse_error& e) {
                MA_LOGE(TAG, "Failed to parse /userdata/flow.json: %s", e.what());
            }
        } else {
            MA_LOGW(TAG, "Could not open /userdata/flow.json for autostart.");
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
            try {  // Ajouter une gestion d'erreur pour le parsing JSON
                flowifs >> flow;
                if (flow.contains("nodes") && flow["nodes"].is_array()) {
                    for (auto& elem : flow["nodes"]) {
                        if (elem.contains("type") && elem["type"].get<std::string>() == "image_preprocessor" && elem.contains("id")) {
                            imageProcessorId = elem["id"].get<std::string>();
                            MA_LOGI(TAG, "Found image_preprocessor node with ID: %s in flow.json", imageProcessorId.c_str());
                            break;
                        }
                    }
                }
            } catch (const json::parse_error& e) {
                MA_LOGE(TAG, "Failed to parse /userdata/flow.json while searching for image_preprocessor ID: %s", e.what());
            }
        }
    }

    // Si on a trouvé l'ID, récupérer le noeud directement
    if (!imageProcessorId.empty()) {
        node = NodeFactory::find(imageProcessorId);
        if (node) {
            MA_LOGI(TAG, "Found node with ID %s from flow.json.", imageProcessorId.c_str());
        } else {
            MA_LOGW(TAG, "Node with ID %s specified in flow.json not found.", imageProcessorId.c_str());
        }
    }

    // Si on n'a pas trouvé l'ID ou le noeud via flow.json, essayer avec des ID par défaut
    if (node == nullptr) {
        MA_LOGI(TAG, "Image processor node ID not found in flow.json or node not found. Trying default IDs...");
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
    } else if (node) {
        MA_LOGE(TAG, "Found node with ID %s, but it is not of type 'image_preprocessor' (type is '%s').", node->id().c_str(), node->type().c_str());
    }

    // Vérifier si le noeud image_preprocessor a été trouvé
    if (!imagePreProcessor) {
        MA_LOGE(TAG, "Erreur: Noeud ImagePreProcessorNode non trouvé ou n'est pas du bon type!");
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
    int old_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, old_flags | O_NONBLOCK);

    // Boucle principale avec invite utilisateur
    bool running = true;
    MA_LOGI(TAG, "=== Application de capture d'images ===");
    MA_LOGI(TAG, "Appuyez sur [Entrée] pour capturer une image");
    MA_LOGI(TAG, "Appuyez sur 'q' pour quitter");

    while (running) {
        char c;
        int result = read(STDIN_FILENO, &c, 1);

        if (result > 0) {
            if (c == 'q' || c == 'Q') {
                MA_LOGI(TAG, "Demande de fermeture de l'application...");

                // S'assurer que toutes les LEDs sont éteintes avant de quitter
                MA_LOGI(TAG, "Extinction de toutes les LEDs...");
                // Éteindre d'abord la LED blanche (utilisée pour le flash)
                Led::controlLed("white", false);
                MA_LOGI(TAG, "Toutes les LEDs ont été éteintes.");
                running = false;

            } else if (c == '\n') {  // Déclencher la capture sur Entrée // Correction: utiliser '\n' au lieu de '\\n'
                // Lire les paramètres de configuration du flash à chaque capture
                // pour permettre l'ajustement sans redémarrer l'application
                FlashConfig currentFlashConfig = readFlashConfigFromFile();  // Renommé pour clarté

                MA_LOGI(TAG, "Activation du flash pour la capture...");

                // Activer le flash (LED blanche) en utilisant directement la méthode statique de Led
                Led::controlLed("white", true, currentFlashConfig.flash_intensity);

                // Attendre un court moment pour que la caméra s'adapte à l'éclairage
                MA_LOGI(TAG, "Attente de %dms avant la capture...", currentFlashConfig.pre_capture_delay_ms);
                Thread::sleep(Tick::fromMilliseconds(currentFlashConfig.pre_capture_delay_ms));

                MA_LOGI(TAG, "Capture d'image demandée...");

                // Demander une capture d'image
                // Le flash sera éteint automatiquement après le traitement dans image_preprocessor.cpp
                imagePreProcessor->requestCapture();

                MA_LOGI(TAG, "Capture en cours, le flash s'éteindra automatiquement après le traitement...");
                MA_LOGI(TAG, "Pour modifier les paramètres du flash, éditez le fichier userdata/flow.json");

                Thread::sleep(Tick::fromMilliseconds(1000));  // Donner du temps pour le traitement
                // Ajout des instructions après chaque capture pour rappeler à l'utilisateur
                MA_LOGI(TAG, "Appuyez sur [Entrée] pour capturer une image");
                MA_LOGI(TAG, "Appuyez sur 'q' pour quitter");
            }
            // Ignorer les autres caractères
        }

        // Dormir un court instant pour éviter de surcharger le CPU
        Thread::sleep(Tick::fromMilliseconds(10));
    }

    // Restaurer les paramètres du terminal
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    fcntl(STDIN_FILENO, F_SETFL, old_flags);  // Restaurer les anciens flags

    // Fermer proprement l'application
    shutdownApplication(&server);  // Le serveur gère la suppression de config

    return 0;  // Succès
}


int main(int argc, char** argv) {

    openlog("sscma", LOG_CONS | LOG_PERROR, LOG_DAEMON);

    MA_LOGD("main", "version: %s build: %s", PROJECT_VERSION, __DATE__ " " __TIME__);

    Signal::install({SIGINT, SIGSEGV, SIGABRT, SIGTRAP, SIGTERM, SIGHUP, SIGQUIT, SIGPIPE}, [](int sig) {
        MA_LOGE(TAG, "received signal %d", sig);
        if (sig == SIGSEGV || sig == SIGABRT) {
            // Tenter une réinitialisation plus agressive en cas de crash
            resetSystemResources();
            deinitVideo();  // Assurez-vous que cette fonction existe et est sûre à appeler ici
        } else {
            // Pour les signaux d'arrêt normaux, essayer un arrêt propre
            NodeFactory::clear();  // Arrêter les noeuds d'abord
        }
        closelog();
        exit(1);  // Quitter après la gestion du signal
    });

    AppArguments args = parseArguments(argc, argv);

    if (args.error) {
        std::cerr << args.error_message << std::endl;
        show_help();  // Afficher l'aide en cas d'erreur d'argument
        closelog();
        return 1;
    }

    if (args.show_version) {
        show_version();
        closelog();
        return 0;
    }

    if (args.show_help) {
        show_help();
        closelog();
        return 0;
    }

    int exit_code = 0;
    if (args.start_service) {
        exit_code = startService(args.config_file, args.daemon);
    }
    // Si start_service n'est pas vrai (et qu'il n'y a pas eu d'erreur ou de demande d'aide/version),
    // le programme se termine simplement ici après avoir potentiellement affiché l'aide dans parseArguments.

    closelog();
    return exit_code;
}
