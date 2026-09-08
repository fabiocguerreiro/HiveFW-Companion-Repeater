#pragma once
#include <cstdint> // For uint8_t, uint32_t
#include <helpers/ConfigSerializer.h>

#define TELEM_MODE_DENY            0
#define TELEM_MODE_ALLOW_FLAGS     1     // use contact.flags
#define TELEM_MODE_ALLOW_ALL       2

#define ADVERT_LOC_NONE       0
#define ADVERT_LOC_SHARE      1

class NodePrefs : public ConfigSerializer {  // persisted to file
public:
  float airtime_factor = 0;
  char node_name[32];
  double node_lat = 0, node_lon = 0;
  float freq = 0;
  uint8_t sf = 0;
  uint8_t cr = 0;
  uint8_t multi_acks = 0;
  uint8_t manual_add_contacts = 0;
  float bw = 0;
  int8_t tx_power_dbm = 0;
  uint8_t telemetry_mode_base = 0;
  uint8_t telemetry_mode_loc = 0;
  uint8_t telemetry_mode_env = 0;
  float rx_delay_base = 0;
  uint32_t ble_pin = 0;
  uint8_t  advert_loc_policy = 0;
  uint8_t  buzzer_quiet = 0;
  uint8_t  vibe_quiet = 0;
  uint8_t  gps_enabled = 0;      // GPS enabled flag (0=disabled, 1=enabled)
  uint32_t gps_interval = 0;     // GPS read interval in seconds
  uint8_t autoadd_config = 0;    // bitmask for auto-add contacts config
  uint8_t rx_boosted_gain = 0; // SX126x RX boosted gain mode (0=power saving, 1=boosted)
  uint8_t radio_fem_rxgain = 0; // external LoRa FEM RX gain (LNA)
  uint8_t radio_fem_txgain = 0; // external LoRa FEM TX gain (low by default)
  uint8_t _client_repeat = 0;  // DEPRECATED -> use repeat.disable_fwd
  uint8_t path_hash_mode = 0;    // which path mode to use when sending
  uint8_t autoadd_max_hops = 0;  // 0 = no limit, 1 = direct (0 hops), N = up to N-1 hops (max 64)
  char default_scope_name[31];
  uint8_t default_scope_key[16];
  uint8_t apps_channel_hash[PATH_HASH_SIZE];

  // HiveFW — cor da barra superior do ecrã a cores.
  // 0=vermelho, 1=verde, 2=azul, 3=ciano,
  // 4=magenta, 5=amarelo, 6=laranja, 7=branco.
  uint8_t header_color = 0;


  // HiveFW — timeout automático do ecrã.
  //
  // 0 = 5 segundos
  // 1 = 15 segundos
  // 2 = 1 minuto
  // 3 = 5 minutos
  // 4 = sempre ligado
  //
  // Default: 15 segundos.
  uint8_t display_timeout = 1;


  // HiveFW — orientação do TFT.
  //
  // 0 = orientação atual/original
  // 1 = invertido 180 graus
  uint8_t display_rotation = 0;

private:
  class RadioPrefs : public ConfigSerializer {  // COPIED from CommonCLI (for now)
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("freq", _parent->freq);
      def("bw", _parent->bw);
      def("sf", _parent->sf);
      def("cr", _parent->cr);
      //def("cad", _parent->cad_enabled);
      //def("int_thr", _parent->interference_threshold);
      def("rxgain", _parent->rx_boosted_gain);
    #if 0
      // NOTE: these cannot be set (yet) so don't load/save until we can.
      //       also, fem_rxgain WAS mapped to wrong JSON property previously
      def("fem_rxgain", _parent->radio_fem_rxgain);
      def("fem_txgain", _parent->radio_fem_txgain);
    #endif
      def("tx", _parent->tx_power_dbm);
      def("af", _parent->airtime_factor);
      def("rxdelay", _parent->rx_delay_base);
      //def("f_txdelay", _parent->tx_delay_factor);   currently hard-coded
      //def("d_txdelay", _parent->direct_tx_delay_factor);  currently hard-coded
      //def("agc_int", _parent->agc_reset_interval);
      def("hash_mode", _parent->path_hash_mode);
      def("multi_ack", _parent->multi_acks);
    }
  public:
    RadioPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  RadioPrefs radio;

  class GPSPrefs : public ConfigSerializer {  // COPIED from CommonCLI (for now)
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("en", _parent->gps_enabled); // boolean
      def("int", _parent->gps_interval);   // interval in seconds
      def("adv_loc", _parent->advert_loc_policy);
    }
  public:
    GPSPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  GPSPrefs gps;

  class RepeatPrefs : public ConfigSerializer {  // COPIED from CommonCLI (for now)
  public:
    uint8_t disable_fwd = 1;
    uint8_t auto_advert = 0;
  protected:
    void structure() override {
      def("disable", disable_fwd);
      def("auto_adv", auto_advert);
      //def("f_max", flood_max);
      //def("f_max_uns", flood_max_unscoped);
      //def("f_max_adv", flood_max_advert);
      //def("loop", loop_detect);
    }
  };
  RepeatPrefs repeat;

  class CompanionPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("auto_max", _parent->autoadd_max_hops);  // 0 = no limit, 1 = direct (0 hops), N = up to N-1 hops (max 64)
      def("defs_nm", _parent->default_scope_name, sizeof(_parent->default_scope_name));
      def("defs_key", (void *) _parent->default_scope_key, sizeof(_parent->default_scope_key));
      def("pin", _parent->ble_pin);
      def("buzz_q", _parent->buzzer_quiet);
      def("vibe_q", _parent->vibe_quiet);
      def("auto_add", _parent->autoadd_config);    // bitmask for auto-add contacts config
      def("man_add", _parent->manual_add_contacts);
      def("tel_base", _parent->telemetry_mode_base);
      def("tel_loc", _parent->telemetry_mode_loc);
      def("tel_env", _parent->telemetry_mode_env);
      def("apps_ch", (void *) _parent->apps_channel_hash,
          sizeof(_parent->apps_channel_hash));
      def("hdr_col", _parent->header_color);
      def("disp_to", _parent->display_timeout);
      def("disp_rot", _parent->display_rotation);
    }
  public:
    CompanionPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  CompanionPrefs companion;

protected:
  void structure() override {
    def("name", node_name, sizeof(node_name));
    //def("adv_int", advert_interval);
    //def("f_adv_int", flood_advert_interval);
    def("lat", node_lat);
    def("lon", node_lon);
    def("radio", radio);
    def("gps", gps);
    def("repeat", repeat);
    def("comp", companion);
  }
public:
  NodePrefs() : radio(this), gps(this), companion(this) {
    node_name[0] = 0;
    default_scope_name[0] = 0;
    memset(default_scope_key, 0, sizeof(default_scope_key));
    memset(apps_channel_hash, 0, sizeof(apps_channel_hash));
  }
  // new accessor methods
  bool isRepeatEn() const { return repeat.disable_fwd == 0; }
  void setRepeatEn(bool en) { repeat.disable_fwd = en ? 0 : 1; }

  bool isAutoAdvertEn() const { return repeat.auto_advert == 1; }
  void setAutoAdvertEn(bool en) { repeat.auto_advert = en ? 1 : 0; }
};
