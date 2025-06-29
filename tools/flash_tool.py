#!/usr/bin/env python3
"""
Outil de flash et configuration pour SecureIoT-VIF
Automatise le processus de compilation, flash et monitoring
"""

import os
import sys
import subprocess
import argparse
import json
from pathlib import Path

def run_command(cmd, check=True):
    """Exécute une commande et affiche le résultat"""
    print(f"🔧 Exécution: {cmd}")
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    
    if result.stdout:
        print(result.stdout)
    if result.stderr and result.returncode != 0:
        print(f"❌ Erreur: {result.stderr}")
    
    if check and result.returncode != 0:
        sys.exit(1)
    
    return result

def check_environment():
    """Vérifie l'environnement ESP-IDF"""
    print("🔍 Vérification environnement ESP-IDF...")
    
    # Vérifier ESP-IDF
    result = run_command("idf.py --version", check=False)
    if result.returncode != 0:
        print("❌ ESP-IDF non trouvé. Veuillez configurer votre environnement.")
        print("💡 Exécutez: source $HOME/esp/esp-idf/export.sh")
        sys.exit(1)
    
    print("✅ ESP-IDF configuré correctement")

def configure_project(args):
    """Configure le projet via menuconfig"""
    print("⚙️ Configuration du projet...")
    
    if args.auto_config:
        # Configuration automatique avec valeurs par défaut
        config = {
            "CONFIG_SECURE_IOT_SECURITY_LEVEL": "3",
            "CONFIG_SECURE_IOT_ENABLE_SECURE_BOOT": "y",
            "CONFIG_SECURE_IOT_I2C_SCL_GPIO": "22",
            "CONFIG_SECURE_IOT_I2C_SDA_GPIO": "21",
            "CONFIG_SECURE_IOT_DHT22_GPIO": "4",
            "CONFIG_SECURE_IOT_INTEGRITY_CHECK_INTERVAL": "60",
            "CONFIG_SECURE_IOT_ATTESTATION_INTERVAL": "30"
        }
        
        # Écrire la configuration
        with open("sdkconfig", "w") as f:
            for key, value in config.items():
                f.write(f"{key}={value}\n")
        
        print("✅ Configuration automatique appliquée")
    else:
        # Configuration interactive
        run_command("idf.py menuconfig")

def build_project():
    """Compile le projet"""
    print("🔨 Compilation du projet...")
    run_command("idf.py build")
    print("✅ Compilation terminée")

def flash_project(port):
    """Flash le firmware"""
    print(f"⚡ Flash du firmware sur {port}...")
    run_command(f"idf.py -p {port} flash")
    print("✅ Flash terminé")

def monitor_project(port):
    """Monitor les logs"""
    print(f"📺 Monitoring des logs sur {port}...")
    print("💡 Appuyez sur Ctrl+] pour quitter")
    run_command(f"idf.py -p {port} monitor")

def detect_port():
    """Détecte automatiquement le port série"""
    import serial.tools.list_ports
    
    ports = list(serial.tools.list_ports.comports())
    esp_ports = [p for p in ports if 'CP210' in p.description or 'CH340' in p.description or 'USB' in p.description]
    
    if esp_ports:
        return esp_ports[0].device
    elif ports:
        return ports[0].device
    else:
        return "/dev/ttyUSB0"  # Défaut Linux

def run_tests():
    """Exécute les tests automatisés"""
    print("🧪 Exécution des tests...")
    
    # Tests de compilation
    print("📋 Test de compilation...")
    run_command("idf.py build")
    
    # Tests de configuration
    print("📋 Test de configuration...")
    if os.path.exists("sdkconfig"):
        print("✅ Configuration trouvée")
    else:
        print("❌ Configuration manquante")
    
    print("✅ Tests terminés")

def clean_project():
    """Nettoie les fichiers de build"""
    print("🧹 Nettoyage du projet...")
    run_command("idf.py clean")
    
    # Supprimer les fichiers temporaires
    temp_files = ["sdkconfig", "sdkconfig.old"]
    for f in temp_files:
        if os.path.exists(f):
            os.remove(f)
            print(f"🗑️ Supprimé: {f}")
    
    print("✅ Nettoyage terminé")

def main():
    parser = argparse.ArgumentParser(description="Outil de développement SecureIoT-VIF")
    parser.add_argument("action", choices=["build", "flash", "monitor", "all", "config", "test", "clean"],
                        help="Action à exécuter")
    parser.add_argument("-p", "--port", help="Port série (auto-détection si non spécifié)")
    parser.add_argument("--auto-config", action="store_true", 
                        help="Configuration automatique (pas d'interface interactive)")
    
    args = parser.parse_args()
    
    # Vérifier l'environnement
    check_environment()
    
    # Détecter le port si non spécifié
    if not args.port:
        args.port = detect_port()
        print(f"🔍 Port détecté: {args.port}")
    
    # Exécuter l'action demandée
    try:
        if args.action == "config":
            configure_project(args)
        elif args.action == "build":
            build_project()
        elif args.action == "flash":
            build_project()
            flash_project(args.port)
        elif args.action == "monitor":
            monitor_project(args.port)
        elif args.action == "all":
            configure_project(args)
            build_project()
            flash_project(args.port)
            monitor_project(args.port)
        elif args.action == "test":
            run_tests()
        elif args.action == "clean":
            clean_project()
        
    except KeyboardInterrupt:
        print("\n🛑 Opération interrompue par l'utilisateur")
    except Exception as e:
        print(f"❌ Erreur: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()