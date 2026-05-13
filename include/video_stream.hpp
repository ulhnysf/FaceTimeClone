/*
 * video_stream.hpp
 * VideoStream sınıfının bildirimi
 *
 * .hpp uzantısı: C++ başlık dosyası olduğunu belirtir
 * (sadece stil farkı, .h ile aynı işlevi görür)
 */

#ifndef VIDEO_STREAM_HPP
#define VIDEO_STREAM_HPP

#include <opencv2/opencv.hpp>  /* OpenCV: Görüntü işleme kütüphanesi */
#include <thread>              /* std::thread: Çoklu iş parçacığı    */
#include <atomic>              /* std::atomic: Thread-safe değişkenler */
#include <functional>          /* std::function: Callback tipi         */
#include <vector>
#include <cstdint>

/* Callback fonksiyon tipi:
 * void foo(const uint8_t* data, size_t len)
 * data: JPEG verisi, len: byte sayısı */
using FrameCallback = std::function<void(const uint8_t*, size_t)>;

class VideoStream {
public:
    /* Yapıcı ve yıkıcı */
    VideoStream();
    ~VideoStream();

    /* Kopyalama yasak (kamera kaynağı kopyalanamaz) */
    VideoStream(const VideoStream&) = delete;
    VideoStream& operator=(const VideoStream&) = delete;

    /* Temel işlemler */
    bool init(int camera_id = 0,
              int width     = 640,
              int height    = 480,
              int fps       = 30);
    bool start();
    void stop();

    /* Ayarlar */
    void setFrameCallback(FrameCallback cb);
    void toggleCamera();
    void setJpegQuality(int q) { m_jpeg_quality = q; }

    /* Durum sorguları */
    bool isRunning()   const { return m_is_running;   }
    bool isCameraOn()  const { return m_is_camera_on; }
    long getFrameCount() const { return m_frame_count; }

    /* Gelen kareyi çöz */
    static cv::Mat decodeFrame(const uint8_t* data, size_t len);

private:
    /* Kamera */
    cv::VideoCapture m_capture;
    int  m_camera_id;
    int  m_width;
    int  m_height;
    int  m_fps;
    int  m_jpeg_quality;

    /* Thread yönetimi */
    std::thread      m_capture_thread;
    std::atomic<bool> m_is_running;   /* Atomic: thread'ler arası güvenli okuma/yazma */
    std::atomic<bool> m_is_camera_on;
    long             m_frame_count;

    /* Callback */
    FrameCallback m_on_frame_ready;

    /* Thread fonksiyonu (private, sadece içeriden çağrılır) */
    void captureLoop();
};

#endif /* VIDEO_STREAM_HPP */
