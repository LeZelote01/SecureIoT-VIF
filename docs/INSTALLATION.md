# Guide d'Installation Complet - SecureIoT-VIF

## Vue d'Ensemble

SecureIoT-VIF est un framework de sécurité IoT avancé pour ESP32 avec ATECC608A, offrant une vérification d'intégrité en temps réel et une attestation continue.

## Prérequis Hardware

### Composants Requis (Budget: ~25-30$)

| Composant | Prix Estimé | Fournisseur Recommandé |
|-----------|-------------|------------------------|
| ESP32-WROOM-32 | ~8$ | Espressif, Adafruit, Amazon |
| ATECC608A-MAHDA | ~2$ | Microchip, Digi-Key, Mouser |
| DHT22 | ~7$ | Adafruit, Amazon, AliExpress |
| Breadboard + Câbles | ~5$ | Amazon, AliExpress |
| Résistances pull-up (4.7kΩ) | ~2$ | Digi-Key, Mouser |
| Alimentation USB | ~3$ | Amazon |

### Schéma de Connexion

```
ESP32-WROOM-32      ATECC608A-MAHDA
GPIO 21 (SDA) -------- Pin 5 (SDA)
GPIO 22 (SCL) -------- Pin 6 (SCL)
3.3V -------------- Pin 8 (VCC)
GND --------------- Pin 4 (GND)
                   Pin 7 (NC)

DHT22 Sensor
GPIO 4 ----------- Data Pin
GPIO 5 ----------- Power Pin (optionnel)
3.3V ------------- VCC Pin
GND -------------- GND Pin

Résistances Pull-up:
4.7kΩ entre GPIO 21 et 3.3V
4.7kΩ entre GPIO 22 et 3.3V
```

## Installation du Environnement de Développement

### 1. Installation ESP-IDF

#### Linux/macOS:
```bash
# Clone du repository ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# Installation des outils
./install.sh esp32

# Configuration de l'environnement
. ./export.sh
```

#### Windows:
1. Télécharger ESP-IDF Tools Installer depuis le site Espressif
2. Suivre l'assistant d'installation
3. Ouvrir "ESP-IDF Command Prompt"

### 2. Vérification de l'Installation

```bash
# Vérifier la version ESP-IDF
idf.py --version

# Doit afficher ESP-IDF v5.x ou supérieur
```

### 3. Installation des Dépendances Python

```bash
# Installation des paquets Python requis
pip install -r SecureIoT-VIF/requirements.txt
```

## Compilation et Flash du Firmware

### 1. Clonage du Projet

```bash
# Cloner le repository du projet
git clone <votre-repo>/SecureIoT-VIF.git
cd SecureIoT-VIF
```

### 2. Configuration du Projet

```bash
# Configuration initiale
idf.py menuconfig
```

#### Configuration Recommandée:

**SecureIoT-VIF Configuration:**
- Security Level: 3 (High)
- Enable Secure Boot: Yes
- Enable Flash Encryption: Yes
- I2C SCL GPIO: 22
- I2C SDA GPIO: 21
- DHT22 GPIO: 4
- Integrity Check Interval: 60 seconds
- Attestation Interval: 30 seconds

**Partition Table:**
- Partition Table: "Custom partition table CSV"
- Custom partition CSV file: "partitions.csv"

**Component config → ESP32-specific:**
- Support for external, SPI-connected RAM: No
- CPU frequency: 240 MHz

### 3. Compilation

```bash
# Compilation du firmware
idf.py build
```

### 4. Flash et Monitoring

```bash
# Flash du firmware (remplacer /dev/ttyUSB0 par votre port)
idf.py -p /dev/ttyUSB0 flash

# Monitoring des logs
idf.py -p /dev/ttyUSB0 monitor

# Flash + Monitor en une commande
idf.py -p /dev/ttyUSB0 flash monitor
```

## Configuration Avancée

### 1. Configuration ATECC608A

Le framework détecte automatiquement l'ATECC608A au démarrage. Si vous rencontrez des problèmes :

```bash
# Test de communication I2C
i2cdetect -y 1  # Linux
# L'ATECC608A devrait apparaître à l'adresse 0x60
```

### 2. Configuration WiFi (Optionnel)

Éditez `main/app_config.h` :

```c
#define WIFI_SSID_MAX_LEN               (32)
#define WIFI_PASSWORD_MAX_LEN           (64)
```

Puis dans `idf.py menuconfig` :
- SecureIoT-VIF Configuration → Network Configuration
- WiFi SSID: "YourNetworkName"
- WiFi Password: "YourPassword"

### 3. Personnalisation des Seuils de Sécurité

Dans `main/app_config.h` :

```c
// Seuils d'anomalie capteurs
#define TEMP_ANOMALY_THRESHOLD          (5.0f)    // °C
#define HUMIDITY_ANOMALY_THRESHOLD      (15.0f)   // %

// Intervalles de vérification
#define INTEGRITY_CHECK_INTERVAL_US     (60000000) // 60s
#define ATTESTATION_INTERVAL_MS         (30000)    // 30s
```

## Tests et Validation

### 1. Tests de Base

Après le flash, vérifiez dans les logs :

```
I (123) SECURE_IOT_VIF: === Démarrage SecureIoT-VIF v1.0.0 ===
I (234) SE_MANAGER: Gestionnaire SE initialisé avec succès
I (345) INTEGRITY_CHECKER: Vérificateur d'intégrité initialisé
I (456) ATTESTATION_MGR: Gestionnaire d'attestation initialisé
I (567) SENSOR_MANAGER: Gestionnaire de capteurs initialisé
```

### 2. Test ATECC608A

```
I (678) SE_MANAGER: Appareil: Serial=0x123456789ABCDEF0, Rev=0x6000
I (789) SE_MANAGER: Auto-test réussi
```

### 3. Test DHT22

```
I (890) SENSOR_MANAGER: Lecture capteur: T=23.5°C, H=65.2%, Q=100
```

### 4. Test Vérification d'Intégrité

```
I (1000) INTEGRITY_CHECKER: Vérification complète terminée: OK (150 ms)
I (1001) INTEGRITY_CHECKER: Chunks: 64 total, 64 vérifiés, 0 corrompus
```

## Dépannage

### Problèmes Courants

#### 1. ATECC608A Non Détecté

**Symptômes:**
```
E (123) SE_MANAGER: Échec test de santé initial: Erreur communication
```

**Solutions:**
- Vérifier les connexions I2C (SDA/SCL)
- Vérifier les résistances pull-up (4.7kΩ)
- Tester avec `i2cdetect`
- Vérifier l'alimentation 3.3V

#### 2. DHT22 Lecture Échouée

**Symptômes:**
```
W (456) SENSOR_MANAGER: Lecture capteur échouée: Timeout
```

**Solutions:**
- Vérifier la connexion GPIO 4
- Attendre 2 secondes entre les lectures
- Vérifier l'alimentation du capteur

#### 3. Erreurs de Compilation

**mbedTLS manquant:**
```bash
# Dans idf.py menuconfig
Component config → mbedTLS → Enable mbedTLS
```

**Problèmes de mémoire:**
```bash
# Augmenter la taille de la pile dans menuconfig
Component config → ESP32-specific → Main task stack size: 8192
```

### Debug Avancé

#### 1. Logs Détaillés

Dans `main/app_config.h` :

```c
#define CONFIG_LOG_DEFAULT_LEVEL_DEBUG  1
```

#### 2. Monitoring des Performances

```bash
# Dans les logs, chercher:
I (XXX) INTEGRITY_CHECKER: Vérification complète terminée: OK (XXX ms)
I (XXX) CRYPTO_OPS: SHA-256 (1024 bytes): XXX µs
```

#### 3. Statistiques Système

Le framework affiche périodiquement :

```
I (XXX) INTEGRITY_CHECKER: === Statistiques Intégrité ===
I (XXX) INTEGRITY_CHECKER: Vérifications totales: 100
I (XXX) INTEGRITY_CHECKER: Réussies: 100, Échouées: 0
```

## Optimisation et Performance

### 1. Optimisation Mémoire

- Réduire `INTEGRITY_MAX_CHUNKS` si mémoire limitée
- Ajuster `ANOMALY_HISTORY_SIZE` selon les besoins
- Utiliser `CONFIG_SECURE_IOT_ENABLE_POWER_SAVE` pour économiser l'énergie

### 2. Optimisation Sécurité

- Activer Flash Encryption en production
- Configurer Secure Boot v2
- Utiliser des certificats réels pour l'ATECC608A

### 3. Optimisation Performance

- Ajuster `INTEGRITY_CHECK_INTERVAL_US` selon les besoins
- Réduire `SECURITY_MONITOR_INTERVAL_MS` pour plus de réactivité
- Optimiser `SENSOR_READ_INTERVAL_MS`

## Structure des Fichiers Générés

```
SecureIoT-VIF/
├── build/                     # Fichiers de compilation
├── main/                      # Code principal
├── components/                # Modules du framework
│   ├── secure_element/       # Gestion ATECC608A
│   ├── firmware_verification/ # Vérification intégrité
│   ├── attestation/          # Attestation continue
│   ├── sensor_interface/     # Interface capteurs
│   └── security_monitor/     # Monitoring sécurité
├── docs/                     # Documentation
└── tools/                    # Outils utilitaires
```

## Support et Ressources

### Documentation Technique

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
- [ATECC608A Datasheet](https://www.microchip.com/en-us/product/ATECC608A)
- [DHT22 Datasheet](https://www.sparkfun.com/datasheets/Sensors/Temperature/DHT22.pdf)

### Communauté

- Forum ESP32: https://esp32.com/
- GitHub Issues: <votre-repo>/SecureIoT-VIF/issues

---

**Remarque:** Ce framework est conçu pour la recherche et l'éducation. Pour un déploiement en production, des adaptations supplémentaires de sécurité peuvent être nécessaires.