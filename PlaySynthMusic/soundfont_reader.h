#include "synth_wavetable.h"

#include "SdFat.h"

#pragma once
struct sample_data {
  // SAMPLE VALUES
   int16_t* sample;
   bool LOOP;
   int INDEX_BITS;
   float PER_HERTZ_PHASE_INCREMENT;
   uint32_t MAX_PHASE;
   uint32_t LOOP_PHASE_END;
   uint32_t LOOP_PHASE_LENGTH;
   uint16_t INITIAL_ATTENUATION_SCALAR;
  
  // VOLUME ENVELOPE VALUES
   uint32_t DELAY_COUNT;
   uint32_t ATTACK_COUNT;
   uint32_t HOLD_COUNT;
   uint32_t DECAY_COUNT;
   uint32_t RELEASE_COUNT;
   int32_t SUSTAIN_MULT;

  // VIRBRATO VALUES
   uint32_t VIBRATO_DELAY;
   uint32_t VIBRATO_INCREMENT;
   float VIBRATO_PITCH_COEFFICIENT_INITIAL;
   float VIBRATO_PITCH_COEFFICIENT_SECOND;

  // MODULATION VALUES
   uint32_t MODULATION_DELAY;
   uint32_t MODULATION_INCREMENT;
   float MODULATION_PITCH_COEFFICIENT_INITIAL;
   float MODULATION_PITCH_COEFFICIENT_SECOND;
   int32_t MODULATION_AMPLITUDE_INITIAL_GAIN;
   int32_t MODULATION_AMPLITUDE_SECOND_GAIN;
};

struct instrument_data {
   uint8_t sample_count;
   uint8_t* sample_note_ranges;
   sample_data* samples;
};

uint32_t padding(uint32_t length, uint32_t block) {
  uint32_t extra;
  extra = length % block;
  if (extra == 0) return 0;
  return block - extra;
}

struct SubChunkHeader {
  uint32_t id;
  uint32_t size;
};
// based on: http://soundfile.sapp.org/doc/WaveFormat/
struct WaveFileHeader {
  uint32_t chunkId;
  uint32_t chunkSize;
  uint32_t format;
  struct {
    struct SubChunkHeader header;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
  } subChunk1;
  struct SubChunkHeader subChunk2Header;
} __attribute__((packed));
//so we just allocate a chunk of memory with this as the header...
//load the wav PCM data at the end and then point to it!
//AudioSynthWavetable inst1;
//instrument_data
uint16_t clip32to16(float indata2) {
  //Constrained<float> constrainedFloat( indata2 , -1.0 , 1.0 );
  //float tf = constrainedFloat;
  //uint16_t oo = tf  * 32767.0f;
  //uint16_t oo = indata2  * 32767.0f;
  return ( indata2  * 32767.0f);
}
float clip16to32(uint16_t indata2) {
  //uint16_t oo = indata2  * 32767.0f;
  return (indata2  / 32767.0f);
}
struct sample_data * loadSampleAsInstrument(uint8_t base_note, const char *f) { /* load a wav and convert to teensy wavetable format */
  /* need to add sountfont conversion as its the only way i know how to add pitch change */

  WaveFileHeader header;
  SubChunkHeader sch;
  int16_t format;
  uint32_t length, padlength = 0;
  int arraylen = 0;
  int subChunk2Offset = 0;
  int headersize = sizeof(WaveFileHeader) - sizeof(SubChunkHeader);
  boolean headergood = 0;
  File32 tmpf;

  __disable_irq();
  //AudioNoInterrupts();
  tmpf.open(f);

  if (tmpf == 0) {
    tmpf.close();
    __enable_irq();
    //AudioInterrupts();
    return 0; //some problem happenend
  }

  if (tmpf.read(&header, headersize) < headersize) {
    tmpf.close();
    __enable_irq();
    // AudioInterrupts();
    return 0; //some problem happenend
  }

  //AudioInterrupts();
  header.chunkId = __REV(header.chunkId);
  header.format = __REV(header.format);
  header.subChunk1.header.id = __REV(header.subChunk1.header.id);


  while (tmpf.available()) {
    //tmpf.read((void *) & (sch), sizeof(sch));
 //Serial.print("reading..");
 // Serial.println(tmpf.curPosition() );
  
    if (tmpf.read((void *) & (sch), sizeof(sch)) != sizeof(sch)) {
      tmpf.close();
      __enable_irq();
      //AudioInterrupts();
      //we have a problem here!
      return 0;
    }

    sch.id = __REV(sch.id);
    //sch.size = __REV(sch.size);
    if (sch.id == 0x64617461) {
      // found the data section
      header.subChunk2Header.id = sch.id;
      header.subChunk2Header.size = sch.size;
      headergood = 1;
      break;
    } else {
      //if (sch.size == 0) {tmpf.seekCur}
      tmpf.seekCur(__REV(sch.size)); /* have to reverse edianess ?? */
    }
  }

  if (!headergood) {
    tmpf.close();
    __enable_irq();
    //AudioInterrupts();
    //Serial.print("wav no good!");
    return 0;
  }
  //AudioInterrupts();
  //Serial.print("sample rate:");
  Serial.println(header.subChunk1.sampleRate);
  //uses code from teensy audio's wav2sketch
  // AudioPlayMemory requires padding to 2.9 ms boundary (128 samples @ 44100)
  length = header.subChunk2Header.size;

  // data.dwMinLength = (data.dwChunkSize / format.dwAvgBytesPerSec) / 60;
  //data.dSecLength = ((double)data.dwChunkSize / (double)format.dwAvgBytesPerSec) - (double)data.dwMinLength*60;
  padlength = 0;
  // the length must be a multiple of the data size
  if (header.subChunk1.numChannels == 2) {
    if (length % 4) {
      // Serial.print(length);
      // Serial.println("length isnt % 4");  //("file %s data length is not a multiple of 4", filename);
    }
    length = length / 4;
  }
  if (header.subChunk1.numChannels == 1) {
    if (length % 1) {

      //Serial.println("length isnt % 2");

    }
    //if (length % 1) die("file %s data length is not a multiple of 2", filename);
    length = length / 2;
  }
  if (header.subChunk1.sampleRate == 44100) {
    padlength += padding(length, 128);
    format = 1;
  } else if (header.subChunk1.sampleRate == 22050) {
    padlength += padding(length, 64);
    format = 2;
  } else if (header.subChunk1.sampleRate == 11025) {
    padlength += padding(length, 32);
    format = 3;
  }
  arraylen = ((length + padlength) * 2 + 3) / 4 + 1;
  //len(f) / f.samplerate)
  //Serial.print ("wav size ms:");
  //Serial.println(arraylen / header.subChunk1.sampleRate);
  format |= 0x80;

  int memneeded = sizeof(sample_data) + (arraylen * 4);
  //int memneeded = sizeof(instrument_data) + (arraylen * 4);
  //noInterrupts();
  uint8_t * memptr = (uint8_t *) malloc(memneeded);
  uint8_t * sampdatastart = memptr + sizeof(sample_data);//+ sizeof(instrument_data);
  if (!memptr) {
    Serial.println("sample is too big");  /*no memory left */
    __enable_irq();
    //AudioInterrupts();
    //interrupts();
    return 0;
  }
  memset(memptr, 0, memneeded);
  //interrupts();
  //uint8_t * mem8ptr =  memptr + 4;
  uint16_t * mem16ptr = (uint16_t *) sampdatastart ;//(uint16_t *) memptr + 2;
  uint32_t * mem32ptr =  (uint32_t *) sampdatastart;
  uint32_t flen = ((length - padlength)  | (format << 24));
  // *mem32ptr++ = flen; dont need
  int len = flen;
  int LENGTH_BITS = 16;
  int LENGTH = len;
  int LOOPEND = LENGTH - 1;
  int LOOPSTART = 0;


  sample_data NULL_SAMPLE =
  {
    (int16_t*)sampdatastart, //16-bit PCM encoded audio sample -- location = memory + sizeof(_sample_data)
    false, //Whether or not to loop this sample
    LENGTH_BITS,  //Number of bits needed to hold length
    ((0x80000000 >> (LENGTH_BITS - 1)) * 1.0 * (44100.0 / AUDIO_SAMPLE_RATE_EXACT)) / WAVETABLE_NOTE_TO_FREQUENCY(base_note)  + 0.5, //((0x80000000 >> (index_bits - 1)) * cents_offset * sampling_rate / AUDIO_SAME_RATE_EXACT) / sample_freq + 0.5
    ((uint32_t)LENGTH - 1) << (32 - LENGTH_BITS), //(sample_length-1) << (32 - sample_length_bits)
    ((uint32_t)LOOPEND - 1) << (32 - LENGTH_BITS), //(loop_end-1) << (32 - sample_length_bits) == LOOP_PHASE_END
    (((uint32_t)LOOPEND - 1) << (32 - LENGTH_BITS)) - (((uint32_t)LOOPSTART - 1) << (32 - LENGTH_BITS)), //LOOP_PHASE_END - (loop_start-1) << (32 - sample_length_bits) == LOOP_PHASE_END - LOOP_PHASE_START == LOOP_PHASE_LENGTH
    uint16_t(UINT16_MAX * WAVETABLE_DECIBEL_SHIFT(-0 / 100.0)), //INITIAL_ATTENUATION_SCALAR
    uint32_t(0 * AudioSynthWavetable::SAMPLES_PER_MSEC / 8.0 + 0.5), //DELAY_COUNT
    uint32_t(0 * AudioSynthWavetable::SAMPLES_PER_MSEC / 8.0 + 0.5), //ATTACK_COUNT
    uint32_t(0 * AudioSynthWavetable::SAMPLES_PER_MSEC / 8.0 + 0.5), //HOLD_COUNT
    uint32_t(0 * AudioSynthWavetable::SAMPLES_PER_MSEC / 8.0 + 0.5), //DECAY_COUNT
    uint32_t(0 * AudioSynthWavetable::SAMPLES_PER_MSEC / 8.0 + 0.5), //RELEASE_COUNT
    int32_t(0 * AudioSynthWavetable::UNITY_GAIN), //SUSTAIN_MULT
    uint32_t(0 * AudioSynthWavetable::SAMPLES_PER_MSEC / (2 * AudioSynthWavetable::LFO_PERIOD)),  // VIBRATO_DELAY
    uint32_t(0 / 1000.0 * AudioSynthWavetable::LFO_PERIOD * (UINT32_MAX / AUDIO_SAMPLE_RATE_EXACT)), // VIBRATO_INCREMENT
    (WAVETABLE_CENTS_SHIFT(-0 / 1000.0) - 1.0) * 4, // VIBRATO_PITCH_COEFFICIENT_INITIAL
    (1.0 - WAVETABLE_CENTS_SHIFT(0 / 1000.0)) * 4, // VIBRATO_COEFFICIENT_SECONDARY
    uint32_t(0 * AudioSynthWavetable::SAMPLES_PER_MSEC / (2 * AudioSynthWavetable::LFO_PERIOD)), // MODULATION_DELAY
    uint32_t(0 / 1000.0 * AudioSynthWavetable::LFO_PERIOD * (UINT32_MAX / AUDIO_SAMPLE_RATE_EXACT)), // MODULATION_INCREMENT
    (WAVETABLE_CENTS_SHIFT(-0 / 1000.0) - 1.0) * 4, // MODULATION_PITCH_COEFFICIENT_INITIAL
    (1.0 - WAVETABLE_CENTS_SHIFT(0 / 1000.0)) * 4, // MODULATION_PITCH_COEFFICIENT_SECOND
    int32_t(UINT16_MAX * (WAVETABLE_DECIBEL_SHIFT(-0.1) - 1.0)) * 4, // MODULATION_AMPLITUDE_INITIAL_GAIN
    int32_t(UINT16_MAX * (1.0 - WAVETABLE_DECIBEL_SHIFT(0.1))) * 4, // MODULATION_AMPLITUDE_FINAL_GAIN
  };

  ///instrument_data NULL_INSTRUMENT = {1,DEFAULT_NOTE_RANGES,0};
  //memcpy(memptr,&NULL_INSTRUMENT,sizeof(instrument_data));
  //memcpy(memptr + sizeof(uint8_t) + sizeof(uint8_t*),&NULL_SAMPLE,sizeof(sample_data));
  memcpy(memptr, &NULL_SAMPLE, sizeof(sample_data));

  //
  //const instrument_data 808snare = {1, 808snare_ranges, 808snare_samples };
  int px = header.subChunk1.blockAlign;
  int le = header.subChunk2Header.size;


  Serial.print("num channels:");
  Serial.println(header.subChunk1.numChannels);
  Serial.print("block align:");
  Serial.println(header.subChunk1.blockAlign);
  Serial.print("bits:");
  Serial.println(header.subChunk1.bitsPerSample);


  while ((px < arraylen) && tmpf.available()) {
    uint16_t samplesize = header.subChunk1.blockAlign * header.subChunk1.numChannels;
    uint8_t samplemem[header.subChunk1.blockAlign * 4];
    //now lets convert the waw, teensy audio expects 16bit * 2
    //I think teensy's instruments are mono
    switch (header.subChunk1.bitsPerSample) {
      case 8:
        tmpf.read(&samplemem, header.subChunk1.blockAlign * 2);
        if (header.subChunk1.numChannels == 1) {
          samplemem[1] = samplemem[0];
        }
        *mem16ptr++ = samplemem[0] * 255;
        *mem16ptr++ = samplemem[1] * 255;
        break;

      case 24:
        tmpf.read(&samplemem, header.subChunk1.blockAlign * 2);
        if (header.subChunk1.numChannels == 1) {
          samplemem[3] = samplemem[0];
          samplemem[4] = samplemem[1];
          samplemem[5] = samplemem[5];
        }
        *mem16ptr++ = word(samplemem[2], samplemem[1]) ;//+ samplemem[0];
        *mem16ptr++ = word(samplemem[5], samplemem[4]) ;//+ samplemem[3];
        break;

      case 16:
        tmpf.read(&samplemem, header.subChunk1.blockAlign * 2);
        if (header.subChunk1.numChannels == 1) {
          samplemem[3] = samplemem[1];
          samplemem[2] = samplemem[0];
        }
        *mem16ptr++ = word(samplemem[1], samplemem[0]);
        *mem16ptr++ = word(samplemem[3], samplemem[2]);
        break;

      case 32:
        tmpf.read(&samplemem, header.subChunk1.blockAlign * 2);
        float * floatdat = (float *) samplemem;
        uint16_t * smp = (uint16_t *) samplemem;
        uint8_t swapbytes[4];
        float * ch;
        swapbytes[0] = samplemem[0];
        swapbytes[1] = samplemem[1];
        swapbytes[2] = samplemem[2];
        swapbytes[3] = samplemem[3];
        ch = (float *)swapbytes;
        *mem16ptr++ = clip32to16(*ch);

        swapbytes[0] = samplemem[4];
        swapbytes[1] = samplemem[5];
        swapbytes[2] = samplemem[6];
        swapbytes[3] = samplemem[7];
        ch = (float *)swapbytes;
        *mem16ptr++ = clip32to16(*ch);
        break;
    }
    px ++;
  }
  tmpf.close();

  __enable_irq();
  AudioInterrupts();
  //return (unsigned int *) memptr;
  // return (instrument_data *) memptr;
  return (sample_data *) memptr;
}
