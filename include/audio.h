/*
 * audio.h
 * Ses katmanının dışarıya açık arayüzü
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ses cihazı yapısı */
typedef struct {
    int   sample_rate;      /* Örnekleme hızı (Hz)       */
    int   channels;         /* Kanal sayısı (1=mono)      */
    int   frame_size;       /* Frame başına örnek sayısı  */
    int   bits_per_sample;  /* Bit derinliği (16 bit)     */
    int   is_active;        /* Cihaz aktif mi?            */
    int   is_muted;         /* Sessiz mi?                 */
    int   volume;           /* Ses seviyesi (0-100)       */
    void* platform_handle;  /* Platform'a özel handle     */
    void (*callback)(const uint8_t*, size_t); /* Frame callback */
} AudioDevice;

/* Yakalama callback tipi */
typedef void (*AudioCaptureCallback)(const uint8_t* frame, size_t len);

/* Fonksiyon prototipleri */
int   audio_init(void);
void  audio_cleanup(void);

int   audio_start_capture(AudioCaptureCallback callback);
void  audio_stop_capture(void);

int   audio_play_frame(const uint8_t* data, size_t len);

void  audio_set_mute(int muted);
void  audio_set_volume(int volume);
float audio_get_level(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_H */
