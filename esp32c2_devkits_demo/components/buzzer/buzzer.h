#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BUZZER_PIN_NUM GPIO_NUM_18    // buzzer pin number

/* Note frequencies in Hz */
#define NOTE_C0     16
#define NOTE_Db0    17
#define NOTE_D0     18
#define NOTE_Eb0    19
#define NOTE_E0     21
#define NOTE_F0     22
#define NOTE_Gb0    23
#define NOTE_G0     25
#define NOTE_Ab0    26
#define NOTE_LA0    28
#define NOTE_Bb0    29
#define NOTE_B0     31
#define NOTE_C1     33
#define NOTE_Db1    35
#define NOTE_D1     37
#define NOTE_Eb1    39
#define NOTE_E1     41
#define NOTE_F1     44
#define NOTE_Gb1    46
#define NOTE_G1     49
#define NOTE_Ab1    52
#define NOTE_LA1    55
#define NOTE_Bb1    58
#define NOTE_B1     62
#define NOTE_C2     65
#define NOTE_Db2    69
#define NOTE_D2     73
#define NOTE_Eb2    78
#define NOTE_E2     82
#define NOTE_F2     87
#define NOTE_Gb2    93
#define NOTE_G2     98
#define NOTE_Ab2    104
#define NOTE_LA2    110
#define NOTE_Bb2    117
#define NOTE_B2     123
#define NOTE_C3     131
#define NOTE_Db3    139
#define NOTE_D3     147
#define NOTE_Eb3    156
#define NOTE_E3     165
#define NOTE_F3     175
#define NOTE_Gb3    185
#define NOTE_G3     196
#define NOTE_Ab3    208
#define NOTE_LA3    220
#define NOTE_Bb3    233
#define NOTE_B3     247
#define NOTE_C4     262
#define NOTE_Db4    277
#define NOTE_D4     294
#define NOTE_Eb4    311
#define NOTE_E4     330
#define NOTE_F4     349
#define NOTE_Gb4    370
#define NOTE_G4     392
#define NOTE_Ab4    415
#define NOTE_LA4    440
#define NOTE_Bb4    466
#define NOTE_B4     494
#define NOTE_C5     523
#define NOTE_Db5    554
#define NOTE_D5     587
#define NOTE_Eb5    622
#define NOTE_E5     659
#define NOTE_F5     698
#define NOTE_Gb5    740
#define NOTE_G5     784
#define NOTE_Ab5    831
#define NOTE_LA5    880
#define NOTE_Bb5    932
#define NOTE_B5     988
#define NOTE_C6     1047
#define NOTE_Db6    1109
#define NOTE_D6     1175
#define NOTE_Eb6    1245
#define NOTE_E6     1319
#define NOTE_F6     1397
#define NOTE_Gb6    1480
#define NOTE_G6     1568
#define NOTE_Ab6    1661
#define NOTE_LA6    1760
#define NOTE_Bb6    1865
#define NOTE_B6     1976
#define NOTE_C7     2093
#define NOTE_Db7    2217
#define NOTE_D7     2349
#define NOTE_Eb7    2489
#define NOTE_E7     2637
#define NOTE_F7     2794
#define NOTE_Gb7    2960
#define NOTE_G7     3136
#define NOTE_Ab7    3322
#define NOTE_LA7    3520
#define NOTE_Bb7    3729
#define NOTE_B7     3951
#define NOTE_C8     4186
#define NOTE_Db8    4435
#define NOTE_D8     4699
#define NOTE_Eb8    4978


/* Timing definitions in milliseconds */
#define MUSIC_BPM               120                         // Beats Per Minute
#define MUSIC_HALF_NOTE         (2 * MUSIC_QUARTER_NOTE)    // Half note (2/4)
#define MUSIC_QUARTER_NOTE      (60000 / MUSIC_BPM)         // Quarter note (1/4)
#define MUSIC_EIGHTH_NOTE       (MUSIC_QUARTER_NOTE / 2)    // Eighth note (1/8)
#define MUSIC_SIXTEENTH_NOTE    (MUSIC_QUARTER_NOTE / 4)    // Sixteenth note (1/16)
#define MUSIC_WHOLE_NOTE        (4 * MUSIC_QUARTER_NOTE)    // Whole note (4/4)

/* tone_t structure */
typedef struct{
    int frequency;
    int duration;
}tone_t;

/* audio_index_t enumeration */
typedef enum {
    AUDIO_INDEX_NONE = 0,      // No audio
    AUDIO_INDEX_START,         // Start tone
    AUDIO_INDEX_END,           // End tone
    AUDIO_INDEX_SINGLE_PRESS,  // Single press tone
    AUDIO_INDEX_DOUBLE_PRESS,  // Double press tone
    AUDIO_INDEX_LONG_PRESS,    // Long press tone
    AUDIO_INDEX_ERROR,         // Error tone
} audio_index_t;

/**
 * @brief   Initialize the buzzer
 * @param buzzer_pin   The pin number of the buzzer
 * @return ESP_OK on success, 
 *         ESP_FAIL on failure
 */
esp_err_t buzzer_init(gpio_num_t buzzer_pin);

/**
 * @brief Play a tone
 * @param audio_index   The index of the audio to play
 */
void buzzer_play_audio(audio_index_t audio_index);

#ifdef __cplusplus
}
#endif
