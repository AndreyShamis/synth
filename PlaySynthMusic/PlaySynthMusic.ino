//* ------------ DEFINES ---------------- *//
//#define ENABLE_ETH
//#define ENABLE_SCREEN
//#define DEBUG_ALLOC
//* ===================================== *//

//* ----------- INCLUDES----------------- *//
#include <Audio.h>
// #include <Bounce.h>
#include <Bounce2.h>
#include <Encoder.h>
#include <Wire.h>
#include <SPI.h>
// #include <SD.h>
#include <SerialFlash.h>
#include <MIDI.h>
#include "MIDIUSB.h"
#include "PlaySynthMusic.h"
#include "MUX74HC4067.h"
#include "Pizzicato_samples.h"
#include "FrenchHorns_samples.h"
#include "Viola_samples.h"
#include "bassoon_samples.h"
#include "clarinet_samples.h"
#include "distortiongt_samples.h"
#include "epiano_samples.h"
#include "flute_samples.h"
#include "frenchhorn_samples.h"
#include "glockenspiel_samples.h"
#include "gtfretnoise_samples.h"
#include "harmonica_samples.h"
#include "harp_samples.h"
#include "mutedgtr_samples.h"
#include "nylonstrgtr_samples.h"
// #include "oboe_samples.h"
#include "overdrivegt_samples.h"
#include "recorder_samples.h"
#include "standard_DRUMS_samples.h"
#include "steelstrgtr_samples.h"
#include "strings_samples.h"
#include "timpani_samples.h"
#include "trombone_samples.h"
#include "trumpet_samples.h"
#include "tuba_samples.h"
#include "piano_samples.h"
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
  int wavetable_id;
  byte channel;
  byte note;
};
//* ===================================== *//
#ifdef ENABLE_SCREEN
#define TFT_DC 20
#define TFT_CS 3     // TODO IN USE BY  MUX2_SIG_PIN
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

#define AMPLITUDE (0.2)

#define ENCODER_CLK_PIN 3
#define ENCODER_DT_PIN 4
#define ENCODER_SW_BUTTON 5

Encoder ctrlEncoder(ENCODER_CLK_PIN, ENCODER_DT_PIN);
Button ctrlEncoderBtn =  Button();

#define MUX_1_EN_PIN 30  // 31
#define MUX_2_EN_PIN 31  // 31
#define MUX_S0_PIN 28
#define MUX_S1_PIN 29
#define MUX_S2_PIN 34
#define MUX_S3_PIN 35
#define MUX_1_SIG_PIN 41
#define MUX_2_SIG_PIN 2
MUX74HC4067 mux1(MUX_1_EN_PIN, MUX_S0_PIN, MUX_S1_PIN, MUX_S2_PIN, MUX_S3_PIN);
MUX74HC4067 mux2(MUX_2_EN_PIN, MUX_S0_PIN, MUX_S1_PIN, MUX_S2_PIN, MUX_S3_PIN);

unsigned char* sp = score;
// allocate a wave type to each channel. The types used and their order is purely arbitrary.
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

int allocateVoice(byte channel, byte note);
int findVoice(byte channel, byte note);
void freeVoices();

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
const char* note_map[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

const short TOTAL_VOICES = 64;
const short TOTAL_MIXERS = 21;
const short SECONDARY_MIXERS = 4;
voice_t voices[TOTAL_VOICES];
AudioOutputI2S audioOut;  //xy=1631,416
AudioControlSGTL5000 codec;  //xy=1684,296
AudioSynthWavetable wavetable[TOTAL_VOICES];
AudioMixer4 mixer[TOTAL_MIXERS];
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
  { mixer[TOTAL_MIXERS - 1], 0, audioOut, 0 },
  { mixer[TOTAL_MIXERS - 1], 0, audioOut, 1 },
};


IntervalTimer myTimer;

void setup() {
  // Setting up pins
  //pinMode(ENCODER_SW_BUTTON, INPUT);
  ctrlEncoderBtn.attach(ENCODER_SW_BUTTON, INPUT);
  ctrlEncoderBtn.interval(5);
  ctrlEncoderBtn.setPressedState(LOW);
  //Bounce(ENCODER_SW_BUTTON, 10);
  // Start Serial
  Serial.begin(115200);
  Serial.println(String("Begin ") + __FILE__);

// Start Screen
#ifdef ENABLE_SCREEN
  setup_screen();
#endif

  mux1.signalPin(MUX_1_SIG_PIN, INPUT, ANALOG);
  mux2.signalPin(MUX_2_SIG_PIN, INPUT_PULLUP, DIGITAL);

  AudioMemory(18);

  codec.enable();
  codec.volume(0.85);

  // Initialize processor and memory measurements
  AudioProcessorUsageMaxReset();
  AudioMemoryUsageMaxReset();
  // sine_fm1.amplitude(0.3);
  // sine_fm1.frequency(1046.5023);

  // waveform1.frequency(440);
  // waveform1.amplitude(1.0);
  // waveform1.begin(WAVEFORM_TRIANGLE);

  Serial.println(String("Setting up ") + TOTAL_VOICES + " voices;");
  for (int i = 0; i < TOTAL_VOICES; ++i) {
    wavetable[i].setInstrument(Pizzicato);
    wavetable[i].amplitude(1);
    voices[i].wavetable_id = i;
    voices[i].channel = voices[i].note = 0xFF;
  }
  Serial.println(String("Setting up gain for ") + TOTAL_MIXERS + " mixers;");
  for (int i = 0; i < TOTAL_MIXERS - 1; ++i) {
    for (int j = 0; j < 4; ++j) {
      mixer[i].gain(j, 0.50);
    }
  }

  for (int i = 0; i < 4; ++i) {
    Serial.println(String("Setting up gain for ") + (TOTAL_MIXERS-1) + " mixer");
    mixer[TOTAL_MIXERS - 1].gain(i, i < SECONDARY_MIXERS ? 1.0 / SECONDARY_MIXERS : 0.0);
  }

  usbMIDI.setHandleNoteOn(OnNoteOn);
  usbMIDI.setHandleNoteOff(OnNoteOff);

#ifdef ENABLE_ETH
  setup_eth();
#endif

  //attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), encoder_stuff, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(ENCODER_DT_PIN), encoder_stuff, CHANGE);
  myTimer.begin(check_muxes, 150000);  // run every 0.15 seconds
  delay(50);
  Serial.println("----------setup done----------");
  delay(100);
  Serial.println("");
  delay(200);
}

long oldPosition = 1;
unsigned long ta = 0;
int prev_val = 0;
String pt_val = "";




/**
 * 
 * 
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
    mux1.setChannel(i, ENABLED);
    delayMicroseconds(20);
    //mux1_pot[i] = mux1.read(i);
    int16_t vn = mux1.read(i);
    if (abs(mux1_pot[i] - vn) > 6 ) {
        mux1_pot[i] = vn;
    }
    if (i == 15) {
      codec.volume((double)(mux1_pot[i]) / 1023);
    }
  }
  mux1.disable();
  for (int i = 0; i < 16; i++) {
    mux2.setChannel(i, ENABLED);
    delayMicroseconds(20);
    if (i == 0) {
      val_pts += String("POTS:") + i + ":[" + mux1_pot[i] + " ";
    } else {
      val_pts += String("") + ", " + mux1_pot[i] + "";
    }
    if(i==15) {
      val_pts += "] ; ";
    }
    
    mux2_pot[i] = mux2.read(i);
    if (i == 0) {
      val += String("BTNS:") + i + ":[" + mux2.read(i) + "-" + mux2_pot[i] + " ";
    } else {
      val += String("") + ", " + mux2_pot[i] + "";
    }
    if(i==15) {
      val += "] ";
    }
  }
  mux2.disable();
  val += val_pts + String(" POS: ") + oldPosition + " || Used:" + used_voices + " Stooped:" + stopped_voices + " evict_voice:" + evict_voice + " notes_played:" + notes_played;
  if(pt_val != val) {
    pt_val = val;
    Serial.println(val);
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

int allocateVoice(byte channel, byte note) {
  int i;
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

  return voices[i].wavetable_id;
}

int findVoice(byte channel, byte note) {
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

  return voices[stopped_voices++].wavetable_id;
}

void freeVoices() {
  for (int i = 0; i < stopped_voices; i++)
    if (wavetable[voices[i].wavetable_id].isPlaying() == false) {
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


void OnNoteOn(byte channel, byte note, byte velocity) {
  notes_played++;
  if (notes_played > 1000000) {
    notes_played = 0;
  }
#ifdef DEBUG_ALLOC
  Serial.printf("**** NoteOn: channel==%hhu,note==%hhu ****\n", channel, note);

#endif  //DEBUG_ALLOC
  freeVoices();
  int wavetable_id = allocateVoice(channel, note);
  switch (oldPosition) {
    case 1:
      wavetable[wavetable_id].setInstrument(nylonstrgtr);
      break;
    case 2:
      wavetable[wavetable_id].setInstrument(FrenchHorns);
      break;
    case 3:
      wavetable[wavetable_id].setInstrument(Ocarina);
      break;
    case 4:
      wavetable[wavetable_id].setInstrument(tuba);
      break;
    case 5:
      wavetable[wavetable_id].setInstrument(Pizzicato);
      break;
    case 6:
      wavetable[wavetable_id].setInstrument(distortiongt);
      break;
    case 7:
      wavetable[wavetable_id].setInstrument(bassoon);
      break;
    case 8:
      wavetable[wavetable_id].setInstrument(clarinet);
      break;
    case 9:
      wavetable[wavetable_id].setInstrument(epiano);
      break;
    case 10:
      wavetable[wavetable_id].setInstrument(flute);
      break;
    case 11:
      wavetable[wavetable_id].setInstrument(Viola);
      break;
    case 12:
      wavetable[wavetable_id].setInstrument(harmonica);
      break;
    case 13:
      wavetable[wavetable_id].setInstrument(harp);
      break;
    case 14:
      wavetable[wavetable_id].setInstrument(mutedgtr);
      break;
    case 15:
      wavetable[wavetable_id].setInstrument(nylonstrgtr);
      break;
    case 16:
      wavetable[wavetable_id].setInstrument(trumpet);
      break;
    case 17:
      wavetable[wavetable_id].setInstrument(tuba);
      break;
    case 18:
      wavetable[wavetable_id].setInstrument(vibraphone);
      break;
    default:
      wavetable[wavetable_id].setInstrument(Pizzicato);
      break;
  }
  wavetable[wavetable_id].playNote(note, velocity);
#ifdef DEBUG_ALLOC
  printVoices();
#endif  //DEBUG_ALLOC
}


void OnNoteOff(byte channel, byte note, byte velocity) {
#ifdef DEBUG_ALLOC
  Serial.printf("\n**** NoteOff: channel==%hhu,note==%hhu ****", channel, note);
  printVoices();
#endif
  int wavetable_id = findVoice(channel, note);
  if (wavetable_id != TOTAL_VOICES)
    wavetable[wavetable_id].stop();
#ifdef DEBUG_ALLOC
  printVoices();
#endif
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