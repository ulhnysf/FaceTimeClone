/*
 * network.h
 * Ağ katmanının dışarıya açık arayüzü (API)
 * C++ tarafı bu başlık dosyası üzerinden C fonksiyonlarını çağırır.
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>  /* uint8_t tipi için */
#include <stddef.h>  /* size_t tipi için */

/* C++ derleyicisi bu dosyayı include ederse,
 * fonksiyon isimlerini C++ name-mangling'den korur */
#ifdef __cplusplus
extern "C" {
#endif

/* Sabitler */
#define DEFAULT_PORT     9090          /* Varsayılan dinleme portu     */
#define PACKET_MAX_SIZE  65507         /* UDP'nin maksimum veri boyutu */
#define IP_STR_LEN       INET_ADDRSTRLEN /* IP string uzunluğu (16)   */

/* Fonksiyon prototipleri */
int  network_init(void);
void network_cleanup(void);

int  socket_create(void);
int  socket_bind(int fd, int port);
void socket_close(int fd);
int  set_nonblocking(int fd);

int  packet_send(int fd, const uint8_t* data, size_t len,
                 const char* dest_ip, int dest_port);
int  packet_recv(int fd, uint8_t* buf, size_t buf_len, char* src_ip);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_H */
