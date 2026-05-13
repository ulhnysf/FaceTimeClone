# CMakeLists.txt
# CMake: Projeyi derlemek için kullanılan araç.
# "Hangi dosyaları, hangi kütüphanelerle derle" tanımlar.
# cmake → Makefile üretir → make → .exe / program üretir

cmake_minimum_required(VERSION 3.16)

project(FaceTimeClone
    VERSION 1.0.0
    DESCRIPTION "C ve C++ ile UDP tabanlı görüntülü görüşme uygulaması"
    LANGUAGES C CXX
)

# C ve C++ standartlarını belirle
set(CMAKE_C_STANDARD   11)   # C11: Modern C standardı
set(CMAKE_CXX_STANDARD 17)   # C++17: std::optional, structured bindings vb.
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimizasyon: Release modunda derle (daha hızlı)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Derleyici uyarıları (hata ayıklamaya yardımcı)
if(MSVC)
    add_compile_options(/W4)        # Windows (MSVC)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)  # Linux/Mac (GCC/Clang)
endif()

# ── Bağımlılıkları bul ──

# Qt6: GUI kütüphanesi
# Qt Widgets: Pencere, buton vb. bileşenler için
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia)
qt_standard_project_setup()

# OpenCV: Görüntü işleme (kamera, JPEG sıkıştırma)
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Threads: std::thread için
find_package(Threads REQUIRED)

# ── Kaynak dosyaları ──
set(C_SOURCES
    src/network.c    # UDP ağ katmanı (C)
    src/audio.c      # Ses yönetimi (C)
)

set(CPP_SOURCES
    src/main.cpp         # Ana pencere ve Qt GUI (C++)
    src/video_stream.cpp # Kamera ve video akışı (C++)
)

set(HEADERS
    include/network.h
    include/audio.h
    include/video_stream.hpp
)

# ── Çalıştırılabilir dosyayı oluştur ──
qt_add_executable(FaceTimeClone
    ${C_SOURCES}
    ${CPP_SOURCES}
    ${HEADERS}
)

# Include dizinlerini ekle
target_include_directories(FaceTimeClone PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

# Kütüphaneleri bağla
target_link_libraries(FaceTimeClone PRIVATE
    Qt6::Core           # Qt temel kütüphanesi
    Qt6::Widgets        # Qt GUI bileşenleri
    Qt6::Multimedia     # Qt ses/multimedia bileşenleri
    ${OpenCV_LIBS}      # OpenCV kütüphaneleri
    Threads::Threads    # POSIX thread (pthread)
    # m                   # Matematik kütüphanesi Linux içindir, Windows'ta kullanılmaz
)

# Windows'ta extra ağ kütüphanesi gerekir
if(WIN32)
    target_link_libraries(FaceTimeClone PRIVATE ws2_32)
endif()

# Kurulum komutu (isteğe bağlı): sudo make install
install(TARGETS FaceTimeClone
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
