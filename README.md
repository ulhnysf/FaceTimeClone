# 📹 FaceTime Klonu — C & C++ ile UDP Görüntülü Görüşme Uygulaması


## Proje Ekibi

|        İsim        |                                                   Görev                                                         |
|--------------------|-----------------------------------------------------------------------------------------------------------------|
| Uluhan Yusuf Dokay | Qt GUI, OpenCV video işleme, Qt Multimedia ses sistemi, entegrasyon, test ve proje düzenlemeleri                |
| Kaan Erol          | UDP ağ altyapısı (network.c), socket oluşturma/gönderme/alma işlemleri, temel ses modülü (audio.c) ve projenin ilk ağ/ses altyapısının hazırlanması |



## Proje Özeti

Bu proje, **C** ve **C++** programlama dilleri kullanılarak sıfırdan geliştirilmiş
bir görüntülü görüşme (FaceTime benzeri) uygulamasıdır.


### Kullanılan Teknolojiler

|     Teknoloji     |                  Amaç                   | Dil |
|-------------------|-----------------------------------------|-----|
| **UDP Sockets**   | Gerçek zamanlı P2P iletişim             | C   |
| **Qt Multimedia** | Gerçek mikrofon yakalama ve ses oynatma | C++ |
| **audio.c**       | Temel ses kontrol/simülasyon katmanı    | C   |
| **OpenCV**        | Kamera ve video işleme                  | C++ |
| **Qt 6**          | Grafik kullanıcı arayüzü                | C++ |
| **std::thread**   | Ağdan veri alma ve arka plan işlemleri  | C++ |

---

## Proje Dosya Yapısı

```
FaceTimeClone/
│
├── CMakeLists.txt          ← Derleme sistemi tanımı
│
├── include/                ← Başlık dosyaları (.h / .hpp)
│   ├── network.h           ← C ağ katmanı API'si
│   ├── audio.h             ← C ses katmanı API'si
│   └── video_stream.hpp    ← C++ VideoStream sınıfı
│
└── src/                    ← Kaynak kodlar
    ├── network.c           ← UDP soket işlemleri [C]
    ├── audio.c             ← Temel ses kontrolü / mute / seviye göstergesi [C]
    ├── video_stream.cpp    ← Kamera + JPEG sıkıştırma [C++]
    └── main.cpp            ← Qt GUI + ana program [C++]
```

---

## Mimari Açıklaması

```
┌────────────────────────────────────────────────────────────────────────┐                   
│                             main.cpp (C++)                             │
│                      Qt Penceresi / GUI Katmanı                        │
│          Qt Multimedia ile gerçek mikrofon yakalama ve ses oynatma     │
│                                                                        │
│      ┌──────────────┐                  ┌───────────────────────────┐   │
│      │ VideoStream  │                  │      FaceTimeWindow       │   │
│      │    (C++)     │                  │   (QMainWindow türevi)    │   │
│      │              │                  │                           │   │
│      │ OpenCV ile   │                  │  Butonlar, Video ekranı,  │   │
│      │ kamera okur  │                  │  IP girişi, Ses göstergesi│   │
│      └──────┬───────┘                  └────────────┬──────────────┘   │
│             │                                       │                  │
└─────────────|───────────────────────────────────────|──────────────────┘
              │ C++ → C çağrısı                       │ C++ → C çağrısı
              ▼ (extern "C")                          ▼ (extern "C")
   ┌──────────────────────┐              ┌──────────────────────────┐
   │     network.c (C)    │              │       audio.c (C)        │
   │                      │              │                          │
   │  UDP soket oluşturur │              │  Temel ses kontrolü      │
   │  Paket gönder        │              │  Mute / Volume seviyesi  │
   │  Paket al            │              │  Simülasyon destek katı  │
   └──────────────────────┘              └──────────────────────────┘
              │
              ▼ (İnternet / LAN)
  ┌──────────────────────────┐
  │  Loopback / Karşı Taraf  │
  └──────────────────────────┘

```


---

## C ve C++ Birlikte Kullanımı

Bu projede C ve C++ kodları birlikte çalışır. Bunun için `extern "C"` kullanılır:

```c
// network.h'da:
#ifdef __cplusplus
extern "C" {
#endif

int socket_create(void);  // Bu C fonksiyonunu C++ çağırabilir

#ifdef __cplusplus
}
#endif
```

**Neden?** C++ derleyicisi fonksiyon isimlerine ekstra bilgi ekler (name mangling).
`extern "C"` bunu engeller ve C fonksiyonlarının doğru bulunmasını sağlar.

---

## Neden UDP ? TCP değil ?

```
TCP (Transmission Control Protocol):
✓ Garantili teslimat
✓ Paket sırası korunur
✗ Gecikme toleranssız → kayıp paket tüm akışı durdurur
✗ Video için çok yavaş

UDP (User Datagram Protocol):
✓ Çok hızlı, düşük gecikme
✓ Gerçek zamanlı uygulama için ideal
✗ Paket kaybolabilir → 1 donuk kare (kabul edilir!)
✓ FaceTime, Zoom, Skype hepsi UDP kullanır
```

---

## Kurulum

### Gereksinimler

**Windows:**
- Visual Studio 2022 Build Tools
- CMake
- Qt 6.11.0 MSVC 2022 64-bit
- OpenCV 4.12.0
- Developer PowerShell for VS 2022

---

Not: Proje Windows ortamında geliştirilmiş ve test edilmiştir. Linux/macOS ortamlarında ayrıca derleme ayarı gerekebilir.



## Giriş Bilgileri

Program açıldığında giriş ekranı gelir.

- Kullanıcı adı: `uluhan`
- Şifre: `1234`

Alternatif kullanıcı:

- Kullanıcı adı: `kaan`
- Şifre: `1234`



## Derleme ve Çalıştırma


### Windows Derleme

Proje, Visual Studio 2022 Developer PowerShell üzerinden derlenmiştir.

```powershell
cd "FaceTimeClone"

cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.11.0\msvc2022_64" -DOpenCV_DIR="C:\opencv\build"

cmake --build build --config Release
```

### Windows Çalıştırma

```powershell
cd .\build\Release

$env:PATH="C:\Qt\6.11.0\msvc2022_64\bin;C:\opencv\build\x64\vc16\bin;$env:PATH"

.\FaceTimeClone.exe
```



## Kullanım ve Test

Program tek bilgisayarda loopback bağlantısı ile test edilmiştir.

Local test için:

- IP: `127.0.0.1`
- Port: `9090`

Adımlar:

1. Programı açın.
2. Giriş yapın.
3. IP alanını `127.0.0.1` bırakın.
4. Port alanını `9090` bırakın.
5. `Ara` butonuna basın.
6. Kamera, mikrofon, ses ve UDP gönderim/alım akışı test edilir.

Not: Karşı bilgisayarda teknik sorun yaşandığı için iki farklı cihaz testi yapılamamıştır.


### IP adresini öğrenmek

Windows için:

```powershell
ipconfig
```

---

## Temel Kavramlar 

### 1. Socket Programlama
Soket, ağ üzerinde iletişim kurmak için kullanılan bir dosya tanımlayıcısıdır.
Tıpkı dosya açar gibi soket açılır, veri yazılır/okunur, kapatılır.

```c
int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // Soket oluştur
bind(fd, &addr, sizeof(addr));                        // Porta bağla
sendto(fd, data, len, 0, &dest, sizeof(dest));       // Gönder
recvfrom(fd, buf, sizeof(buf), 0, &src, &src_len);  // Al
close(fd);                                            // Kapat
```


### 2. Thread (İş Parçacığı)

Aynı anda birden fazla iş yapmak için thread yapısı kullanılmıştır:

- **Ana Thread**: Qt arayüzü ve kullanıcı etkileşimleri
- **Alıcı Thread**: UDP üzerinden gelen paketleri dinleme ve işleme
- **Video Callback Yapısı**: Kameradan alınan görüntüyü işleme ve görüşme aktifse UDP ile gönderme

Bu yapı sayesinde arayüz donmadan ağdan veri alınabilir ve video/ses akışı sürdürülebilir.


### 3. JPEG Sıkıştırma

Projede UDP paket boyutunu daha güvenli seviyede tutabilmek için:

- Çözünürlük: 320x240
- FPS: 20
- JPEG kalite değeri: 45

olarak ayarlanmıştır.

Bu sayede ağ üzerinden daha stabil ve düşük gecikmeli görüntü aktarımı sağlanmıştır.


## Sonuç ve Değerlendirme

Bu proje kapsamında UDP tabanlı gerçek zamanlı görüntülü görüşme uygulaması başarıyla geliştirilmiş ve çalışan prototip seviyesine ulaştırılmıştır.

Tamamlanan özellikler:

- Qt tabanlı kullanıcı arayüzü
- Kullanıcı giriş ekranı
- UDP soket oluşturma, bind, gönderme ve alma
- OpenCV ile kamera görüntüsü alma
- JPEG sıkıştırma ile video gönderimi
- Gelen görüntünün arayüzde gösterilmesi
- Qt Multimedia ile gerçek mikrofon yakalama
- Ses paketlerinin UDP üzerinden gönderilmesi
- Mikrofon sessize alma
- Kamera aç/kapat
- Görüşme başlat/sonlandır
- Uygulamayı kapatma butonu
- 127.0.0.1 loopback testi

Sınırlılıklar:

- İki farklı bilgisayarda test yapılamamıştır.
- Testler tek bilgisayarda localhost/loopback üzerinden yapılmıştır.
- Loopback ses testinde yankı oluşabilir.

Proje ekip çalışması şeklinde geliştirilmiştir. Ağ altyapısı ve temel ses modülü ilk aşamada hazırlanmış, daha sonra Qt GUI, OpenCV video sistemi ve Qt Multimedia gerçek ses entegrasyonu ile genişletilmiştir.

## Lisans
Eğitim amaçlı geliştirilmiştir.
