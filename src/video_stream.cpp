/*
 * video_stream.cpp
 * Kamera yakalama ve video akışı yöneticisi (C++ ile yazılmış)
 *
 * OpenCV kütüphanesi kullanarak kameradan görüntü alır,
 * MJPEG formatına sıkıştırır ve ağa göndermek için
 * callback ile üst katmana iletir.
 *
 * MJPEG: Her kareyi ayrı JPEG olarak sıkıştırır.
 * Avantajı: Basit, anlık, çözme hızlı.
 * Dezavantajı: H.264'e göre daha fazla bant genişliği.
 */

#include "../include/video_stream.hpp"
#include <iostream>
#include <chrono>
#include <stdexcept>

/* -------------------------------------------------------
 * VideoStream::VideoStream()
 * Yapıcı fonksiyon. Nesne oluşturulunca çalışır.
 * RAII prensibi: Kaynaklar constructor'da alınır,
 *               destructor'da serbest bırakılır.
 * ------------------------------------------------------- */
VideoStream::VideoStream()
    : m_camera_id(0)
    , m_width(320)
    , m_height(240)
    , m_fps(20)
    , m_jpeg_quality(45)
    , m_is_running(false)
    , m_is_camera_on(true)
    , m_frame_count(0)
    , m_on_frame_ready(nullptr)
{
    std::cout << "[OK] VideoStream nesnesi olusturuldu." << std::endl;
}

/* -------------------------------------------------------
 * VideoStream::~VideoStream()
 * Yıkıcı fonksiyon. Nesne silinince otomatik çağrılır.
 * Kamerayı ve thread'i temiz kapatır.
 * ------------------------------------------------------- */
VideoStream::~VideoStream() {
    stop();
    std::cout << "[OK] VideoStream nesnesi yok edildi." << std::endl;
}

/* -------------------------------------------------------
 * VideoStream::init()
 * Kamerayı açar ve çözünürlük/FPS ayarlarını yapar.
 * camera_id: 0 = dahili kamera, 1 = harici USB kamera
 * Dönüş: true başarılı, false hata
 * ------------------------------------------------------- */
bool VideoStream::init(int camera_id, int width, int height, int fps) {
    m_camera_id = camera_id;
    m_width     = width;
    m_height    = height;
    m_fps       = fps;

    /* OpenCV ile kamerayı aç */
    m_capture.open(camera_id);
    if (!m_capture.isOpened()) {
        std::cerr << "[HATA] Kamera acilamadi." << std::endl;
        std::cerr << "       Kamera bagli mi? Driver kurulu mu?" << std::endl;
        return false;
    }

    /* Kamera özelliklerini ayarla */
    m_capture.set(cv::CAP_PROP_FRAME_WIDTH,  m_width);
    m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, m_height);
    m_capture.set(cv::CAP_PROP_FPS,          m_fps);

    std::cout << "[OK] Kamera acildi." << std::endl;
    std::cout << "     Cozunurluk: " << m_width << "x" << m_height << std::endl;
    std::cout << "     FPS: " << m_fps << std::endl;

    return true;
}

/* -------------------------------------------------------
 * VideoStream::start()
 * Ayrı bir thread'de video yakalamayı başlatır.
 * Thread kullanıyoruz çünkü kamera okuma bloklanabilir
 * ve bu UI thread'ini dondurmamalı.
 * ------------------------------------------------------- */
bool VideoStream::start() {
    if (m_is_running) {
        std::cout << "[UYARI] Video akisi zaten calisiyor." << std::endl;
        return true;
    }

    m_is_running = true;
    /* std::thread: C++11 ile gelen yerleşik thread desteği */
    m_capture_thread = std::thread(&VideoStream::captureLoop, this);

    std::cout << "[OK] Video yakalama basladi." << std::endl;
    return true;
}

/* -------------------------------------------------------
 * VideoStream::stop()
 * Video akışını durdurur ve thread'in bitmesini bekler.
 * join(): Thread tamamen bitene kadar bekle (güvenli kapatma)
 * ------------------------------------------------------- */
void VideoStream::stop() {
    m_is_running = false;

    if (m_capture_thread.joinable()) {
        m_capture_thread.join(); /* Thread bitene kadar bekle */
    }

    if (m_capture.isOpened()) {
        m_capture.release();
        std::cout << "[OK] Kamera serbest birakildi." << std::endl;
    }
}

/* -------------------------------------------------------
 * VideoStream::captureLoop()
 * Thread içinde sürekli dönen ana döngü.
 * Her iterasyonda:
 * 1. Kameradan bir kare al
 * 2. JPEG'e sıkıştır
 * 3. Callback ile üst katmana ilet
 * 4. FPS sınırlaması için bekle
 * ------------------------------------------------------- */
void VideoStream::captureLoop() {
    cv::Mat frame;
    std::vector<uint8_t> jpeg_buf;

    /* JPEG sıkıştırma parametreleri */
    std::vector<int> encode_params = {
        cv::IMWRITE_JPEG_QUALITY, m_jpeg_quality
    };

    /* Frame süresi: 1/FPS saniye (mikrosaniye cinsinden) */
    auto frame_duration = std::chrono::microseconds(1000000 / m_fps);

    while (m_is_running) {
        auto frame_start = std::chrono::steady_clock::now();

        /* Kameradan kare al */
        if (!m_capture.read(frame) || frame.empty()) {
            std::cerr << "[UYARI] Bos kare alindi, atlaniyor..." << std::endl;
            continue;
        }

        /* Kamera kapalıysa siyah kare gönder (gizlilik modu) */
        if (!m_is_camera_on) {
            frame = cv::Mat::zeros(m_height, m_width, CV_8UC3);
            /* İsteğe bağlı: Ortaya "Kamera Kapalı" yazısı ekle */
            cv::putText(frame, "Kamera Kapali",
                        cv::Point(m_width/2 - 120, m_height/2),
                        cv::FONT_HERSHEY_SIMPLEX, 1.0,
                        cv::Scalar(128, 128, 128), 2);
        }

        /* BGR'den RGB'ye çevir (OpenCV BGR, ağ standartları RGB kullanır) */
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

        /* JPEG sıkıştırma: Ham kare çok büyük, ağda göndermek için küçültüyoruz
         * 320x240 ham BGR = 320*240*3 = ~225KB/kare
         * 320x240 JPEG kalite 45 = daha küçük paket boyutu
         */
        cv::imencode(".jpg", frame, jpeg_buf, encode_params);

        /* Callback çağır: Sıkıştırılmış veriyi üst katmana ilet */
        if (m_on_frame_ready && !jpeg_buf.empty()) {
            m_on_frame_ready(jpeg_buf.data(), jpeg_buf.size());
        }

        m_frame_count++;

        /* FPS sınırlaması: Bir sonraki kareye kadar bekle */
        auto elapsed = std::chrono::steady_clock::now() - frame_start;
        if (elapsed < frame_duration) {
            std::this_thread::sleep_for(frame_duration - elapsed);
        }
    }

    std::cout << "[OK] Yakalama dongusu bitti. Toplam kare: "
              << m_frame_count << std::endl;
}

/* -------------------------------------------------------
 * VideoStream::setFrameCallback()
 * Her kare hazır olduğunda çağrılacak fonksiyonu kaydeder.
 * Bu fonksiyon tipik olarak kareyi ağa gönderir.
 * ------------------------------------------------------- */
void VideoStream::setFrameCallback(FrameCallback cb) {
    m_on_frame_ready = cb;
}

/* -------------------------------------------------------
 * VideoStream::toggleCamera()
 * Kamerayı açıp kapatır (gizlilik modu).
 * ------------------------------------------------------- */
void VideoStream::toggleCamera() {
    m_is_camera_on = !m_is_camera_on;
    std::cout << "[OK] Kamera " << (m_is_camera_on ? "acildi." : "kapatildi.") << std::endl;
}

/* -------------------------------------------------------
 * VideoStream::decodeFrame()
 * Ağdan gelen JPEG verisini çözer ve görüntülenebilir
 * cv::Mat nesnesine dönüştürür.
 * ------------------------------------------------------- */
cv::Mat VideoStream::decodeFrame(const uint8_t* data, size_t len) {
    std::vector<uint8_t> buf(data, data + len);
    cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
    if (img.empty()) {
        std::cerr << "[HATA] JPEG cozme basarisiz!" << std::endl;
    }
    return img;
}
