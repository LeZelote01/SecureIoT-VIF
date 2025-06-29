/**
 * @file se_manager.h
 * @brief Gestionnaire de l'élément sécurisé ATECC608A
 * 
 * Ce module gère toutes les interactions avec l'élément sécurisé ATECC608A,
 * incluant l'initialisation, la gestion des clés, et les opérations cryptographiques.
 * 
 * @author Framework SecureIoT-VIF
 * @version 1.0.0
 * @date 2025
 */

#ifndef SE_MANAGER_H
#define SE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// ================================
// Constantes et définitions
// ================================

#define SE_SERIAL_NUMBER_SIZE       (9)
#define SE_PUBLIC_KEY_SIZE          (64)
#define SE_PRIVATE_KEY_SIZE         (32)
#define SE_SIGNATURE_SIZE           (64)
#define SE_CERTIFICATE_SIZE         (512)
#define SE_RANDOM_BYTES_SIZE        (32)
#define SE_CONFIG_ZONE_SIZE         (128)
#define SE_OTP_ZONE_SIZE            (64)

// Adresses I2C possibles pour ATECC608A
#define ATECC608A_I2C_ADDR_DEFAULT  (0xC0)
#define ATECC608A_I2C_ADDR_ALT1     (0xC2)
#define ATECC608A_I2C_ADDR_ALT2     (0xC4)
#define ATECC608A_I2C_ADDR_ALT3     (0xC6)

// Slots de configuration ATECC608A
#define SE_SLOT_DEVICE_PRIVATE_KEY   (0)    // Clé privée de l'appareil
#define SE_SLOT_ATTESTATION_KEY      (1)    // Clé d'attestation
#define SE_SLOT_ENCRYPTION_KEY       (2)    // Clé de chiffrement
#define SE_SLOT_TRANSPORT_KEY        (3)    // Clé de transport
#define SE_SLOT_ROOT_CA_PUBLIC_KEY   (8)    // Clé publique CA racine
#define SE_SLOT_INTERMEDIATE_CERT    (9)    // Certificat intermédiaire
#define SE_SLOT_DEVICE_CERT          (10)   // Certificat de l'appareil
#define SE_SLOT_TEMP_DATA            (11)   // Données temporaires

// États de l'élément sécurisé
typedef enum {
    SE_STATE_UNINITIALIZED = 0,
    SE_STATE_INITIALIZING,
    SE_STATE_CONFIGURED,
    SE_STATE_LOCKED,
    SE_STATE_ERROR,
    SE_STATE_TAMPERED
} se_state_t;

// Types d'opérations cryptographiques
typedef enum {
    SE_CRYPTO_SIGN = 0,
    SE_CRYPTO_VERIFY,
    SE_CRYPTO_ENCRYPT,
    SE_CRYPTO_DECRYPT,
    SE_CRYPTO_ECDH,
    SE_CRYPTO_HMAC,
    SE_CRYPTO_RANDOM
} se_crypto_operation_t;

// Résultat des opérations SE
typedef enum {
    SE_SUCCESS = 0,
    SE_ERROR_COMMUNICATION = -1,
    SE_ERROR_INVALID_PARAM = -2,
    SE_ERROR_NOT_INITIALIZED = -3,
    SE_ERROR_DEVICE_NOT_FOUND = -4,
    SE_ERROR_SLOT_LOCKED = -5,
    SE_ERROR_VERIFICATION_FAILED = -6,
    SE_ERROR_EXECUTION_FAILED = -7,
    SE_ERROR_TAMPER_DETECTED = -8,
    SE_ERROR_TIMEOUT = -9,
    SE_ERROR_MEMORY = -10
} se_result_t;

// ================================
// Structures de données
// ================================

/**
 * @brief Configuration de l'élément sécurisé
 */
typedef struct {
    uint8_t i2c_address;           // Adresse I2C
    int scl_gpio;                  // Pin GPIO SCL
    int sda_gpio;                  // Pin GPIO SDA
    uint32_t i2c_frequency;        // Fréquence I2C en Hz
    uint16_t wake_delay_ms;        // Délai de réveil en ms
    uint8_t max_retries;           // Nombre max de tentatives
    bool enable_crc;               // Activer vérification CRC
    bool enable_watchdog;          // Activer watchdog
} se_config_t;

/**
 * @brief Informations sur l'élément sécurisé
 */
typedef struct {
    uint8_t serial_number[SE_SERIAL_NUMBER_SIZE];
    uint16_t revision;
    uint8_t config_zone[SE_CONFIG_ZONE_SIZE];
    uint8_t otp_zone[SE_OTP_ZONE_SIZE];
    bool is_locked;
    bool is_configured;
    se_state_t state;
    uint32_t error_count;
    uint32_t operation_count;
    uint64_t last_operation_time;
} se_device_info_t;

/**
 * @brief Structure pour les clés
 */
typedef struct {
    uint8_t slot_id;
    uint8_t key_type;              // 0=Private, 1=Public, 2=Symmetric
    size_t key_size;
    uint8_t key_data[SE_PUBLIC_KEY_SIZE];
    bool is_valid;
    uint32_t usage_count;
} se_key_info_t;

/**
 * @brief Structure pour les signatures
 */
typedef struct {
    uint8_t signature[SE_SIGNATURE_SIZE];
    size_t signature_size;
    uint8_t message_hash[32];      // SHA-256 du message
    bool is_valid;
} se_signature_t;

/**
 * @brief Structure pour l'attestation
 */
typedef struct {
    uint8_t challenge[32];         // Challenge reçu
    uint8_t response[SE_SIGNATURE_SIZE]; // Réponse signée
    uint8_t device_cert[SE_CERTIFICATE_SIZE]; // Certificat de l'appareil
    uint32_t timestamp;            // Timestamp de l'attestation
    bool is_valid;
} se_attestation_t;

// ================================
// Fonctions principales
// ================================

/**
 * @brief Initialise le gestionnaire d'élément sécurisé
 * 
 * @param config Configuration de l'élément sécurisé (NULL pour config par défaut)
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t se_manager_init(const se_config_t* config);

/**
 * @brief Dé-initialise le gestionnaire d'élément sécurisé
 * 
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t se_manager_deinit(void);

/**
 * @brief Obtient les informations de l'élément sécurisé
 * 
 * @param info Pointeur vers la structure d'informations à remplir
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_get_device_info(se_device_info_t* info);

/**
 * @brief Vérifie l'état de santé de l'élément sécurisé
 * 
 * @return se_result_t SE_SUCCESS si l'élément est en bon état
 */
se_result_t se_health_check(void);

/**
 * @brief Configure l'élément sécurisé (première utilisation)
 * 
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_configure_device(void);

/**
 * @brief Verrouille la configuration de l'élément sécurisé
 * 
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_lock_configuration(void);

// ================================
// Gestion des clés
// ================================

/**
 * @brief Génère une paire de clés ECC dans un slot
 * 
 * @param slot_id Numéro du slot (0-15)
 * @param public_key Buffer pour stocker la clé publique (64 bytes)
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_generate_key_pair(uint8_t slot_id, uint8_t* public_key);

/**
 * @brief Obtient la clé publique d'un slot
 * 
 * @param slot_id Numéro du slot
 * @param public_key Buffer pour la clé publique (64 bytes)
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_get_public_key(uint8_t slot_id, uint8_t* public_key);

/**
 * @brief Écrit une clé dans un slot
 * 
 * @param slot_id Numéro du slot
 * @param key_data Données de la clé
 * @param key_size Taille de la clé
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_write_key(uint8_t slot_id, const uint8_t* key_data, size_t key_size);

/**
 * @brief Obtient les informations d'une clé
 * 
 * @param slot_id Numéro du slot
 * @param key_info Informations sur la clé
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_get_key_info(uint8_t slot_id, se_key_info_t* key_info);

// ================================
// Opérations cryptographiques
// ================================

/**
 * @brief Signe un message avec une clé privée
 * 
 * @param slot_id Numéro du slot contenant la clé privée
 * @param message_hash Hash du message à signer (32 bytes SHA-256)
 * @param signature Buffer pour la signature (64 bytes)
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_sign_message(uint8_t slot_id, const uint8_t* message_hash, uint8_t* signature);

/**
 * @brief Vérifie une signature
 * 
 * @param public_key Clé publique pour la vérification (64 bytes)
 * @param message_hash Hash du message original (32 bytes)
 * @param signature Signature à vérifier (64 bytes)
 * @return se_result_t SE_SUCCESS si la signature est valide
 */
se_result_t se_verify_signature(const uint8_t* public_key, const uint8_t* message_hash, const uint8_t* signature);

/**
 * @brief Génère des bytes aléaoires sécurisés
 * 
 * @param random_bytes Buffer pour les bytes aléatoires
 * @param length Nombre de bytes à générer (max 32)
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_generate_random(uint8_t* random_bytes, size_t length);

/**
 * @brief Calcule un HMAC
 * 
 * @param key_slot Slot contenant la clé HMAC
 * @param data Données à authentifier
 * @param data_length Longueur des données
 * @param hmac Buffer pour le HMAC (32 bytes)
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_calculate_hmac(uint8_t key_slot, const uint8_t* data, size_t data_length, uint8_t* hmac);

/**
 * @brief Effectue un échange de clés ECDH
 * 
 * @param private_key_slot Slot de la clé privée locale
 * @param public_key Clé publique distante (64 bytes)
 * @param shared_secret Secret partagé résultant (32 bytes)
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_ecdh_key_exchange(uint8_t private_key_slot, const uint8_t* public_key, uint8_t* shared_secret);

// ================================
// Gestion des certificats
// ================================

/**
 * @brief Écrit un certificat dans un slot
 * 
 * @param slot_id Numéro du slot
 * @param certificate Données du certificat
 * @param cert_size Taille du certificat
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_write_certificate(uint8_t slot_id, const uint8_t* certificate, size_t cert_size);

/**
 * @brief Lit un certificat depuis un slot
 * 
 * @param slot_id Numéro du slot
 * @param certificate Buffer pour le certificat
 * @param cert_size Pointeur vers la taille du buffer/certificat
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_read_certificate(uint8_t slot_id, uint8_t* certificate, size_t* cert_size);

// ================================
// Attestation et vérification
// ================================

/**
 * @brief Effectue une attestation de l'appareil
 * 
 * @param challenge Challenge reçu du vérifieur
 * @param challenge_size Taille du challenge
 * @param attestation Structure d'attestation à remplir
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_perform_attestation(const uint8_t* challenge, size_t challenge_size, se_attestation_t* attestation);

/**
 * @brief Vérifie l'intégrité de l'élément sécurisé
 * 
 * @return se_result_t SE_SUCCESS si l'intégrité est OK
 */
se_result_t se_verify_integrity(void);

// ================================
// Gestion d'état et monitoring
// ================================

/**
 * @brief Met à jour le heartbeat dans l'élément sécurisé
 * 
 * @param counter Compteur de heartbeat
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_update_heartbeat(uint32_t counter);

/**
 * @brief Stocke l'état d'urgence dans l'élément sécurisé
 * 
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_store_emergency_state(void);

/**
 * @brief Lit l'état d'urgence depuis l'élément sécurisé
 * 
 * @param emergency_data Buffer pour les données d'urgence
 * @param data_size Taille du buffer
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_read_emergency_state(uint8_t* emergency_data, size_t data_size);

/**
 * @brief Active le mode de sécurité renforcée
 * 
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_enable_secure_mode(void);

/**
 * @brief Désactive le mode de sécurité renforcée
 * 
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_disable_secure_mode(void);

// ================================
// Utilitaires et debugging
// ================================

/**
 * @brief Convertit un code d'erreur SE en string
 * 
 * @param error Code d'erreur SE
 * @return const char* Description de l'erreur
 */
const char* se_error_to_string(se_result_t error);

/**
 * @brief Affiche les informations de l'élément sécurisé (debug)
 */
void se_print_device_info(void);

/**
 * @brief Teste les fonctionnalités de base de l'élément sécurisé
 * 
 * @return se_result_t SE_SUCCESS si tous les tests passent
 */
se_result_t se_self_test(void);

/**
 * @brief Obtient les statistiques d'utilisation
 * 
 * @param operations_count Nombre total d'opérations
 * @param error_count Nombre d'erreurs
 * @param last_operation_time Timestamp de la dernière opération
 * @return se_result_t SE_SUCCESS en cas de succès
 */
se_result_t se_get_statistics(uint32_t* operations_count, uint32_t* error_count, uint64_t* last_operation_time);

#ifdef __cplusplus
}
#endif

#endif /* SE_MANAGER_H */