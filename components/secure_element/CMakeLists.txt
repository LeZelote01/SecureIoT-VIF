# Configuration du composant secure_element
idf_component_register(
    SRCS 
        "se_manager.c"
        "crypto_operations.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        driver 
        esp_system 
        esp_log
        nvs_flash
    PRIV_REQUIRES 
        freertos
)

# Configuration des flags de compilation spécifiques à la sécurité
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Werror
    -O2
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
)

# Définition des macros de configuration
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ATECC608A_DRIVER_VERSION="2.0.0"
    CRYPTO_AUTH_LIB_ENABLED=1
)