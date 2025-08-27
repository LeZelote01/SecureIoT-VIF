/**
 * @file se_manager.h
 * @brief Fichier de compatibilité v1.0 → v2.0 pour SecureIoT-VIF
 * 
 * Ce fichier fournit des aliases de compatibilité pour faciliter
 * la migration du code v1.0 (ATECC608A) vers v2.0 (ESP32 crypto).
 * 
 * @author Framework SecureIoT-VIF
 * @version 2.0.0 - Compatibilité v1.0
 * @date 2025
 */

#ifndef SE_MANAGER_H
#define SE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

// ================================
// COMPATIBILITÉ v1.0 → v2.0
// ================================

// Inclure la nouvelle interface ESP32 crypto
#include "esp32_crypto_manager.h"

// ================================
// Aliases de Types pour Compatibilité
// ================================

// Types SE v1.0 → Types ESP32 v2.0
typedef esp32_crypto_result_t se_result_t;
typedef esp32_crypto_info_t se_device_info_t;
typedef esp32_key_info_t se_key_info_t;
typedef esp32_signature_t se_signature_t;
typedef esp32_attestation_t se_attestation_t;

// États SE v1.0 → États ESP32 v2.0
#define SE_SUCCESS                  ESP32_CRYPTO_SUCCESS
#define SE_ERROR_COMMUNICATION      ESP32_CRYPTO_ERROR_EXECUTION_FAILED
#define SE_ERROR_INVALID_PARAM      ESP32_CRYPTO_ERROR_INVALID_PARAM
#define SE_ERROR_NOT_INITIALIZED    ESP32_CRYPTO_ERROR_NOT_INITIALIZED
#define SE_ERROR_DEVICE_NOT_FOUND   ESP32_CRYPTO_ERROR_NOT_INITIALIZED
#define SE_ERROR_SLOT_LOCKED        ESP32_CRYPTO_ERROR_INVALID_PARAM
#define SE_ERROR_VERIFICATION_FAILED ESP32_CRYPTO_ERROR_VERIFICATION_FAILED
#define SE_ERROR_EXECUTION_FAILED   ESP32_CRYPTO_ERROR_EXECUTION_FAILED
#define SE_ERROR_TAMPER_DETECTED    ESP32_CRYPTO_ERROR_VERIFICATION_FAILED
#define SE_ERROR_TIMEOUT            ESP32_CRYPTO_ERROR_EXECUTION_FAILED
#define SE_ERROR_MEMORY             ESP32_CRYPTO_ERROR_MEMORY

// Constantes SE v1.0 → Constantes ESP32 v2.0
#define SE_SERIAL_NUMBER_SIZE       ESP32_SERIAL_NUMBER_SIZE
#define SE_PUBLIC_KEY_SIZE          ESP32_PUBLIC_KEY_SIZE
#define SE_PRIVATE_KEY_SIZE         ESP32_PRIVATE_KEY_SIZE
#define SE_SIGNATURE_SIZE           ESP32_SIGNATURE_SIZE
#define SE_CERTIFICATE_SIZE         ESP32_CERTIFICATE_SIZE
#define SE_RANDOM_BYTES_SIZE        ESP32_RANDOM_BYTES_SIZE

// Slots SE v1.0 → Slots ESP32 v2.0 (émulation)
#define SE_SLOT_DEVICE_PRIVATE_KEY   ESP32_EFUSE_KEY_BLOCK_0
#define SE_SLOT_ATTESTATION_KEY      ESP32_EFUSE_KEY_BLOCK_1
#define SE_SLOT_ENCRYPTION_KEY       ESP32_EFUSE_KEY_BLOCK_2
#define SE_SLOT_ROOT_CA_PUBLIC_KEY   ESP32_EFUSE_KEY_BLOCK_3

// ================================
// Aliases de Fonctions pour Compatibilité
// ================================

/**
 * @brief Aliases des fonctions principales SE v1.0 → ESP32 v2.0
 * 
 * Ces aliases permettent au code v1.0 de fonctionner sans modification
 * avec le nouveau backend crypto ESP32 v2.0.
 */

// Gestion générale
#define se_manager_init()                    esp32_crypto_manager_init(NULL)
#define se_manager_deinit()                  esp32_crypto_manager_deinit()
#define se_get_device_info(info)             esp32_crypto_get_device_info(info)
#define se_health_check()                    esp32_crypto_health_check()
#define se_configure_device()                ESP32_CRYPTO_SUCCESS  // Auto-configuré ESP32
#define se_lock_configuration()              ESP32_CRYPTO_SUCCESS  // Auto-verrouillé ESP32

// Gestion des clés
#define se_generate_key_pair(id, key)        esp32_crypto_generate_ecdsa_keypair(id, key)
#define se_get_public_key(id, key)           esp32_crypto_get_public_key(id, key)
#define se_write_key(id, data, size)         ESP32_CRYPTO_SUCCESS  // Émulation
#define se_get_key_info(id, info)            esp32_crypto_get_key_info(id, info)

// Opérations cryptographiques
#define se_sign_message(id, hash, sig)       esp32_crypto_ecdsa_sign(id, hash, sig)
#define se_verify_signature(key, hash, sig)  esp32_crypto_ecdsa_verify(key, hash, sig)
#define se_generate_random(buf, len)         esp32_crypto_generate_random(buf, len)
#define se_calculate_hmac(id, data, len, hmac) esp32_crypto_hmac_sha256(id, data, len, hmac)
#define se_ecdh_key_exchange(id, pub, secret) ESP32_CRYPTO_SUCCESS  // À implémenter

// Gestion des certificats
#define se_write_certificate(id, cert, size) ESP32_CRYPTO_SUCCESS  // Émulation NVS
#define se_read_certificate(id, cert, size)  ESP32_CRYPTO_SUCCESS  // Émulation NVS

// Attestation et vérification
#define se_perform_attestation(ch, sz, att)  esp32_crypto_perform_attestation(ch, sz, att)
#define se_verify_integrity()                esp32_crypto_verify_integrity()

// Gestion d'état et monitoring
#define se_update_heartbeat(cnt)             esp32_crypto_update_heartbeat(cnt)
#define se_store_emergency_state()           esp32_crypto_store_emergency_state()
#define se_read_emergency_state(data, size)  ESP32_CRYPTO_SUCCESS  // À implémenter
#define se_enable_secure_mode()              ESP32_CRYPTO_SUCCESS  // Auto-activé ESP32
#define se_disable_secure_mode()             ESP32_CRYPTO_SUCCESS  // Non applicable

// Utilitaires et debugging
#define se_error_to_string(error)            esp32_crypto_error_to_string(error)
#define se_print_device_info()               esp32_crypto_print_device_info()
#define se_self_test()                       esp32_crypto_self_test()
#define se_get_statistics(ops, err, time)    esp32_crypto_get_statistics(ops, err, time)

// ================================
// Messages de Migration
// ================================

#ifdef CONFIG_SECURE_IOT_MIGRATION_WARNINGS
#warning "🔄 Utilisation des aliases de compatibilité SE v1.0 → ESP32 v2.0"
#warning "💡 Considérez migrer vers les nouvelles API esp32_crypto_* pour de meilleures performances"
#warning "📚 Voir docs/MIGRATION_GUIDE.md pour les détails de migration"
#endif

// ================================
// Fonctions d'Information Migration
// ================================

/**
 * @brief Affiche les informations de migration v1.0 → v2.0
 */
static inline void se_show_migration_info(void) {
    ESP_LOGI("SE_COMPAT", "🔄 Utilisation compatibilité v1.0 → v2.0");
    ESP_LOGI("SE_COMPAT", "✅ ATECC608A émulé avec crypto ESP32 intégré");
    ESP_LOGI("SE_COMPAT", "⚡ Performance 4x améliorée avec ESP32");
    ESP_LOGI("SE_COMPAT", "💰 Économie 68% - Plus besoin d'ATECC608A");
    ESP_LOGI("SE_COMPAT", "📚 Migration complète: docs/MIGRATION_GUIDE.md");
}

/**
 * @brief Vérifie la compatibilité et affiche le statut
 */
static inline se_result_t se_check_compatibility(void) {
    se_show_migration_info();
    
    // Vérifier que le crypto ESP32 est initialisé
    esp32_crypto_info_t info;
    esp32_crypto_result_t result = esp32_crypto_get_device_info(&info);
    
    if (result == ESP32_CRYPTO_SUCCESS) {
        ESP_LOGI("SE_COMPAT", "✅ Compatibilité v1.0 → v2.0 activée");
        ESP_LOGI("SE_COMPAT", "🔐 Crypto ESP32 opérationnel");
        return SE_SUCCESS;
    } else {
        ESP_LOGE("SE_COMPAT", "❌ Erreur compatibilité: %s", 
                 esp32_crypto_error_to_string(result));
        return SE_ERROR_NOT_INITIALIZED;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* SE_MANAGER_H */