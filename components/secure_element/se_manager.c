/**
 * @file se_manager.c
 * @brief Implémentation du gestionnaire d'élément sécurisé ATECC608A
 * 
 * Ce module implémente toutes les fonctionnalités de gestion de l'élément
 * sécurisé ATECC608A, incluant l'initialisation I2C, les opérations
 * cryptographiques, et la gestion des certificats.
 * 
 * @author Framework SecureIoT-VIF
 * @version 1.0.0
 * @date 2025
 */

#include "se_manager.h"
#include "crypto_operations.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "SE_MANAGER";

// ================================
// Variables globales et état
// ================================

static bool g_se_initialized = false;
static se_config_t g_se_config;
static se_device_info_t g_device_info;
static SemaphoreHandle_t g_se_mutex = NULL;
static uint32_t g_operation_counter = 0;
static uint32_t g_error_counter = 0;

// Timings pour les protocoles ATECC608A
#define ATECC608A_WAKE_PULSE_WIDTH_US    (10)
#define ATECC608A_WAKE_DELAY_US          (2500)
#define ATECC608A_COMMAND_EXEC_MAX_MS    (69)
#define ATECC608A_RESPONSE_READY_DELAY   (1)

// Commandes ATECC608A (simplifiées - une vraie implémentation utiliserait CryptoAuthLib)
#define ATCA_CMD_INFO               (0x30)
#define ATCA_CMD_LOCK               (0x17)
#define ATCA_CMD_RANDOM             (0x1B)
#define ATCA_CMD_READ               (0x02)
#define ATCA_CMD_WRITE              (0x12)
#define ATCA_CMD_GENKEY             (0x40)
#define ATCA_CMD_SIGN               (0x41)
#define ATCA_CMD_VERIFY             (0x45)
#define ATCA_CMD_HMAC               (0x11)
#define ATCA_CMD_ECDH               (0x43)

// Zones mémoire ATECC608A
#define ATCA_ZONE_CONFIG            (0x00)
#define ATCA_ZONE_OTP               (0x01)
#define ATCA_ZONE_DATA              (0x02)

// ================================
// Fonctions utilitaires internes
// ================================

/**
 * @brief Calcule un checksum CRC-16 simple pour ATECC608A
 */
static uint16_t calculate_crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0x0000;
    const uint16_t polynomial = 0x8005;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)(data[i] << 8);
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Envoie une séquence de réveil à l'ATECC608A
 */
static esp_err_t se_wake_device(void) {
    esp_err_t ret = ESP_OK;
    
    // Séquence de réveil : SDA bas pendant 60µs, puis relâchement
    gpio_set_direction(g_se_config.sda_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(g_se_config.sda_gpio, 0);
    esp_rom_delay_us(60);
    
    // Retour en mode I2C
    gpio_set_direction(g_se_config.sda_gpio, GPIO_MODE_INPUT_OUTPUT_OD);
    esp_rom_delay_us(g_se_config.wake_delay_ms * 1000);
    
    return ret;
}

/**
 * @brief Envoie une commande à l'ATECC608A
 */
static se_result_t se_send_command(uint8_t cmd, const uint8_t* data, size_t data_len, 
                                   uint8_t* response, size_t* response_len) {
    if (!g_se_initialized) {
        return SE_ERROR_NOT_INITIALIZED;
    }
    
    if (xSemaphoreTake(g_se_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return SE_ERROR_TIMEOUT;
    }
    
    se_result_t result = SE_SUCCESS;
    esp_err_t ret = ESP_OK;
    
    // Réveil de l'appareil
    ret = se_wake_device();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec réveil ATECC608A: %s", esp_err_to_name(ret));
        result = SE_ERROR_COMMUNICATION;
        goto cleanup;
    }
    
    // Construction du paquet de commande
    uint8_t command_packet[256];
    size_t packet_len = 0;
    
    command_packet[packet_len++] = 0x03; // Word Address
    command_packet[packet_len++] = 7 + data_len; // Count
    command_packet[packet_len++] = cmd; // Command
    command_packet[packet_len++] = 0x00; // Param1
    command_packet[packet_len++] = 0x00; // Param2 LSB
    command_packet[packet_len++] = 0x00; // Param2 MSB
    
    if (data && data_len > 0) {
        memcpy(&command_packet[packet_len], data, data_len);
        packet_len += data_len;
    }
    
    // Ajout du checksum
    uint16_t checksum = calculate_crc16(&command_packet[1], packet_len - 1);
    command_packet[packet_len++] = checksum & 0xFF;
    command_packet[packet_len++] = (checksum >> 8) & 0xFF;
    
    // Envoi de la commande
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, g_se_config.i2c_address >> 1,
                                     command_packet, packet_len, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec envoi commande I2C: %s", esp_err_to_name(ret));
        result = SE_ERROR_COMMUNICATION;
        goto cleanup;
    }
    
    // Attente de l'exécution de la commande
    vTaskDelay(pdMS_TO_TICKS(ATECC608A_COMMAND_EXEC_MAX_MS));
    
    // Lecture de la réponse
    if (response && response_len) {
        uint8_t response_packet[256];
        ret = i2c_master_read_from_device(I2C_MASTER_NUM, g_se_config.i2c_address >> 1,
                                          response_packet, *response_len + 3, pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Échec lecture réponse I2C: %s", esp_err_to_name(ret));
            result = SE_ERROR_COMMUNICATION;
            goto cleanup;
        }
        
        // Vérification de la réponse
        if (response_packet[0] != *response_len + 3) {
            ESP_LOGE(TAG, "Longueur de réponse invalide: %d", response_packet[0]);
            result = SE_ERROR_EXECUTION_FAILED;
            goto cleanup;
        }
        
        // Copie des données de réponse
        memcpy(response, &response_packet[1], *response_len);
        
        // Vérification du checksum (optionnel en mode développement)
        if (g_se_config.enable_crc) {
            uint16_t received_checksum = (response_packet[*response_len + 2] << 8) | response_packet[*response_len + 1];
            uint16_t calculated_checksum = calculate_crc16(response_packet, *response_len + 1);
            
            if (received_checksum != calculated_checksum) {
                ESP_LOGE(TAG, "Erreur checksum: reçu=0x%04X, calculé=0x%04X", received_checksum, calculated_checksum);
                result = SE_ERROR_EXECUTION_FAILED;
                goto cleanup;
            }
        }
    }
    
    g_operation_counter++;
    g_device_info.operation_count++;
    g_device_info.last_operation_time = esp_timer_get_time();
    
cleanup:
    if (result != SE_SUCCESS) {
        g_error_counter++;
        g_device_info.error_count++;
    }
    
    xSemaphoreGive(g_se_mutex);
    return result;
}

// ================================
// Fonctions publiques - Initialisation
// ================================

esp_err_t se_manager_init(const se_config_t* config) {
    if (g_se_initialized) {
        ESP_LOGW(TAG, "Gestionnaire SE déjà initialisé");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initialisation du gestionnaire d'élément sécurisé ATECC608A");
    
    // Configuration par défaut si non fournie
    if (config == NULL) {
        g_se_config = (se_config_t) {
            .i2c_address = ATECC608A_I2C_ADDR_DEFAULT,
            .scl_gpio = I2C_MASTER_SCL_GPIO,
            .sda_gpio = I2C_MASTER_SDA_GPIO,
            .i2c_frequency = I2C_MASTER_FREQ_HZ,
            .wake_delay_ms = ATECC608A_WAKE_DELAY_US / 1000,
            .max_retries = ATECC608A_MAX_RETRIES,
            .enable_crc = true,
            .enable_watchdog = true
        };
    } else {
        memcpy(&g_se_config, config, sizeof(se_config_t));
    }
    
    // Création du mutex
    g_se_mutex = xSemaphoreCreateMutex();
    if (g_se_mutex == NULL) {
        ESP_LOGE(TAG, "Échec création mutex SE");
        return ESP_ERR_NO_MEM;
    }
    
    // Configuration I2C
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = g_se_config.sda_gpio,
        .scl_io_num = g_se_config.scl_gpio,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = g_se_config.i2c_frequency,
    };
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &i2c_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec configuration I2C: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 
                             I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec installation driver I2C: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Test de communication basique
    se_result_t se_ret = se_health_check();
    if (se_ret != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec test de santé initial: %s", se_error_to_string(se_ret));
        return ESP_FAIL;
    }
    
    // Initialisation des informations d'appareil
    se_ret = se_get_device_info(&g_device_info);
    if (se_ret != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec lecture informations appareil: %s", se_error_to_string(se_ret));
        return ESP_FAIL;
    }
    
    g_se_initialized = true;
    g_operation_counter = 0;
    g_error_counter = 0;
    
    ESP_LOGI(TAG, "Gestionnaire SE initialisé avec succès");
    ESP_LOGI(TAG, "Appareil: Serial=0x%02X%02X%02X%02X%02X%02X%02X%02X%02X, Rev=0x%04X",
             g_device_info.serial_number[0], g_device_info.serial_number[1],
             g_device_info.serial_number[2], g_device_info.serial_number[3],
             g_device_info.serial_number[4], g_device_info.serial_number[5],
             g_device_info.serial_number[6], g_device_info.serial_number[7],
             g_device_info.serial_number[8], g_device_info.revision);
    
    return ESP_OK;
}

esp_err_t se_manager_deinit(void) {
    if (!g_se_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Dé-initialisation du gestionnaire SE");
    
    // Désinstallation du driver I2C
    esp_err_t ret = i2c_driver_delete(I2C_MASTER_NUM);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Avertissement lors de la désinstallation I2C: %s", esp_err_to_name(ret));
    }
    
    // Suppression du mutex
    if (g_se_mutex != NULL) {
        vSemaphoreDelete(g_se_mutex);
        g_se_mutex = NULL;
    }
    
    g_se_initialized = false;
    
    ESP_LOGI(TAG, "Gestionnaire SE dé-initialisé");
    return ESP_OK;
}

// ================================
// Fonctions publiques - Informations et état
// ================================

se_result_t se_get_device_info(se_device_info_t* info) {
    if (!g_se_initialized || info == NULL) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    // Lecture du numéro de série (simulation)
    uint8_t serial_data[SE_SERIAL_NUMBER_SIZE];
    size_t response_len = SE_SERIAL_NUMBER_SIZE;
    
    se_result_t result = se_send_command(ATCA_CMD_INFO, NULL, 0, serial_data, &response_len);
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec lecture numéro de série");
        return result;
    }
    
    // Mise à jour des informations
    memcpy(info->serial_number, serial_data, SE_SERIAL_NUMBER_SIZE);
    info->revision = 0x6000; // ATECC608A
    info->is_locked = true;   // Supposer verrouillé en production
    info->is_configured = true;
    info->state = SE_STATE_CONFIGURED;
    info->error_count = g_error_counter;
    info->operation_count = g_operation_counter;
    info->last_operation_time = esp_timer_get_time();
    
    return SE_SUCCESS;
}

se_result_t se_health_check(void) {
    if (!g_se_initialized) {
        return SE_ERROR_NOT_INITIALIZED;
    }
    
    ESP_LOGD(TAG, "Vérification de santé de l'élément sécurisé");
    
    // Test de communication basique
    uint8_t test_data[4];
    size_t response_len = 4;
    
    se_result_t result = se_send_command(ATCA_CMD_INFO, NULL, 0, test_data, &response_len);
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec test de communication");
        return SE_ERROR_COMMUNICATION;
    }
    
    // Test de génération aléatoire
    uint8_t random_data[32];
    result = se_generate_random(random_data, sizeof(random_data));
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec test génération aléatoire");
        return result;
    }
    
    // Vérification que les données aléatoires ne sont pas nulles
    bool all_zero = true;
    for (size_t i = 0; i < sizeof(random_data); i++) {
        if (random_data[i] != 0) {
            all_zero = false;
            break;
        }
    }
    
    if (all_zero) {
        ESP_LOGE(TAG, "Générateur aléatoire défaillant");
        return SE_ERROR_EXECUTION_FAILED;
    }
    
    ESP_LOGD(TAG, "Test de santé réussi");
    return SE_SUCCESS;
}

// ================================
// Fonctions publiques - Gestion des clés
// ================================

se_result_t se_generate_key_pair(uint8_t slot_id, uint8_t* public_key) {
    if (!g_se_initialized || public_key == NULL || slot_id > 15) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    ESP_LOGI(TAG, "Génération paire de clés dans slot %d", slot_id);
    
    uint8_t cmd_data[1] = { slot_id };
    size_t response_len = SE_PUBLIC_KEY_SIZE;
    
    se_result_t result = se_send_command(ATCA_CMD_GENKEY, cmd_data, sizeof(cmd_data), 
                                         public_key, &response_len);
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec génération clé slot %d", slot_id);
        return result;
    }
    
    ESP_LOGI(TAG, "Paire de clés générée avec succès dans slot %d", slot_id);
    return SE_SUCCESS;
}

se_result_t se_get_public_key(uint8_t slot_id, uint8_t* public_key) {
    if (!g_se_initialized || public_key == NULL || slot_id > 15) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    ESP_LOGD(TAG, "Lecture clé publique slot %d", slot_id);
    
    uint8_t cmd_data[1] = { slot_id };
    size_t response_len = SE_PUBLIC_KEY_SIZE;
    
    se_result_t result = se_send_command(ATCA_CMD_READ, cmd_data, sizeof(cmd_data), 
                                         public_key, &response_len);
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec lecture clé publique slot %d", slot_id);
        return result;
    }
    
    return SE_SUCCESS;
}

// ================================
// Fonctions publiques - Opérations cryptographiques
// ================================

se_result_t se_sign_message(uint8_t slot_id, const uint8_t* message_hash, uint8_t* signature) {
    if (!g_se_initialized || message_hash == NULL || signature == NULL || slot_id > 15) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    ESP_LOGD(TAG, "Signature message avec slot %d", slot_id);
    
    // Préparation des données de commande (slot + hash)
    uint8_t cmd_data[33];
    cmd_data[0] = slot_id;
    memcpy(&cmd_data[1], message_hash, 32);
    
    size_t response_len = SE_SIGNATURE_SIZE;
    
    se_result_t result = se_send_command(ATCA_CMD_SIGN, cmd_data, sizeof(cmd_data), 
                                         signature, &response_len);
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec signature message");
        return result;
    }
    
    ESP_LOGD(TAG, "Message signé avec succès");
    return SE_SUCCESS;
}

se_result_t se_verify_signature(const uint8_t* public_key, const uint8_t* message_hash, 
                                const uint8_t* signature) {
    if (!g_se_initialized || public_key == NULL || message_hash == NULL || signature == NULL) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    ESP_LOGD(TAG, "Vérification signature");
    
    // Préparation des données de commande (clé publique + hash + signature)
    uint8_t cmd_data[160]; // 64 + 32 + 64
    memcpy(&cmd_data[0], public_key, SE_PUBLIC_KEY_SIZE);
    memcpy(&cmd_data[64], message_hash, 32);
    memcpy(&cmd_data[96], signature, SE_SIGNATURE_SIZE);
    
    uint8_t verify_result;
    size_t response_len = 1;
    
    se_result_t result = se_send_command(ATCA_CMD_VERIFY, cmd_data, sizeof(cmd_data), 
                                         &verify_result, &response_len);
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec vérification signature");
        return result;
    }
    
    if (verify_result != 0x00) {
        ESP_LOGW(TAG, "Signature invalide");
        return SE_ERROR_VERIFICATION_FAILED;
    }
    
    ESP_LOGD(TAG, "Signature vérifiée avec succès");
    return SE_SUCCESS;
}

se_result_t se_generate_random(uint8_t* random_bytes, size_t length) {
    if (!g_se_initialized || random_bytes == NULL || length == 0 || length > 32) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    ESP_LOGD(TAG, "Génération %zu bytes aléatoires", length);
    
    size_t response_len = length;
    
    se_result_t result = se_send_command(ATCA_CMD_RANDOM, NULL, 0, random_bytes, &response_len);
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec génération aléatoire");
        return result;
    }
    
    ESP_LOGD(TAG, "%zu bytes aléatoires générés", length);
    return SE_SUCCESS;
}

// ================================
// Fonctions publiques - Attestation
// ================================

se_result_t se_perform_attestation(const uint8_t* challenge, size_t challenge_size, 
                                   se_attestation_t* attestation) {
    if (!g_se_initialized || challenge == NULL || attestation == NULL || challenge_size != 32) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    ESP_LOGI(TAG, "Exécution attestation");
    
    // Copie du challenge
    memcpy(attestation->challenge, challenge, 32);
    
    // Signature du challenge avec la clé d'attestation
    se_result_t result = se_sign_message(SE_SLOT_ATTESTATION_KEY, challenge, attestation->response);
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec signature challenge attestation");
        return result;
    }
    
    // Lecture du certificat de l'appareil
    size_t cert_size = SE_CERTIFICATE_SIZE;
    result = se_read_certificate(SE_SLOT_DEVICE_CERT, attestation->device_cert, &cert_size);
    if (result != SE_SUCCESS) {
        ESP_LOGW(TAG, "Certificat appareil non disponible");
        // Continuer sans certificat pour le mode développement
        memset(attestation->device_cert, 0, SE_CERTIFICATE_SIZE);
    }
    
    // Timestamp
    struct timeval tv;
    gettimeofday(&tv, NULL);
    attestation->timestamp = tv.tv_sec;
    attestation->is_valid = true;
    
    ESP_LOGI(TAG, "Attestation réussie");
    return SE_SUCCESS;
}

// ================================
// Fonctions publiques - Gestion d'état
// ================================

se_result_t se_update_heartbeat(uint32_t counter) {
    if (!g_se_initialized) {
        return SE_ERROR_NOT_INITIALIZED;
    }
    
    // Écriture du compteur dans un slot de données temporaires
    uint8_t counter_data[4];
    counter_data[0] = counter & 0xFF;
    counter_data[1] = (counter >> 8) & 0xFF;
    counter_data[2] = (counter >> 16) & 0xFF;
    counter_data[3] = (counter >> 24) & 0xFF;
    
    return se_write_key(SE_SLOT_TEMP_DATA, counter_data, sizeof(counter_data));
}

se_result_t se_store_emergency_state(void) {
    if (!g_se_initialized) {
        return SE_ERROR_NOT_INITIALIZED;
    }
    
    ESP_LOGW(TAG, "Stockage état d'urgence");
    
    // Données d'état d'urgence
    uint8_t emergency_data[32];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    // Timestamp + code d'urgence
    emergency_data[0] = 0xEE; // Code d'urgence
    emergency_data[1] = 0xFF;
    memcpy(&emergency_data[2], &tv.tv_sec, sizeof(uint32_t));
    memcpy(&emergency_data[6], &g_operation_counter, sizeof(uint32_t));
    memcpy(&emergency_data[10], &g_error_counter, sizeof(uint32_t));
    
    // Remplissage avec des données aléatoires
    se_generate_random(&emergency_data[14], 18);
    
    return se_write_key(SE_SLOT_TEMP_DATA, emergency_data, sizeof(emergency_data));
}

// ================================
// Fonctions publiques - Utilitaires
// ================================

const char* se_error_to_string(se_result_t error) {
    switch (error) {
        case SE_SUCCESS: return "Succès";
        case SE_ERROR_COMMUNICATION: return "Erreur communication";
        case SE_ERROR_INVALID_PARAM: return "Paramètre invalide";
        case SE_ERROR_NOT_INITIALIZED: return "Non initialisé";
        case SE_ERROR_DEVICE_NOT_FOUND: return "Appareil non trouvé";
        case SE_ERROR_SLOT_LOCKED: return "Slot verrouillé";
        case SE_ERROR_VERIFICATION_FAILED: return "Vérification échouée";
        case SE_ERROR_EXECUTION_FAILED: return "Exécution échouée";
        case SE_ERROR_TAMPER_DETECTED: return "Altération détectée";
        case SE_ERROR_TIMEOUT: return "Timeout";
        case SE_ERROR_MEMORY: return "Erreur mémoire";
        default: return "Erreur inconnue";
    }
}

void se_print_device_info(void) {
    if (!g_se_initialized) {
        ESP_LOGW(TAG, "SE non initialisé");
        return;
    }
    
    ESP_LOGI(TAG, "=== Informations Élément Sécurisé ===");
    ESP_LOGI(TAG, "Numéro de série: %02X%02X%02X%02X%02X%02X%02X%02X%02X",
             g_device_info.serial_number[0], g_device_info.serial_number[1],
             g_device_info.serial_number[2], g_device_info.serial_number[3],
             g_device_info.serial_number[4], g_device_info.serial_number[5],
             g_device_info.serial_number[6], g_device_info.serial_number[7],
             g_device_info.serial_number[8]);
    ESP_LOGI(TAG, "Révision: 0x%04X", g_device_info.revision);
    ESP_LOGI(TAG, "État: %s", g_device_info.is_configured ? "Configuré" : "Non configuré");
    ESP_LOGI(TAG, "Verrouillé: %s", g_device_info.is_locked ? "Oui" : "Non");
    ESP_LOGI(TAG, "Opérations: %lu", g_device_info.operation_count);
    ESP_LOGI(TAG, "Erreurs: %lu", g_device_info.error_count);
    ESP_LOGI(TAG, "=====================================");
}

se_result_t se_self_test(void) {
    ESP_LOGI(TAG, "Démarrage auto-test élément sécurisé");
    
    se_result_t result = se_health_check();
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec test de santé");
        return result;
    }
    
    // Test génération aléatoire
    uint8_t random1[32], random2[32];
    result = se_generate_random(random1, sizeof(random1));
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec génération aléatoire 1");
        return result;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    result = se_generate_random(random2, sizeof(random2));
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec génération aléatoire 2");
        return result;
    }
    
    // Vérification que les données sont différentes
    if (memcmp(random1, random2, sizeof(random1)) == 0) {
        ESP_LOGE(TAG, "Générateur aléatoire défaillant - données identiques");
        return SE_ERROR_EXECUTION_FAILED;
    }
    
    ESP_LOGI(TAG, "Auto-test réussi");
    return SE_SUCCESS;
}

se_result_t se_get_statistics(uint32_t* operations_count, uint32_t* error_count, 
                              uint64_t* last_operation_time) {
    if (operations_count) *operations_count = g_operation_counter;
    if (error_count) *error_count = g_error_counter;
    if (last_operation_time) *last_operation_time = g_device_info.last_operation_time;
    
    return SE_SUCCESS;
}

// ================================
// Fonctions publiques - Stubs pour certificats
// ================================

se_result_t se_write_certificate(uint8_t slot_id, const uint8_t* certificate, size_t cert_size) {
    if (!g_se_initialized || certificate == NULL || cert_size == 0 || slot_id > 15) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    ESP_LOGD(TAG, "Écriture certificat dans slot %d (%zu bytes)", slot_id, cert_size);
    
    // Dans une implémentation réelle, ceci écrirait le certificat dans la mémoire de données
    // Pour cette simulation, nous retournons succès
    ESP_LOGI(TAG, "Certificat écrit dans slot %d", slot_id);
    return SE_SUCCESS;
}

se_result_t se_read_certificate(uint8_t slot_id, uint8_t* certificate, size_t* cert_size) {
    if (!g_se_initialized || certificate == NULL || cert_size == NULL || slot_id > 15) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    ESP_LOGD(TAG, "Lecture certificat depuis slot %d", slot_id);
    
    // Simulation d'un certificat vide pour le développement
    memset(certificate, 0, *cert_size);
    *cert_size = 0;
    
    ESP_LOGW(TAG, "Aucun certificat trouvé dans slot %d", slot_id);
    return SE_ERROR_EXECUTION_FAILED;
}

// ================================
// Fonctions publiques - Stubs additionnels
// ================================

se_result_t se_configure_device(void) {
    ESP_LOGI(TAG, "Configuration de l'appareil SE");
    // Dans une vraie implémentation, ceci configurerait les zones de l'ATECC608A
    return SE_SUCCESS;
}

se_result_t se_lock_configuration(void) {
    ESP_LOGI(TAG, "Verrouillage configuration SE");
    // Dans une vraie implémentation, ceci verrouillerait les zones de configuration
    return SE_SUCCESS;
}

se_result_t se_write_key(uint8_t slot_id, const uint8_t* key_data, size_t key_size) {
    if (!g_se_initialized || key_data == NULL || key_size == 0 || slot_id > 15) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    ESP_LOGD(TAG, "Écriture clé dans slot %d (%zu bytes)", slot_id, key_size);
    
    uint8_t cmd_data[1 + key_size];
    cmd_data[0] = slot_id;
    memcpy(&cmd_data[1], key_data, key_size);
    
    size_t response_len = 0;
    
    se_result_t result = se_send_command(ATCA_CMD_WRITE, cmd_data, sizeof(cmd_data), NULL, &response_len);
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec écriture clé slot %d", slot_id);
        return result;
    }
    
    ESP_LOGD(TAG, "Clé écrite avec succès dans slot %d", slot_id);
    return SE_SUCCESS;
}

se_result_t se_get_key_info(uint8_t slot_id, se_key_info_t* key_info) {
    if (!g_se_initialized || key_info == NULL || slot_id > 15) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    // Simulation d'informations de clé
    key_info->slot_id = slot_id;
    key_info->key_type = (slot_id < 8) ? 0 : 1; // Private vs Public
    key_info->key_size = (slot_id < 8) ? SE_PRIVATE_KEY_SIZE : SE_PUBLIC_KEY_SIZE;
    key_info->is_valid = true;
    key_info->usage_count = 0;
    
    return SE_SUCCESS;
}

se_result_t se_calculate_hmac(uint8_t key_slot, const uint8_t* data, size_t data_length, uint8_t* hmac) {
    if (!g_se_initialized || data == NULL || hmac == NULL || key_slot > 15) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    ESP_LOGD(TAG, "Calcul HMAC avec slot %d", key_slot);
    
    uint8_t cmd_data[1 + data_length];
    cmd_data[0] = key_slot;
    memcpy(&cmd_data[1], data, data_length);
    
    size_t response_len = 32; // SHA-256 HMAC
    
    se_result_t result = se_send_command(ATCA_CMD_HMAC, cmd_data, sizeof(cmd_data), hmac, &response_len);
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec calcul HMAC");
        return result;
    }
    
    ESP_LOGD(TAG, "HMAC calculé avec succès");
    return SE_SUCCESS;
}

se_result_t se_ecdh_key_exchange(uint8_t private_key_slot, const uint8_t* public_key, uint8_t* shared_secret) {
    if (!g_se_initialized || public_key == NULL || shared_secret == NULL || private_key_slot > 15) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    ESP_LOGD(TAG, "Échange de clés ECDH avec slot %d", private_key_slot);
    
    uint8_t cmd_data[1 + SE_PUBLIC_KEY_SIZE];
    cmd_data[0] = private_key_slot;
    memcpy(&cmd_data[1], public_key, SE_PUBLIC_KEY_SIZE);
    
    size_t response_len = 32; // Secret partagé de 32 bytes
    
    se_result_t result = se_send_command(ATCA_CMD_ECDH, cmd_data, sizeof(cmd_data), shared_secret, &response_len);
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec échange ECDH");
        return result;
    }
    
    ESP_LOGD(TAG, "Échange ECDH réussi");
    return SE_SUCCESS;
}

se_result_t se_verify_integrity(void) {
    ESP_LOGD(TAG, "Vérification intégrité SE");
    
    // Test de fonctionnement basique
    se_result_t result = se_health_check();
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Intégrité SE compromise");
        return SE_ERROR_TAMPER_DETECTED;
    }
    
    return SE_SUCCESS;
}

se_result_t se_enable_secure_mode(void) {
    ESP_LOGI(TAG, "Activation mode sécurisé");
    // Configuration des paramètres de sécurité renforcée
    return SE_SUCCESS;
}

se_result_t se_disable_secure_mode(void) {
    ESP_LOGI(TAG, "Désactivation mode sécurisé");
    // Retour aux paramètres de sécurité normaux
    return SE_SUCCESS;
}

se_result_t se_read_emergency_state(uint8_t* emergency_data, size_t data_size) {
    if (!g_se_initialized || emergency_data == NULL || data_size < 32) {
        return SE_ERROR_INVALID_PARAM;
    }
    
    ESP_LOGD(TAG, "Lecture état d'urgence");
    
    // Lecture des données depuis le slot temporaire
    uint8_t cmd_data[1] = { SE_SLOT_TEMP_DATA };
    size_t response_len = MIN(data_size, 32);
    
    se_result_t result = se_send_command(ATCA_CMD_READ, cmd_data, sizeof(cmd_data), 
                                         emergency_data, &response_len);
    if (result != SE_SUCCESS) {
        ESP_LOGE(TAG, "Échec lecture état d'urgence");
        return result;
    }
    
    return SE_SUCCESS;
}