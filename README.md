# SecureIoT-VIF: Framework de Vérification d'Intégrité pour Dispositifs IoT

## Description
Framework léger de détection et prévention des attaques par compromission de firmware dans les dispositifs IoT grand public, basé sur le crypto ESP32 intégré et le microcontrôleur ESP32.

## Caractéristiques Uniques
- ✅ Vérification d'intégrité en temps réel (pas seulement au boot)
- ✅ Attestation continue avec renouvellement automatique
- ✅ Détection d'anomalies comportementales sur données capteurs
- ✅ Architecture modulaire extensible
- ✅ Gestion énergétique optimisée pour IoT
- ✅ Crypto ESP32 intégré haute performance

## Hardware Requis
- ESP32-WROOM-32 (~8$)
- DHT22 (~7$)
- Composants additionnels (~3$)
- **Budget total: ~18$**

## Prérequis Logiciels
- ESP-IDF v5.x
- Git
- VS Code + ESP-IDF Extension

## Installation et Configuration
Voir `docs/INSTALLATION_V2.md` pour les instructions détaillées.

## Architecture
```
SecureIoT-VIF/
├── main/                         # Point d'entrée principal
├── components/                   # Modules du framework
│   ├── secure_element/          # Gestion crypto ESP32
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