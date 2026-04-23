cmake_minimum_required(VERSION 3.16)

project(GameForAPClass LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_PREFIX_PATH "/opt/homebrew")

find_package(SFML 3 REQUIRED COMPONENTS Audio Graphics Window)

add_executable(game
    src/main.cpp
    src/UpgradeShop.cpp
    src/entities/Enemy.cpp
    src/entities/Entity.cpp
    src/entities/Player.cpp
    src/entities/Projectile.cpp
    src/UI/DashCooldownIndicator.cpp
    src/UI/EnemyDeathEffect.cpp
    src/UI/ScreenShakeController.cpp
)

target_link_libraries(game PRIVATE SFML::Audio SFML::Graphics SFML::Window)
