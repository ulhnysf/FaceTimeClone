/*
 * audio.c
 * Ses yakalama ve oynatma katmanı (C ile yazılmış)
 *
 * Gerçek bir uygulamada ALSA (Linux), CoreAudio (macOS)
 * veya WASAPI (Windows) kullanılır. Bu dosya platform-bağımsız
 * bir soyutlama katmanı sunar.
 */

#include "../include/audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -------------------------------------------------------
 * Ses parametreleri
 * Sample rate: Saniyede kaç ses örneği alınır
 * 44100 Hz = CD kalitesi (insan kulağı ~20kHz'e kadar duyar)
 * Channels: 1 = mono (telefon), 2 = stereo (müzik)
 * Frame size: Tek seferde işlenen örnek sayısı
 * ------------------------------------------------------- */
#define SAMPLE_RATE   44100
#define CHANNELS      1
#define FRAME_SIZE    1024
#define BITS_PER_SAMPLE 16

/* Ses cihazının iç durumu */
static AudioDevice g_capture_dev  = {0};
static AudioDevice g_playback_dev = {0};
static int g_audio_initialized    = 0;

/* -------------------------------------------------------
 * audio_init()
 * Ses sistemini başlatır.
 * Mikrofon (yakalama) ve hoparlör (oynatma) cihazlarını açar.
 * Dönüş: 0 başarılı, -1 hata
 * ------------------------------------------------------- */
int audio_init(void) {
    if (g_audio_initialized) {
        printf("[UYARİ] Ses sistemi zaten baslatilmis.\n");
        return 0;
    }

    /* Yakalama cihazını başlat (mikrofon) */
    g_capture_dev.sample_rate    = SAMPLE_RATE;
    g_capture_dev.channels       = CHANNELS;
    g_capture_dev.frame_size     = FRAME_SIZE;
    g_capture_dev.bits_per_sample = BITS_PER_SAMPLE;
    g_capture_dev.is_active      = 0;
    g_capture_dev.is_muted       = 0;

    /* Oynatma cihazını başlat (hoparlör) */
    g_playback_dev.sample_rate    = SAMPLE_RATE;
    g_playback_dev.channels       = CHANNELS;
    g_playback_dev.frame_size     = FRAME_SIZE;
    g_playback_dev.bits_per_sample = BITS_PER_SAMPLE;
    g_playback_dev.is_active      = 0;
    g_playback_dev.volume         = 80; /* %80 ses */

    printf("[OK] Ses sistemi baslatildi.\n");
    printf("     Sample Rate: %d Hz\n", SAMPLE_RATE);
    printf("     Kanal sayisi: %d (Mono)\n", CHANNELS);
    printf("     Frame boyutu: %d ornek\n", FRAME_SIZE);

    g_audio_initialized = 1;
    return 0;
}

/* -------------------------------------------------------
 * audio_start_capture()
 * Mikrofonu açar, ses yakalamaya başlar.
 * callback: Her frame dolduğunda çağrılacak fonksiyon
 *           Bu fonksiyon ses verisini ağa gönderir.
 * ------------------------------------------------------- */
int audio_start_capture(AudioCaptureCallback callback) {
    if (!g_audio_initialized) {
        fprintf(stderr, "[HATA] Ses sistemi baslatilmamis!\n");
        return -1;
    }

    g_capture_dev.callback  = callback;
    g_capture_dev.is_active = 1;

    printf("[OK] Mikrofon aktif, ses yakalaniyor...\n");
    return 0;
}

/* -------------------------------------------------------
 * audio_stop_capture()
 * Mikrofonu kapatır.
 * ------------------------------------------------------- */
void audio_stop_capture(void) {
    g_capture_dev.is_active = 0;
    printf("[OK] Mikrofon durduruldu.\n");
}

/* -------------------------------------------------------
 * audio_play_frame()
 * Ağdan gelen ses verisini hoparlörden çalar.
 * data:    PCM ses verisi
 * len:     Byte cinsinden uzunluk
 *
 * PCM (Pulse-Code Modulation): Ham dijital ses formatı.
 * Sıkıştırma yok, anlık işlenebilir.
 * ------------------------------------------------------- */
int audio_play_frame(const uint8_t* data, size_t len) {
    if (!g_playback_dev.is_active) return 0;
    if (data == NULL || len == 0)  return -1;

    /* Gerçek uygulamada burada ALSA/CoreAudio write() çağrısı olur */
    /* Şu an simüle ediyoruz */
    (void)data;
    (void)len;
    return 0;
}

/* -------------------------------------------------------
 * audio_set_mute()
 * Mikrofonu sessize alır veya açar.
 * muted: 1 = sessiz, 0 = aktif
 * ------------------------------------------------------- */
void audio_set_mute(int muted) {
    g_capture_dev.is_muted = muted;
    printf("[OK] Mikrofon %s\n", muted ? "sessize alindi." : "acildi.");
}

/* -------------------------------------------------------
 * audio_set_volume()
 * Hoparlör ses seviyesini ayarlar.
 * volume: 0-100 arası değer
 * ------------------------------------------------------- */
void audio_set_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    g_playback_dev.volume = volume;
    printf("[OK] Ses seviyesi: %d%%\n", volume);
}

/* -------------------------------------------------------
 * audio_get_level()
 * Anlık mikrofon ses seviyesini döndürür (0.0 - 1.0)
 * UI'daki ses göstergesi için kullanılır.
 * ------------------------------------------------------- */
float audio_get_level(void) {
    if (g_capture_dev.is_muted || !g_capture_dev.is_active) return 0.0f;
    /* Gerçek uygulamada son frame'in RMS değeri hesaplanır */
    /* Şu an rastgele simüle ediyoruz */
    return (float)(rand() % 60) / 100.0f;
}

/* -------------------------------------------------------
 * audio_cleanup()
 * Ses kaynaklarını serbest bırakır.
 * ------------------------------------------------------- */
void audio_cleanup(void) {
    audio_stop_capture();
    g_playback_dev.is_active = 0;
    g_audio_initialized = 0;
    printf("[OK] Ses kaynaklari temizlendi.\n");
}
