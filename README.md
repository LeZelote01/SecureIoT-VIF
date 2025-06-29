# SecureIoT-VIF: Framework de Vérification d'Intégrité pour Dispositifs IoT

## Description
Framework léger de détection et prévention des attaques par compromission de firmware dans les dispositifs IoT grand public, basé sur l'élément sécurisé ATECC608A et le microcontrôleur ESP32.

## Caractéristiques Uniques
- ✅ Vérification d'intégrité en temps réel (pas seulement au boot)
- ✅ Attestation continue avec renouvellement automatique
- ✅ Détection d'anomalies comportementales sur données capteurs
- ✅ Architecture modulaire extensible
- ✅ Gestion énergétique optimisée pour IoT
- ✅ Intégration étroite ESP32 + ATECC608A

## Hardware Requis
- ESP32-WROOM-32 (~8$)
- ATECC608A-MAHDA (~2$) 
- DHT22 (~7$)
- Composants additionnels (~8$)
- **Budget total: ~25$**

## Prérequis Logiciels
- ESP-IDF v5.x
- CryptoAuthLib
- Git
- VS Code + ESP-IDF Extension

## Installation et Configuration
Voir `docs/INSTALLATION.md` pour les instructions détaillées.

## Architecture
```
SecureIoT-VIF/
├── main/                         # Point d'entrée principal
├── components/                   # Modules du framework
│   ├── secure_element/          # Gestion ATECC608A
│   ├── firmware_verification/   # Vérification intégrité
│   ├── attestation/            # Attestation continue
│   ├── sensor_interface/       # Interface capteurs
│   └── security_monitor/       # Monitoring sécurité
├── tests/                      # Tests unitaires
├── docs/                       # Documentation
└── tools/                      # Outils utilitaires
```

## Utilisation
```bash
# Configuration
idf.py menuconfig

# Compilation
idf.py build

# Flash du firmware
idf.py -p /dev/ttyUSB0 flash monitor
```

## Licence
MIT License - Voir LICENSE pour plus de détails.

## Contribution
Ce projet fait partie d'un mémoire de recherche sur la sécurité IoT. Les contributions sont les bienvenues.