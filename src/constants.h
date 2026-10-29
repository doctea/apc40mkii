#pragma once

namespace APC40MKII {

    namespace sysex {
        constexpr uint8_t MESSAGE_TYPE_INTRODUCTION_IDENTIFIER = 0x60;
        enum APPLICATION_MODES {
            MODE_0 = 0x40,
            MODE_1 = 0x41,
            MODE_2 = 0x42
        };
    }

    #define NOTE_BUTTON_SHIFT  98

    namespace device {
        constexpr uint8_t CC_KNOB_1  = 16;
        constexpr uint8_t CC_KNOB_2  = 17;
        constexpr uint8_t CC_KNOB_3  = 18;
        constexpr uint8_t CC_KNOB_4  = 19;
        constexpr uint8_t CC_KNOB_5  = 20;
        constexpr uint8_t CC_KNOB_6  = 21;
        constexpr uint8_t CC_KNOB_7  = 22;
        constexpr uint8_t CC_KNOB_8  = 23;

        constexpr uint8_t NOTE_BUTTON_1  = 58;
        constexpr uint8_t NOTE_BUTTON_2  = 59;
        constexpr uint8_t NOTE_BUTTON_3  = 60;
        constexpr uint8_t NOTE_BUTTON_4  = 61;
        constexpr uint8_t NOTE_BUTTON_5  = 62;
        constexpr uint8_t NOTE_BUTTON_6  = 63;
        constexpr uint8_t NOTE_BUTTON_7  = 64;
        constexpr uint8_t NOTE_BUTTON_8  = 65;

        constexpr uint8_t NOTE_BUTTON_DEVICE_LEFT      = 58;
        constexpr uint8_t NOTE_BUTTON_DEVICE_RIGHT     = 59;
        constexpr uint8_t NOTE_BUTTON_BANK_LEFT        = 60;
        constexpr uint8_t NOTE_BUTTON_BANK_RIGHT       = 61;
        constexpr uint8_t NOTE_BUTTON_DEVICE_ONOFF     = 62;
        constexpr uint8_t NOTE_BUTTON_DEVICE_LOCK      = 63;
        constexpr uint8_t NOTE_BUTTON_CLIP_DEVICE_VIEW = 64;
        constexpr uint8_t NOTE_BUTTON_DETAIL_VIEW      = 65;

        constexpr uint8_t NOTE_BUTTON_BANK_SELECT_UP     = 94;
        constexpr uint8_t NOTE_BUTTON_BANK_SELECT_DOWN   = 95;
        constexpr uint8_t NOTE_BUTTON_BANK_SELECT_RIGHT  = 96;
        constexpr uint8_t NOTE_BUTTON_BANK_SELECT_LEFT   = 97;

        constexpr uint8_t NOTE_BUTTON_BANK = 103;   
    }

    constexpr uint8_t CHAN_TRACK_1    = 1;
    constexpr uint8_t CHAN_TRACK_2    = 2;
    constexpr uint8_t CHAN_TRACK_3    = 3;
    constexpr uint8_t CHAN_TRACK_4    = 4;
    constexpr uint8_t CHAN_TRACK_5    = 5;
    constexpr uint8_t CHAN_TRACK_6    = 6;
    constexpr uint8_t CHAN_TRACK_7    = 7;
    constexpr uint8_t CHAN_TRACK_8    = 8;

    constexpr uint8_t CC_CROSSFADER  = 15;
    constexpr uint8_t CHAN_CROSSFADER  = 1;

    namespace tracks {
        constexpr uint8_t CHAN_KNOBS     = 1;
        constexpr uint8_t CC_CUE_LEVEL   = 47;       // uses that weird twos-complement encoding for cue level
        constexpr uint8_t CC_KNOB_1      = 48;
        constexpr uint8_t CC_KNOB_2      = 49;
        constexpr uint8_t CC_KNOB_3      = 50;
        constexpr uint8_t CC_KNOB_4      = 51;
        constexpr uint8_t CC_KNOB_5      = 52;
        constexpr uint8_t CC_KNOB_6      = 53;
        constexpr uint8_t CC_KNOB_7      = 54;
        constexpr uint8_t CC_KNOB_8      = 55;

        constexpr uint8_t NOTE_BUTTON_PAN   = 87;
        constexpr uint8_t NOTE_BUTTON_SENDS = 88;
        constexpr uint8_t NOTE_BUTTON_USER  = 89;

        // note: all faders use CC 7, channel determined by the fader's track number (1-8)
        constexpr uint8_t CC_FADER_MASTER   = 14;
        constexpr uint8_t CHAN_FADER_MASTER  = 1;

        constexpr uint8_t CC_FADER      = 7;

        // note: similar to faders, the buttons use a fixed note number, 
        // channel determined by the track number
        constexpr uint8_t BUTTON_ASSIGN_AB    = 66;
        constexpr uint8_t BUTTON_RECORD       = 48;
        constexpr uint8_t BUTTON_SOLO         = 49;
        constexpr uint8_t BUTTON_ACTIVATOR    = 50;

        constexpr uint8_t BUTTON_SELECTOR     = 51;      // the wide pads in the row underneath the 'clip stop' row
    }

    namespace scene {
        // channel determined by the track number
        constexpr uint8_t BUTTON_CLIP_STOP    = 52;      // the row of buttons under the main clip launch grid

        constexpr uint8_t BUTTON_CLIP_STOP_ALL = 81;     // channel 1

        constexpr uint8_t CLIP_LAUNCH_1_1  = 32;
        constexpr uint8_t CLIP_LAUNCH_1_2  = 33;
        constexpr uint8_t CLIP_LAUNCH_1_3  = 34;
        constexpr uint8_t CLIP_LAUNCH_1_4  = 35;
        constexpr uint8_t CLIP_LAUNCH_1_5  = 36;
        constexpr uint8_t CLIP_LAUNCH_1_6  = 37;
        constexpr uint8_t CLIP_LAUNCH_1_7  = 38;
        constexpr uint8_t CLIP_LAUNCH_1_8  = 39;

        constexpr uint8_t CLIP_LAUNCH_2_1  = 24;
        constexpr uint8_t CLIP_LAUNCH_2_2  = 25;
        constexpr uint8_t CLIP_LAUNCH_2_3  = 26;
        constexpr uint8_t CLIP_LAUNCH_2_4  = 27;
        constexpr uint8_t CLIP_LAUNCH_2_5  = 28;
        constexpr uint8_t CLIP_LAUNCH_2_6  = 29;
        constexpr uint8_t CLIP_LAUNCH_2_7  = 30;
        constexpr uint8_t CLIP_LAUNCH_2_8  = 31;

        constexpr uint8_t CLIP_LAUNCH_3_1  = 16;
        constexpr uint8_t CLIP_LAUNCH_3_2  = 17;
        constexpr uint8_t CLIP_LAUNCH_3_3  = 18;
        constexpr uint8_t CLIP_LAUNCH_3_4  = 19;
        constexpr uint8_t CLIP_LAUNCH_3_5  = 20;
        constexpr uint8_t CLIP_LAUNCH_3_6  = 21;
        constexpr uint8_t CLIP_LAUNCH_3_7  = 22;
        constexpr uint8_t CLIP_LAUNCH_3_8  = 23;

        constexpr uint8_t CLIP_LAUNCH_4_1  = 8;
        constexpr uint8_t CLIP_LAUNCH_4_2  = 9;
        constexpr uint8_t CLIP_LAUNCH_4_3  = 10;
        constexpr uint8_t CLIP_LAUNCH_4_4  = 11;
        constexpr uint8_t CLIP_LAUNCH_4_5  = 12;
        constexpr uint8_t CLIP_LAUNCH_4_6  = 13;
        constexpr uint8_t CLIP_LAUNCH_4_7  = 14;
        constexpr uint8_t CLIP_LAUNCH_4_8  = 15;

        constexpr uint8_t CLIP_LAUNCH_5_1  = 0;
        constexpr uint8_t CLIP_LAUNCH_5_2  = 1;
        constexpr uint8_t CLIP_LAUNCH_5_3  = 2;
        constexpr uint8_t CLIP_LAUNCH_5_4  = 3;
        constexpr uint8_t CLIP_LAUNCH_5_5  = 4;
        constexpr uint8_t CLIP_LAUNCH_5_6  = 5;
        constexpr uint8_t CLIP_LAUNCH_5_7  = 6;
        constexpr uint8_t CLIP_LAUNCH_5_8  = 7;

        constexpr uint8_t SCENE_LAUNCH_ROW_1  = 82;
        constexpr uint8_t SCENE_LAUNCH_ROW_2  = 83;
        constexpr uint8_t SCENE_LAUNCH_ROW_3  = 84;
        constexpr uint8_t SCENE_LAUNCH_ROW_4  = 85;
        constexpr uint8_t SCENE_LAUNCH_ROW_5  = 86;
    }

    namespace tempo {
        constexpr uint8_t NOTE_BUTTON_METRONOME   = 90;
        constexpr uint8_t NOTE_BUTTON_TAP_TEMPO   = 99;
        constexpr uint8_t NOTE_BUTTON_NUDGE_LEFT  = 100;
        constexpr uint8_t NOTE_BUTTON_NUDGE_RIGHT = 101;

        constexpr uint8_t CC_TEMPO    = 13;  // that weird two's complement thing for tempo control too
    }

    namespace play {
        constexpr uint8_t NOTE_BUTTON_PLAY     = 91;
        constexpr uint8_t NOTE_BUTTON_RECORD   = 93;
        constexpr uint8_t NOTE_BUTTON_SESSION  = 102;
    }

} // namespace APC40MKII!