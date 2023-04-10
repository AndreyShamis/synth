class MixerKeeper {
private:
  uint8_t id;
  AudioMixer4* synth_mixer;
  short int layer;
  bool channel1 = false, channel2 = false, channel3 = false, channel4 = false, full = false, output = false;
  AudioConnection *ac_channel1 = nullptr, *ac_channel2 = nullptr, *ac_channel3 = nullptr, *ac_channel4 = nullptr, *ac_output = nullptr;
public:
  MixerKeeper(uint8_t _id, AudioMixer4* _synth_mixer)
    : id(_id), synth_mixer(_synth_mixer) {}


  uint8_t get_id() const {
    return id;
  }
  void set_id(uint8_t id) {
    this->id = id;
  }

  AudioMixer4* get_synth_mixer() const {
    return synth_mixer;
  }
  void set_synth_mixer(AudioMixer4* synth_mixer) {
    this->synth_mixer = synth_mixer;
  }

  short int get_layer() const {
    return layer;
  }
  void set_layer(short int layer) {
    this->layer = layer;
  }

  bool is_channel1() const {
    return channel1;
  }
  void set_channel1(bool channel1) {
    this->channel1 = channel1;
  }

  bool is_channel2() const {
    return channel2;
  }
  void set_channel2(bool channel2) {
    this->channel2 = channel2;
  }

  bool is_channel3() const {
    return channel3;
  }
  void set_channel3(bool channel3) {
    this->channel3 = channel3;
  }

  bool is_channel4() const {
    return channel4;
  }
  void set_channel4(bool channel4) {
    this->channel4 = channel4;
  }

  bool is_full() const {
    return full;
  }
  void set_full(bool full) {
    this->full = full;
  }

  bool is_output() const {
    return output;
  }
  void set_output(bool output) {
    this->output = output;
  }

  AudioConnection* get_ac_channel1() const {
    return ac_channel1;
  }
  void set_ac_channel1(AudioConnection* ac_channel1) {
    this->ac_channel1 = ac_channel1;
  }

  AudioConnection* get_ac_channel2() const {
    return ac_channel2;
  }
  void set_ac_channel2(AudioConnection* ac_channel2) {
    this->ac_channel2 = ac_channel2;
  }

  AudioConnection* get_ac_channel3() const {
    return ac_channel3;
  }
  void set_ac_channel3(AudioConnection* ac_channel3) {
    this->ac_channel3 = ac_channel3;
  }

  AudioConnection* get_ac_channel4() const {
    return ac_channel4;
  }
  void set_ac_channel4(AudioConnection* ac_channel4) {
    this->ac_channel4 = ac_channel4;
  }

  AudioConnection* get_ac_output() const {
    return ac_output;
  }
  void set_ac_output(AudioConnection* ac_output) {
    this->ac_output = ac_output;
  }

  bool inUse() const {
    return (channel1 || channel2 || channel3 || channel4);
  }


  bool in_have_free_port() const {
    return (!full && !(channel1 && channel2 && channel3 && channel4));
  }

  bool in_use_disconnected() const {
    return (inUse() && output == 0);
  }

  unsigned char get_free_port() const {
    if (!channel1) return 1;
    if (!channel2) return 2;
    if (!channel3) return 3;
    if (!channel4) return 4;
    return 0;
  }
};
