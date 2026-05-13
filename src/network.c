/*
 * network.c
 * UDP tabanlı Peer-to-Peer ağ katmanı
 * Görevi: İki bilgisayar arasında doğrudan bağlantı kurmak
 */

#include "../include/network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

/* -------------------------------------------------------
 * network_init()
 * Ağ sistemini başlatır. Windows'ta Winsock gerekli,
 * Linux/Mac'te ekstra başlatma gerekmez.
 * Dönüş: 0 başarılı, -1 hata
 * ------------------------------------------------------- */
int network_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[HATA] Winsock baslatilamadi: %d\n", WSAGetLastError());
        return -1;
    }
#endif
    printf("[OK] Ag sistemi baslatildi.\n");
    return 0;
}

/* -------------------------------------------------------
 * network_cleanup()
 * Program kapanırken ağ kaynaklarını serbest bırakır.
 * ------------------------------------------------------- */
void network_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
    printf("[OK] Ag kaynaklari temizlendi.\n");
}

/* -------------------------------------------------------
 * socket_create()
 * UDP soketi oluşturur.
 * UDP kullanıyoruz çünkü video için hız, güvenilirlikten
 * daha önemlidir. Kaybolan kare = donuk ekran (kabul edilir)
 * Kaybolan TCP paketi = tüm akış durur (kabul edilemez)
 * Dönüş: soket tanımlayıcısı (fd), hata varsa -1
 * ------------------------------------------------------- */
int socket_create(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        perror("[HATA] Soket olusturulamadi");
        return -1;
    }

    /* SO_REUSEADDR: Programı yeniden başlatınca aynı portu hemen kullanabilmek için */
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    printf("[OK] UDP soketi olusturuldu (fd=%d)\n", fd);
    return fd;
}

/* -------------------------------------------------------
 * socket_bind()
 * Soketi belirtilen porta bağlar.
 * Yani "Bu porta gelen paketleri ben alıyorum" der.
 * port: Dinlenecek port numarası (örn: 9090)
 * ------------------------------------------------------- */
int socket_bind(int fd, int port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family      = AF_INET;       /* IPv4 */
    addr.sin_addr.s_addr = INADDR_ANY;    /* Tüm ağ arayüzlerinden dinle */
    addr.sin_port        = htons(port);   /* Port'u ağ byte sırasına çevir */

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[HATA] Soket bind edilemedi");
        return -1;
    }

    printf("[OK] Port %d'de dinleniyor.\n", port);
    return 0;
}

/* -------------------------------------------------------
 * packet_send()
 * Veriyi karşı tarafa UDP paketi olarak gönderir.
 * fd:       Soket tanımlayıcısı
 * data:     Gönderilecek ham veri (video karesi, ses verisi vb.)
 * len:      Verinin byte cinsinden uzunluğu
 * dest_ip:  Hedef IP adresi ("192.168.1.5" gibi)
 * dest_port: Hedef port
 * Dönüş: Gönderilen byte sayısı, hata varsa -1
 * ------------------------------------------------------- */
int packet_send(int fd, const uint8_t* data, size_t len,
                const char* dest_ip, int dest_port) {

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(dest_port);

    if (inet_pton(AF_INET, dest_ip, &dest.sin_addr) <= 0) {
        fprintf(stderr, "[HATA] Gecersiz İP adresi: %s\n", dest_ip);
        return -1;
    }

    int sent = sendto(fd, (const char*)data, len, 0,
                      (struct sockaddr*)&dest, sizeof(dest));
    if (sent < 0) {
        perror("[HATA] Paket gonderilemedi");
    }
    return sent;
}

/* -------------------------------------------------------
 * packet_recv()
 * Karşı taraftan gelen UDP paketini alır.
 * Bloklanır (gelen paket yoksa bekler).
 * buf:      Verinin yazılacağı tampon
 * buf_len:  Tamponun maksimum boyutu
 * src_ip:   Paketin geldiği IP (çıktı parametresi)
 * Dönüş: Alınan byte sayısı, hata varsa -1
 * ------------------------------------------------------- */
int packet_recv(int fd, uint8_t* buf, size_t buf_len, char* src_ip) {
    struct sockaddr_in src;
    socklen_t src_len = sizeof(src);

    int received = recvfrom(fd, (char*)buf, buf_len, 0,
                            (struct sockaddr*)&src, &src_len);
    if (received < 0) {
        return -1;
    }

    /* Paketin geldiği IP adresini string'e çevir */
    if (src_ip != NULL) {
        inet_ntop(AF_INET, &src.sin_addr, src_ip, INET_ADDRSTRLEN);
    }

    return received;
}

/* -------------------------------------------------------
 * socket_close()
 * Soketi kapatır ve işletim sistemine geri verir.
 * ------------------------------------------------------- */
void socket_close(int fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
    printf("[OK] Soket kapatildi (fd=%d)\n", fd);
}

/* -------------------------------------------------------
 * set_nonblocking()
 * Soketi non-blocking moda alır.
 * Non-blocking: Veri yoksa hemen geri döner (beklemez)
 * Bu sayede UI donmadan ağ işlemleri paralel yürür.
 * ------------------------------------------------------- */
int set_nonblocking(int fd) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}
