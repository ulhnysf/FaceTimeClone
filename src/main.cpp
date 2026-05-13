/*
 * main.cpp
 * Uygulamanın giriş noktası ve Qt GUI katmanı
 *
 * Qt: C++ için kapsamlı bir GUI (Grafik Arayüz) kütüphanesi.
 * Windows, Linux, macOS'ta tek kodla çalışır.
 *
 * Mimari:
 * main() → QApplication → FaceTimeWindow
 *   └─ FaceTimeWindow (ana pencere)
 *       ├─ VideoStream  (kamera - C++ sınıfı)
 *       ├─ network.c    (UDP ağ - C kodu)
 *       └─ audio.c      (ses - C kodu)
 */

#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QPixmap>
#include <QImage>
#include <QStatusBar>
#include <QMessageBox>
#include <QFont>
#include <QStyle>
#include <QFormLayout>
#include <QAudioSource>
#include <QAudioSink>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QIODevice>
#include <QByteArray>
#include <QDebug>

#include <iostream>
#include <thread>
#include <atomic>
#include <cstring>

#include "../include/network.h"
#include "../include/audio.h"
#include "../include/video_stream.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif
#endif


/* ============================================================
 * LoginWindow - Giriş ekranı
 * ============================================================ */
class LoginWindow : public QWidget {
    Q_OBJECT

public:
    explicit LoginWindow(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setWindowTitle("Giris Yap");
        setFixedSize(350, 220);

        auto* layout = new QVBoxLayout(this);

        auto* title = new QLabel("🔐 FaceTime Giris", this);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("font-size: 22px; font-weight: bold; color: #4fc3f7;");
        layout->addWidget(title);

        auto* form = new QFormLayout();

        m_username = new QLineEdit(this);
        m_password = new QLineEdit(this);

        m_password->setEchoMode(QLineEdit::Password);

        form->addRow("Kullanici Adi:", m_username);
        form->addRow("Sifre:", m_password);

        layout->addLayout(form);

        m_login_btn = new QPushButton("Giris Yap", this);
        m_login_btn->setFixedHeight(40);

        layout->addWidget(m_login_btn);

        connect(m_login_btn, &QPushButton::clicked,
                this, &LoginWindow::login);
    }

signals:
    void loginSuccess();

private slots:
    void login() {
        QString username = m_username->text();
        QString password = m_password->text();

        if ((username == "uluhan" && password == "1234") ||
            (username == "kaan" && password == "1234"))
        {
            emit loginSuccess();
            close();
        }
        else {
            QMessageBox::warning(this,
                                 "Hatali Giris",
                                 "Kullanici adi veya sifre yanlis!");
        }
    }

private:
    QLineEdit*  m_username;
    QLineEdit*  m_password;
    QPushButton* m_login_btn;
};

/* ============================================================
 * CallState - Görüşme durumunu tutan basit enum
 * ============================================================ */
enum class CallState {
    IDLE,       /* Bekleniyor, görüşme yok     */
    CALLING,    /* Arama yapılıyor              */
    IN_CALL,    /* Görüşme devam ediyor         */
    ENDING      /* Görüşme sonlandırılıyor      */
};

/* ============================================================
 * FaceTimeWindow - Ana uygulama penceresi
 * QMainWindow'dan türeyen C++ sınıfı
 * ============================================================ */
class FaceTimeWindow : public QMainWindow {
    Q_OBJECT  /* Qt'nin meta-object sistemi için gerekli makro */

public:
    explicit FaceTimeWindow(QWidget* parent = nullptr)
        : QMainWindow(parent)
        , m_call_state(CallState::IDLE)
        , m_socket_fd(-1)
        , m_is_muted(false)
        , m_is_camera_on(true)
    {
        setWindowTitle("FaceTime Klonu - C/C++ & Qt");
        setMinimumSize(900, 600);
        setStyleSheet(getStyleSheet());

        buildUI();
        connectSignals();
        initNetwork();
        initAudio();
        initRealAudio();
        initVideo();

        /* UI'yı her 33ms'de güncelle (~30 FPS UI güncellemesi) */
        m_ui_timer = new QTimer(this);
        connect(m_ui_timer, &QTimer::timeout, this, &FaceTimeWindow::updateUI);
        m_ui_timer->start(33);

        statusBar()->showMessage("Hazir. Bir İP adresi girin ve arama yapin.");
    }

    ~FaceTimeWindow() {

    m_receiver_running = false;
    if (m_receiver_thread.joinable()) {
        m_receiver_thread.join();
    }

    m_video.stop();

    if (m_socket_fd >= 0) {
        socket_close(m_socket_fd);
    }

    if (m_audio_source) {
        m_audio_source->stop();
    }

    if (m_audio_sink) {
        m_audio_sink->stop();
    }

    audio_cleanup();
    network_cleanup();
}

private slots:
    /* -------------------------------------------------------
     * onCallButtonClicked()
     * "Ara" butonuna basılınca çalışır.
     * ------------------------------------------------------- */
    void onCallButtonClicked() {
        if (m_call_state == CallState::IDLE) {
            startCall();
        } else {
            endCall();
        }
    }

    /* -------------------------------------------------------
     * onMuteButtonClicked()
     * Mikrofonu sessize al / aç.
     * ------------------------------------------------------- */
    void onMuteButtonClicked() {
        m_is_muted = !m_is_muted;
        m_real_audio_enabled = !m_is_muted;

        audio_set_mute(m_is_muted ? 1 : 0);

        m_btn_mute->setText(m_is_muted ? "🔇 Sessiz" : "🎤 Mikrofon");
        m_btn_mute->setProperty("muted", m_is_muted);
        m_btn_mute->style()->unpolish(m_btn_mute);
        m_btn_mute->style()->polish(m_btn_mute);
    }

    /* -------------------------------------------------------
     * onCameraButtonClicked()
     * Kamerayı aç / kapat.
     * ------------------------------------------------------- */
    void onCameraButtonClicked() {
        m_is_camera_on = !m_is_camera_on;
        m_video.toggleCamera();
        m_btn_camera->setText(m_is_camera_on ? "📷 Kamera" : "🚫 Kamera Kapali");
    }

    /* -------------------------------------------------------
     * updateUI()
     * Timer tarafından her 33ms'de çağrılır.
     * Ekrandaki video görüntüsünü ve durum bilgilerini günceller.
     * ------------------------------------------------------- */
    void updateUI() {
        /* Kendi kamera görüntüsünü güncelle */
        if (m_latest_local_frame.rows > 0) {
            QImage img(m_latest_local_frame.data,
                       m_latest_local_frame.cols,
                       m_latest_local_frame.rows,
                       m_latest_local_frame.step,
                       QImage::Format_RGB888);
            m_local_video->setPixmap(QPixmap::fromImage(img).scaled(
                m_local_video->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }

        if (m_latest_remote_frame.rows > 0) {
            QImage img(m_latest_remote_frame.data,
                       m_latest_remote_frame.cols,
                       m_latest_remote_frame.rows,
                       m_latest_remote_frame.step,
                       QImage::Format_RGB888);

            m_remote_video->setPixmap(QPixmap::fromImage(img).scaled(
                m_remote_video->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }

        /* Görüşme süresini güncelle */
        if (m_call_state == CallState::IN_CALL) {
            auto elapsed = std::chrono::steady_clock::now() - m_call_start;
            int  seconds = (int)std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            int  mins    = seconds / 60;
            int  secs    = seconds % 60;
            m_lbl_duration->setText(QString("%1:%2")
                .arg(mins, 2, 10, QChar('0'))
                .arg(secs, 2, 10, QChar('0')));
        }

        /* Ses seviyesi göstergesi */
        float level = audio_get_level();
        int bar_width = (int)(level * 100);
        m_audio_bar->setFixedWidth(bar_width);
    }

private:
    /* -------------------------------------------------------
     * buildUI() - Arayüz bileşenlerini oluşturur
     * Qt widget hiyerarşisi:
     * centralWidget
     *   └─ mainLayout (dikey)
     *       ├─ topLayout (başlık)
     *       ├─ videoLayout (yatay)
     *       │   ├─ remote_video (büyük, karşı taraf)
     *       │   └─ local_video  (küçük, kendi görüntüsü)
     *       ├─ controlLayout (butonlar)
     *       └─ connectLayout (IP girişi)
     * ------------------------------------------------------- */
    void buildUI() {
        QWidget* central = new QWidget(this);
        setCentralWidget(central);

        auto* mainLayout = new QVBoxLayout(central);
        mainLayout->setSpacing(12);
        mainLayout->setContentsMargins(20, 20, 20, 20);

        /* ── Başlık ── */
        auto* lbl_title = new QLabel("📹 FaceTime", this);
        lbl_title->setObjectName("title");
        lbl_title->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(lbl_title);

        /* ── Video alanı ── */
        auto* videoLayout = new QHBoxLayout();

        /* Karşı tarafın büyük görüntüsü */
        m_remote_video = new QLabel(this);
        m_remote_video->setObjectName("remoteVideo");
        m_remote_video->setMinimumSize(600, 400);
        m_remote_video->setAlignment(Qt::AlignCenter);
        m_remote_video->setText("Gorusme bekleniyor...");
        videoLayout->addWidget(m_remote_video, 3);

        /* Kendi küçük görüntün */
        m_local_video = new QLabel(this);
        m_local_video->setObjectName("localVideo");
        m_local_video->setMinimumSize(200, 150);
        m_local_video->setAlignment(Qt::AlignCenter);
        m_local_video->setText("Kamera");
        videoLayout->addWidget(m_local_video, 1);

        mainLayout->addLayout(videoLayout);

        /* ── Süre ve durum ── */
        auto* statusLayout = new QHBoxLayout();
        m_lbl_duration = new QLabel("00:00", this);
        m_lbl_duration->setObjectName("duration");
        m_lbl_status   = new QLabel("Bosta", this);
        m_lbl_status->setObjectName("statusLabel");

        /* Ses seviyesi çubuğu */
        auto* audio_bg = new QWidget(this);
        audio_bg->setObjectName("audioBar_bg");
        audio_bg->setFixedSize(100, 8);
        m_audio_bar = new QWidget(audio_bg);
        m_audio_bar->setObjectName("audioBar");
        m_audio_bar->setFixedHeight(8);
        m_audio_bar->move(0, 0);

        statusLayout->addWidget(m_lbl_duration);
        statusLayout->addWidget(m_lbl_status);
        statusLayout->addStretch();
        statusLayout->addWidget(new QLabel("🎤", this));
        statusLayout->addWidget(audio_bg);

        mainLayout->addLayout(statusLayout);

        /* ── Kontrol butonları ── */
        auto* controlLayout = new QHBoxLayout();
        controlLayout->setSpacing(16);

        m_btn_call = new QPushButton("📞 Ara", this);
        m_btn_call->setObjectName("callBtn");
        m_btn_call->setFixedHeight(52);

        m_btn_mute = new QPushButton("🎤 Mikrofon", this);
        m_btn_mute->setObjectName("controlBtn");
        m_btn_mute->setFixedHeight(52);

        m_btn_camera = new QPushButton("📷 Kamera", this);
        m_btn_camera->setObjectName("controlBtn");
        m_btn_camera->setFixedHeight(52);

        m_btn_end = new QPushButton("📵 Kapat", this);
        m_btn_end->setObjectName("endBtn");
        m_btn_end->setFixedHeight(52);
        m_btn_end->setEnabled(true);

        controlLayout->addWidget(m_btn_call);
        controlLayout->addWidget(m_btn_mute);
        controlLayout->addWidget(m_btn_camera);
        controlLayout->addWidget(m_btn_end);

        mainLayout->addLayout(controlLayout);

        /* ── Bağlantı alanı ── */
        auto* connectLayout = new QHBoxLayout();

        auto* lbl_ip = new QLabel("Hedef İP:", this);
        m_input_ip   = new QLineEdit(this);
        m_input_ip->setPlaceholderText("Orn: 192.168.1.100");
        m_input_ip->setText("127.0.0.1");
        m_input_ip->setObjectName("ipInput");

        auto* lbl_port = new QLabel("Port:", this);
        m_input_port   = new QLineEdit(this);
        m_input_port->setText("9090");
        m_input_port->setObjectName("ipInput");
        m_input_port->setFixedWidth(80);

        connectLayout->addWidget(lbl_ip);
        connectLayout->addWidget(m_input_ip, 3);
        connectLayout->addWidget(lbl_port);
        connectLayout->addWidget(m_input_port, 1);

        mainLayout->addLayout(connectLayout);
    }

    /* -------------------------------------------------------
     * connectSignals() - Qt sinyal-slot bağlantıları
     * Qt'nin olay sistemi: Butona basılınca (sinyal) hangi
     * fonksiyonun (slot) çalışacağını tanımlar.
     * ------------------------------------------------------- */
    void connectSignals() {
        connect(m_btn_call,   &QPushButton::clicked, this, &FaceTimeWindow::onCallButtonClicked);
        connect(m_btn_mute,   &QPushButton::clicked, this, &FaceTimeWindow::onMuteButtonClicked);
        connect(m_btn_camera, &QPushButton::clicked, this, &FaceTimeWindow::onCameraButtonClicked);
        connect(m_btn_end,    &QPushButton::clicked, this, &FaceTimeWindow::close);
    }

    /* -------------------------------------------------------
     * initNetwork() - C'deki network fonksiyonlarını çağırır
     * C++ → C interoperability örneği
     * ------------------------------------------------------- */
    void initNetwork() {
        if (network_init() != 0) {
            QMessageBox::critical(this, "Hata", "Ag sistemi baslatilamadi!");
            return;
        }
        m_socket_fd = socket_create();
        if (m_socket_fd < 0) {
            QMessageBox::critical(this, "Hata", "Soket olusturulamadi!");
            return;
        }
        if (socket_bind(m_socket_fd, DEFAULT_PORT) != 0) {
            QMessageBox::warning(this, "Uyari", "Port baglanamadi, sadece gonderme modu.");
        }
        set_nonblocking(m_socket_fd);

        /* Alıcı thread başlat */
        m_receiver_running = true;
        m_receiver_thread  = std::thread(&FaceTimeWindow::receiveLoop, this);
    }

    void initAudio() {
        audio_init();
        /* Ses yakalanınca ağa gönder */
        audio_start_capture([](const uint8_t* frame, size_t len) {
            /* Gerçek uygulamada: packet_send(fd, frame, len, ip, port) */
            (void)frame; (void)len;
        });
    }

    void initRealAudio() {
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    auto inputDevice = QMediaDevices::defaultAudioInput();
    auto outputDevice = QMediaDevices::defaultAudioOutput();

    if (!inputDevice.isFormatSupported(format)) {
        qWarning() << "[WARN] Input audio format not supported, using preferred format.";
        format = inputDevice.preferredFormat();
    }

    m_audio_source = new QAudioSource(inputDevice, format, this);
    m_audio_sink = new QAudioSink(outputDevice, format, this);

    m_audio_source->setBufferSize(8192);
    m_audio_sink->setBufferSize(16384);
    
    m_audio_input = m_audio_source->start();
    m_audio_output = m_audio_sink->start();

    if (!m_audio_input || !m_audio_output) {
        qWarning() << "[ERROR] Real audio could not be started.";
        return;
    }

    connect(m_audio_input, &QIODevice::readyRead, this, [this]() {
        QByteArray audioData = m_audio_input->read(2048);

        if (!m_real_audio_enabled || m_call_state != CallState::IN_CALL) {
            return;
        }

        if (audioData.isEmpty() || m_socket_fd < 0) {
            return;
        }

        QString ip = m_input_ip->text().trimmed();
        int port = m_input_port->text().trimmed().toInt();

        QByteArray packet;
        packet.append("AUD", 3);
        packet.append(audioData);

        packet_send(
            m_socket_fd,
            reinterpret_cast<const uint8_t*>(packet.constData()),
            static_cast<size_t>(packet.size()),
            ip.toStdString().c_str(),
            port
        );
    });

    qDebug() << "[OK] Real audio system started.";
}

    void initVideo() {
        /* Kamera başarılı açılamazsa devam et (geliştirme ortamı) */
        if (!m_video.init(0, 320, 240, 20)) {
            statusBar()->showMessage("Kamera bulunamadi - simulasyon modunda calisiyor.");
            return;
        }
        
        m_video.setJpegQuality(45);

        /* Her kare hazır olduğunda ağa gönder ve yerel ekrana yaz */
        m_video.setFrameCallback([this](const uint8_t* frameData, size_t len) {
            /* Kendi görüntüsünü güncelle */
            cv::Mat frame = VideoStream::decodeFrame(frameData, len);
            if (!frame.empty()) {
                std::lock_guard<std::mutex> lock(m_frame_mutex);
                cv::cvtColor(frame, m_latest_local_frame, cv::COLOR_BGR2RGB);
            }
            /* Görüşmedeyse karşıya gönder */
            if (m_call_state == CallState::IN_CALL && m_socket_fd >= 0) {
                std::string ip   = m_input_ip->text().toStdString();
                int         port = m_input_port->text().toInt();
                packet_send(m_socket_fd, frameData, len, ip.c_str(), port);
            }
        });
        m_video.start();
    }

    /* -------------------------------------------------------
     * receiveLoop() - Ağdan gelen veriyi işler (ayrı thread)
     * ------------------------------------------------------- */
    void receiveLoop() {
        static uint8_t buf[PACKET_MAX_SIZE];
        char src_ip[IP_STR_LEN];

        while (m_receiver_running) {
            int received = packet_recv(m_socket_fd, buf, sizeof(buf), src_ip);
            if (received > 0 && m_call_state == CallState::IN_CALL) {
                
                /* Ses paketi mi? */
                if (received > 3 &&
                   
                    buf[0] == 'A' &&
                    buf[1] == 'U' &&
                    buf[2] == 'D')
                {
                    /*
                     * Ses paketi alindi.
                     * Loopback testte ayni bilgisayarda kendi sesimizi tekrar calmak
                     * yankiya ve cizirtiya neden oldugu icin local oynatma kapatildi.
                     * Iki farkli bilgisayarda test edilirken m_audio_output->write()
                     * satiri tekrar acilabilir.
                     */

                if (m_audio_output) {
                    
                    m_audio_output->write (
                        reinterpret_cast<const char*>(buf + 3),
                        received - 3
                    );
                    
                }

                continue;
                }
                
                /* Gelen veri video karesi mi ses mi? Basit bir ayrım:
                 * JPEG dosyaları 0xFF 0xD8 ile başlar */
                if (received > 2 && buf[0] == 0xFF && buf[1] == 0xD8) {
                    /* Video karesi */
                    cv::Mat frame = VideoStream::decodeFrame(buf, received);
                    if (!frame.empty()) {
                        std::lock_guard<std::mutex> lock(m_frame_mutex);
                        cv::cvtColor(frame, m_latest_remote_frame, cv::COLOR_BGR2RGB);
                        /* UI thread'inde güncelleme için QMetaObject::invokeMethod kullanılır */
                    }
                } else {
                    /* Ses verisi */
                    audio_play_frame(buf, received);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    /* -------------------------------------------------------
     * startCall() / endCall() - Görüşme yönetimi
     * ------------------------------------------------------- */
    void startCall() {
        QString ip   = m_input_ip->text().trimmed();
        QString port = m_input_port->text().trimmed();

        if (ip.isEmpty()) {
            QMessageBox::warning(this, "Uyari", "Lutfen bir İP adresi girin!");
            return;
        }

        bool ok = false;
        int portNumber = port.toInt(&ok);

        if (!ok || portNumber <= 0 || portNumber > 65535) {
            QMessageBox::warning(this, "Uyari", "Gecerli bir port numarasi girin!");
            return;
        }

        m_call_state  = CallState::IN_CALL;
        m_call_start  = std::chrono::steady_clock::now();

        m_btn_call->setText("📵 Kapat");
        m_btn_call->setObjectName("endBtn");
        m_btn_call->style()->unpolish(m_btn_call);
        m_btn_call->style()->polish(m_btn_call);
        m_btn_end->setEnabled(true);
        m_lbl_status->setText("Gorusme aktif: " + ip);

        statusBar()->showMessage("Gorusme basladi → " + ip + ":" + port);
        std::cout << "[OK] Gorusme basladi. Hedef: "
                  << ip.toStdString() << ":" << port.toStdString() << std::endl;
    }

    void endCall() {
        
        if (m_call_state == CallState::IDLE) {
        return;
    }
        m_call_state = CallState::IDLE;

        m_btn_call->setText("📞 Ara");
        m_btn_call->setObjectName("callBtn");
        m_btn_call->style()->unpolish(m_btn_call);
        m_btn_call->style()->polish(m_btn_call);
        m_btn_end->setEnabled(true);
        m_lbl_status->setText("Bosta");
        m_lbl_duration->setText("00:00");
        m_remote_video->setText("Gorusme bekleniyor...");

        /* Uzak video görüntüsünü temizle */
        {
            std::lock_guard<std::mutex> lock(m_frame_mutex);
            m_latest_remote_frame = cv::Mat();
        }

        statusBar()->showMessage("Gorusme sonlandirildi.");
        std::cout << "[OK] Gorusme sonlandirildi." << std::endl;
    }

    /* -------------------------------------------------------
     * getStyleSheet() - Uygulamanın görsel teması (CSS benzeri)
     * ------------------------------------------------------- */
    QString getStyleSheet() {
        return R"(
            QMainWindow, QWidget {
                background-color: #1a1a2e;
                color: #e0e0e0;
                font-family: 'Segoe UI', 'SF Pro Display', sans-serif;
            }
            QLabel#title {
                font-size: 26px;
                font-weight: bold;
                color: #4fc3f7;
                padding: 8px;
            }
            QLabel#remoteVideo {
                background-color: #0d0d1a;
                border: 2px solid #4fc3f7;
                border-radius: 12px;
                color: #555;
                font-size: 16px;
            }
            QLabel#localVideo {
                background-color: #0d1117;
                border: 2px solid #333;
                border-radius: 8px;
                color: #555;
                font-size: 12px;
            }
            QLabel#duration {
                font-size: 22px;
                font-weight: bold;
                color: #4fc3f7;
                font-family: monospace;
            }
            QLabel#statusLabel {
                color: #aaa;
                padding: 0 12px;
            }
            QPushButton#callBtn {
                background-color: #2e7d32;
                color: white;
                border: none;
                border-radius: 10px;
                font-size: 15px;
                font-weight: bold;
                padding: 0 24px;
            }
            QPushButton#callBtn:hover { background-color: #388e3c; }
            QPushButton#endBtn {
                background-color: #c62828;
                color: white;
                border: none;
                border-radius: 10px;
                font-size: 15px;
                font-weight: bold;
                padding: 0 24px;
            }
            QPushButton#endBtn:hover { background-color: #e53935; }
            QPushButton#controlBtn {
                background-color: #263238;
                color: #ccc;
                border: 1px solid #37474f;
                border-radius: 10px;
                font-size: 14px;
                padding: 0 20px;
            }
            QPushButton#controlBtn:hover { background-color: #37474f; }
            QPushButton:disabled { opacity: 0.4; }
            QLineEdit#ipInput {
                background-color: #0d1117;
                color: #e0e0e0;
                border: 1px solid #37474f;
                border-radius: 8px;
                padding: 8px 12px;
                font-size: 14px;
            }
            QLineEdit#ipInput:focus { border-color: #4fc3f7; }
            QStatusBar { color: #888; }
            QWidget#audioBar_bg { background-color: #263238; border-radius: 4px; }
            QWidget#audioBar { background-color: #4fc3f7; border-radius: 4px; }
        )";
    }

    /* ── Üye değişkenler ── */
    CallState  m_call_state;
    int        m_socket_fd;
    bool       m_is_muted;
    bool       m_is_camera_on;

    /* Qt widget'ları */
    QLabel*     m_remote_video;
    QLabel*     m_local_video;
    QLabel*     m_lbl_duration;
    QLabel*     m_lbl_status;
    QWidget*    m_audio_bar;
    QPushButton* m_btn_call;
    QPushButton* m_btn_mute;
    QPushButton* m_btn_camera;
    QPushButton* m_btn_end;
    QLineEdit*  m_input_ip;
    QLineEdit*  m_input_port;
    QTimer*     m_ui_timer;

    /* Video ve ses */
    VideoStream m_video;
    cv::Mat     m_latest_local_frame;
    cv::Mat     m_latest_remote_frame;
    std::mutex  m_frame_mutex;

    /* Gerçek ses aktarımı */
    QAudioSource* m_audio_source = nullptr;
    QAudioSink*   m_audio_sink = nullptr;
    QIODevice*    m_audio_input = nullptr;
    QIODevice*    m_audio_output = nullptr;

    bool m_real_audio_enabled = true;

    /* Ağ */
    std::thread  m_receiver_thread;
    std::atomic<bool> m_receiver_running{false};

    /* Görüşme süresi */
    std::chrono::steady_clock::time_point m_call_start;
};

/* ============================================================
 * main() - Programın giriş noktası
 * Her C/C++ programı buradan başlar.
 * ============================================================ */
int main(int argc, char* argv[]) {
    /* QApplication: Qt'nin olay döngüsünü yönetir */
    QApplication app(argc, argv);
    app.setApplicationName("FaceTime Clone");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("BilgisayarAglari");

    LoginWindow login;
    FaceTimeWindow window;

    QObject::connect(&login, &LoginWindow::loginSuccess,
                     [&]() {
                         window.show();
                     });

    login.show();

    std::cout << "=== FaceTime Klonu baslatildi ===" << std::endl;
    std::cout << "Arayuz: Qt 6.x" << std::endl;
    std::cout << "Ag: C UDP Sockets" << std::endl;
    std::cout << "Video: OpenCV" << std::endl;
    std::cout << "=================================" << std::endl;

    /* Qt olay döngüsünü başlat - pencere kapanana kadar burada kalır */
    return app.exec();
}

/* Qt'nin MOC (Meta-Object Compiler) sistemi için gerekli */
#ifndef __INTELLISENSE__
#include "main.moc"
#endif
