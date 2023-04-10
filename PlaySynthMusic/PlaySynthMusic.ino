//* ------------ DEFINES ---------------- *//
//#define ENABLE_ETH
//#define ENABLE_SCREEN
//#define DEBUG_ALLOC
//* ===================================== *//
//
//* ----------- INCLUDES----------------- *//
#include <vector>
#include <algorithm>
#include <Audio.h>
#include "soundfont_reader.h"
#include "mixer_keeper.h"
#include <Bounce2.h>
#include <Encoder.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <MIDI.h>
#include <serialMIDI.h>
#include "MIDIUSB.h"
#include "PlaySynthMusic.h"
#include "MUX74HC4067.h"
// #include "Pizzicato_samples.h"
#include "FrenchHorns_samples.h"
#include "Viola_samples.h"
#include "bassoon_samples.h"
#include "clarinet_samples.h"
#include "distortiongt_samples.h"
// #include "epiano_samples.h"
#include "flute_samples.h"
// #include "frenchhorn_samples.h"
// #include "glockenspiel_samples.h"
// #include "gtfretnoise_samples.h"
#include "harmonica_samples.h"
#include "harp_samples.h"
#include "mutedgtr_samples.h"
// #include "nylonstrgtr_samples.h"
// #include "oboe_samples.h"
// #include "overdrivegt_samples.h"
// #include "recorder_samples.h"
// #include "standard_DRUMS_samples.h"
// #include "steelstrgtr_samples.h"
// #include "strings_samples.h"
// #include "timpani_samples.h"
// #include "trombone_samples.h"
#include "trumpet_samples.h"
#include "tuba_samples.h"
// #include "piano_samples.h"
#include "vibraphone_samples.h"
// #include "BasicFlute1_samples.h"
#include "Ocarina_samples.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#ifdef ENABLE_ETH
#include <Ethernet.h>
// #include <NativeEthernet.h>
#endif
//* ===================================== *//

//* ------------ STRUCTS ---------------- *//
struct voice_t {
  unsigned short id;
  byte channel;
  byte note;
};

short wave_type[16] = {
  WAVEFORM_SINE,
  WAVEFORM_SQUARE,
  WAVEFORM_SAWTOOTH,
  WAVEFORM_TRIANGLE,
  WAVEFORM_SINE,
  WAVEFORM_SQUARE,
  WAVEFORM_SAWTOOTH,
  WAVEFORM_TRIANGLE,
  WAVEFORM_SINE,
  WAVEFORM_SQUARE,
  WAVEFORM_SAWTOOTH,
  WAVEFORM_TRIANGLE,
  WAVEFORM_SINE,
  WAVEFORM_SQUARE,
  WAVEFORM_SAWTOOTH,
  WAVEFORM_TRIANGLE
};

//* ===================================== *//


MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, myMIDI);
#define MAX_INPUTS_IN_MIXER 4

#ifdef ENABLE_SCREEN

#define TFT_DC 20
#define TFT_CS 6     // TODO IN USE BY  MUX2_SIG_PIN
#define TFT_RST 255  // 255 = unused, connect to 3.3V
#define TFT_MOSI 26
#define TFT_SCK 27
#define TFT_MISO 39
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST, TFT_MISO);

#endif


#ifdef ENABLE_ETH
// Enter a MAC address and IP address for your controller below. The IP address will be dependent on your local network:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);
IPAddress myDns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
bool eth_started = false;
// Initialize the Ethernet server library with the IP address and port you want to use (port 80 is default for HTTP):
EthernetServer server(80);

#endif

#define ENCODER_CLK_PIN 3
#define ENCODER_DT_PIN 4
#define ENCODER_SW_BUTTON 5
#define MUX_1_EN_PIN 30  // 31
#define MUX_2_EN_PIN 31  // 31
#define MUX_S0_PIN 28
#define MUX_S1_PIN 29
#define MUX_S2_PIN 34
#define MUX_S3_PIN 35
#define MUX_1_SIG_PIN 41
#define MUX_2_SIG_PIN 2

Encoder ctrlEncoder(ENCODER_CLK_PIN, ENCODER_DT_PIN);
Button ctrlEncoderBtn = Button();
MUX74HC4067 mux1(MUX_1_EN_PIN, MUX_S0_PIN, MUX_S1_PIN, MUX_S2_PIN, MUX_S3_PIN);
MUX74HC4067 mux2(MUX_2_EN_PIN, MUX_S0_PIN, MUX_S1_PIN, MUX_S2_PIN, MUX_S3_PIN);



int used_voices = 0;
int stopped_voices = 0;
int evict_voice = 0;
int notes_played = 0;
int volume = 50;
unsigned long t = 0;
unsigned long last_time = millis();
unsigned long counter = 0;
int16_t mux1_pot[16];
bool mux2_pot[16];
const char *note_map[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
float ampl = 0;
float ampl2 = 0;

const short TOTAL_MIXER_KEEPERS = 100;
const short TOTAL_VOICES = 64;
const short TOTAL_MIXERS = 21;
const short SECONDARY_MIXERS = 4;

struct mixer_keeper {
  uint8_t id;
  AudioMixer4 *synth_mixer;
  short int layer = 0;
  bool channel1 = false, channel2 = false, channel3 = false, channel4 = false, full = false, output = false;
  AudioConnection *ac_channel1 = nullptr, *ac_channel2 = nullptr, *ac_channel3 = nullptr, *ac_channel4 = nullptr, *ac_output = nullptr;
};

voice_t voices[TOTAL_VOICES];
//AudioInputI2S             audioInput;             //xy=682,741    // TODO Add Connection
AudioOutputI2S audioOut;     //xy=1631,416
AudioControlSGTL5000 codec;  //xy=1684,296
AudioSynthWavetable *synth_wavetable[TOTAL_VOICES];
AudioConnection *synth_patch[200];
int synth_patch_used = 0;
int synth_mixer_keeper_used = 0;
mixer_keeper synth_mixer_keeper[TOTAL_MIXER_KEEPERS];
AudioMixer4 *synth_mixer[TOTAL_MIXER_KEEPERS];

AudioMixer4 mainMixer;
AudioSynthWavetable wavetable[TOTAL_VOICES];
// AudioSynthKarplusStrong   synth_string[TOTAL_VOICES];        //xy=210,535
AudioSynthWaveformSine synth_sine[TOTAL_VOICES];   //xy=219,443
AudioSynthWaveform *synth_waveform[TOTAL_VOICES];  //xy=228,497
AudioMixer4 mixer[TOTAL_MIXERS];
// build_wavetable_synth();



AudioConnection patchCord[] = {
  { wavetable[0], 0, mixer[0], 0 },
  { wavetable[1], 0, mixer[0], 1 },
  { wavetable[2], 0, mixer[0], 2 },
  { wavetable[3], 0, mixer[0], 3 },
  { mixer[0], 0, mixer[TOTAL_MIXERS - 2], 0 },
  { wavetable[4], 0, mixer[1], 0 },
  { wavetable[5], 0, mixer[1], 1 },
  { wavetable[6], 0, mixer[1], 2 },
  { wavetable[7], 0, mixer[1], 3 },
  { mixer[1], 0, mixer[TOTAL_MIXERS - 2], 1 },
  { wavetable[8], 0, mixer[2], 0 },
  { wavetable[9], 0, mixer[2], 1 },
  { wavetable[10], 0, mixer[2], 2 },
  { wavetable[11], 0, mixer[2], 3 },
  { mixer[2], 0, mixer[TOTAL_MIXERS - 2], 2 },
  { wavetable[12], 0, mixer[3], 0 },
  { wavetable[13], 0, mixer[3], 1 },
  { wavetable[14], 0, mixer[3], 2 },
  { wavetable[15], 0, mixer[3], 3 },
  { mixer[3], 0, mixer[TOTAL_MIXERS - 2], 3 },
  { wavetable[16], 0, mixer[4], 0 },
  { wavetable[17], 0, mixer[4], 1 },
  { wavetable[18], 0, mixer[4], 2 },
  { wavetable[19], 0, mixer[4], 3 },
  { mixer[4], 0, mixer[TOTAL_MIXERS - 3], 0 },
  { wavetable[20], 0, mixer[5], 0 },
  { wavetable[21], 0, mixer[5], 1 },
  { wavetable[22], 0, mixer[5], 2 },
  { wavetable[23], 0, mixer[5], 3 },
  { mixer[5], 0, mixer[TOTAL_MIXERS - 3], 1 },
  { wavetable[24], 0, mixer[6], 0 },
  { wavetable[25], 0, mixer[6], 1 },
  { wavetable[26], 0, mixer[6], 2 },
  { wavetable[27], 0, mixer[6], 3 },
  { mixer[6], 0, mixer[TOTAL_MIXERS - 3], 2 },
  { wavetable[28], 0, mixer[7], 0 },
  { wavetable[29], 0, mixer[7], 1 },
  { wavetable[30], 0, mixer[7], 2 },
  { wavetable[31], 0, mixer[7], 3 },
  { mixer[7], 0, mixer[TOTAL_MIXERS - 3], 3 },
  { wavetable[32], 0, mixer[8], 0 },
  { wavetable[33], 0, mixer[8], 1 },
  { wavetable[34], 0, mixer[8], 2 },
  { wavetable[35], 0, mixer[8], 3 },
  { mixer[8], 0, mixer[TOTAL_MIXERS - 4], 0 },
  { wavetable[36], 0, mixer[9], 0 },
  { wavetable[37], 0, mixer[9], 1 },
  { wavetable[38], 0, mixer[9], 2 },
  { wavetable[39], 0, mixer[9], 3 },
  { mixer[9], 0, mixer[TOTAL_MIXERS - 4], 1 },
  { wavetable[40], 0, mixer[10], 0 },
  { wavetable[41], 0, mixer[10], 1 },
  { wavetable[42], 0, mixer[10], 2 },
  { wavetable[43], 0, mixer[10], 3 },
  { mixer[10], 0, mixer[TOTAL_MIXERS - 4], 2 },
  { wavetable[44], 0, mixer[11], 0 },
  { wavetable[45], 0, mixer[11], 1 },
  { wavetable[46], 0, mixer[11], 2 },
  { wavetable[47], 0, mixer[11], 3 },
  { mixer[11], 0, mixer[TOTAL_MIXERS - 4], 3 },
  { wavetable[48], 0, mixer[12], 0 },
  { wavetable[49], 0, mixer[12], 1 },
  { wavetable[50], 0, mixer[12], 2 },
  { wavetable[51], 0, mixer[12], 3 },
  { mixer[12], 0, mixer[TOTAL_MIXERS - 5], 0 },
  { wavetable[52], 0, mixer[13], 0 },
  { wavetable[53], 0, mixer[13], 1 },
  { wavetable[54], 0, mixer[13], 2 },
  { wavetable[55], 0, mixer[13], 3 },
  { mixer[13], 0, mixer[TOTAL_MIXERS - 5], 1 },
  { wavetable[56], 0, mixer[14], 0 },
  { wavetable[57], 0, mixer[14], 1 },
  { wavetable[58], 0, mixer[14], 2 },
  { wavetable[59], 0, mixer[14], 3 },
  { mixer[14], 0, mixer[TOTAL_MIXERS - 5], 2 },
  { wavetable[60], 0, mixer[15], 0 },
  { wavetable[61], 0, mixer[15], 1 },
  { wavetable[62], 0, mixer[15], 2 },
  { wavetable[63], 0, mixer[15], 3 },
  { mixer[15], 0, mixer[TOTAL_MIXERS - 5], 3 },
  { mixer[TOTAL_MIXERS - 2], 0, mixer[TOTAL_MIXERS - 1], 0 },
  { mixer[TOTAL_MIXERS - 3], 0, mixer[TOTAL_MIXERS - 1], 1 },
  { mixer[TOTAL_MIXERS - 4], 0, mixer[TOTAL_MIXERS - 1], 2 },
  { mixer[TOTAL_MIXERS - 5], 0, mixer[TOTAL_MIXERS - 1], 3 },
  { mixer[TOTAL_MIXERS - 1], 0, mainMixer, 0 },
  { mainMixer, 0, audioOut, 0 },
  { mainMixer, 0, audioOut, 1 },
};
//  { mixer[TOTAL_MIXERS - 1], 0, mainMixer, 1 },
long oldPosition = 1;
String pt_val = "";
IntervalTimer myTimer;
Sd2Card card;
SdVolume SDvolume;

// MIDIInstrument instrument;
void build_wavetable_synth(const unsigned short voices = 1);

void setup() {

  ctrlEncoderBtn.attach(ENCODER_SW_BUTTON, INPUT);
  ctrlEncoderBtn.interval(5);
  ctrlEncoderBtn.setPressedState(LOW);

  Serial.begin(921600);
  Serial.println(String("Begin ") + __FILE__);

  mux1.signalPin(MUX_1_SIG_PIN, INPUT, ANALOG);
  mux2.signalPin(MUX_2_SIG_PIN, INPUT_PULLUP, DIGITAL);

  AudioMemory(80);

  codec.enable();
  codec.volume(0.85);

  // Initialize processor and memory measurements
  AudioProcessorUsageMaxReset();
  AudioMemoryUsageMaxReset();

  Serial.println(String("Setting up ") + TOTAL_VOICES + " voices;");

  for (int i = 0; i < TOTAL_VOICES; ++i) {
    // wavetable[i].setInstrument(FrenchHorns);
    wavetable[i].setInstrument(FrenchHorns);
    wavetable[i].amplitude(1);
    voices[i].id = i;
    voices[i].channel = voices[i].note = 0xFF;
  }
  Serial.println(String("Setting up gain for ") + TOTAL_MIXERS + " mixers;");
  for (int i = 0; i < TOTAL_MIXERS - 1; ++i) {
    for (int j = 0; j < 4; ++j) {
      mixer[i].gain(j, 0.50);
    }
  }

  for (int i = 0; i < 4; ++i) {
    Serial.println(String("Setting up gain for ") + (TOTAL_MIXERS - 1) + " mixer");
    mixer[TOTAL_MIXERS - 1].gain(i, i < SECONDARY_MIXERS ? 1.0 / SECONDARY_MIXERS : 0.0);
  }

  usbMIDI.setHandleNoteOn(OnNoteOn);
  usbMIDI.setHandleNoteOff(OnNoteOff);
  myMIDI.begin(MIDI_CHANNEL_OMNI);
  myMIDI.setHandleNoteOff(OnNoteOff);
  myMIDI.setHandleNoteOn(OnNoteOn);



// Start Screen
#ifdef ENABLE_SCREEN
  setup_screen();
#endif
#ifdef ENABLE_ETH
  setup_eth();
#endif

  card_staff();

  // MIDI.createInstrument(&instrument, soundfont);
  delay(50);
  Serial.println("----------setup done----------");
  build_wavetable_synth(TOTAL_VOICES);
  Serial.println("----------setup done----------");
  myTimer.begin(check_muxes, 200000);  // run every 0.15 seconds
  sample_data *inst = loadSampleAsInstrument(0, "/soundfont/GMExtBank.SF2");
  instrument_data d;
  d.sample_count = 1;
  d.samples = *inst->sample;
  // Serial.println(String("Setting up ") + inst);
  // Serial.println(String("Setting up ") + *inst);
  Serial.println(String("Here 1 sizeof(inst) = ") + sizeof(inst) + " Size pointer " + sizeof(*inst) + " Size d " + sizeof(d) + " Size d.samples " + sizeof(d.samples));
  // // memcpy(d.samples, inst, sizeof(inst));
  Serial.println(String("Here 2 "));
  Serial.println(String("sample_count ") + d.sample_count);
  Serial.println(String("SIZE:  ") + sizeof(d.samples));
  delay(2000);
}

/**
 *  LOOP
 */
void loop() {

  ctrlEncoderBtn.update();
  encoder_stuff();
  if (ctrlEncoderBtn.pressed()) {
    Serial.println(String("Button pressed"));
  }
#ifdef ENABLE_ETH
  loop_eth();
#endif


  usbMIDI.read();
  myMIDI.read();
  t = millis();
  if (millis() - t > 10000) {
    t += 10000;
    Serial.println("(inactivity)");
  }
}

/**
 */
void check_muxes() {
  String val = "";
  String val_pts = "";
  for (int i = 0; i < 16; i++) {
    //mux1.setChannel(i, ENABLED);
    //delayMicroseconds(2);
    int16_t vn = mux1.read(i);
    delayMicroseconds(2);
    vn += mux1.read(i);
    delayMicroseconds(2);
    vn += mux1.read(i);
    delayMicroseconds(2);
    vn += mux1.read(i);
    delayMicroseconds(2);
    vn += mux1.read(i);
    vn = vn / 5;
    if (abs(mux1_pot[i] - vn) > 2) {
      mux1_pot[i] = vn;
      if (i == 0) {

        //codec.volume((double)(mux1_pot[i]) / 1023);
      }
      if (i == 14) {
        ampl = (double)(mux1_pot[i]) / 1023;
      }
      if (i == 13) {
        ampl2 = (double)(mux1_pot[i]) / 1023;
      }
      if (i == 15) {
        codec.volume((double)(mux1_pot[i]) / 1023);
      }
    }
  }
  //mux1.disable();
  for (int i = 0; i < 16; i++) {
    mux2.setChannel(i, ENABLED);
    delayMicroseconds(5);
    if (i == 0) {
      val_pts += String("POTS:") + i + ":[" + mux1_pot[i] + " ";
    } else {
      val_pts += String("") + ", " + mux1_pot[i] + "";
    }
    if (i == 15) {
      val_pts += "] ; ";
    }

    mux2_pot[i] = mux2.read(i);
    if (i == 0) {
      val += String("BTNS:") + i + ":[" + mux2.read(i) + "-" + mux2_pot[i] + " ";
    } else {
      val += String("") + ", " + mux2_pot[i] + "";
    }
    if (i == 15) {
      val += "] ";
    }
  }
  //mux2.disable();
  val += val_pts + String(" POS: ") + oldPosition + " || Used:" + used_voices + " Stooped:" + stopped_voices + " evict_voice:" + evict_voice + " notes_played:" + notes_played + " ampl2:" + ampl2 + " ampl:" + ampl;
  if (pt_val != val) {
    pt_val = val;
    Serial.println(val);
  }
}

void printSpaces(int num) {
  for (int i = 0; i < num; i++) {
    Serial.print(" ");
  }
}

/**
 * 
 * 
 */
void encoder_stuff() {
#define ENCODER_DEVIDER 4
  long newPosition = ctrlEncoder.read() / ENCODER_DEVIDER;
  if (newPosition != oldPosition) {
    if (newPosition <= 0) {
      newPosition = 1;
      ctrlEncoder.write(1 * ENCODER_DEVIDER);
    }
    oldPosition = newPosition;
  }
}

const unsigned short mixers_for_one_layer(const unsigned short voices) {
  return (voices / MAX_INPUTS_IN_MIXER) + (voices % MAX_INPUTS_IN_MIXER != 0);
}

const unsigned short total_layers(const unsigned short voices) {
  unsigned short t = voices;
  unsigned short counter = 0;
  while (t > 1) {
    t = mixers_for_one_layer(t);
    counter++;
  }
  return counter;
}

const bool mk_in_use(struct mixer_keeper *mk) {
  return (mk->channel1 || mk->channel2 || mk->channel3 || mk->channel4);
}

const bool mk_in_have_free_port(struct mixer_keeper *mk) {
  return (!mk->full && !(mk->channel1 && mk->channel2 && mk->channel3 && mk->channel4));
}

const bool mk_in_use_disconnected(struct mixer_keeper *mk) {
  return (mk_in_use(mk) && mk->output == 0);
}

const unsigned char mk_get_free_port(struct mixer_keeper *mk) {
  if (!mk->channel1) return 1;
  if (!mk->channel2) return 2;
  if (!mk->channel3) return 3;
  if (!mk->channel4) return 4;
  return 0;
}

struct mixer_keeper *get_free_mx() {
  for (short i = 0; i < TOTAL_MIXER_KEEPERS; i++) {
    if (mk_in_have_free_port(&synth_mixer_keeper[i])) {
      return &synth_mixer_keeper[i];
    }
  }
  Serial.println("ERROR: [get_free_mx] NEED TO MALLOC");
  return NULL;
}

// Define the mixer_keeper struct with bitfields
struct mixer_keeper2 {
  AudioMixer4 *synth_mixer;
  AudioConnection *ac_channel1;
  AudioConnection *ac_channel2;
  AudioConnection *ac_channel3;
  AudioConnection *ac_channel4;
  AudioConnection *ac_output;
  uint8_t layer : 4;
  uint8_t channel : 2;
  bool full : 1;
  bool in_use : 1;
  bool output : 1;
};

// Define a vector of mixer_keeper structs
std::vector<mixer_keeper> synth_mixer_keepers2;

// // Find a mixer with an available channel
// mixer_keeper2 *get_free_mx2() {
//   auto it = std::find_if(synth_mixer_keepers2.begin(), synth_mixer_keepers2.end(),
//                          [](const mixer_keeper &mk) {
//                            return !mk.in_use && !mk.full;
//                          });
//   if (it != synth_mixer_keepers.end()) {
//     it->in_use = true;
//     return &(*it);
//   } else {
//     Serial.println("ERROR: [get_free_mx] NEED TO MALLOC");
//     return nullptr;
//   }
// }

// // Create an audio connection and assign it to a mixer keeper
// void create_audio_connection(AudioStream &src, uint8_t src_ch, AudioStream &dst, uint8_t dst_ch=0) {
//   AudioConnection *ac = new AudioConnection(src, src_ch, dst, dst_ch);
//   mk.output = true;
//   mk.ac_output = ac;
//   switch (mk.channel) {
//     case 0:
//       break;
//     case 1:
//       mk.channel = 2;
//       mk.ac_channel2 = ac;
//       break;
//     case 2:
//       mk.channel = 3;
//       mk.ac_channel3 = ac;
//       break;
//     case 3:
//       mk.channel = 4;
//       mk.ac_channel4 = ac;
//       mk.full = true;  // Set the full flag to true
//       break;
//   }
// }


void build_wavetable_synth(const unsigned short voices = 1) {
  AudioNoInterrupts();
  unsigned short total_mixers_reuired = 0;

  unsigned short t = voices;
  unsigned short layers = 0;
  while (1) {
    t = mixers_for_one_layer(t);
    total_mixers_reuired += t;
    layers++;
    if (t <= 1)
      break;
  }
  Serial.println(String("VOICES=") + voices + " Layers=" + layers + " Total Mixeres required=" + total_mixers_reuired);
  for (short i = 0; i < total_mixers_reuired; i++) {
    synth_mixer[i] = new AudioMixer4();  //(AudioMixer4 *)malloc(sizeof(AudioMixer4));
    synth_mixer[i]->gain(0, 0.5);
    synth_mixer[i]->gain(1, 0.5);
    synth_mixer[i]->gain(2, 0.5);
    synth_mixer[i]->gain(3, 0.5);
    synth_mixer_keeper[i].synth_mixer = synth_mixer[i];
    synth_mixer_keeper[i].id = synth_mixer_keeper_used;
    synth_mixer_keeper_used++;
  }
  struct mixer_keeper *mk;
  AudioConnection *newac;
  unsigned char used_channel = 0;
  unsigned char zero_ch = 0;
  for (short i = 0; i < voices; i++) {
    synth_wavetable[i] = new AudioSynthWavetable();  //(AudioSynthWavetable *)malloc(sizeof(AudioSynthWavetable));
    synth_wavetable[i]->amplitude(1);
    mk = get_free_mx();
    used_channel = mk_get_free_port(mk);
    newac = new AudioConnection(*synth_wavetable[i], 0, *mk->synth_mixer, used_channel);
    if (used_channel == 1) {
      mk->channel1 = 1;
      mk->ac_channel1 = newac;
    } else if (used_channel == 2) {
      mk->channel2 = 1;
      mk->ac_channel2 = newac;
    } else if (used_channel == 3) {
      mk->channel3 = 1;
      mk->ac_channel3 = newac;
    } else if (used_channel == 4) {
      mk->channel4 = 1;
      mk->ac_channel4 = newac;
      mk->full = 1;
    }
    synth_patch[synth_patch_used] = newac;
    synth_patch_used++;
    mk->layer = 1;
  }

  Serial.println(String("synth_mixer_keeper_used=") + synth_mixer_keeper_used + " synth_patch_used=" + synth_patch_used);
  t = voices;
  for (short layer = 1; layer < layers; layer++) {
    t = mixers_for_one_layer(t);
    Serial.println(String("Work with LAYER: ") + layer + " addtional X reuired = " + t);

    for (short i = 0; i < total_mixers_reuired; i++) {
      if (mk_in_use_disconnected(&synth_mixer_keeper[i]) && synth_mixer_keeper[i].layer != layer + 1) {
        mk = get_free_mx();
        used_channel = mk_get_free_port(mk);
        newac = new AudioConnection(*synth_mixer_keeper[i].synth_mixer, 0, *mk->synth_mixer, used_channel);
        synth_mixer_keeper[i].output = 1;
        synth_mixer_keeper[i].ac_output = newac;
        if (used_channel == 1) {
          mk->channel1 = 1;
          mk->ac_channel1 = newac;
        } else if (used_channel == 2) {
          mk->channel2 = 1;
          mk->ac_channel2 = newac;
        } else if (used_channel == 3) {
          mk->channel3 = 1;
          mk->ac_channel3 = newac;
        } else if (used_channel == 4) {
          mk->channel4 = 1;
          mk->ac_channel4 = newac;
          mk->full = 1;
        }
        synth_patch[synth_patch_used] = newac;
        synth_patch_used++;
        mk->layer = layer + 1;
        Serial.println(String("Created AUTDIO CONNECTION=") + synth_mixer_keeper[i].id + "->" + mk->id + " LAYER=" + mk->layer);
      }
    }
  }
  newac = new AudioConnection(*mk->synth_mixer, 0, mainMixer, 1);  // (AudioConnection *)malloc(sizeof(AudioConnection));
  synth_patch[synth_patch_used] = newac;
  synth_patch_used++;
  Serial.println(String("Created AUTDIO CONNECTION LAST") + "->" + mk->id + " mainMixer");
  mk->output = 1;
  mk->ac_output = newac;
  Serial.println(String("synth_mixer_keeper_used=") + synth_mixer_keeper_used + " synth_patch_used=" + synth_patch_used);
  AudioInterrupts();
}

unsigned short allocateVoice(byte channel, byte note) {
  unsigned short i;
  int nonfree_voices = stopped_voices + used_voices;
  if (nonfree_voices < TOTAL_VOICES) {
    for (i = nonfree_voices; i < TOTAL_VOICES && voices[i].channel != channel; ++i)
      ;
    if (i < TOTAL_VOICES) {
      voice_t temp = voices[i];
      voices[i] = voices[nonfree_voices];
      voices[nonfree_voices] = temp;
    }
    i = nonfree_voices;
    used_voices++;
  } else {
    if (stopped_voices) {
      i = evict_voice % stopped_voices;
      voice_t temp = voices[i];
      stopped_voices--;
      voices[i] = voices[stopped_voices];
      voices[stopped_voices] = temp;
      used_voices++;
      i = stopped_voices;
    } else
      i = evict_voice;
  }

  voices[i].channel = channel;
  voices[i].note = note;

  evict_voice++;
  evict_voice %= TOTAL_VOICES;

  return voices[i].id;
}

unsigned short findVoice(byte channel, byte note) {
  int i;
  //find match
  int nonfree_voices = stopped_voices + used_voices;
  for (i = stopped_voices; i < nonfree_voices && !(voices[i].channel == channel && voices[i].note == note); ++i)
    ;
  //return TOTAL_VOICES if no match
  if (i == (nonfree_voices)) return TOTAL_VOICES;

  voice_t temp = voices[i];
  voices[i] = voices[stopped_voices];
  voices[stopped_voices] = temp;
  --used_voices;

  return voices[stopped_voices++].id;
}

void freeVoices() {
  for (int i = 0; i < stopped_voices; i++)
    //if (wavetable[voices[i].id].isPlaying() == false) {
    if (synth_wavetable[voices[i].id]->isPlaying() == false) {
      voice_t temp = voices[i];
      --stopped_voices;
      voices[i] = voices[stopped_voices];
      int nonfree_voices = stopped_voices + used_voices;
      voices[stopped_voices] = voices[nonfree_voices];
      voices[nonfree_voices] = temp;
    }
}

void printVoices() {
  static int last_notes_played = notes_played;
  if (last_notes_played == notes_played)
    return;
  last_notes_played = notes_played;
  int usage = AudioProcessorUsage();
  Serial.printf("\nCPU:%03i voices:%02i CPU/Voice:%02i evict:%02i", usage, used_voices, usage / used_voices, evict_voice);
  for (int i = 0; i < used_voices; ++i)
    Serial.printf(" %02hhu %-2s", voices[i].channel, note_map[voices[i].note % 12]);
}

/**
 *
 */
void OnNoteOn(byte channel, byte note, byte velocity) {
  notes_played++;
  if (notes_played > 1000000) {
    notes_played = 0;
  }
#ifdef DEBUG_ALLOC
  Serial.printf("**** NoteOn: channel==%hhu,note==%hhu ****\n", channel, note);

#endif  //DEBUG_ALLOC
  freeVoices();
  unsigned short id = allocateVoice(channel, note);
  AudioSynthWavetable *wt;
  wt = &wavetable[id];
  switch (oldPosition) {
    case 1:
      wt->setInstrument(FrenchHorns);
      break;
    case 2:
      wt->setInstrument(vibraphone);
      break;
    case 3:
      wt->setInstrument(Ocarina);
      break;
    case 4:
      wt->setInstrument(tuba);
      break;
    case 5:
      wt->setInstrument(trumpet);
      break;
    case 6:
      wt->setInstrument(distortiongt);
      break;
    case 7:
      wt->setInstrument(bassoon);
      break;
    case 8:
      wt->setInstrument(clarinet);
      break;
    case 9:
      wt->setInstrument(mutedgtr);
      break;
    case 10:
      wt->setInstrument(flute);
      break;
    case 11:
      wt->setInstrument(Viola);
      break;
    case 12:
      wt->setInstrument(harmonica);
      break;
    case 13:
      wt->setInstrument(harp);
      break;
    case 14:
      wt->setInstrument(mutedgtr);
      break;
    // case 15:
    //   wt->setInstrument(vibraphone);
    //   break;
    // case 16:
    //   wt->setInstrument(trumpet);
    //   break;
    // case 17:
    //   wt->setInstrument(tuba);
    //   break;
    // case 18:
    //   wt->setInstrument(vibraphone);
    //   break;
    default:
      wt->setInstrument(FrenchHorns);
      break;
  }
  mainMixer.gain(0, ampl2);
  wt->playNote(note, velocity);

  mainMixer.gain(1, ampl);
  synth_wavetable[id]->setInstrument(FrenchHorns);
  synth_wavetable[id]->playNote(note, velocity);
#ifdef DEBUG_ALLOC
  printVoices();
#endif  //DEBUG_ALLOC
}


void OnNoteOff(byte channel, byte note, byte velocity) {
#ifdef DEBUG_ALLOC
  Serial.printf("\n**** NoteOff: channel==%hhu,note==%hhu ****", channel, note);
  printVoices();
#endif
  unsigned short id = findVoice(channel, note);
  if (id != TOTAL_VOICES)
    wavetable[id].stop();
  synth_wavetable[id]->stop();
#ifdef DEBUG_ALLOC
  printVoices();
#endif
}

void card_staff() {

  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!card.init(SPI_FULL_SPEED, BUILTIN_SDCARD)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card inserted?");
    Serial.println("* is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");
    return;
  } else {
    Serial.println("Wiring is correct and a card is present.");
  }

  // print the type of card
  Serial.print("\nCard type: ");
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknown");
  }

  // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
  if (!SDvolume.init(card)) {
    Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
    return;
  }


  // print the type and size of the first FAT-type volume
  uint32_t volumesize;
  Serial.print("\nVolume type is FAT");
  Serial.println(SDvolume.fatType(), DEC);
  Serial.println();

  volumesize = SDvolume.blocksPerCluster();  // clusters are collections of blocks
  volumesize *= SDvolume.clusterCount();     // we'll have a lot of clusters
  if (volumesize < 8388608ul) {
    Serial.print("Volume size (bytes): ");
    Serial.println(volumesize * 512);  // SD card blocks are always 512 bytes
  }
  Serial.print("Volume size (Kbytes): ");
  volumesize /= 2;
  Serial.println(volumesize);
  Serial.print("Volume size (Mbytes): ");
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print("Volume size (Gbytes): ");
  Serial.println(volumesize / 1024.0);

  Serial.println("Print directory using SD functions");
  File root = SD.open("/soundfont/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;  // no more files
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
    } else {
      printSpaces(40 - strlen(entry.name()));
      Serial.print("  ");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }

  Serial.println();
  Serial.println("Print directory using SdFat ls() function");
  SD.sdfs.ls();
  //attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), encoder_stuff, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(ENCODER_DT_PIN), encoder_stuff, CHANGE);

  File myFile;
  myFile = SD.open("test.txt");

  if (myFile) {
    Serial.println("test.txt:");

    // read from the file until there's nothing else in it:
    while (myFile.available()) {
      Serial.write(myFile.read());
    }
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }

  // File soundfont = SD.open("soundfont.sf2");
  // GMExtBank.SF2
}

#ifdef ENABLE_SCREEN
void setup_scrren() {
  tft.begin();
  Serial.println("read diagnostics (optional but can help debug problems)");
  uint8_t x = tft.readcommand8(ILI9341_RDMODE);
  Serial.print("Display Power Mode: 0x");
  Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDMADCTL);
  Serial.print("MADCTL Mode: 0x");
  Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDPIXFMT);
  Serial.print("Pixel Format: 0x");
  Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDIMGFMT);
  Serial.print("Image Format: 0x");
  Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDSELFDIAG);
  Serial.print("Self Diagnostic: 0x");
  Serial.println(x, HEX);
}
#endif

#ifdef ENABLE_ETH
void setup_eth() {
  // start the Ethernet connection:
  Serial.println("Trying to get an IP address using DHCP");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      // while (true) {
      //   delay(1); // do nothing, no point running without Ethernet hardware
      // }
    } else {
      eth_started = true;
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // initialize the Ethernet device not using DHCP:
    Ethernet.begin(mac, ip, myDns, gateway, subnet);
  }
  // print your local IP address:
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
  // start the server
  if (eth_started) {
    server.begin();
    Serial.print("server is at ");
    Serial.println(Ethernet.localIP());
  }
}
#endif

#ifdef ENABLE_ETH
void loop_eth() {
  if (eth_started) {
    // listen for incoming clients
    EthernetClient client = server.available();
    if (client) {
      Serial.println("new client");
      // an http request ends with a blank line
      boolean currentLineIsBlank = true;
      //while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");         // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // output the value of each analog input pin
          // for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
          //   int sensorReading = analogRead(analogChannel);
          //   client.print("analog input ");
          //   client.print(analogChannel);
          //   client.print(" is ");
          //   client.print(sensorReading);
          //   client.println("<br />");
          // }
          client.println("</html>");
          //break;
        }
        if (c == '\n') {
          currentLineIsBlank = true;  // you're starting a new line
        } else if (c != '\r') {
          currentLineIsBlank = false;  // you've gotten a character on the current line
        }
      }
      //}
      // give the web browser time to receive the data
      delay(1);
      // close the connection:
      client.stop();
      Serial.println("client disconnected");
    }
  }
}
#endif