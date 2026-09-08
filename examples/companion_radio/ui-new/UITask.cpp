#include "UITask.h"

#if defined(NRF52) || defined(NRF52_SERIES)
#include <bluefruit.h>
#endif
#include <helpers/TxtDataHelpers.h>
#include "../MyMesh.h"
#include "target.h"
#ifdef WIFI_SSID
  #include <WiFi.h>
#endif

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS     20000   // 20 seconds
#endif
#define BOOT_SCREEN_MILLIS   3000   // 3 seconds

#ifdef PIN_STATUS_LED
#define LED_ON_MILLIS     20
#define LED_ON_MSG_MILLIS 200
#define LED_CYCLE_MILLIS  4000
#endif

#define LONG_PRESS_MILLIS   1200

#ifndef UI_RECENT_LIST_SIZE
  #define UI_RECENT_LIST_SIZE 4
#endif

#if UI_HAS_JOYSTICK
  #define PRESS_LABEL "press Enter"
#else
  #define PRESS_LABEL "long press"
#endif

#include "icons.h"


// ============================================================================
// HIVEFW — MAPA OFICIAL DA ESTRUTURA DO MENU
// ============================================================================
//
// MP-00 — FIRST
//   ├── MENSAGENS / BLE / GPS
//   ├── SMARTPHONE BLE: nome do dispositivo ou Desligado
//   └── NICKNAME: nome do nó
// MP-01 — RECENTES
// MP-02 — MENSAGENS
//
// MP-03 — COMPANION
//   ├── INFO COMPANION
//   │   ├── BLE / BATERIA / VOLTAGEM / UPTIME
//   │   ├── ADVERT TX / ADVERT RX
//   │   ├── NOME BLE / SMARTPHONE / NOME DO NO
//   │   └── MODO / FIRMWARE / VERSAO
//   ├── SINCRONIZAR RELÓGIO
//   ├── SINCRONIZAR VIA GPS (quando disponível)
//   ├── DESCOBRIR REPETIDORES
//   ├── REPETIDORES DESCOBERTOS
//   └── [ SAIR ]
//
// MP-04 — RÁDIO
//   ├── FREQUÊNCIA / BANDWIDTH / SPREADING FACTOR
//   ├── CODING RATE / TX POWER / PATH / RX GAIN
//   ├── PRESETS
//   └── [ SAIR ]
//
// MP-05 — REPETIDOR
//   ├── REPETIDOR
//   ├── AUTOADVERT
//   ├── DUTY CYCLE
//   ├── VIZINHOS
//   ├── INFO REPETIDOR
//   └── [ SAIR ]
//
// MP-06 — SOS
//   └── ENTER -> confirmação e envio SOS
//
// MP-07 — APLICAÇÕES
//   ├── HOME ASSISTANT
//   │   ├── <COMANDOS CONFIGURADOS PELO UTILIZADOR>
//   │   ├── ADICIONAR COMANDO
//   │   ├── GERIR COMANDOS (quando existirem comandos)
//   │   └── [ SAIR ]
//   ├── GPS / SENSORES (quando disponíveis)
//   ├── RELÓGIO
//   └── [ SAIR ]
//
// MP-08 — DEFINIÇÕES
//   ├── BLUETOOTH
//   ├── ANUNCIAR NÓ
//   ├── CANAL APPS/SOS
//   ├── COR DA BARRA (T114 com ecrã a cores)
//   ├── GPS (quando disponível)
//   ├── DESLIGAR
//   └── [ SAIR ]
//
// NAVEGAÇÃO:
// FIRST → RECENTES → MENSAGENS → COMPANION → RÁDIO
//       → REPETIDOR → SOS → APLICAÇÕES → DEFINIÇÕES → FIRST
//
// ============================================================================
// FIM DO MAPA OFICIAL DA ESTRUTURA DO MENU
// ============================================================================


class SplashScreen : public UIScreen {
  UITask* _task;
  unsigned long dismiss_after;
  char _version_info[32];

public:
  SplashScreen(UITask* task) : _task(task) {
    // Custom firmware version shown on the boot screen.
    // build.sh appends the Git commit hash to FIRMWARE_VERSION.
    // Example: V1.01-368c97d
    // Only the human-readable version is shown here: V1.01.
    const char *ver = FIRMWARE_VERSION;

    snprintf(_version_info, sizeof(_version_info), "%s", ver);

    char *hash_sep = strrchr(_version_info, '-');
    if (hash_sep) {
      const char *hash = hash_sep + 1;
      size_t hash_len = strlen(hash);
      bool is_hex = hash_len >= 7 && hash_len <= 40;

      for (size_t i = 0; i < hash_len && is_hex; i++) {
        char c = hash[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) {
          is_hex = false;
        }
      }

      if (is_hex) {
        *hash_sep = '\0';
      }
    }

    dismiss_after = millis() + BOOT_SCREEN_MILLIS;
  }

  int render(DisplayDriver& display) override {
    // meshcore logo
    display.setColor(UIColor::corp_blue);
    int logoWidth = 128;
    display.drawXbm(0, 3, hivefw_logo, logoWidth, 13);

    // firmware name
    const char* firmware_name = "Companion & Repeater";
    display.setColor(UIColor::primary_txt);
    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 22, firmware_name);

    // version info
    display.setColor(UIColor::primary_txt);
    display.setTextSize(1);
    display.drawTextCentered(
      display.width()/2,
      35,
      _version_info
    );

    // build date
    char build_date[16];
    char build_month[4] = {0};
    int build_day = 0;
    int build_year = 0;

    // FIRMWARE_BUILD_DATE aceita os dois formatos:
    //
    //   Sep 08 2026
    //   08-Sep-2026
    //
    // O primeiro é o formato oficial atual e coincide com
    // o formato standard de __DATE__.

    const char* raw_build_date = FIRMWARE_BUILD_DATE;

    bool build_date_ok = false;

    // Formato standard:
    // Sep 08 2026
    if (
      sscanf(
        raw_build_date,
        "%3s %d %d",
        build_month,
        &build_day,
        &build_year
      ) == 3
    ) {
      build_date_ok = true;
    }

    // Compatibilidade com builds HiveFW antigos:
    // 08-Sep-2026
    if (!build_date_ok) {
      build_month[0] = '\0';
      build_day = 0;
      build_year = 0;

      if (
        sscanf(
          raw_build_date,
          "%d-%3s-%d",
          &build_day,
          build_month,
          &build_year
        ) == 3
      ) {
        build_date_ok = true;
      }
    }

    if (build_date_ok) {

      const char* months_en[] = {
        "Jan", "Feb", "Mar", "Apr",
        "May", "Jun", "Jul", "Aug",
        "Sep", "Oct", "Nov", "Dec"
      };

      const char* months_pt[] = {
        "JAN", "FEV", "MAR", "ABR",
        "MAI", "JUN", "JUL", "AGO",
        "SET", "OUT", "NOV", "DEZ"
      };

      for (int i = 0; i < 12; i++) {
        if (strcmp(build_month, months_en[i]) == 0) {
          strncpy(
            build_month,
            months_pt[i],
            sizeof(build_month) - 1
          );

          build_month[
            sizeof(build_month) - 1
          ] = '\0';

          break;
        }
      }

      snprintf(
        build_date,
        sizeof(build_date),
        "%02d %s %04d",
        build_day,
        build_month,
        build_year
      );

    } else {

      // Nunca deixar a linha vazia mesmo que apareça
      // futuramente um formato de data desconhecido.
      snprintf(
        build_date,
        sizeof(build_date),
        "%s",
        raw_build_date
      );
    }

    display.setColor(UIColor::secondary_txt);
    display.setTextSize(1);
    display.drawTextCentered(
      display.width()/2,
      48,
      build_date
    );

    return 1000;
  }

  void poll() override {
    if (millis() >= dismiss_after) {
      _task->gotoHomeScreen();
    }
  }
};


// ========================================================================
// PRESETS MESHCORE — tabela única
//
// Esta tabela é usada pelo menu PRESETS para:
//   - apresentar o nome
//   - identificar o preset actualmente activo
//   - aplicar frequência / BW / SF / CR
//
// TX POWER, PATH e RX GAIN não fazem parte do preset.
// ========================================================================
struct RadioPreset {
  const char* name;
  float freq;
  float bw;
  uint8_t sf;
  uint8_t cr;
};

static const RadioPreset radio_presets[] = {
  { "AUSTRALIA",             915.800f, 250.0f, 10, 5 },
  { "AUSTRALIA NARROW",      916.575f,  62.5f,  7, 5 },
  { "AUSTRALIA SA/WA/QLD",   923.125f,  62.5f,  8, 5 },
  { "EU/UK NARROW",          869.618f,  62.5f,  8, 8 },
  { "EU/UK LONG RANGE",      869.525f, 250.0f, 11, 5 },
  { "EU/UK MEDIUM RANGE",    869.525f, 250.0f, 10, 5 },
  { "CZECH REPUBLIC",        869.432f,  62.5f,  7, 5 },
  { "EU 433MHZ",             433.650f, 250.0f, 11, 5 },
  { "NEW ZEALAND",            917.375f, 250.0f, 11, 5 },
  { "NEW ZEALAND NARROW",     917.375f,  62.5f,  7, 5 },
  { "PORTUGAL 433",           433.375f,  62.5f,  9, 5 },
  { "PORTUGAL 868",           869.618f,  62.5f,  7, 5 },
  { "SWITZERLAND",            869.618f,  62.5f,  8, 8 },
  { "USA/CANADA",             910.525f,  62.5f,  7, 5 },
  { "VIETNAM",                920.250f, 250.0f, 11, 5 }
};

static const int RADIO_PRESET_COUNT =
  sizeof(radio_presets) / sizeof(radio_presets[0]);


// ========================================================================
// HIVEFW — CORES DA BARRA SUPERIOR
//
// A opção só aparece em hardware T114 com TFT a cores.
// ========================================================================

#ifdef HELTEC_T114_WITH_DISPLAY

static const char* hivefw_header_color_names[] = {
  "VERMELHO",
  "VERDE",
  "AZUL",
  "CIANO",
  "MAGENTA",
  "AMARELO",
  "LARANJA",
  "BRANCO"
};

static const uint8_t HIVEFW_HEADER_COLOR_COUNT =
  sizeof(hivefw_header_color_names) /
  sizeof(hivefw_header_color_names[0]);

#define HIVEFW_SETTINGS_COLOR_OFFSET 1

#else

#define HIVEFW_SETTINGS_COLOR_OFFSET 0

#endif

static int findRadioPreset(
  float freq,
  float bw,
  uint8_t sf,
  uint8_t cr
) {
  for (int i = 0; i < RADIO_PRESET_COUNT; i++) {
    if (
      fabs(freq - radio_presets[i].freq) < 0.001f &&
      fabs(bw - radio_presets[i].bw) < 0.01f &&
      sf == radio_presets[i].sf &&
      cr == radio_presets[i].cr
    ) {
      return i;
    }
  }

  return -1;
}

class HomeScreen : public UIScreen {
  enum HomePage {
    FIRST,
    RECENT,
    MESSAGES,
    COMPANION,
    RADIO,
    REPETIDOR,
    SOS,
    APPS,
    SETTINGS,
    Count,    // keep as last

    // Estados internos das APPS.
    INTERNAL_HOME_ASSISTANT,
    INTERNAL_CLOCK,
    INTERNAL_GPS,
    INTERNAL_SENSORS
  };

  // ========================================================
  // COMPANION — estados nomeados
  // ========================================================

  enum CompanionMenu : uint8_t {
    COMP_MENU_INFO = 0,
    COMP_MENU_SYNC_CLOCK,
#if ENV_INCLUDE_GPS == 1
    COMP_MENU_SYNC_GPS,
#endif
    COMP_MENU_DISCOVERY,
    COMP_MENU_DISCOVERED,
    COMP_MENU_EXIT,
    COMP_MENU_COUNT
  };

  enum CompanionInfoPage : uint8_t {
    COMP_INFO_BLE = 0,
    COMP_INFO_BATTERY,
    COMP_INFO_VOLTAGE,
    COMP_INFO_UPTIME,
    COMP_INFO_ADVERT_TX,
    COMP_INFO_ADVERT_RX,
    COMP_INFO_BLE_NAME,
    COMP_INFO_SMARTPHONE,
    COMP_INFO_NODE_NAME,
    COMP_INFO_MODE,
    COMP_INFO_FIRMWARE,
    COMP_INFO_VERSION,
    COMP_INFO_COUNT
  };

  enum RepeaterMenu : uint8_t {
    REPEATER_MENU_TOGGLE = 0,
    REPEATER_MENU_AUTOADVERT,
    REPEATER_MENU_DUTY_CYCLE,
    REPEATER_MENU_NEIGHBOURS,
    REPEATER_MENU_INFO,
    REPEATER_MENU_EXIT,
    REPEATER_MENU_COUNT
  };

  static const uint8_t REPEATER_INFO_PAGE_COUNT = 13;

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  uint8_t _page;
  uint8_t _companion_menu;
  bool _companion_submenu;
  bool _companion_info_submenu;
  uint8_t _companion_info_page;

  uint8_t _sms_menu;
  bool _sms_submenu;
  uint8_t _sms_messages_menu;
  bool _sms_messages_submenu;
  uint8_t _sms_new_menu;
  bool _sms_new_submenu;

  // Nova mensagem
  // 0 = menu principal
  // 1 = escolher contacto
  // 2 = escrever
  // 3 = presets
  // 4 = confirmação
  // 5 = ações da mensagem
  uint8_t _sms_new_stage;
  uint8_t _sms_contact_menu;
  uint8_t _sms_preset_menu;
  uint8_t _sms_char_index;
  uint8_t _sms_action_menu;
  uint8_t _sms_confirm;
  uint8_t _sms_flow_type;
  char _sms_text[128];
  ContactInfo _sms_recipient;
  bool _sms_recipient_valid;
  uint8_t _sms_target_type;       // 0 = contacto, 1 = canal
  uint8_t _sms_target_menu;
  uint8_t _sms_channel_menu;
  ChannelDetails _sms_channel;
  bool _sms_channel_valid;

  struct ChannelUnread {
    uint8_t hash[PATH_HASH_SIZE];
    uint8_t count;
    bool valid;
  };

  static const int MAX_CHANNEL_UNREAD = 16;
  ChannelUnread _channel_unread[MAX_CHANNEL_UNREAD];
  uint8_t _settings_menu;
  bool _settings_submenu;

  bool _settings_channel_submenu;
  uint8_t _settings_channel_menu;

  bool _settings_color_submenu;
  uint8_t _settings_color_menu;

  uint8_t _settings_advert_menu;
  bool _settings_advert_submenu;
  bool _settings_confirm;
  uint8_t _settings_confirm_menu;
  // ========================================================
  // HOME ASSISTANT — comandos totalmente configuráveis
  // ========================================================

  enum HAStage : uint8_t {
    HA_STAGE_MAIN = 0,
    HA_STAGE_EDIT_NAME,
    HA_STAGE_EDIT_COMMAND,
    HA_STAGE_MANAGE_LIST,
    HA_STAGE_MANAGE_ACTION,
    HA_STAGE_DELETE_CONFIRM
  };

  static const uint8_t HA_EDIT_NEW = 0xFF;

  uint8_t _ha_menu;
  bool _ha_submenu;
  uint8_t _ha_stage;

  uint8_t _ha_manage_menu;
  uint8_t _ha_action_menu;
  uint8_t _ha_delete_confirm;

  uint8_t _ha_edit_index;
  uint8_t _ha_char_index;
  uint8_t _ha_command_count;

  HiveFWHACommand
    _ha_commands[HIVEFW_HA_MAX_COMMANDS];

  char _ha_edit_name[HIVEFW_HA_NAME_LEN];
  char _ha_edit_command[HIVEFW_HA_COMMAND_LEN];

  // APLICAÇÕES — índices oficiais do menu
  enum AppsMenu {
    APPS_HOME_ASSISTANT = 0,
#if ENV_INCLUDE_GPS == 1
    APPS_GPS,
#endif
#if UI_SENSORS_PAGE == 1
    APPS_SENSORS,
#endif
    APPS_CLOCK,
    APPS_EXIT,
    APPS_MENU_COUNT
  };

  // APLICAÇÕES — vistas internas
  // Os valores são mantidos para preservar o comportamento actual.
  enum AppsView {
    APPS_VIEW_NONE = 0,
    APPS_VIEW_CLOCK = 1,
    APPS_VIEW_HOME_ASSISTANT = 2,
    APPS_VIEW_GPS = 3,
    APPS_VIEW_SENSORS = 4,
    APPS_VIEW_DISCOVERY = 5,
    APPS_VIEW_DISCOVERED = 6,
    APPS_VIEW_SOS = 7
  };

  uint8_t _apps_menu;
  uint8_t _apps_view;

  bool _sos_submenu;
  uint8_t _sos_menu;
  bool _sos_confirm_submenu;

  // HiveFW Repeater information pages:
  // 0 RSSI
  // 1 SNR
  // 2 TX Airtime
  // 3 RX Airtime
  // 4 Uptime
  // 5 Battery
  // 6 Noise Floor
  // 7 Packets RX
  // 8 Packets TX
  // 9 RX Errors
  // 10 Flood RX/TX
  // 11 Direct RX/TX
  // 12 Localização
  uint8_t _repeater_stats_page;

  // ========================================================================
  // MP-05 — REPETIDOR
  //
  // _repeater_submenu:
  //   false = MP-05.1 página/logo do REPETIDOR
  //   true  = MP-05.2 menu principal do REPETIDOR
  //
  // _repeater_info_submenu:
  //   false = página/logo ou menu principal
  //   true  = MP-05.3 selector de estatísticas
  //
  // _repeater_menu:
  //   0 = REPETIDOR
  //   1 = AUTOADVERT
  //   2 = INFO REPETIDOR
  //   3 = [ SAIR ]
  // ========================================================================
  bool _repeater_submenu;
  bool _repeater_info_submenu;
  bool _repeater_neighbours_submenu;
  bool _repeater_duty_submenu;
  uint8_t _repeater_menu;
  uint8_t _repeater_neighbour_menu;
  uint8_t _repeater_duty_value;

  // ========================================================================
  // MP-04 — RÁDIO
  //
  // _radio_submenu:
  //   false = página principal do Rádio
  //   true  = MENU RÁDIO
  //
  // _radio_menu:
  //   0 = FREQUÊNCIA
  //   1 = BANDWIDTH
  //   2 = SPREADING FACTOR
  //   3 = CODING RATE
  //   4 = TX POWER
  //   5 = PATH
  //   6 = RX GAIN
  //   7 = PRESETS
  //   8 = [ SAIR ]
  //
  // O MENU RÁDIO contém directamente a configuração.
  // ========================================================================
  bool _radio_submenu;
  uint8_t _radio_menu;

  bool _radio_freq_edit;
  uint8_t _radio_freq_digit;
  uint8_t _radio_freq_digits[6];

  bool _radio_tx_edit;
  uint8_t _radio_tx_digit;
  uint8_t _radio_tx_digits[2];

  bool _radio_value_edit;
  uint8_t _radio_value_index;

  bool _apps_submenu;
  bool _apps_return;


  // ========================================================================
  // HOME ASSISTANT — HELPERS
  // ========================================================================

  static const char* haEditorCharset() {

    // Índice 0 = apagar
    // Índice 1 = cancelar
    // Índice 2 = espaço
    // Índice 3 = A (posição inicial)
    //
    // "!" não existe no editor.
    // É acrescentado automaticamente ao enviar.

    return
      "\b\x1B "
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789"
      "-_/.,?@#():";
  }


  int haMainCount() const {

    // comandos guardados
    // + ADICIONAR COMANDO
    // + GERIR COMANDOS, apenas se existirem comandos
    // + [ SAIR ]

    return
      _ha_command_count +
      2 +
      (_ha_command_count > 0 ? 1 : 0);
  }


  int haAddIndex() const {
    return _ha_command_count;
  }


  int haManageIndex() const {

    if (_ha_command_count == 0)
      return -1;

    return _ha_command_count + 1;
  }


  int haExitIndex() const {

    return
      _ha_command_count +
      1 +
      (_ha_command_count > 0 ? 1 : 0);
  }


  const char* haMainLabel() {

    if (_ha_menu < _ha_command_count)
      return _ha_commands[_ha_menu].name;

    if (_ha_menu == haAddIndex())
      return "ADICIONAR COMANDO";

    if (
      _ha_command_count > 0 &&
      _ha_menu == haManageIndex()
    )
      return "GERIR COMANDOS";

    return "[ SAIR ]";
  }


  void loadHACommands() {

    memset(
      _ha_commands,
      0,
      sizeof(_ha_commands)
    );

    int count =
      the_mesh.loadHACommands(
        _ha_commands,
        HIVEFW_HA_MAX_COMMANDS
      );

    if (count < 0)
      count = 0;

    if (count > HIVEFW_HA_MAX_COMMANDS)
      count = HIVEFW_HA_MAX_COMMANDS;

    _ha_command_count =
      (uint8_t)count;

    _ha_menu = 0;
    _ha_manage_menu = 0;
    _ha_action_menu = 0;
    _ha_delete_confirm = 0;
    _ha_stage = HA_STAGE_MAIN;
  }


  static void trimHAField(char* text) {

    if (text == nullptr)
      return;

    char* start = text;

    while (*start == ' ')
      start++;

    if (start != text) {

      memmove(
        text,
        start,
        strlen(start) + 1
      );
    }

    size_t len = strlen(text);

    while (
      len > 0 &&
      text[len - 1] == ' '
    ) {

      text[len - 1] = '\0';
      len--;
    }
  }


  bool haNameExists(
    const char* name,
    int ignore_index
  ) const {

    for (
      int i = 0;
      i < _ha_command_count;
      i++
    ) {

      if (i == ignore_index)
        continue;

      if (
        strcmp(
          _ha_commands[i].name,
          name
        ) == 0
      )
        return true;
    }

    return false;
  }


  void beginHAAdd() {

    _ha_edit_index = HA_EDIT_NEW;
    _ha_char_index = 3;

    _ha_edit_name[0] = '\0';
    _ha_edit_command[0] = '\0';

    _ha_stage = HA_STAGE_EDIT_NAME;
  }


  void beginHAEdit(uint8_t index) {

    if (index >= _ha_command_count)
      return;

    _ha_edit_index = index;
    _ha_char_index = 3;

    strncpy(
      _ha_edit_name,
      _ha_commands[index].name,
      sizeof(_ha_edit_name) - 1
    );

    _ha_edit_name[
      sizeof(_ha_edit_name) - 1
    ] = '\0';

    strncpy(
      _ha_edit_command,
      _ha_commands[index].command,
      sizeof(_ha_edit_command) - 1
    );

    _ha_edit_command[
      sizeof(_ha_edit_command) - 1
    ] = '\0';

    _ha_stage = HA_STAGE_EDIT_NAME;
  }


  void cancelHAEditor() {

    if (
      _ha_stage ==
      HA_STAGE_EDIT_COMMAND
    ) {

      _ha_stage =
        HA_STAGE_EDIT_NAME;

      _ha_char_index = 3;

      return;
    }

    if (_ha_edit_index == HA_EDIT_NEW) {

      _ha_stage = HA_STAGE_MAIN;
      _ha_menu = haAddIndex();

    } else {

      _ha_stage =
        HA_STAGE_MANAGE_ACTION;

      _ha_action_menu = 0;
    }
  }


  bool saveHAEditor() {

    trimHAField(_ha_edit_name);
    trimHAField(_ha_edit_command);

    // Nunca guardar "!".
    while (_ha_edit_command[0] == '!') {

      memmove(
        _ha_edit_command,
        _ha_edit_command + 1,
        strlen(_ha_edit_command)
      );
    }

    if (_ha_edit_name[0] == '\0') {

      _task->showAlert(
        "Nome vazio",
        1200
      );

      _ha_stage =
        HA_STAGE_EDIT_NAME;

      return false;
    }

    if (_ha_edit_command[0] == '\0') {

      _task->showAlert(
        "Comando vazio",
        1200
      );

      return false;
    }

    int ignore =
      (_ha_edit_index == HA_EDIT_NEW)
        ? -1
        : _ha_edit_index;

    if (
      haNameExists(
        _ha_edit_name,
        ignore
      )
    ) {

      _task->showAlert(
        "Nome já existe",
        1500
      );

      _ha_stage =
        HA_STAGE_EDIT_NAME;

      return false;
    }

    if (
      _ha_edit_index == HA_EDIT_NEW &&
      _ha_command_count >=
        HIVEFW_HA_MAX_COMMANDS
    ) {

      _task->showAlert(
        "Lista cheia",
        1500
      );

      _ha_stage =
        HA_STAGE_MAIN;

      return false;
    }

    HiveFWHACommand
      backup[HIVEFW_HA_MAX_COMMANDS];

    memcpy(
      backup,
      _ha_commands,
      sizeof(_ha_commands)
    );

    uint8_t old_count =
      _ha_command_count;

    // ALTERAR nome/comando não deve modificar
    // a opção LOCALIZAÇÃO já existente.
    uint8_t preserved_flags = 0;

    if (
      _ha_edit_index != HA_EDIT_NEW &&
      _ha_edit_index <
        _ha_command_count
    ) {

      preserved_flags =
        _ha_commands[
          _ha_edit_index
        ].flags;
    }

    uint8_t target;

    if (_ha_edit_index == HA_EDIT_NEW) {

      target =
        _ha_command_count;

      _ha_command_count++;

    } else {

      target =
        _ha_edit_index;
    }

    memset(
      &_ha_commands[target],
      0,
      sizeof(HiveFWHACommand)
    );

    _ha_commands[target].flags =
      preserved_flags;

    strncpy(
      _ha_commands[target].name,
      _ha_edit_name,
      sizeof(_ha_commands[target].name) - 1
    );

    strncpy(
      _ha_commands[target].command,
      _ha_edit_command,
      sizeof(_ha_commands[target].command) - 1
    );

    if (
      !the_mesh.saveHACommands(
        _ha_commands,
        _ha_command_count
      )
    ) {

      memcpy(
        _ha_commands,
        backup,
        sizeof(_ha_commands)
      );

      _ha_command_count =
        old_count;

      _task->showAlert(
        "Falha ao guardar",
        1500
      );

      return false;
    }

    bool was_new =
      (_ha_edit_index == HA_EDIT_NEW);

    _task->notify(
      UIEventType::ack
    );

    _task->showAlert(
      "Comando guardado",
      1200
    );

    if (was_new) {

      _ha_stage =
        HA_STAGE_MAIN;

      _ha_menu = target;

    } else {

      _ha_stage =
        HA_STAGE_MANAGE_LIST;

      _ha_manage_menu =
        target;
    }

    _ha_edit_index =
      HA_EDIT_NEW;

    return true;
  }


  bool deleteHACommand(uint8_t index) {

    if (index >= _ha_command_count)
      return false;

    HiveFWHACommand
      backup[HIVEFW_HA_MAX_COMMANDS];

    memcpy(
      backup,
      _ha_commands,
      sizeof(_ha_commands)
    );

    uint8_t old_count =
      _ha_command_count;

    for (
      int i = index;
      i < _ha_command_count - 1;
      i++
    ) {

      _ha_commands[i] =
        _ha_commands[i + 1];
    }

    if (_ha_command_count > 0)
      _ha_command_count--;

    if (
      _ha_command_count <
      HIVEFW_HA_MAX_COMMANDS
    ) {

      memset(
        &_ha_commands[_ha_command_count],
        0,
        sizeof(HiveFWHACommand)
      );
    }

    if (
      !the_mesh.saveHACommands(
        _ha_commands,
        _ha_command_count
      )
    ) {

      memcpy(
        _ha_commands,
        backup,
        sizeof(_ha_commands)
      );

      _ha_command_count =
        old_count;

      _task->showAlert(
        "Falha ao apagar",
        1500
      );

      return false;
    }

    _task->notify(
      UIEventType::ack
    );

    _task->showAlert(
      "Comando apagado",
      1200
    );

    if (_ha_command_count == 0) {

      _ha_stage =
        HA_STAGE_MAIN;

      _ha_menu = 0;

    } else {

      _ha_stage =
        HA_STAGE_MANAGE_LIST;

      if (
        index >=
        _ha_command_count
      ) {

        _ha_manage_menu =
          _ha_command_count - 1;

      } else {

        _ha_manage_menu =
          index;
      }
    }

    return true;
  }


  bool toggleHALocation(
    uint8_t index
  ) {

    if (
      index >=
      _ha_command_count
    ) {
      return false;
    }

    uint8_t old_flags =
      _ha_commands[index].flags;

    _ha_commands[index].flags ^=
      HIVEFW_HA_FLAG_LOCATION;

    if (
      !the_mesh.saveHACommands(
        _ha_commands,
        _ha_command_count
      )
    ) {

      _ha_commands[index].flags =
        old_flags;

      _task->showAlert(
        "Falha ao guardar",
        1500
      );

      return false;
    }

    bool enabled =
      (
        _ha_commands[index].flags &
        HIVEFW_HA_FLAG_LOCATION
      ) != 0;

    _task->notify(
      UIEventType::ack
    );

    _task->showAlert(
      enabled
        ? "Localização ON"
        : "Localização OFF",
      1200
    );

    return true;
  }


  bool sendHACommand(uint8_t index) {

    if (
      index >=
      _ha_command_count
    ) {
      return false;
    }

    ChannelDetails channel;

    if (!getAppsChannel(channel)) {

      _task->showAlert(
        "Canal APPS não definido",
        1500
      );

      return false;
    }

    char command[96];

    bool with_location =
      (
        _ha_commands[index].flags &
        HIVEFW_HA_FLAG_LOCATION
      ) != 0;

    if (with_location) {

#if ENV_INCLUDE_GPS == 1

      LocationProvider* location =
        _sensors->getLocationProvider();

      if (
        location != nullptr &&
        location->isValid()
      ) {

        snprintf(
          command,
          sizeof(command),
          "!%s %.4f %.4f",
          _ha_commands[index].command,
          location->getLatitude() /
            1000000.0,
          location->getLongitude() /
            1000000.0
        );

      } else {

        snprintf(
          command,
          sizeof(command),
          "!%s SEM GPS",
          _ha_commands[index].command
        );
      }

#else

      snprintf(
        command,
        sizeof(command),
        "!%s SEM GPS",
        _ha_commands[index].command
      );

#endif

    } else {

      snprintf(
        command,
        sizeof(command),
        "!%s",
        _ha_commands[index].command
      );
    }

    bool success =
      the_mesh.sendGroupMessage(
        _rtc->getCurrentTime(),
        channel.channel,
        _node_prefs->node_name,
        command,
        strlen(command)
      );

    _task->notify(
      UIEventType::ack
    );

    _task->showAlert(
      success
        ? "Comando Enviado"
        : "Falha ao enviar",
      1200
    );

    return success;
  }


  void exitHomeAssistant() {

    _page =
      HomePage::APPS;

    _apps_submenu = false;
    _apps_menu = 0;
    _apps_view = APPS_VIEW_NONE;
    _apps_return = false;

    _ha_submenu = false;
    _ha_stage = HA_STAGE_MAIN;

    _ha_menu = 0;
    _ha_manage_menu = 0;
    _ha_action_menu = 0;
    _ha_delete_confirm = 0;
    _ha_edit_index = HA_EDIT_NEW;
    _ha_char_index = 3;
  }


  // Discovery ativo — INDEPENDENTE de AdvertPath.
  static const uint8_t ACTIVE_DISCOVERY_MAX_NODES = 8;
  NodeDiscoveryResult _active_discovery_nodes[ACTIVE_DISCOVERY_MAX_NODES];
  uint8_t _active_discovery_count;
  uint8_t _active_discovery_menu;

  // ========================================================
  // CANAL APPS
  // ========================================================

  int getAppsChannelCount() {
    int count = 0;

#ifdef MAX_GROUP_CHANNELS
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      ChannelDetails channel;

      if (the_mesh.getChannel(i, channel) &&
          channel.name[0] != '\0') {
        count++;
      }
    }
#endif

    return count;
  }

  bool getAppsChannel(ChannelDetails& selected) {
    bool configured = false;

    for (int i = 0; i < PATH_HASH_SIZE; i++) {
      if (_node_prefs->apps_channel_hash[i] != 0) {
        configured = true;
        break;
      }
    }

    if (!configured)
      return false;

#ifdef MAX_GROUP_CHANNELS
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      ChannelDetails channel;

      if (the_mesh.getChannel(i, channel) &&
          channel.name[0] != '\0' &&
          memcmp(
            channel.channel.hash,
            _node_prefs->apps_channel_hash,
            PATH_HASH_SIZE
          ) == 0) {

        selected = channel;
        return true;
      }
    }
#endif

    return false;
  }

  int getAppsChannelMenuIndex() {
    ChannelDetails selected;

    if (!getAppsChannel(selected))
      return getAppsChannelCount();

    int found = 0;

#ifdef MAX_GROUP_CHANNELS
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      ChannelDetails channel;

      if (the_mesh.getChannel(i, channel) &&
          channel.name[0] != '\0') {

        if (memcmp(
              channel.channel.hash,
              selected.channel.hash,
              PATH_HASH_SIZE
            ) == 0) {
          return found;
        }

        found++;
      }
    }
#endif

    return getAppsChannelCount();
  }

  bool selectAppsChannelByMenuIndex(int menu_index) {
    int found = 0;

#ifdef MAX_GROUP_CHANNELS
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      ChannelDetails channel;

      if (the_mesh.getChannel(i, channel) &&
          channel.name[0] != '\0') {

        if (found == menu_index) {

          memcpy(
            _node_prefs->apps_channel_hash,
            channel.channel.hash,
            PATH_HASH_SIZE
          );

          the_mesh.savePrefs();

          return true;
        }

        found++;
      }
    }
#endif

    return false;
  }

  void refreshActiveDiscoveryNodes() {

    _active_discovery_count =
      the_mesh.getNodeDiscoveryResults(
        _active_discovery_nodes,
        ACTIVE_DISCOVERY_MAX_NODES
      );

    if (_active_discovery_count == 0) {
      _active_discovery_menu = 0;
    } else if (_active_discovery_menu >= _active_discovery_count) {
      _active_discovery_menu = 0;
    }
  }

  // Nós descobertos = tabela AdvertPath (PASSIVO / ADVERT).
  static const uint8_t DISCOVER_MAX_NODES = 8;
  AdvertPath _discover_nodes[DISCOVER_MAX_NODES];
  uint8_t _discover_count;
  uint8_t _discover_menu;

  void refreshDiscoveredNodes() {

    AdvertPath temp[DISCOVER_MAX_NODES];

    int total =
      the_mesh.getRecentlyHeard(
        temp,
        DISCOVER_MAX_NODES
      );

    _discover_count = 0;

    for (int i = 0;
         i < total &&
         _discover_count < DISCOVER_MAX_NODES;
         i++) {

      if (temp[i].recv_timestamp == 0)
        continue;

      _discover_nodes[_discover_count] =
        temp[i];

      _discover_count++;
    }

    if (_discover_count == 0) {

      _discover_menu = 0;

    } else if (
      _discover_menu >= _discover_count
    ) {

      _discover_menu = 0;
    }
  }

  bool _shutdown_init;
  AdvertPath recent[UI_RECENT_LIST_SIZE];


  static int batteryPercentageFromMilliVolts(
    uint16_t batteryMilliVolts
  ) {
#ifndef BATT_MIN_MILLIVOLTS
#define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
#define BATT_MAX_MILLIVOLTS 4200
#endif

    const int minMilliVolts = BATT_MIN_MILLIVOLTS;
    const int maxMilliVolts = BATT_MAX_MILLIVOLTS;

    int percentage =
      (((int)batteryMilliVolts - minMilliVolts) * 100) /
      (maxMilliVolts - minMilliVolts);

    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;

    return percentage;
  }

  static void formatUptime(
    char* dest,
    size_t dest_len
  ) {
    uint32_t seconds = millis() / 1000;
    uint32_t days = seconds / 86400;
    seconds %= 86400;

    uint32_t hours = seconds / 3600;
    seconds %= 3600;

    uint32_t minutes = seconds / 60;
    seconds %= 60;

    if (days > 0) {
      snprintf(dest, dest_len, "%lud %luh",
        (unsigned long)days,
        (unsigned long)hours);
    } else if (hours > 0) {
      snprintf(dest, dest_len, "%luh %lum",
        (unsigned long)hours,
        (unsigned long)minutes);
    } else {
      snprintf(dest, dest_len, "%lum %lus",
        (unsigned long)minutes,
        (unsigned long)seconds);
    }
  }

  static uint8_t dutyCyclePercentFromAirtimeFactor(
    float airtime_factor
  ) {

    // No HiveFW Companion mantemos a gama oficial
    // suportada pelo firmware atual: AF 1..9.
    if (airtime_factor < 1.0f)
      airtime_factor = 1.0f;

    if (airtime_factor > 9.0f)
      airtime_factor = 9.0f;

    int duty =
      (int)(
        100.0f /
        (1.0f + airtime_factor)
        + 0.5f
      );

    if (duty < 10)
      duty = 10;

    if (duty > 50)
      duty = 50;

    return (uint8_t)duty;
  }


  static float airtimeFactorFromDutyCyclePercent(
    uint8_t duty
  ) {

    if (duty < 10)
      duty = 10;

    if (duty > 50)
      duty = 50;

    return
      (100.0f / (float)duty)
      - 1.0f;
  }


  void getCompanionPeerName(
    char* dest,
    size_t dest_len
  ) {

    if (
      dest == NULL ||
      dest_len == 0
    ) {
      return;
    }

    dest[0] = '\0';

    if (!_task->hasConnection()) {

      snprintf(
        dest,
        dest_len,
        "Desligado"
      );

      return;
    }

#if defined(NRF52) || defined(NRF52_SERIES)

    char peer_name[32] = { 0 };

    uint16_t conn_handle =
      Bluefruit.connHandle();

    BLEConnection* connection =
      Bluefruit.Connection(
        conn_handle
      );

    if (
      connection != nullptr &&
      connection->connected()
    ) {

      uint16_t peer_len =
        connection->getPeerName(
          peer_name,
          sizeof(peer_name)
        );

      peer_name[
        sizeof(peer_name) - 1
      ] = '\0';

      if (
        peer_len > 0 &&
        peer_name[0] != '\0'
      ) {

        snprintf(
          dest,
          dest_len,
          "%s",
          peer_name
        );

      } else {

        // Ligação BLE válida, mas o dispositivo
        // remoto não forneceu um nome utilizável.
        snprintf(
          dest,
          dest_len,
          "Ligado"
        );
      }

    } else {

      snprintf(
        dest,
        dest_len,
        "Ligado"
      );
    }

#else

    // A build V3 Companion Wi-Fi não deve
    // apresentar uma ligação Wi-Fi como se
    // fosse um smartphone Bluetooth.
    snprintf(
      dest,
      dest_len,
      "Desligado"
    );

#endif
  }


  void renderCompanionInfo(DisplayDriver& display) {
    char counter[8];
    char title[24];
    char value[48];

    title[0] = '\0';
    value[0] = '\0';

    snprintf(
      counter,
      sizeof(counter),
      "%d/%d",
      (int)(_companion_info_page + 1),
      (int)COMP_INFO_COUNT
    );

    display.setTextSize(1);
    display.setColor(UIColor::secondary_txt);

    display.drawTextRightAlign(
      display.width() - 1,
      8,
      counter
    );

    switch (_companion_info_page) {

      case COMP_INFO_BLE:
        snprintf(title, sizeof(title), "BLE");
        snprintf(
          value,
          sizeof(value),
          "%s",
          _task->hasConnection()
            ? "LIGADO"
            : "DESLIGADO"
        );
        break;

      case COMP_INFO_BATTERY: {
        uint16_t mv = _task->getBattMilliVolts();

        snprintf(title, sizeof(title), "BATERIA");

        if (mv > 0) {
          snprintf(
            value,
            sizeof(value),
            "%d%%",
            batteryPercentageFromMilliVolts(mv)
          );
        } else {
          snprintf(value, sizeof(value), "N/D");
        }

        break;
      }

      case COMP_INFO_VOLTAGE: {
        uint16_t mv = _task->getBattMilliVolts();

        snprintf(title, sizeof(title), "VOLTAGEM");

        if (mv > 0) {
          snprintf(
            value,
            sizeof(value),
            "%.2f V",
            mv / 1000.0f
          );
        } else {
          snprintf(value, sizeof(value), "N/D");
        }

        break;
      }

      case COMP_INFO_UPTIME:
        snprintf(title, sizeof(title), "UPTIME");
        formatUptime(value, sizeof(value));
        break;

      case COMP_INFO_ADVERT_TX:
        snprintf(title, sizeof(title), "ADVERT TX");
        snprintf(
          value,
          sizeof(value),
          "%lu",
          (unsigned long)the_mesh.getCompanionAdvertTX()
        );
        break;

      case COMP_INFO_ADVERT_RX:
        snprintf(title, sizeof(title), "ADVERT RX");
        snprintf(
          value,
          sizeof(value),
          "%lu",
          (unsigned long)the_mesh.getCompanionAdvertRX()
        );
        break;

      case COMP_INFO_BLE_NAME:
        snprintf(title, sizeof(title), "NOME BLE");
        snprintf(
          value,
          sizeof(value),
          "%s%s",
          BLE_NAME_PREFIX,
          the_mesh.getNodePrefs()->node_name
        );
        break;

      case COMP_INFO_SMARTPHONE:
        snprintf(title, sizeof(title), "SMARTPHONE");
        getCompanionPeerName(value, sizeof(value));
        break;

      case COMP_INFO_NODE_NAME:
        snprintf(title, sizeof(title), "NOME DO NO");
        snprintf(
          value,
          sizeof(value),
          "%s",
          the_mesh.getNodePrefs()->node_name
        );
        break;

      case COMP_INFO_MODE:
        snprintf(title, sizeof(title), "MODO");
        snprintf(
          value,
          sizeof(value),
          "%s",
          the_mesh.getNodePrefs()->isRepeatEn()
            ? "REPETIDOR"
            : "COMPANION"
        );
        break;

      case COMP_INFO_FIRMWARE:
        snprintf(title, sizeof(title), "FIRMWARE");
        snprintf(value, sizeof(value), "HiveFW");
        break;

      case COMP_INFO_VERSION:
      default:
        snprintf(title, sizeof(title), "VERSAO");
        snprintf(
          value,
          sizeof(value),
          "%s",
          FIRMWARE_VERSION
        );
        break;
    }

    display.setColor(UIColor::primary_txt);

    display.drawTextCentered(
      display.width() / 2,
      24,
      title
    );

    display.drawTextCentered(
      display.width() / 2,
      44,
      value
    );
  }

  void renderBatteryIndicator(
    DisplayDriver& display,
    uint16_t batteryMilliVolts
  ) {

    int batteryPercentage =
      batteryPercentageFromMilliVolts(
        batteryMilliVolts
      );

    char batteryText[8];

    if (batteryMilliVolts > 0) {

      snprintf(
        batteryText,
        sizeof(batteryText),
        "%d%%",
        batteryPercentage
      );

    } else {

      snprintf(
        batteryText,
        sizeof(batteryText),
        "--%%"
      );
    }

    display.setTextSize(1);
    display.setColor(
      UIColor::title_txt
    );

    int text_width =
      display.getTextWidth(
        batteryText
      );

    // Bateria 11x6 + terminal.
    const int icon_width = 11;
    const int gap = 2;

    int icon_x =
      display.width() -
      1 -
      text_width -
      gap -
      icon_width;

    const int icon_y = 3;

    display.drawRect(
      icon_x,
      icon_y,
      9,
      6
    );

    display.fillRect(
      icon_x + 9,
      icon_y + 2,
      2,
      2
    );

    // Área útil interna = 5 pixels.
    int fill =
      (
        batteryPercentage *
        5 +
        99
      ) / 100;

    if (fill < 0)
      fill = 0;

    if (fill > 5)
      fill = 5;

    if (fill > 0) {

      display.fillRect(
        icon_x + 2,
        icon_y + 2,
        fill,
        2
      );
    }

    display.drawTextRightAlign(
      display.width() - 1,
      1,
      batteryText
    );
  }


  // ========================================================
  // HIVEFW — HORA LOCAL LISBOA
  //
  // RTC interno permanece em UTC.
  // O header mostra a hora local portuguesa,
  // incluindo mudança automática de horário.
  // ========================================================

  bool isLisbonSummerTime(
    uint32_t timestamp
  ) const {

    DateTime utc(timestamp);

    int month = utc.month();

    if (
      month < 3 ||
      month > 10
    ) {
      return false;
    }

    if (
      month > 3 &&
      month < 10
    ) {
      return true;
    }

    int days_in_month = 31;

    if (
      month == 4 ||
      month == 6 ||
      month == 9 ||
      month == 11
    ) {

      days_in_month = 30;

    } else if (month == 2) {

      int year =
        utc.year();

      bool leap =
        (
          (
            year % 4 == 0 &&
            year % 100 != 0
          ) ||
          year % 400 == 0
        );

      days_in_month =
        leap ? 29 : 28;
    }

    int y =
      utc.year();

    int m =
      month;

    if (m < 3) {
      y--;
      m += 12;
    }

    // Zeller:
    // 0 = sábado
    // 1 = domingo
    int weekday_last_day =
      (
        days_in_month +
        (13 * (m + 1)) / 5 +
        y +
        y / 4 -
        y / 100 +
        y / 400
      ) % 7;

    int sunday_offset =
      (
        weekday_last_day +
        6
      ) % 7;

    int last_sunday =
      days_in_month -
      sunday_offset;

    if (month == 3) {

      return
        utc.day() >
          last_sunday ||
        (
          utc.day() ==
            last_sunday &&
          utc.hour() >= 1
        );
    }

    // Outubro:
    // horário de verão termina às
    // 01:00 UTC do último domingo.
    return
      utc.day() <
        last_sunday ||
      (
        utc.day() ==
          last_sunday &&
        utc.hour() < 1
      );
  }


  void formatHiveHeaderTime(
    char* dest,
    size_t dest_len
  ) const {

    if (
      dest == NULL ||
      dest_len == 0
    ) {
      return;
    }

    uint32_t now =
      _rtc->getCurrentTime();

    // RTC claramente não inicializado.
    if (
      now <
      946684800UL
    ) {

      snprintf(
        dest,
        dest_len,
        "--:--"
      );

      return;
    }

    uint32_t local_now =
      now +
      (
        isLisbonSummerTime(now)
          ? 3600UL
          : 0UL
      );

    DateTime local_time(
      local_now
    );

    snprintf(
      dest,
      dest_len,
      "%02d:%02d",
      local_time.hour(),
      local_time.minute()
    );
  }


  uint8_t getHeaderTopLevelPage()
    const {

    switch (_page) {

      case HomePage::INTERNAL_HOME_ASSISTANT:
      case HomePage::INTERNAL_CLOCK:
      case HomePage::INTERNAL_GPS:
      case HomePage::INTERNAL_SENSORS:

        return
          HomePage::APPS;

      default:
        break;
    }

    if (
      _page <
      HomePage::Count
    ) {

      return _page;
    }

    return
      HomePage::FIRST;
  }


  void renderHiveHeader(
    DisplayDriver& display
  ) {

#ifdef HELTEC_T114_WITH_DISPLAY

    uint8_t header_color =
      _node_prefs->header_color;

    if (
      header_color >=
      HIVEFW_HEADER_COLOR_COUNT
    ) {
      header_color = 0;
    }

    // Durante o seletor mostramos a cor imediatamente,
    // sem a gravar até ENTER.
    if (
      _page == HomePage::SETTINGS &&
      _settings_submenu &&
      _settings_color_submenu
    ) {

      header_color =
        _settings_color_menu;
    }

    display.setHeaderAccent(
      header_color
    );

#endif

    // Barra superior.
    display.setColor(
      UIColor::title_bkg
    );

    display.fillRect(
      0,
      0,
      display.width(),
      11
    );

    // ------------------------------------------------------
    // HIVEFW — SEPARADOR INFERIOR DA BARRA
    //
    // Uma única unidade lógica de altura.
    //
    // A barra colorida ocupa y=0..10 e este separador ocupa
    // y=11. A altura total do header continua portanto a ser
    // exatamente as 12 unidades originais.
    //
    // Fica imediatamente abaixo da barra colorida e mantém
    // sempre a cor branca/primária, independentemente da
    // cor selecionada em DEFINIÇÕES -> COR DA BARRA.
    //
    // Se visualmente ficar demasiado fino ou grosso,
    // ajustamos apenas este valor mais tarde.
    // ------------------------------------------------------

    display.setColor(
      UIColor::primary_txt
    );

    display.fillRect(
      0,
      11,
      display.width(),
      1
    );

    display.setTextSize(1);
    display.setColor(
      UIColor::title_txt
    );

    // ------------------------------------------------------
    // ESQUERDA
    //
    // FIRST:
    //   HiveFW
    //
    // restantes páginas:
    //   01/08 ... 08/08
    // ------------------------------------------------------

    uint8_t page =
      getHeaderTopLevelPage();

    char page_text[12];

    if (
      page ==
      HomePage::FIRST
    ) {

      snprintf(
        page_text,
        sizeof(page_text),
        "HiveFW"
      );

    } else {

      snprintf(
        page_text,
        sizeof(page_text),
        "%02u/%02u",
        (unsigned)page,
        (unsigned)(
          HomePage::Count - 1
        )
      );
    }

    display.setCursor(
      1,
      2
    );

    display.print(
      page_text
    );

    // ------------------------------------------------------
    // CENTRO — HORA
    // ------------------------------------------------------

    char time_text[8];

    formatHiveHeaderTime(
      time_text,
      sizeof(time_text)
    );

    display.drawTextCentered(
      display.width() / 2,
      2,
      time_text
    );

    // ------------------------------------------------------
    // DIREITA — BATERIA
    // ------------------------------------------------------

    renderBatteryIndicator(
      display,
      _task->getBattMilliVolts()
    );
  }


  void drawCenteredClippedText(
    DisplayDriver& display,
    int y,
    const char* text
  ) {

    if (text == NULL)
      return;

    char filtered[64];

    display.translateUTF8ToBlocks(
      filtered,
      text,
      sizeof(filtered)
    );

    display.setTextSize(1);

    size_t len =
      strlen(filtered);

    while (
      len > 0 &&
      display.getTextWidth(
        filtered
      ) >
      display.width() - 4
    ) {

      filtered[
        --len
      ] = '\0';
    }

    display.drawTextCentered(
      display.width() / 2,
      y,
      filtered
    );
  }


  // ========================================================
  // HIVEFW — NOME DO NÓ CENTRADO COM ÍCONE RADIOATIVO
  //
  // O Unicode U+2622 é substituído por um bitmap próprio.
  //
  // A largura usada para centrar é:
  //
  //   prefixo + espaço + ícone + espaço + sufixo
  //
  // Assim a posição deixa de depender da representação
  // interna dos bytes UTF-8.
  // ========================================================

  // ========================================================
  // HIVEFW — NOME DO NÓ CENTRADO
  //
  // O símbolo U+2622 (☢) e os variation selectors associados
  // são omitidos apenas no dashboard.
  //
  // O node_name original não é alterado.
  // ========================================================

  void drawCenteredNodeName(
    DisplayDriver& display,
    const char* text
  ) {

    if (
      text == NULL ||
      text[0] == '\0'
    ) {
      return;
    }

    char filtered[64];

    size_t out = 0;
    bool last_space = false;

    for (
      size_t i = 0;
      text[i] != '\0';
    ) {

      uint8_t c =
        (uint8_t)text[i];


      // ----------------------------------------------------
      // U+2622 ☢
      //
      // UTF-8:
      //   E2 98 A2
      //
      // Não mostrar no dashboard.
      // ----------------------------------------------------

      if (
        c == 0xE2 &&
        text[i + 1] != '\0' &&
        text[i + 2] != '\0' &&
        (uint8_t)text[i + 1] == 0x98 &&
        (uint8_t)text[i + 2] == 0xA2
      ) {

        // Manter separação visual entre o texto dos dois lados.
        if (
          out > 0 &&
          filtered[out - 1] != ' ' &&
          out < sizeof(filtered) - 1
        ) {

          filtered[out++] = ' ';
          last_space = true;
        }

        i += 3;


        // U+FE0E / U+FE0F imediatamente depois de ☢.
        if (
          text[i] != '\0' &&
          text[i + 1] != '\0' &&
          text[i + 2] != '\0' &&
          (uint8_t)text[i] == 0xEF &&
          (uint8_t)text[i + 1] == 0xB8 &&
          (
            (uint8_t)text[i + 2] == 0x8E ||
            (uint8_t)text[i + 2] == 0x8F
          )
        ) {

          i += 3;
        }

        continue;
      }


      // ----------------------------------------------------
      // Variation selector isolado.
      // ----------------------------------------------------

      if (
        c == 0xEF &&
        text[i + 1] != '\0' &&
        text[i + 2] != '\0' &&
        (uint8_t)text[i + 1] == 0xB8 &&
        (
          (uint8_t)text[i + 2] == 0x8E ||
          (uint8_t)text[i + 2] == 0x8F
        )
      ) {

        i += 3;
        continue;
      }


      // ----------------------------------------------------
      // ASCII.
      // ----------------------------------------------------

      if (
        c >= 32 &&
        c <= 126
      ) {

        char ch =
          (char)c;

        if (ch == ' ') {

          // Colapsar espaços duplicados que possam surgir
          // depois da remoção do símbolo.
          if (
            out == 0 ||
            last_space
          ) {

            i++;
            continue;
          }

          last_space = true;

        } else {

          last_space = false;
        }


        if (
          out <
          sizeof(filtered) - 1
        ) {

          filtered[out++] =
            ch;
        }

        i++;
        continue;
      }


      // ----------------------------------------------------
      // Outros Unicode:
      //
      // um codepoint continua a ocupar apenas um bloco.
      // ----------------------------------------------------

      if (c >= 0x80) {

        if (
          out <
          sizeof(filtered) - 1
        ) {

          filtered[out++] =
            (char)0xDB;
        }

        last_space = false;

        i++;

        while (
          text[i] != '\0' &&
          (
            (uint8_t)text[i] &
            0xC0
          ) == 0x80
        ) {

          i++;
        }

        continue;
      }

      i++;
    }


    // Retirar espaços no final.
    while (
      out > 0 &&
      filtered[out - 1] == ' '
    ) {

      out--;
    }

    filtered[out] =
      '\0';


    // ------------------------------------------------------
    // Tamanho e posição originais.
    // ------------------------------------------------------

    display.setTextSize(1);


    // Cortar apenas se necessário.
    size_t len =
      strlen(filtered);

    while (
      len > 0 &&
      display.getTextWidth(
        filtered
      ) >
      display.width() - 4
    ) {

      filtered[
        --len
      ] = '\0';
    }


    // Centrar a largura EXATA do que vai aparecer.
    display.drawTextCentered(
      display.width() / 2,
      52,
      filtered
    );
  }


  void drawDashboardStatusItem(
    DisplayDriver& display,
    int center_x,
    int y,
    const uint8_t* icon,
    const char* label
  ) {

    display.setTextSize(1);
    display.setColor(
      UIColor::primary_txt
    );

    const int icon_width = 8;
    const int gap = 2;

    int label_width =
      display.getTextWidth(
        label
      );

    int total_width =
      icon_width +
      gap +
      label_width;

    int x =
      center_x -
      total_width / 2;

    display.drawXbm(
      x,
      y,
      icon,
      8,
      8
    );

    display.setCursor(
      x +
      icon_width +
      gap,
      y
    );

    display.print(
      label
    );
  }


  void renderFirstDashboard(
    DisplayDriver& display
  ) {

    display.setColor(
      UIColor::primary_txt
    );

    display.setTextSize(1);

    // ------------------------------------------------------
    // LINHA 1:
    //
    // mensagem   BT ON/OFF   GPS ON/OFF
    // ------------------------------------------------------

    int msg_count =
      _task->getMsgCount();

    char msg_text[8];

    if (msg_count > 99) {

      snprintf(
        msg_text,
        sizeof(msg_text),
        "99+"
      );

    } else {

      snprintf(
        msg_text,
        sizeof(msg_text),
        "%d",
        msg_count
      );
    }

    const int status_y = 19;

    drawDashboardStatusItem(
      display,
      display.width() / 6,
      status_y,
      hivefw_status_message_icon,
      msg_text
    );

    // BLE:
    // ON/OFF representa o estado da função Bluetooth.
    // A linha seguinte continua a indicar o peer:
    // nome do smartphone ou "Desligado".
    bool ble_enabled =
      _task->isBluetoothEnabled();

    drawDashboardStatusItem(
      display,
      display.width() / 2,
      status_y,
      hivefw_status_ble_icon,
      ble_enabled
        ? "ON"
        : "OFF"
    );

    // GPS:
    // ON/OFF representa GPS ligado/desligado.
    // FIX / NO FIX continua reservado ao ecrã GPS.
#if ENV_INCLUDE_GPS == 1

    bool gps_enabled =
      _task->getGPSState();

#else

    bool gps_enabled = false;

#endif

    drawDashboardStatusItem(
      display,
      (
        display.width() *
        5
      ) / 6,
      status_y,
      hivefw_status_gps_icon,
      gps_enabled
        ? "ON"
        : "OFF"
    );


    // ------------------------------------------------------
    // HIVEFW — SEPARADOR DO DASHBOARD
    //
    // Ícones:
    //   y = 19..26
    //
    // Smartphone / Desligado:
    //   y = 36
    //
    // O separador em y=31 fica visualmente centrado
    // entre as duas zonas.
    //
    // Mantém a mesma espessura lógica do separador
    // existente imediatamente abaixo do header.
    // ------------------------------------------------------

    display.setColor(
      UIColor::primary_txt
    );

    display.fillRect(
      0,
      31,
      display.width(),
      1
    );


    // ------------------------------------------------------
    // LINHA 2:
    //
    // nome real do smartphone BLE
    // ou "Desligado"
    // ------------------------------------------------------

    char peer_name[48];

    getCompanionPeerName(
      peer_name,
      sizeof(peer_name)
    );

    display.setColor(
      UIColor::primary_txt
    );

    drawCenteredClippedText(
      display,
      36,
      peer_name
    );

    // ------------------------------------------------------
    // LINHA 3:
    //
    // nickname / nome do nó
    // ------------------------------------------------------

    display.setColor(
      UIColor::secondary_txt
    );

    drawCenteredNodeName(
      display,
      _node_prefs->node_name
    );
  }


  CayenneLPP sensors_lpp;
  int sensors_nb = 0;
  bool sensors_scroll = false;
  int sensors_scroll_offset = 0;
  int next_sensors_refresh = 0;

  void refresh_sensors() {
    if (millis() > next_sensors_refresh) {
      sensors_lpp.reset();
      sensors_nb = 0;
      sensors_lpp.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      sensors.querySensors(0xFF, sensors_lpp);
      LPPReader reader (sensors_lpp.getBuffer(), sensors_lpp.getSize());
      uint8_t channel, type;
      while(reader.readHeader(channel, type)) {
        reader.skipData(type);
        sensors_nb ++;
      }
      sensors_scroll = sensors_nb > UI_RECENT_LIST_SIZE;
#if AUTO_OFF_MILLIS > 0
      next_sensors_refresh = millis() + 5000; // refresh sensor values every 5 sec
#else
      next_sensors_refresh = millis() + 60000; // refresh sensor values every 1 min
#endif
    }
  }

public:
  HomeScreen(UITask* task, mesh::RTCClock* rtc, SensorManager* sensors, NodePrefs* node_prefs)
     : _task(task), _rtc(rtc), _sensors(sensors), _node_prefs(node_prefs), _page(0),
       _companion_menu(0), _companion_submenu(false),
       _companion_info_submenu(false),
       _companion_info_page(0),
       _sms_menu(0), _sms_submenu(false),
       _sms_messages_menu(0), _sms_messages_submenu(false),
       _sms_new_menu(0), _sms_new_submenu(false),
       _sms_new_stage(0),
       _sms_contact_menu(0),
       _sms_preset_menu(0),
       _sms_char_index(0),
       _sms_action_menu(0),
       _sms_confirm(0),
       _sms_flow_type(0),
       _sms_recipient_valid(false),
       _sms_target_type(0),
       _sms_target_menu(0),
       _sms_channel_menu(0),
       _sms_channel_valid(false),
       _settings_menu(0), _settings_submenu(false),
       _settings_channel_submenu(false),
       _settings_channel_menu(0),
       _settings_color_submenu(false),
       _settings_color_menu(0),
       _repeater_submenu(false),
       _repeater_info_submenu(false),
       _repeater_neighbours_submenu(false),
       _repeater_duty_submenu(false),
       _repeater_menu(0),
       _repeater_neighbour_menu(0),
       _repeater_duty_value(50),
       _radio_submenu(false),
       _radio_menu(0),
       _radio_freq_edit(false),
       _radio_freq_digit(0),
       _radio_freq_digits{0, 0, 0, 0, 0, 0},
       _radio_tx_edit(false),
       _radio_tx_digit(0),
       _radio_tx_digits{0, 0},
       _radio_value_edit(false),
       _radio_value_index(0),
      _settings_advert_menu(0), _settings_advert_submenu(false),
      _settings_confirm(false), _settings_confirm_menu(0),
      _ha_menu(0), _ha_submenu(false),
      _ha_stage(HA_STAGE_MAIN),
      _ha_manage_menu(0),
      _ha_action_menu(0),
      _ha_delete_confirm(0),
      _ha_edit_index(HA_EDIT_NEW),
      _ha_char_index(3),
      _ha_command_count(0),
       _apps_menu(0), _apps_view(0),
       _sos_submenu(false), _sos_menu(0),
       _sos_confirm_submenu(false),
       _repeater_stats_page(0),
       _apps_submenu(false), _apps_return(false),
       _active_discovery_count(0), _active_discovery_menu(0),
       _discover_count(0), _discover_menu(0),
       _shutdown_init(false), sensors_lpp(200) {
    _sms_text[0] = '\0';
    memset(&_sms_recipient, 0, sizeof(_sms_recipient));

    memset(
      _ha_commands,
      0,
      sizeof(_ha_commands)
    );

    _ha_edit_name[0] = '\0';
    _ha_edit_command[0] = '\0';

    for (int i = 0; i < MAX_CHANNEL_UNREAD; i++) {
      _channel_unread[i].count = 0;
      _channel_unread[i].valid = false;
      memset(_channel_unread[i].hash, 0, PATH_HASH_SIZE);
    }
  }

  void addUnreadChannelMessage(const uint8_t* hash) {
    if (hash == NULL) return;

    int free_slot = -1;

    for (int i = 0; i < MAX_CHANNEL_UNREAD; i++) {
      if (_channel_unread[i].valid &&
          memcmp(_channel_unread[i].hash, hash, PATH_HASH_SIZE) == 0) {
        if (_channel_unread[i].count < 255)
          _channel_unread[i].count++;
        return;
      }

      if (!_channel_unread[i].valid && free_slot < 0)
        free_slot = i;
    }

    if (free_slot >= 0) {
      memcpy(_channel_unread[free_slot].hash, hash, PATH_HASH_SIZE);
      _channel_unread[free_slot].count = 1;
      _channel_unread[free_slot].valid = true;
    }
  }

  int getUnreadChannelCount(const uint8_t* hash) {
    if (hash == NULL) return 0;

    for (int i = 0; i < MAX_CHANNEL_UNREAD; i++) {
      if (_channel_unread[i].valid &&
          memcmp(_channel_unread[i].hash, hash, PATH_HASH_SIZE) == 0)
        return _channel_unread[i].count;
    }

    return 0;
  }

  void clearUnreadChannel(const uint8_t* hash) {
    if (hash == NULL) return;

    for (int i = 0; i < MAX_CHANNEL_UNREAD; i++) {
      if (_channel_unread[i].valid &&
          memcmp(_channel_unread[i].hash, hash, PATH_HASH_SIZE) == 0) {
        _channel_unread[i].count = 0;
        _channel_unread[i].valid = false;
        return;
      }
    }
  }

  void resetNewMessageState() {
    _sms_new_stage = 0;
    _sms_contact_menu = 0;
    _sms_preset_menu = 0;
    _sms_char_index = 0;
    _sms_action_menu = 0;
    _sms_confirm = 0;
    _sms_flow_type = 0;
    _sms_target_type = 0;
    _sms_target_menu = 0;
    _sms_channel_menu = 0;
    _sms_text[0] = '\0';
    _sms_recipient_valid = false;
    _sms_channel_valid = false;
    memset(&_sms_recipient, 0, sizeof(_sms_recipient));
    memset(&_sms_channel, 0, sizeof(_sms_channel));
  }

  bool sendComposedMessage() {

    // --------------------------------------------------------
    // LOCALIZAÇÃO
    // --------------------------------------------------------

    // Obter a posição novamente no momento do envio.
    if (_sms_flow_type == 2) {

      if (!prepareCurrentLocationMessage()) {
        return false;
      }
    }

    if (_sms_text[0] == '\0') {
      _task->showAlert("Mensagem vazia", 1500);
      return false;
    }

    // --------------------------------------------------------
    // CONTACTO
    // --------------------------------------------------------

    if (_sms_target_type == 0) {

      if (!_sms_recipient_valid) {
        _task->showAlert("Contacto inválido", 1500);
        return false;
      }

      uint32_t expected_ack = 0;
      uint32_t est_timeout = 0;

      int result = the_mesh.sendMessage(
        _sms_recipient,
        _rtc->getCurrentTimeUnique(),
        0,
        _sms_text,
        expected_ack,
        est_timeout
      );

      if (result == MSG_SEND_FAILED) {
        _task->showAlert("Falha ao enviar", 1800);
        return false;
      }

      _task->showAlert("Mensagem enviada", 1200);
      return true;
    }

    // --------------------------------------------------------
    // CANAL
    // --------------------------------------------------------

    if (_sms_target_type == 1) {

      if (!_sms_channel_valid) {
        _task->showAlert("Canal inválido", 1500);
        return false;
      }

      bool success = the_mesh.sendGroupMessage(
        _rtc->getCurrentTime(),
        _sms_channel.channel,
        _node_prefs->node_name,
        _sms_text,
        strlen(_sms_text)
      );

      if (!success) {
        _task->showAlert("Falha ao enviar", 1800);
        return false;
      }

      _task->showAlert("Mensagem enviada", 1200);
      return true;
    }

    return false;
  }

  bool prepareCurrentLocationMessage() {

#if ENV_INCLUDE_GPS == 1

    LocationProvider* location = _sensors->getLocationProvider();

    if (location == NULL) {
      _task->showAlert("GPS indisponível", 1800);
      return false;
    }

    if (!location->isValid()) {
      _task->showAlert("Sem fix GPS", 1800);
      return false;
    }

    snprintf(
      _sms_text,
      sizeof(_sms_text),
      "Localização: %.6f, %.6f",
      _sensors->node_lat,
      _sensors->node_lon
    );

    return true;

#else

    _task->showAlert("GPS não compilado", 1800);
    return false;

#endif
  }

  void poll() override {
    if (_shutdown_init && !_task->isButtonPressed()) {  // must wait for USR button to be released
      _task->shutdown();
    }
  }

  // ----------------------------------------------------------------
  // RENDERERS COMUNS DE MENU
  //
  // Todos os menus devem passar por estes helpers sempre que o
  // comportamento visual seja o mesmo.
  // ----------------------------------------------------------------

  inline void drawSelectedMenuText(
    DisplayDriver& display,
    int centerX,
    int y,
    const char* text
  ) {
    display.drawTextCentered(centerX, y, text);
  }

  inline void drawMenuListItem(
    DisplayDriver& display,
    const char* text,
    int y = 40
  ) {
    drawMenuSelection(display, text, y);
  }

  inline void drawMenuSelection(
    DisplayDriver& display,
    const char* text,
    int y = 38,
    int text_offset_x = 8
  ) {
    const int centerX = display.width() / 2;

    display.drawTextCentered(
      centerX - 42,
      y,
      ">"
    );

    drawSelectedMenuText(
      display,
      centerX + text_offset_x,
      y,
      text
    );
  }

  inline void drawMenuItemText(
    DisplayDriver& display,
    const char* text,
    int margin
  ) {
    const char* space = strchr(text, ' ');

    if (space && display.getTextWidth(text) > display.width() - margin) {
      char line1[32];
      char line2[32];
      size_t n = space - text;

      if (n >= sizeof(line1))
        n = sizeof(line1) - 1;

      memcpy(line1, text, n);
      line1[n] = '\0';

      strncpy(line2, space + 1, sizeof(line2) - 1);
      line2[sizeof(line2) - 1] = '\0';

      display.drawTextCentered(display.width() / 2, 29, line1);
      display.drawTextCentered(display.width() / 2, 47, line2);
    } else {
      display.drawTextCentered(display.width() / 2, 38, text);
    }
  }

  void renderSectionHome(
    DisplayDriver& display,
    const uint8_t* icon,
    uint8_t icon_width,
    uint8_t icon_height,
    const char* title,
    int title_offset_x = 0,
    int icon_offset_y = 0
  ) {

    const int centerX = display.width() / 2;
    const int iconY = 15 + icon_offset_y;
    const int titleY = 55;

    display.setColor(UIColor::corp_blue);

    display.drawXbm(
      (display.width() - icon_width) / 2,
      iconY,
      icon,
      icon_width,
      icon_height
    );

    display.setColor(UIColor::primary_txt);
    display.setTextSize(1);

    const int textCenterX = centerX + title_offset_x;

    display.drawTextCentered(
      textCenterX,
      titleY,
      title
    );
  }

  void renderSOSHome(
    DisplayDriver& display
  ) {
    renderSectionHome(
      display,
      sos_icon,
      64,
      32,
      "SOS",
      0,
      2
    );
  }


  int render(DisplayDriver& display) override {
    char tmp[80];

    // Header único HiveFW:
    //
    // FIRST  -> HiveFW | hora | bateria
    // OUTRAS -> XX/08  | hora | bateria
    //
    // Os antigos pontos de páginas deixaram
    // de existir.
    renderHiveHeader(display);

    // ======================================================
    // MP-03 — COMPANION
    // ======================================================
    if (_page == HomePage::COMPANION) {

      // ======================================================
      // MP-03.1 — HOME COMPANION
      // ======================================================

      if (!_companion_submenu) {

        renderSectionHome(
          display,
          companion_icon,
          64,
          32,
          "COMPANION"
        );

        return 20000;
      }

      // ======================================================
      // MP-03.2 — INFO COMPANION
      // ======================================================

      if (_companion_info_submenu) {
        renderCompanionInfo(display);
        return 20000;
      }

      // MP-03 — MENU
      // ======================================================

      display.setColor(UIColor::primary_txt);
      display.setTextSize(1);

      const char* companion_items[] = {
        "INFO COMPANION",
        "SINCRONIZAR RELÓGIO",
#if ENV_INCLUDE_GPS == 1
        "SINCRONIZAR VIA GPS",
#endif
        "DESCOBRIR REPETIDORES",
        "REPETIDORES DESCOBERTOS",
        "[ SAIR ]"
      };

      // Selector oficial HiveFW:
      // texto centrado, tamanho 2 e duas linhas quando necessário.
      display.setColor(UIColor::primary_txt);
      display.setTextSize(2);

      const char* text = companion_items[_companion_menu];
      drawMenuItemText(display, text, 32);

      return 20000;
    }

    if (_page == HomePage::FIRST) {

      renderFirstDashboard(
        display
      );

    } else if (_page == HomePage::RECENT) {
      the_mesh.getRecentlyHeard(recent, UI_RECENT_LIST_SIZE);
      display.setColor(UIColor::primary_txt);
      int y = 20;
      for (int i = 0; i < UI_RECENT_LIST_SIZE; i++, y += 11) {
        auto a = &recent[i];
        if (a->name[0] == 0) continue;  // empty slot
        int secs = _rtc->getCurrentTime() - a->recv_timestamp;
        if (secs < 60) {
          sprintf(tmp, "%ds", secs);
        } else if (secs < 60*60) {
          sprintf(tmp, "%dm", secs / 60);
        } else {
          sprintf(tmp, "%dh", secs / (60*60));
        }

        int timestamp_width = display.getTextWidth(tmp);
        int max_name_width = display.width() - timestamp_width - 1;

        char filtered_recent_name[sizeof(a->name)];
        display.translateUTF8ToBlocks(filtered_recent_name, a->name, sizeof(filtered_recent_name));
        display.drawTextEllipsized(0, y, max_name_width, filtered_recent_name);
        display.setCursor(display.width() - timestamp_width - 1, y);
        display.print(tmp);
      }
    } else if (_page == HomePage::MESSAGES) {
    if (!_sms_submenu) {
      renderSectionHome(
        display,
        sms_icon,
        64,
        32,
        "MENSAGENS"
      );


  } else if (_sms_new_submenu) {

    display.setColor(UIColor::primary_txt);
    display.setTextSize(1);

    // ========================================================
    // MENU NOVA MENSAGEM
    // ========================================================

    if (_sms_new_stage == 0) {

      display.drawTextCentered(
        display.width() / 2,
        18,
        "Nova Mensagem"
      );

      const char* items[] = {
        "Escrever",
        "Presets",
        "Localização",
        "[ SAIR ]"
      };

      drawMenuSelection(
        display,
        items[_sms_new_menu],
        40,
        0
      );

    // ========================================================
    // DESTINO
    // ========================================================

    } else if (_sms_new_stage == 1) {

      display.drawTextCentered(
        display.width() / 2,
        18,
        "Enviar para"
      );

      const char* items[] = {
        "Contactos",
        "Canais",
        "[ SAIR ]"
      };

      drawMenuSelection(
        display,
        items[_sms_target_menu],
        40,
        0
      );

    // ========================================================
    // CONTACTOS
    // ========================================================

    } else if (_sms_new_stage == 2) {

      display.drawTextCentered(
        display.width() / 2,
        18,
        "Contacto"
      );

      int count = the_mesh.getNumContacts();

      if (_sms_contact_menu < count) {

        ContactInfo contact;

        if (the_mesh.getContactByIdx(
              _sms_contact_menu + MAX_ANON_CONTACTS,
              contact
            )) {

          drawMenuListItem(
            display,
            contact.name
          );
        }

      } else {

        drawMenuSelection(
          display,
          "[ SAIR ]",
          40,
          0
        );
      }

    // ========================================================
    // EDITOR
    // ========================================================

    } else if (_sms_new_stage == 3) {

      display.setColor(UIColor::primary_txt);
      display.setTextSize(1);

      // Texto atualmente escrito
      if (_sms_text[0] == '\0') {

        display.setColor(UIColor::secondary_txt);

        display.drawTextCentered(
          display.width() / 2,
          31,
          "Mensagem vazia"
        );

      } else {

        display.setColor(UIColor::primary_txt);

        display.drawTextEllipsized(
          2,
          29,
          display.width() - 4,
          _sms_text
        );
      }

      // Carácter atualmente selecionado
      const char* charset =
        " ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        ".,!?-_/@#()";

      char selected_char = charset[_sms_char_index];

      char char_label[32];

      if (selected_char == ' ') {

        snprintf(
          char_label,
          sizeof(char_label),
          "> ESPACO <"
        );

      } else {

        snprintf(
          char_label,
          sizeof(char_label),
          "> %c <",
          selected_char
        );
      }

      display.setColor(UIColor::secondary_txt);

      display.drawTextCentered(
        display.width() / 2,
        49,
        char_label
      );

    } else if (_sms_new_stage == 4) {

      display.setColor(UIColor::primary_txt);
      display.setTextSize(1);

      display.drawTextCentered(
        display.width() / 2,
        18,
        "Ações"
      );

      const char* actions[] = {
        "APAGAR",
        "ENVIAR",
        "SAIR"
      };

      drawMenuSelection(
        display,
        actions[_sms_action_menu],
        40
      );

    } else if (_sms_new_stage == 5) {

      display.drawTextCentered(
        display.width() / 2,
        17,
        "Enviar?"
      );

      // Destino
      if (_sms_target_type == 0 &&
          _sms_recipient_valid) {

        display.drawTextEllipsized(
          4,
          28,
          display.width() - 8,
          _sms_recipient.name
        );

      } else if (_sms_target_type == 1 &&
                 _sms_channel_valid) {

        display.drawTextEllipsized(
          4,
          28,
          display.width() - 8,
          _sms_channel.name
        );
      }

      // Conteúdo
      display.drawTextEllipsized(
        4,
        39,
        display.width() - 8,
        _sms_text
      );

      const char* confirms[] = {
        "SIM",
        "NÃO"
      };

      drawMenuSelection(
        display,
        confirms[_sms_confirm],
        54
      );

    // ========================================================
    // CANAIS
    // ========================================================

    } else if (_sms_new_stage == 6) {

      display.drawTextCentered(
        display.width() / 2,
        18,
        "Canal"
      );

      ChannelDetails selected;
      bool valid = false;
      int found = 0;

#ifdef MAX_GROUP_CHANNELS
      for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {

        ChannelDetails channel;

        if (the_mesh.getChannel(i, channel) &&
            channel.name[0] != '\0') {

          if (found == _sms_channel_menu) {
            selected = channel;
            valid = true;
            break;
          }

          found++;
        }
      }
#endif

      if (valid) {

        drawMenuListItem(
          display,
          selected.name
        );

      } else {

        drawMenuSelection(
          display,
          "[ SAIR ]",
          40
        );
      }

    // ========================================================
    // PRESETS
    // ========================================================

    } else if (_sms_new_stage == 7) {

      const char* presets[] = {
        "Estou em casa",
        "Cheguei bem",
        "A caminho",
        "Preciso de ajuda",
        "Estou no trabalho",
        "Ja vou",
        "OK",
        "Sim",
        "Não",
        "[ SAIR ]"
      };

      display.drawTextCentered(
        display.width() / 2,
        18,
        "Presets"
      );

      drawMenuListItem(
        display,
        presets[_sms_preset_menu]
      );
    }
    } else if (_sms_messages_submenu) {

      display.setColor(UIColor::primary_txt);
      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, 18, "Mensagens");

      int channel_count = 0;

      #ifdef MAX_GROUP_CHANNELS
        for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
          ChannelDetails channel;

          if (the_mesh.getChannel(i, channel) &&
              channel.name[0] != '\0') {
            channel_count++;
          }
        }
      #endif

      int total_items = channel_count + 1;

      // Mostrar apenas a opção atualmente selecionada.
      // A navegação continua a ser feita com NEXT/PREV.
      if (_sms_messages_menu < channel_count) {

        ChannelDetails selected;
        bool valid = false;
        int found = 0;

        #ifdef MAX_GROUP_CHANNELS
          for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
            ChannelDetails channel;

            if (the_mesh.getChannel(i, channel) &&
                channel.name[0] != '\0') {

              if (found == _sms_messages_menu) {
                selected = channel;
                valid = true;
                break;
              }

              found++;
            }
          }
        #endif

        if (valid) {
          display.setColor(UIColor::primary_txt);
          int unread_count = getUnreadChannelCount(selected.channel.hash);
          char channel_label[48];

          if (unread_count > 0)
            snprintf(channel_label, sizeof(channel_label), "%s (%d)", selected.name, unread_count);
          else
            snprintf(channel_label, sizeof(channel_label), "%s", selected.name);

          display.setColor(UIColor::primary_txt);
          display.drawTextCentered(
            display.width() / 2,
            34,
            channel_label
          );
        }

      } else {

        // Última opção: avançar para a página seguinte
        display.setColor(UIColor::primary_txt);
        display.drawTextCentered(
          display.width() / 2,
          34,
          "[ SAIR ]"
        );
      }

      // Indicação fixa da função do botão ENTER
      display.setColor(UIColor::secondary_txt);
      display.drawTextCentered(
        display.width() / 2,
        54,
        "ENTER = selecionar"
      );

    } else {

      display.setColor(UIColor::primary_txt);
      display.setTextSize(1);

      const char* sms_items[] = {
        "Nova Mensagem",
        "Caixa de entrada",
        "[ SAIR ]"
      };

      for (int i = 0; i < 3; i++) {
        int y = 25 + (i * 12);

        if (i == _sms_menu) {
          display.setColor(UIColor::primary_txt);
          drawMenuSelection(display, sms_items[i], y);
        } else {
          display.setColor(UIColor::secondary_txt);
          display.drawTextCentered(display.width() / 2, y, sms_items[i]);
        }
      }
    }

  } else if (_page == HomePage::RADIO) {

      // ======================================================
      // MP-04 — RÁDIO
      //
      // MP-03.1 — Página Rádio original
      // MP-03.2 — Menu Rádio
      // ======================================================

      // ------------------------------------------------------
      // MP-04.1 — PÁGINA RÁDIO
      //
      // Página principal: logo 64x32 + título.
      // MENU RÁDIO apresenta e permite alterar os parâmetros do Rádio.
      // ------------------------------------------------------

      if (!_radio_submenu) {

        renderSectionHome(
          display,
          radio_icon,
          64, 32,
          "RÁDIO"
        );

      }

      // ------------------------------------------------------
      // MP-04.2 — MENU RÁDIO
      //
      // Menu direto de configuração do Rádio.
      // ------------------------------------------------------

      else if (_radio_submenu) {

        const char* radio_items[] = {
          "FREQUÊNCIA",
          "BANDWIDTH",
          "SPREADING FACTOR",
          "CODING RATE",
          "TX POWER",
          "PATH",
          "RX GAIN",
          "PRESETS",
          "[ SAIR ]"
        };

        display.setColor(UIColor::primary_txt);
        display.setTextSize(2);

        // ------------------------------------------------------
        // MENU
        //
        // FREQUÊNCIA e TX POWER também são editados directamente
        // neste mesmo menu, tal como BW / SF / CR / PATH / RX GAIN.
        // ------------------------------------------------------

        {
          const char* title =
            radio_items[_radio_menu];

          char value[32];
          value[0] = '\0';

          // ------------------------------------------------------
          // FREQUÊNCIA — valor candidato durante edição
          // ------------------------------------------------------

          if (_radio_freq_edit) {

            snprintf(
              value,
              sizeof(value),
              "%d%d%d.%d%d%d Mhz",
              _radio_freq_digits[0],
              _radio_freq_digits[1],
              _radio_freq_digits[2],
              _radio_freq_digits[3],
              _radio_freq_digits[4],
              _radio_freq_digits[5]
            );

          // ------------------------------------------------------
          // TX POWER — valor candidato durante edição
          // ------------------------------------------------------

          } else if (_radio_tx_edit) {

            snprintf(
              value,
              sizeof(value),
              "%d%d",
              _radio_tx_digits[0],
              _radio_tx_digits[1]
            );

          // ------------------------------------------------------
          // OUTROS VALORES — valor candidato durante edição
          // ------------------------------------------------------

          } else if (_radio_value_edit) {

            if (_radio_menu == 1) {

              const float bw_values[] = {
                7.8f,
                10.4f,
                15.6f,
                20.8f,
                31.25f,
                41.7f,
                62.5f,
                125.0f,
                250.0f,
                500.0f
              };

              snprintf(
                value,
                sizeof(value),
                "%g Khz",
                bw_values[_radio_value_index]
              );

            } else if (_radio_menu == 2) {

              snprintf(
                value,
                sizeof(value),
                "%d",
                5 + _radio_value_index
              );

            } else if (_radio_menu == 3) {

              snprintf(
                value,
                sizeof(value),
                "%d",
                5 + _radio_value_index
              );

            } else if (_radio_menu == 5) {

              // PATH:
              // 0 = 1 byte
              // 1 = 2 bytes
              // 2 = 3 bytes
              snprintf(
                value,
                sizeof(value),
                "%d byte%s",
                _radio_value_index + 1,
                (_radio_value_index + 1) == 1 ? "" : "s"
              );

            } else if (_radio_menu == 6) {

              snprintf(
                value,
                sizeof(value),
                "%s",
                _radio_value_index
                  ? "BOOSTED"
                  : "POWER SAVE"
              );

            } else if (_radio_menu == 7) {

              static const char* preset_names[] = {
                "AUSTRALIA",
                "AUSTRALIA NARROW",
                "AUSTRALIA SA/WA/QLD",
                "EU/UK NARROW",
                "EU/UK LONG RANGE",
                "EU/UK MEDIUM RANGE",
                "CZECH REPUBLIC",
                "EU 433MHZ",
                "NEW ZEALAND",
                "NEW ZEALAND NARROW",
                "PORTUGAL 433",
                "PORTUGAL 868",
                "SWITZERLAND",
                "USA/CANADA",
                "VIETNAM"
              };

              snprintf(
                value,
                sizeof(value),
                "%s",
                preset_names[_radio_value_index]
              );

            }

          } else {

            switch (_radio_menu) {

              case 0:
                snprintf(
                  value,
                  sizeof(value),
                  "%.3f Mhz",
                  _node_prefs->freq
                );
                break;

              case 1:
                snprintf(
                  value,
                  sizeof(value),
                  "%g Khz",
                  _node_prefs->bw
                );
                break;

              case 2:
                snprintf(
                  value,
                  sizeof(value),
                  "%d",
                  _node_prefs->sf
                );
                break;

              case 3:
                snprintf(
                  value,
                  sizeof(value),
                  "%d",
                  _node_prefs->cr
                );
                break;

              case 4:
                snprintf(
                  value,
                  sizeof(value),
                  "%d",
                  _node_prefs->tx_power_dbm
                );
                break;

              case 5:
                snprintf(
                  value,
                  sizeof(value),
                  "%d byte%s",
                  _node_prefs->path_hash_mode + 1,
                  (_node_prefs->path_hash_mode + 1) == 1 ? "" : "s"
                );
                break;

              case 6:
                snprintf(
                  value,
                  sizeof(value),
                  "%s",
                  _node_prefs->rx_boosted_gain
                    ? "BOOSTED"
                    : "POWER SAVE"
                );
                break;

              case 7: {
                int preset_index = findRadioPreset(
                  _node_prefs->freq,
                  _node_prefs->bw,
                  _node_prefs->sf,
                  _node_prefs->cr
                );

                if (preset_index >= 0) {
                  snprintf(
                    value,
                    sizeof(value),
                    "%s",
                    radio_presets[preset_index].name
                  );
                } else {
                  snprintf(
                    value,
                    sizeof(value),
                    "PERSONALIZADO"
                  );
                }

                break;
              }

              case 8:
                value[0] = '\0';
                break;
            }
          }

          display.setColor(UIColor::primary_txt);
          display.setTextSize(2);

          // Título do item — centrado horizontalmente
          drawSelectedMenuText(
            display,
            display.width() / 2,
            27,
            title
          );

          if (_radio_menu != 8) {

            display.setTextSize(1);

            display.drawTextCentered(
              display.width() / 2,
              48,
              value
            );

            if (
              _radio_freq_edit ||
              _radio_tx_edit ||
              _radio_value_edit
            ) {

              char pos[8];

              if (_radio_freq_edit) {

                snprintf(
                  pos,
                  sizeof(pos),
                  "%d/6",
                  _radio_freq_digit + 1
                );

              } else if (_radio_tx_edit) {

                snprintf(
                  pos,
                  sizeof(pos),
                  "%d/2",
                  _radio_tx_digit + 1
                );

              } else {

                int total = 1;

                if (_radio_menu == 1)
                  total = 10;
                else if (_radio_menu == 2)
                  total = 8;
                else if (_radio_menu == 3)
                  total = 4;
                else if (_radio_menu == 5)
                  total = 3;
                else if (_radio_menu == 6)
                  total = 2;
                else if (_radio_menu == 7)
                  total = 15;

                snprintf(
                  pos,
                  sizeof(pos),
                  "%d/%d",
                  _radio_value_index + 1,
                  total
                );
              }

              display.drawTextRightAlign(
                display.width() - 2,
                10,
                pos
              );
            }
          }
        }
      }

    } else if (_page == HomePage::REPETIDOR) {
      // ======================================================
      // MP-05 — REPETIDOR
      //
      // MP-04.1 — Página/logo
      // MP-04.2 — Menu REPETIDOR
      // MP-04.3 — Selector de estatísticas
      // ======================================================

      // ======================================================
      // MP-05.1 — PÁGINA REPETIDOR
      //
      // Apenas logo + título.
      // ENTER -> MP-04.2 MENU REPETIDOR
      // CANCEL/SELECT -> RADIO
      // ======================================================

      if (!_repeater_submenu && !_repeater_info_submenu) {

        renderSectionHome(
          display,
          repeater_icon,
          64, 32,
          "REPETIDOR"
        );
      }

      // ======================================================
      // MP-05.2 — MENU REPETIDOR
      //
      // 1. REPETIDOR
      // 2. AUTOADVERT
      // 3. DUTY CYCLE
      // 4. VIZINHOS
      // 5. INFO REPETIDOR
      // 6. SAIR
      // ======================================================

      else if (_repeater_submenu &&
               !_repeater_info_submenu &&
               !_repeater_neighbours_submenu &&
               !_repeater_duty_submenu) {

        char repeater_state[32];
        char autoadvert_state[32];
        char duty_state[32];

        snprintf(
          repeater_state,
          sizeof(repeater_state),
          "REPETIDOR: %s",
          the_mesh.getNodePrefs()->isRepeatEn() ? "ON" : "OFF"
        );

        snprintf(
          autoadvert_state,
          sizeof(autoadvert_state),
          "AUTOADVERT: %s",
          the_mesh.getNodePrefs()->isAutoAdvertEn() ? "ON" : "OFF"
        );

        uint8_t current_duty =
          dutyCyclePercentFromAirtimeFactor(
            the_mesh.getNodePrefs()->airtime_factor
          );

        snprintf(
          duty_state,
          sizeof(duty_state),
          "DUTY CYCLE: %u%%",
          (unsigned)current_duty
        );

        const char* repeater_items[] = {
          repeater_state,
          autoadvert_state,
          duty_state,
          "VIZINHOS",
          "INFO REPETIDOR",
          "[ SAIR ]"
        };

        // Selector oficial HiveFW:
        // texto centrado, tamanho 2 e duas linhas quando necessário.
        display.setColor(UIColor::primary_txt);
        display.setTextSize(2);

        const char* text = repeater_items[_repeater_menu];
        drawMenuItemText(display, text, 32);

      }

      // ======================================================
      // MP-05.2.1 — DUTY CYCLE
      //
      // NEXT / RIGHT -> +1%
      // PREV / LEFT  -> -1%
      // ENTER        -> guardar
      // SELECT/CANCEL -> cancelar
      // ======================================================

      else if (_repeater_duty_submenu) {

        display.setColor(
          UIColor::primary_txt
        );

        display.setTextSize(1);

        display.drawTextCentered(
          display.width() / 2,
          23,
          "DUTY CYCLE"
        );

        char dutyText[12];

        snprintf(
          dutyText,
          sizeof(dutyText),
          "%u%%",
          (unsigned)_repeater_duty_value
        );

        display.setTextSize(2);

        display.drawTextCentered(
          display.width() / 2,
          41,
          dutyText
        );
      }

      // ======================================================
      // MP-05.2.2 — VIZINHOS
      //
      // Mostra os repetidores vizinhos ouvidos directamente.
      //
      //  1/3
      //  Nome
      //  Chave
      //  SNR
      //  Tempo desde o ultimo advert
      // ======================================================

      else if (_repeater_neighbours_submenu) {

        int neighbour_count =
          the_mesh.getRepeaterNeighbourCount();

        display.setColor(UIColor::primary_txt);
        display.setTextSize(1);

        if (neighbour_count == 0) {

          display.drawTextCentered(
            display.width() / 2,
            37,
            "NENHUM VIZINHO"
          );

        } else {

          if (_repeater_neighbour_menu >= neighbour_count) {
            _repeater_neighbour_menu = 0;
          }

          const mesh::Identity* neighbour =
            the_mesh.getRepeaterNeighbour(
              _repeater_neighbour_menu
            );

          if (neighbour == NULL) {
            display.drawTextCentered(
              display.width() / 2,
              37,
              "NENHUM VIZINHO"
            );
          } else {

            char counterText[12];

            snprintf(
              counterText,
              sizeof(counterText),
              "%d/%d",
              (int)(_repeater_neighbour_menu + 1),
              neighbour_count
            );

            display.setColor(UIColor::secondary_txt);

            display.drawTextCentered(
              display.width() - 12,
              14,
              counterText
            );

            char name[33];
            name[0] = '\0';

            ContactInfo* contact =
              the_mesh.lookupContactByPubKey(
                neighbour->pub_key,
                PUB_KEY_SIZE
              );

            if (contact != NULL && contact->name[0] != '\0') {
              strncpy(
                name,
                contact->name,
                sizeof(name) - 1
              );
              name[sizeof(name) - 1] = '\0';
            }

            if (name[0] == '\0') {
              snprintf(
                name,
                sizeof(name),
                "%02X%02X%02X%02X%02X%02X%02X",
                neighbour->pub_key[0],
                neighbour->pub_key[1],
                neighbour->pub_key[2],
                neighbour->pub_key[3],
                neighbour->pub_key[4],
                neighbour->pub_key[5],
                neighbour->pub_key[6]
              );
            }

            display.setColor(UIColor::primary_txt);

            display.drawTextCentered(
              display.width() / 2,
              28,
              name
            );

            char keyText[32];

            snprintf(
              keyText,
              sizeof(keyText),
              "%02X%02X%02X%02X%02X%02X%02X",
              neighbour->pub_key[0],
              neighbour->pub_key[1],
              neighbour->pub_key[2],
              neighbour->pub_key[3],
              neighbour->pub_key[4],
              neighbour->pub_key[5],
              neighbour->pub_key[6]
            );

            display.setColor(UIColor::secondary_txt);

            display.drawTextCentered(
              display.width() / 2,
              42,
              keyText
            );

            int snr =
              (int)(the_mesh.getRepeaterNeighbourSNR(
                _repeater_neighbour_menu
              ) / 4);

            uint32_t heard_ago =
              the_mesh.getRepeaterNeighbourHeardAgo(
                _repeater_neighbour_menu
              );

            char infoText[32];

            snprintf(
              infoText,
              sizeof(infoText),
              "SNR %d  %lus",
              snr,
              (unsigned long)heard_ago
            );

            display.setColor(UIColor::secondary_txt);

            display.drawTextCentered(
              display.width() / 2,
              56,
              infoText
            );
          }
        }

      }
      // ======================================================
      // MP-05.3 — INFO REPETIDOR
      //
      // Estatísticas do repetidor.
      //
      //  1/13  RSSI
      //  2/13  SNR
      //  3/13  TX AIRTIME
      //  4/13  RX AIRTIME
      //  5/13  UPTIME
      //  6/13  BATERIA
      //  7/13  NOISE FLOOR
      //  8/13  PACOTES RX
      //  9/13  PACOTES TX
      // 10/13  RX ERRORS
      // 11/13  FLOOD RX/TX
      // 12/13  DIRECT RX/TX
      // 13/13  LOCALIZAÇÃO
      //
      // Uma única designação por página.
      // ======================================================

      else if (_repeater_info_submenu) {

        display.setColor(UIColor::secondary_txt);
        display.setTextSize(1);

        char repeater_counter[8];

        snprintf(
          repeater_counter,
          sizeof(repeater_counter),
          "%d/%d",
          (int)(_repeater_stats_page + 1),
          (int)REPEATER_INFO_PAGE_COUNT
        );

        display.drawTextRightAlign(
          display.width() - 1,
          8,
          repeater_counter
        );

        display.setColor(UIColor::primary_txt);
        display.setTextSize(1);

        char title[24];
        char value[40];

        title[0] = '\0';
        value[0] = '\0';

        switch (_repeater_stats_page) {

          case 0:
            snprintf(title, sizeof(title), "RSSI");

            snprintf(
              value,
              sizeof(value),
              "%d dBm",
              (int)the_mesh.getRepeaterRSSI()
            );
            break;

          case 1:
            snprintf(title, sizeof(title), "SNR");

            snprintf(
              value,
              sizeof(value),
              "%.2f dB",
              (float)radio_driver.getLastSNR()
            );
            break;

          case 2: {
            uint32_t seconds =
              the_mesh.getRepeaterTXAirtime() / 1000;

            snprintf(title, sizeof(title), "TX AIRTIME");

            snprintf(
              value,
              sizeof(value),
              "%lus",
              (unsigned long)seconds
            );
            break;
          }

          case 3: {
            uint32_t seconds =
              the_mesh.getReceiveAirTime() / 1000;

            snprintf(title, sizeof(title), "RX AIRTIME");

            snprintf(
              value,
              sizeof(value),
              "%lus",
              (unsigned long)seconds
            );
            break;
          }

          case 4:
            snprintf(
              title,
              sizeof(title),
              "UPTIME"
            );

            formatUptime(
              value,
              sizeof(value)
            );

            break;

          case 5:
            snprintf(title, sizeof(title), "BATERIA");

            snprintf(
              value,
              sizeof(value),
              "%.2f V",
              (float)board.getBattMilliVolts() / 1000.0f
            );
            break;

          case 6:
            snprintf(title, sizeof(title), "NOISE FLOOR");

            snprintf(
              value,
              sizeof(value),
              "%d dBm",
              (int)radio_driver.getNoiseFloor()
            );
            break;

          case 7:
            snprintf(title, sizeof(title), "PACOTES RX");

            snprintf(
              value,
              sizeof(value),
              "%lu",
              (unsigned long)the_mesh.getRepeaterMessagesIn()
            );
            break;

          case 8:
            snprintf(title, sizeof(title), "PACOTES TX");

            snprintf(
              value,
              sizeof(value),
              "%lu",
              (unsigned long)the_mesh.getRepeaterMessagesOut()
            );
            break;

          case 9:
            snprintf(title, sizeof(title), "RX ERRORS");

            snprintf(
              value,
              sizeof(value),
              "%lu",
              (unsigned long)radio_driver.getPacketsRecvErrors()
            );
            break;

          case 10:
            snprintf(title, sizeof(title), "FLOOD RX/TX");

            snprintf(
              value,
              sizeof(value),
              "%lu/%lu",
              (unsigned long)the_mesh.getNumRecvFlood(),
              (unsigned long)the_mesh.getNumSentFlood()
            );
            break;

          case 11:
            snprintf(title, sizeof(title), "DIRECT RX/TX");

            snprintf(
              value,
              sizeof(value),
              "%lu/%lu",
              (unsigned long)the_mesh.getNumRecvDirect(),
              (unsigned long)the_mesh.getNumSentDirect()
            );
            break;

          case 12:
            snprintf(
              title,
              sizeof(title),
              "%s",
              the_mesh.getNodePrefs()->advert_loc_policy == ADVERT_LOC_SHARE
                ? "LOCALIZAÇÃO ATIVA"
                : "LOCALIZAÇÃO OCULTA"
            );

            snprintf(
              value,
              sizeof(value),
              "%.4f %.4f",
              _sensors->node_lat,
              _sensors->node_lon
            );
            break;
        }

        display.setTextSize(1);

        display.drawTextCentered(
          display.width() / 2,
          28,
          title
        );

        display.drawTextCentered(
          display.width() / 2,
          49,
          value
        );

        display.setTextSize(1);
      }
    }

    else if (_page == HomePage::SETTINGS) {

      if (!_settings_submenu) {

        renderSectionHome(
          display,
          settings_icon,
          64, 32,
          "DEFINIÇÕES",
          7
        );

#ifdef HELTEC_T114_WITH_DISPLAY
      } else if (_settings_color_submenu) {

        display.setColor(
          UIColor::primary_txt
        );

        display.setTextSize(1);

        display.drawTextCentered(
          display.width() / 2,
          27,
          "COR DA BARRA"
        );

        display.setTextSize(2);

        uint8_t color_index =
          _settings_color_menu;

        if (
          color_index >=
          HIVEFW_HEADER_COLOR_COUNT
        ) {
          color_index = 0;
        }

        display.drawTextCentered(
          display.width() / 2,
          45,
          hivefw_header_color_names[
            color_index
          ]
        );

#endif
      } else if (_settings_advert_submenu) {

        const char* advert_items[] = {
          "Anuncio ZeroHOP",
          "Anuncio Flood",
          "[ SAIR ]"
        };

        display.setColor(UIColor::primary_txt);
        display.setTextSize(1);

        display.drawTextCentered(
          display.width() / 2,
          38,
          advert_items[_settings_advert_menu]
        );

      } else if (_settings_channel_submenu) {

        int channel_count = getAppsChannelCount();

        display.setColor(UIColor::primary_txt);
        display.setTextSize(1);

        if (channel_count == 0) {

          display.drawTextCentered(
            display.width() / 2,
            38,
            "SEM CANAIS"
          );

        } else {

          int selected_index = _settings_channel_menu;

          if (selected_index >= channel_count)
            selected_index = 0;

          int found = 0;
          ChannelDetails channel;
          bool valid = false;

          for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {

            ChannelDetails candidate;

            if (the_mesh.getChannel(i, candidate) &&
                candidate.name[0] != '\0') {

              if (found == selected_index) {
                channel = candidate;
                valid = true;
                break;
              }

              found++;
            }
          }

          if (valid) {

            char channel_text[32];

            snprintf(
              channel_text,
              sizeof(channel_text),
              "%d/%d %s",
              selected_index + 1,
              channel_count,
              channel.name
            );

            display.setTextSize(1);

            display.drawTextCentered(
              display.width() / 2,
              29,
              "CANAL APPS"
            );

            display.setTextSize(2);

            display.drawTextCentered(
              display.width() / 2,
              47,
              channel_text
            );
          }
        }

      } else {

        char bluetooth_item[32];

        snprintf(
          bluetooth_item,
          sizeof(bluetooth_item),
          "BLUETOOTH: %s",
          _task->isBluetoothEnabled() ? "ON" : "OFF"
        );

        const char* settings_items[] = {
          bluetooth_item,
          "ANUNCIAR NÓ",
          "CANAL APPS/SOS",
#ifdef HELTEC_T114_WITH_DISPLAY
          "COR DA BARRA",
#endif
#if ENV_INCLUDE_GPS == 1
          _task->getGPSState() ? "GPS: ON" : "GPS: OFF",
#endif
          "DESLIGAR",
          "[ SAIR ]"
        };

        display.setColor(UIColor::primary_txt);
        display.setTextSize(2);

        const char* text = settings_items[_settings_menu];
        drawMenuItemText(display, text, 10);
      }

#if ENV_INCLUDE_GPS == 1
    } else if (_page == HomePage::INTERNAL_GPS) {
      LocationProvider* nmea = sensors.getLocationProvider();
      char buf[50];
      int y = 18;
      bool gps_state = _task->getGPSState();
#ifdef PIN_GPS_SWITCH
      bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);
      if (gps_state != hw_gps_state) {
        strcpy(buf, gps_state ? "gps off(hw)" : "gps off(sw)");
      } else {
        strcpy(buf, gps_state ? "gps on" : "gps off");
      }
#else
      strcpy(buf, gps_state ? "gps on" : "gps off");
#endif
      display.setColor(UIColor::primary_txt);
      display.drawTextLeftAlign(0, y, buf);
      if (nmea == NULL) {
        y = y + 12;
        display.setColor(UIColor::secondary_txt);
        display.drawTextLeftAlign(0, y, "Can't access GPS");
      } else {
        display.setColor(UIColor::primary_txt);
        strcpy(buf, nmea->isValid()?"fix":"no fix");
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.setColor(UIColor::secondary_txt);
        display.drawTextLeftAlign(0, y, "sat");
        display.setColor(UIColor::primary_txt);
        sprintf(buf, "%d", nmea->satellitesCount());
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.setColor(UIColor::secondary_txt);
        display.drawTextLeftAlign(0, y, "pos");
        display.setColor(UIColor::primary_txt);
        sprintf(buf, "%.4f %.4f",
          nmea->getLatitude()/1000000., nmea->getLongitude()/1000000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.setColor(UIColor::secondary_txt);
        display.drawTextLeftAlign(0, y, "alt");
        display.setColor(UIColor::primary_txt);
        sprintf(buf, "%.2f", nmea->getAltitude()/1000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
      }
#endif
#if UI_SENSORS_PAGE == 1
    } else if (_page == HomePage::INTERNAL_SENSORS) {
      int y = 18;
      refresh_sensors();
      char buf[30];
      char name[30];
      LPPReader r(sensors_lpp.getBuffer(), sensors_lpp.getSize());

      for (int i = 0; i < sensors_scroll_offset; i++) {
        uint8_t channel, type;
        r.readHeader(channel, type);
        r.skipData(type);
      }

      for (int i = 0; i < (sensors_scroll?UI_RECENT_LIST_SIZE:sensors_nb); i++) {
        uint8_t channel, type;
        if (!r.readHeader(channel, type)) { // reached end, reset
          r.reset();
          r.readHeader(channel, type);
        }

        display.setCursor(0, y);
        float v;
        switch (type) {
          case LPP_GPS: // GPS
            float lat, lon, alt;
            r.readGPS(lat, lon, alt);
            strcpy(name, "gps"); sprintf(buf, "%.4f %.4f", lat, lon);
            break;
          case LPP_VOLTAGE:
            r.readVoltage(v);
            strcpy(name, "voltage"); sprintf(buf, "%6.2f", v);
            break;
          case LPP_CURRENT:
            r.readCurrent(v);
            strcpy(name, "current"); sprintf(buf, "%.3f", v);
            break;
          case LPP_TEMPERATURE:
            r.readTemperature(v);
            strcpy(name, "temperature"); sprintf(buf, "%.2f", v);
            break;
          case LPP_RELATIVE_HUMIDITY:
            r.readRelativeHumidity(v);
            strcpy(name, "humidity"); sprintf(buf, "%.2f", v);
            break;
          case LPP_BAROMETRIC_PRESSURE:
            r.readPressure(v);
            strcpy(name, "pressure"); sprintf(buf, "%.2f", v);
            break;
          case LPP_ALTITUDE:
            r.readAltitude(v);
            strcpy(name, "altitude"); sprintf(buf, "%.0f", v);
            break;
          case LPP_POWER:
            r.readPower(v);
            strcpy(name, "power"); sprintf(buf, "%6.2f", v);
            break;
          default:
            r.skipData(type);
            strcpy(name, "unk"); sprintf(buf, "");
        }
        display.setCursor(0, y);
        display.setColor(UIColor::secondary_txt);
        display.print(name);
        display.setColor(UIColor::primary_txt);
        display.setCursor(
          display.width()-display.getTextWidth(buf)-1, y
        );
        display.print(buf);
        y = y + 12;
      }
      if (sensors_scroll) sensors_scroll_offset = (sensors_scroll_offset+1)%sensors_nb;
      else sensors_scroll_offset = 0;
#endif

    } else if (_page == HomePage::SOS) {

      // ======================================================
      // MP-06 — SOS
      // ======================================================

      if (
        _apps_view == APPS_VIEW_SOS &&
        _sos_submenu &&
        _sos_confirm_submenu
      ) {

        display.setColor(
          UIColor::primary_txt
        );

        display.setTextSize(1);

        display.drawTextCentered(
          display.width() / 2,
          18,
          "QUER ENVIAR?"
        );

        const char* sos_confirm_items[] = {
          "SIM",
          "NÃO"
        };

        drawMenuSelection(
          display,
          sos_confirm_items[_sos_menu],
          40
        );

      } else {

        renderSOSHome(display);
      }

    } else if (_page == HomePage::APPS) {

      // ======================================================
      // APLICAÇÕES
      // ======================================================

      // ------------------------------------------------------
      // APLICAÇÕES -> DESCOBRIR NÓS
      // ------------------------------------------------------
      // Discovery ATIVO. Não usa AdvertPath.
      // ------------------------------------------------------



      if (!_apps_submenu && _apps_view == APPS_VIEW_DISCOVERY) {

        refreshActiveDiscoveryNodes();

        display.setColor(UIColor::primary_txt);
        display.setTextSize(1);

        if (_active_discovery_count == 0) {

          // Linha reservada intencionalmente vazia.

          display.drawTextCentered(
            display.width() / 2,
            37,
            "Pressione"
          );

        } else {


          NodeDiscoveryResult& result =
            _active_discovery_nodes[_active_discovery_menu];
          char counterText[12];

          snprintf(
            counterText,
            sizeof(counterText),
            "%d/%d",
            (int)(_active_discovery_menu + 1),
            (int)_active_discovery_count
          );

          display.setColor(UIColor::secondary_txt);

          display.drawTextCentered(
            display.width() - 12,
            14,
            counterText
          );


          char name[33];
          name[0] = '\0';

          // Nome obtido através do próprio Discovery/ANON.
          if (result.name[0] != '\0') {

            strncpy(
              name,
              result.name,
              sizeof(name) - 1
            );

            name[sizeof(name) - 1] = '\0';
          }

          // Fallback visual através do Advert conhecido.
          // Isto NÃO altera "REPETIDORES DESCOBERTOS".
          if (name[0] == '\0') {

            the_mesh.getAdvertNameByPrefix(
              result.pub_key,
              name,
              sizeof(name)
            );
          }

          if (name[0] == '\0') {

            if (result.node_type == ADV_TYPE_REPEATER) {
              strcpy(name, "Repetidor");
            } else if (result.node_type == ADV_TYPE_ROOM) {
              strcpy(name, "Room");
            } else {
              strcpy(name, "Nó descoberto");
            }
          }

          display.setColor(UIColor::primary_txt);

          display.drawTextCentered(
            display.width() / 2,
            28,
            name
          );

          char keyText[32];

          snprintf(
            keyText,
            sizeof(keyText),
            "%02X%02X%02X%02X%02X%02X%02X",
            result.pub_key[0],
            result.pub_key[1],
            result.pub_key[2],
            result.pub_key[3],
            result.pub_key[4],
            result.pub_key[5],
            result.pub_key[6]
          );

          display.setColor(UIColor::secondary_txt);

          display.drawTextCentered(
            display.width() / 2,
            42,
            keyText
          );

          char infoText[32];

          snprintf(
            infoText,
            sizeof(infoText),
            "SNR %d",
            (int)result.snr
          );

          display.setColor(UIColor::secondary_txt);

          display.drawTextCentered(
            display.width() / 2,
            56,
            infoText
          );
        }
}

      // ------------------------------------------------------
      // APLICAÇÕES -> NÓS DESCOBERTOS
      // render discovered nodes final
      // ------------------------------------------------------

      else if (!_apps_submenu && _apps_view == APPS_VIEW_DISCOVERED) {

        refreshDiscoveredNodes();

        display.setColor(UIColor::primary_txt);
        display.setTextSize(1);

        if (_discover_count == 0) {

          display.drawTextCentered(
            display.width() / 2,
            33,
            "Nenhum nó conhecido"
          );

          display.drawTextCentered(
            display.width() / 2,
            49,
            "A aguardar anúncios..."
          );

        } else {

          AdvertPath& node =
            _discover_nodes[_discover_menu];

          char name[33];

          strncpy(
            name,
            node.name,
            sizeof(name) - 1
          );

          name[sizeof(name) - 1] = '\0';

          if (name[0] == '\0') {
            strcpy(name, "Nó sem nome");
          }

          display.drawTextCentered(
            display.width() / 2,
            29,
            name
          );

          uint32_t now =
            _rtc->getCurrentTime();

          uint32_t age = 0;

          if (now >= node.recv_timestamp) {
            age = now - node.recv_timestamp;
          }

          char ageText[24];

          if (age < 60) {

            snprintf(
              ageText,
              sizeof(ageText),
              "há %lus",
              (unsigned long)age
            );

          } else if (age < 3600) {

            snprintf(
              ageText,
              sizeof(ageText),
              "há %lum",
              (unsigned long)(age / 60)
            );

          } else {

            snprintf(
              ageText,
              sizeof(ageText),
              "há %luh",
              (unsigned long)(age / 3600)
            );
          }

          display.drawTextCentered(
            display.width() / 2,
            41,
            ageText
          );

          char infoText[24];

          snprintf(
            infoText,
            sizeof(infoText),
            "%d saltos   %d/%d",
            (int)(node.path_len & 0x3F),
            (int)(_discover_menu + 1),
            (int)_discover_count
          );

          display.drawTextCentered(
            display.width() / 2,
            53,
            infoText
          );
        }

        display.drawTextCentered(
          display.width() / 2,
          63,
          "[ SAIR ]"
        );
      }

      // ------------------------------------------------------
      // APLICAÇÕES -> PÁGINA PRINCIPAL
      // ------------------------------------------------------

      else if (!_apps_submenu) {

        renderSectionHome(
          display,
          apps_icon,
          64, 32,
          "APLICAÇÕES",
          7
        );

      }

      // ------------------------------------------------------
      // APLICAÇÕES -> MENU
      // ------------------------------------------------------

      else {

        const char* apps_items[] = {
          "HOME ASSISTANT",
#if ENV_INCLUDE_GPS == 1
          "GPS",
#endif
#if UI_SENSORS_PAGE == 1
          "SENSORES",
#endif
          "RELÓGIO",
          "[ SAIR ]"
        };

        display.setColor(UIColor::primary_txt);
        display.setTextSize(2);

        const char* text = apps_items[_apps_menu];
        drawMenuItemText(display, text, 32);
      }

    } else if (_page == HomePage::INTERNAL_HOME_ASSISTANT) {

      if (!_ha_submenu) {

        display.setColor(
          UIColor::corp_blue
        );

        display.drawXbm(
          (display.width() - 32) / 2,
          15,
          home_assistant_icon,
          32,
          32
        );

        display.setColor(
          UIColor::primary_txt
        );

        display.setTextSize(1);

        display.drawTextCentered(
          display.width() / 2,
          55,
          "Home Assistant"
        );
      }

      // ======================================================
      // MENU PRINCIPAL
      // ======================================================

      else if (_ha_stage == HA_STAGE_MAIN) {

        int total =
          haMainCount();

        if (
          total <= 0 ||
          _ha_menu >= total
        )
          _ha_menu = 0;

        const char* text =
          haMainLabel();

        display.setColor(
          UIColor::primary_txt
        );

        display.setTextSize(2);

        if (
          display.getTextWidth(text) >
          display.width() - 8
        )
          display.setTextSize(1);

        drawSelectedMenuText(
          display,
          display.width() / 2,
          38,
          text
        );
      }

      // ======================================================
      // EDITOR NOME / COMANDO
      // ======================================================

      else if (
        _ha_stage == HA_STAGE_EDIT_NAME ||
        _ha_stage == HA_STAGE_EDIT_COMMAND
      ) {

        bool command_field =
          _ha_stage ==
          HA_STAGE_EDIT_COMMAND;

        display.setColor(
          UIColor::secondary_txt
        );

        display.setTextSize(1);

        display.drawTextRightAlign(
          display.width() - 1,
          14,
          command_field
            ? "2/2"
            : "1/2"
        );

        display.setColor(
          UIColor::primary_txt
        );

        display.drawTextCentered(
          display.width() / 2,
          22,
          command_field
            ? "COMANDO"
            : "NOME DO COMANDO"
        );

        if (command_field) {

          display.setCursor(
            2,
            36
          );

          display.print("!");

          display.drawTextEllipsized(
            10,
            36,
            display.width() - 12,
            _ha_edit_command
          );

        } else {

          display.drawTextEllipsized(
            2,
            36,
            display.width() - 4,
            _ha_edit_name
          );
        }

        const char* charset =
          haEditorCharset();

        int charset_len =
          strlen(charset);

        if (
          _ha_char_index >=
          charset_len
        )
          _ha_char_index = 3;

        char selected =
          charset[_ha_char_index];

        char label[24];

        if (selected == '\b') {

          snprintf(
            label,
            sizeof(label),
            "< APAGAR"
          );

        } else if (
          selected == '\x1B'
        ) {

          snprintf(
            label,
            sizeof(label),
            "[ CANCELAR ]"
          );

        } else if (
          selected == ' '
        ) {

          snprintf(
            label,
            sizeof(label),
            "CARACTER: ESPACO"
          );

        } else {

          snprintf(
            label,
            sizeof(label),
            "CARACTER: %c",
            selected
          );
        }

        display.setColor(
          UIColor::secondary_txt
        );

        display.drawTextCentered(
          display.width() / 2,
          54,
          label
        );
      }

      // ======================================================
      // GERIR COMANDOS — LISTA
      // ======================================================

      else if (
        _ha_stage ==
        HA_STAGE_MANAGE_LIST
      ) {

        if (_ha_command_count == 0) {

          _ha_stage =
            HA_STAGE_MAIN;

          _ha_menu = 0;

        } else {

          int total =
            _ha_command_count + 1;

          if (_ha_manage_menu >= total)
            _ha_manage_menu = 0;

          const char* text =
            (_ha_manage_menu <
             _ha_command_count)
              ? _ha_commands[
                  _ha_manage_menu
                ].name
              : "[ SAIR ]";

          display.setColor(
            UIColor::primary_txt
          );

          display.setTextSize(2);

          if (
            display.getTextWidth(text) >
            display.width() - 8
          )
            display.setTextSize(1);

          drawSelectedMenuText(
            display,
            display.width() / 2,
            38,
            text
          );
        }
      }

      // ======================================================
      // GERIR — ALTERAR / LOCALIZAÇÃO / APAGAR
      // ======================================================

      else if (
        _ha_stage ==
        HA_STAGE_MANAGE_ACTION
      ) {

#if ENV_INCLUDE_GPS == 1

        char location_item[24];

        bool location_enabled =
          (
            _ha_edit_index <
              _ha_command_count &&
            (
              _ha_commands[
                _ha_edit_index
              ].flags &
              HIVEFW_HA_FLAG_LOCATION
            ) != 0
          );

        snprintf(
          location_item,
          sizeof(location_item),
          "LOCALIZAÇÃO: %s",
          location_enabled
            ? "ON"
            : "OFF"
        );

        const char* items[] = {
          "ALTERAR",
          location_item,
          "APAGAR",
          "[ VOLTAR ]"
        };

        const uint8_t action_count = 4;

#else

        const char* items[] = {
          "ALTERAR",
          "APAGAR",
          "[ VOLTAR ]"
        };

        const uint8_t action_count = 3;

#endif

        if (
          _ha_action_menu >=
          action_count
        ) {
          _ha_action_menu = 0;
        }

        display.setColor(
          UIColor::primary_txt
        );

        display.setTextSize(2);

        drawMenuItemText(
          display,
          items[_ha_action_menu],
          32
        );
      }

      // ======================================================
      // CONFIRMAR APAGAR
      // ======================================================

      else if (
        _ha_stage ==
        HA_STAGE_DELETE_CONFIRM
      ) {

        display.setColor(
          UIColor::primary_txt
        );

        display.setTextSize(1);

        display.drawTextCentered(
          display.width() / 2,
          21,
          "APAGAR COMANDO?"
        );

        const char* items[] = {
          "SIM",
          "NAO"
        };

        drawMenuSelection(
          display,
          items[
            _ha_delete_confirm
          ],
          42
        );
      }

    } else if (_page == HomePage::INTERNAL_CLOCK) {
      uint32_t now = _rtc->getCurrentTime();

      // Lisbon timezone:
      // Winter = UTC+0
      // Summer = UTC+1
      // DST starts on the last Sunday of March
      // DST ends on the last Sunday of October

      DateTime utc = DateTime(now);

      int daysInMonth = 31;
      if (utc.month() == 4 || utc.month() == 6 || utc.month() == 9 || utc.month() == 11) {
        daysInMonth = 30;
      } else if (utc.month() == 2) {
        daysInMonth = ((utc.year() % 4 == 0 && utc.year() % 100 != 0) || (utc.year() % 400 == 0)) ? 29 : 28;
      }

      // Calculate weekday of the last day of the month.
      // 0 = Sunday, 1 = Monday, ... 6 = Saturday
      int y = utc.year();
      int m = utc.month();
      int d = daysInMonth;

      if (m < 3) {
        y--;
        m += 12;
      }

      int weekdayLastDay =
        (d + (13 * (m + 1)) / 5 + y + y / 4 - y / 100 + y / 400) % 7;

      // Zeller: 0 = Saturday, 1 = Sunday, ...
      int sundayOffset = (weekdayLastDay + 6) % 7;
      int lastSunday = daysInMonth - sundayOffset;

      bool summerTime = false;

      if (utc.month() > 3 && utc.month() < 10) {
        summerTime = true;
      } else if (utc.month() == 3) {
        if (utc.day() > lastSunday) {
          summerTime = true;
        } else if (utc.day() == lastSunday && utc.hour() >= 1) {
          // EU DST transition occurs at 01:00 UTC
          summerTime = true;
        }
      } else if (utc.month() == 10) {
        if (utc.day() < lastSunday) {
          summerTime = true;
        } else if (utc.day() == lastSunday && utc.hour() < 1) {
          // Until 01:00 UTC on the last Sunday of October
          summerTime = true;
        }
      }

      uint32_t localTimestamp = now + (summerTime ? 3600 : 0);
      DateTime localTime = DateTime(localTimestamp);

      char timeText[16];
      char dateText[16];

      snprintf(
        timeText,
        sizeof(timeText),
        "%02d:%02d",
        localTime.hour(),
        localTime.minute()
      );

      snprintf(
        dateText,
        sizeof(dateText),
        "%02d/%02d/%04d",
        localTime.day(),
        localTime.month(),
        localTime.year()
      );

      display.setColor(UIColor::primary_txt);
      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, 25, timeText);

      display.setColor(UIColor::secondary_txt);
      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, 41, dateText);

      display.setTextSize(1);
      display.drawTextCentered(
        display.width() / 2 + 2,
        53,
        summerTime ? "Horário de Verão" : "Horário de Inverno"
      );

    }
    return 20000;   // next render after 5000 ms
  }

  bool handleInput(char c) override {

    // ========================================================
    // SMS -> MENSAGENS -> PUBLIC
    // ========================================================

    if (_page == HomePage::MESSAGES && _sms_messages_submenu) {

      int channel_count = 0;

      #ifdef MAX_GROUP_CHANNELS
        for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
          ChannelDetails channel;

          if (the_mesh.getChannel(i, channel) &&
              channel.name[0] != '\0') {
            channel_count++;
          }
        }
      #endif

      int total_items = channel_count + 1;

      if (c == KEY_NEXT) {
        if (total_items > 0) {
          _sms_messages_menu =
            (_sms_messages_menu + 1) % total_items;
        }
        return true;
      }

      if (c == KEY_PREV) {
        if (total_items > 0) {
          _sms_messages_menu =
            (_sms_messages_menu + total_items - 1) % total_items;
        }
        return true;
      }

      if (c == KEY_CANCEL) {
        _sms_messages_submenu = false;
        return true;
      }

      if (c == KEY_ENTER) {

        // Canal selecionado
        if (_sms_messages_menu < channel_count) {

          ChannelDetails selected;
          bool valid = false;
          int found = 0;

          #ifdef MAX_GROUP_CHANNELS
            for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
              ChannelDetails channel;

              if (the_mesh.getChannel(i, channel) &&
                  channel.name[0] != '\0') {

                if (found == _sms_messages_menu) {
                  selected = channel;
                  valid = true;
                  break;
                }

                found++;
              }
            }
          #endif

          if (valid) {
            _task->gotoChannelMessages(
              (uint8_t)found
            );
            clearUnreadChannel(selected.channel.hash);
          }

          return true;
        }

      // [ SAIR ]
      if (_sms_messages_menu == channel_count) {
        _sms_messages_submenu = false;
        return true;
        }
      }
    }

    // ========================================================
    // NOVA MENSAGEM MENU
    // ========================================================

    // Este bloco tem prioridade sobre o SMS MENU.
    // _sms_submenu permanece TRUE durante todo o fluxo.

    if (_page == HomePage::MESSAGES && _sms_new_submenu) {

      // ------------------------------------------------------
      // MENU PRINCIPAL
      // ------------------------------------------------------

      if (_sms_new_stage == 0) {

        if (c == KEY_NEXT || c == KEY_RIGHT) {
          _sms_new_menu = (_sms_new_menu + 1) % 4;
          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {
          _sms_new_menu = (_sms_new_menu + 3) % 4;
          return true;
        }

        if (c == KEY_CANCEL || c == KEY_SELECT) {
          _sms_new_submenu = false;
          _sms_submenu = true;
          return true;
        }

        if (c == KEY_ENTER) {

          // Escrever
          if (_sms_new_menu == 0) {

            _sms_flow_type = 0;
            _sms_text[0] = '\0';
            _sms_target_menu = 0;
            _sms_new_stage = 1;

            return true;
          }

          // Presets
          if (_sms_new_menu == 1) {

            _sms_flow_type = 1;
            _sms_preset_menu = 0;
            _sms_target_menu = 0;
            _sms_new_stage = 7;

            return true;
          }

          // Localizacao
          if (_sms_new_menu == 2) {

            _sms_flow_type = 2;
            _sms_target_menu = 0;

            // Não capturar a posição aqui.
            // Será capturada imediatamente antes do envio.
            _sms_text[0] = '\0';

            _sms_new_stage = 1;

            return true;
          }

          // SAIR
          if (_sms_new_menu == 3) {

            _sms_new_submenu = false;
            _sms_submenu = true;

            return true;
          }
        }

        return true;
      }

      // ------------------------------------------------------
      // DESTINO
      // ------------------------------------------------------

      if (_sms_new_stage == 1) {

        if (c == KEY_NEXT || c == KEY_RIGHT) {

          _sms_target_menu =
            (_sms_target_menu + 1) % 3;

          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {

          _sms_target_menu =
            (_sms_target_menu + 2) % 3;

          return true;
        }

        if (c == KEY_CANCEL || c == KEY_SELECT) {

          _sms_new_stage = 0;
          return true;
        }

        if (c == KEY_ENTER) {

          // Contactos
          if (_sms_target_menu == 0) {

            _sms_target_type = 0;
            _sms_contact_menu = 0;
            _sms_recipient_valid = false;
            _sms_new_stage = 2;

            return true;
          }

          // Canais
          if (_sms_target_menu == 1) {

            _sms_target_type = 1;
            _sms_channel_menu = 0;
            _sms_channel_valid = false;
            _sms_new_stage = 6;

            return true;
          }

          // SAIR
          if (_sms_target_menu == 2) {

            _sms_new_stage = 0;
            return true;
          }
        }

        return true;
      }

      // ------------------------------------------------------
      // CONTACTOS
      // ------------------------------------------------------

      if (_sms_new_stage == 2) {

        int count = the_mesh.getNumContacts();
        int total = count + 1;

        if (c == KEY_NEXT || c == KEY_RIGHT) {

          _sms_contact_menu =
            (_sms_contact_menu + 1) % total;

          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {

          _sms_contact_menu =
            (_sms_contact_menu + total - 1) % total;

          return true;
        }

        if (c == KEY_CANCEL || c == KEY_SELECT) {

          _sms_new_stage = 1;
          return true;
        }

        if (c == KEY_ENTER) {

          // SAIR
          if (_sms_contact_menu == count) {

            _sms_new_stage = 1;
            return true;
          }

          if (count <= 0) {

            _task->showAlert(
              "Sem contactos",
              1500
            );

            return true;
          }

          if (!the_mesh.getContactByIdx(
                _sms_contact_menu + MAX_ANON_CONTACTS,
                _sms_recipient
              )) {

            _task->showAlert(
              "Contacto inválido",
              1500
            );

            return true;
          }

          _sms_recipient_valid = true;

          // Preset / localização
          if (_sms_flow_type == 1 ||
              _sms_flow_type == 2) {

            if (_sms_flow_type == 2) {
              prepareCurrentLocationMessage();
            }

            _sms_confirm = 0;
            _sms_new_stage = 5;

            return true;
          }

          // Escrever
          _sms_char_index = 0;
          _sms_new_stage = 3;

          return true;
        }

        return true;
      }

      // ------------------------------------------------------
      // EDITOR
      // ------------------------------------------------------

      if (_sms_new_stage == 3) {

        const char* charset =
          " ABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyz"
          "0123456789"
          ".,!?-_/@#()";

        int charset_len = strlen(charset);

        // 1 clique -> letra para a frente
        if (c == KEY_NEXT || c == KEY_RIGHT) {

          _sms_char_index =
            (_sms_char_index + 1) % charset_len;

          return true;
        }

        // 2 cliques -> letra para trás
        if (c == KEY_PREV || c == KEY_LEFT) {

          _sms_char_index =
            (_sms_char_index + charset_len - 1)
            % charset_len;

          return true;
        }

        // Cancelar -> voltar aos destinos
        if (c == KEY_CANCEL) {

          if (_sms_target_type == 0) {
            _sms_new_stage = 2;
          } else {
            _sms_new_stage = 6;
          }

          return true;
        }

        // 3 cliques -> abrir menu de ações
        if (c == KEY_SELECT) {

          _sms_action_menu = 0;  // APAGAR
          _sms_new_stage = 4;

          return true;
        }

        // Pressionado -> inserir letra
        if (c == KEY_ENTER) {

          int text_len = strlen(_sms_text);

          if (text_len <
              (int)sizeof(_sms_text) - 1) {

            _sms_text[text_len] =
              charset[_sms_char_index];

            _sms_text[text_len + 1] =
              '\0';
          }

          return true;
        }

        return true;
      }

      if (_sms_new_stage == 4) {

        const int ACTION_COUNT = 3;

        // 1 clique -> próxima ação
        if (c == KEY_NEXT || c == KEY_RIGHT) {

          _sms_action_menu =
            (_sms_action_menu + 1) % ACTION_COUNT;

          return true;
        }

        // 2 cliques -> ação anterior
        if (c == KEY_PREV || c == KEY_LEFT) {

          _sms_action_menu =
            (_sms_action_menu + ACTION_COUNT - 1)
            % ACTION_COUNT;

          return true;
        }

        // 3 cliques -> voltar ao editor
        if (c == KEY_SELECT) {

          _sms_new_stage = 3;
          return true;
        }

        // Pressionado -> executar ação
        if (c == KEY_ENTER) {

          // APAGAR
          if (_sms_action_menu == 0) {

            int len = strlen(_sms_text);

            if (len > 0) {
              _sms_text[len - 1] = '\0';
            }

            _sms_new_stage = 3;
            return true;
          }

          // ENVIAR
          if (_sms_action_menu == 1) {

            if (_sms_text[0] == '\0') {

              _task->showAlert(
                "Mensagem vazia",
                1500
              );

              _sms_new_stage = 3;
              return true;
            }

            _sms_confirm = 0;
            _sms_new_stage = 5;

            return true;
          }

          // SAIR
          if (_sms_action_menu == 2) {

            if (_sms_target_type == 0) {
              _sms_new_stage = 2;
            } else {
              _sms_new_stage = 6;
            }

            return true;
          }
        }

        return true;
      }

      if (_sms_new_stage == 5) {

        if (c == KEY_NEXT || c == KEY_RIGHT) {

          _sms_confirm =
            (_sms_confirm + 1) % 2;

          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {

          _sms_confirm =
            (_sms_confirm + 1) % 2;

          return true;
        }

        if (c == KEY_CANCEL ||
            c == KEY_SELECT) {

          if (_sms_flow_type == 0) {
            _sms_new_stage = 3;
          } else {
            _sms_new_stage =
              (_sms_target_type == 0) ? 2 : 6;
          }

          return true;
        }

        if (c == KEY_ENTER) {

          // SIM
          if (_sms_confirm == 0) {

            if (sendComposedMessage()) {

              _sms_new_submenu = false;
              _sms_submenu = true;
              resetNewMessageState();
            }

            return true;
          }

          // NAO
          if (_sms_confirm == 1) {

            if (_sms_flow_type == 0) {
              _sms_new_stage = 3;
            } else {
              _sms_new_stage =
                (_sms_target_type == 0) ? 2 : 6;
            }

            return true;
          }
        }

        return true;
      }

      // ------------------------------------------------------
      // CANAIS
      // ------------------------------------------------------

      if (_sms_new_stage == 6) {

        int count = 0;

#ifdef MAX_GROUP_CHANNELS
        for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {

          ChannelDetails channel;

          if (the_mesh.getChannel(i, channel) &&
              channel.name[0] != '\0') {

            count++;
          }
        }
#endif

        int total = count + 1;

        if (c == KEY_NEXT || c == KEY_RIGHT) {

          _sms_channel_menu =
            (_sms_channel_menu + 1) % total;

          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {

          _sms_channel_menu =
            (_sms_channel_menu + total - 1) % total;

          return true;
        }

        if (c == KEY_CANCEL ||
            c == KEY_SELECT) {

          _sms_new_stage = 1;
          return true;
        }

        if (c == KEY_ENTER) {

          // SAIR
          if (_sms_channel_menu == count) {

            _sms_new_stage = 1;
            return true;
          }

          int found = 0;
          bool valid = false;

#ifdef MAX_GROUP_CHANNELS
          for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {

            ChannelDetails channel;

            if (the_mesh.getChannel(i, channel) &&
                channel.name[0] != '\0') {

              if (found == _sms_channel_menu) {

                _sms_channel = channel;
                valid = true;
                break;
              }

              found++;
            }
          }
#endif

          if (!valid) {

            _task->showAlert(
              "Canal inválido",
              1500
            );

            return true;
          }

          _sms_channel_valid = true;

          // Preset / localização
          if (_sms_flow_type == 1 ||
              _sms_flow_type == 2) {

            if (_sms_flow_type == 2) {
              prepareCurrentLocationMessage();
            }

            _sms_confirm = 0;
            _sms_new_stage = 5;

            return true;
          }

          // Escrever
          _sms_char_index = 0;
          _sms_new_stage = 3;

          return true;
        }

        return true;
      }

      // ------------------------------------------------------
      // PRESETS
      // ------------------------------------------------------

      if (_sms_new_stage == 7) {

        const char* presets[] = {
          "Estou em casa",
          "Cheguei bem",
          "A caminho",
          "Preciso de ajuda",
          "Estou no trabalho",
          "Ja vou",
          "OK",
          "Sim",
          "Não",
          "[ SAIR ]"
        };

        if (c == KEY_NEXT || c == KEY_RIGHT) {

          _sms_preset_menu =
            (_sms_preset_menu + 1) % 10;

          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {

          _sms_preset_menu =
            (_sms_preset_menu + 9) % 10;

          return true;
        }

        if (c == KEY_CANCEL ||
            c == KEY_SELECT) {

          _sms_new_stage = 0;
          return true;
        }

        if (c == KEY_ENTER) {

          // SAIR
          if (_sms_preset_menu == 9) {

            _sms_new_stage = 0;
            return true;
          }

          strncpy(
            _sms_text,
            presets[_sms_preset_menu],
            sizeof(_sms_text) - 1
          );

          _sms_text[
            sizeof(_sms_text) - 1
          ] = '\0';

          _sms_target_menu = 0;
          _sms_new_stage = 1;

          return true;
        }

        return true;
      }

      return true;
    }

    // ========================================================
    // SMS MENU
    // ========================================================

    if (_page == HomePage::MESSAGES && _sms_submenu) {

      if (c == KEY_NEXT || c == KEY_RIGHT) {

        _sms_menu = (_sms_menu + 1) % 3;
        return true;
      }

      if (c == KEY_PREV || c == KEY_LEFT) {

        _sms_menu = (_sms_menu + 2) % 3;
        return true;
      }

      if (c == KEY_CANCEL || c == KEY_SELECT) {

        _sms_submenu = false;
        return true;
      }

      if (c == KEY_ENTER) {

        // Nova Mensagem
        if (_sms_menu == 0) {

          _sms_messages_submenu = false;
          _sms_new_submenu = true;
          _sms_new_menu = 0;
          resetNewMessageState();

          return true;
        }

        // Caixa de entrada
        if (_sms_menu == 1) {

          _sms_messages_submenu = true;
          _sms_messages_menu = 0;

          return true;
        }

        // SAIR
        if (_sms_menu == 2) {

          _sms_submenu = false;
          _task->gotoHomeScreen();

          return true;
        }
      }

      return true;
    }

    // ========================================================
    // SETTINGS MENU
    // ========================================================

    if (_page == HomePage::SETTINGS && _settings_submenu) {

#ifdef HELTEC_T114_WITH_DISPLAY

      // ======================================================
      // SUBMENU COR DA BARRA
      // ======================================================

      if (_settings_color_submenu) {

        if (
          c == KEY_NEXT ||
          c == KEY_RIGHT
        ) {

          _settings_color_menu =
            (
              _settings_color_menu +
              1
            ) %
            HIVEFW_HEADER_COLOR_COUNT;

          return true;
        }

        if (
          c == KEY_PREV ||
          c == KEY_LEFT
        ) {

          _settings_color_menu =
            (
              _settings_color_menu +
              HIVEFW_HEADER_COLOR_COUNT -
              1
            ) %
            HIVEFW_HEADER_COLOR_COUNT;

          return true;
        }

        if (
          c == KEY_CANCEL ||
          c == KEY_SELECT
        ) {

          _settings_color_submenu =
            false;

          _settings_color_menu =
            _node_prefs->header_color;

          return true;
        }

        if (c == KEY_ENTER) {

          if (
            _settings_color_menu >=
            HIVEFW_HEADER_COLOR_COUNT
          ) {
            _settings_color_menu = 0;
          }

          _node_prefs->header_color =
            _settings_color_menu;

          the_mesh.savePrefs();

          _task->notify(
            UIEventType::ack
          );

          char color_alert[32];

          snprintf(
            color_alert,
            sizeof(color_alert),
            "Cor: %s",
            hivefw_header_color_names[
              _settings_color_menu
            ]
          );

          _task->showAlert(
            color_alert,
            1000
          );

          _settings_color_submenu =
            false;

          return true;
        }

        return true;
      }

#endif

      // ======================================================
      // SUBMENU ANUNCIAR NÓ
      // ======================================================

      if (_settings_advert_submenu) {

        if (c == KEY_NEXT || c == KEY_RIGHT) {
          _settings_advert_menu =
            (_settings_advert_menu + 1) % 3;
          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {
          _settings_advert_menu =
            (_settings_advert_menu + 2) % 3;
          return true;
        }

        if (c == KEY_CANCEL || c == KEY_SELECT) {
          _settings_advert_submenu = false;
          _settings_advert_menu = 0;
          return true;
        }

        if (c == KEY_ENTER) {

          if (_settings_advert_menu == 0) {
            _task->notify(UIEventType::ack);

            if (the_mesh.advert(false)) {
              _task->showAlert("Advert ZeroHOP OK", 1000);
            } else {
              _task->showAlert("Advert failed..", 1000);
            }

            return true;
          }

          if (_settings_advert_menu == 1) {
            _task->notify(UIEventType::ack);

            if (the_mesh.advert(true)) {
              _task->showAlert("Advert Flood OK", 1000);
            } else {
              _task->showAlert("Advert failed..", 1000);
            }

            return true;
          }

          if (_settings_advert_menu == 2) {
            _settings_advert_submenu = false;
            _settings_advert_menu = 0;
            return true;
          }
        }

        return true;
      }

      // ======================================================
      // SUBMENU CANAL APPS
      // ======================================================

      if (_settings_channel_submenu) {

        int channel_count = getAppsChannelCount();

        if (channel_count == 0) {

          if (c == KEY_CANCEL ||
              c == KEY_SELECT ||
              c == KEY_ENTER) {

            _settings_channel_submenu = false;
            _settings_channel_menu = 0;
          }

          return true;
        }

        if (c == KEY_NEXT || c == KEY_RIGHT) {

          _settings_channel_menu =
            (_settings_channel_menu + 1)
            % channel_count;

          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {

          _settings_channel_menu =
            (_settings_channel_menu + channel_count - 1)
            % channel_count;

          return true;
        }

        if (c == KEY_CANCEL ||
            c == KEY_SELECT) {

          _settings_channel_submenu = false;
          _settings_channel_menu = 0;

          return true;
        }

        if (c == KEY_ENTER) {

          if (selectAppsChannelByMenuIndex(
                _settings_channel_menu)) {

            _settings_channel_submenu = false;
            _settings_channel_menu = 0;

            _task->notify(UIEventType::ack);
            _task->showAlert(
              "Canal APPS guardado",
              1000
            );

          } else {

            _task->showAlert(
              "Falha ao guardar",
              1000
            );
          }

          return true;
        }

        return true;
      }

      // ======================================================
      // MENU DE DEFINIÇÕES
      // ======================================================

      if (c == KEY_NEXT || c == KEY_RIGHT) {
#if ENV_INCLUDE_GPS == 1
        _settings_menu =
          (_settings_menu + 1) %
          (6 + HIVEFW_SETTINGS_COLOR_OFFSET);
#else
        _settings_menu =
          (_settings_menu + 1) %
          (5 + HIVEFW_SETTINGS_COLOR_OFFSET);
#endif
        return true;
      }

      if (c == KEY_PREV || c == KEY_LEFT) {
#if ENV_INCLUDE_GPS == 1
        _settings_menu =
          (
            _settings_menu +
            5 +
            HIVEFW_SETTINGS_COLOR_OFFSET
          ) %
          (6 + HIVEFW_SETTINGS_COLOR_OFFSET);
#else
        _settings_menu =
          (
            _settings_menu +
            4 +
            HIVEFW_SETTINGS_COLOR_OFFSET
          ) %
          (5 + HIVEFW_SETTINGS_COLOR_OFFSET);
#endif
        return true;
      }

      if (c == KEY_CANCEL || c == KEY_SELECT) {
        _settings_submenu = false;
        _settings_menu = 0;
        _settings_advert_submenu = false;
        _settings_advert_menu = 0;
        _settings_channel_submenu = false;
        _settings_channel_menu = 0;
        return true;
      }

      if (c == KEY_ENTER) {

        // BLUETOOTH
        if (_settings_menu == 0) {

          bool bluetooth_enable =
            !_task->isBluetoothEnabled();

          if (bluetooth_enable) {
            _task->enableBluetooth();
          } else {
            _task->disableBluetooth();
          }

          _task->notify(UIEventType::ack);

          _task->showAlert(
            bluetooth_enable
              ? "Bluetooth ON"
              : "Bluetooth OFF",
            1000
          );

          return true;
        }

        // ANUNCIAR NÓ
        if (_settings_menu == 1) {

          _settings_advert_submenu = true;
          _settings_advert_menu = 0;

          return true;
        }

        // CANAL APPS
        if (_settings_menu == 2) {

          int channel_count = getAppsChannelCount();

          if (channel_count == 0) {

            _settings_channel_submenu = true;
            _settings_channel_menu = 0;

          } else {

            int selected = getAppsChannelMenuIndex();

            if (selected >= channel_count)
              selected = 0;

            _settings_channel_menu = selected;
            _settings_channel_submenu = true;
          }

          return true;
        }

#ifdef HELTEC_T114_WITH_DISPLAY
        // COR DA BARRA
        if (_settings_menu == 3) {

          _settings_color_submenu =
            true;

          _settings_color_menu =
            _node_prefs->header_color;

          if (
            _settings_color_menu >=
            HIVEFW_HEADER_COLOR_COUNT
          ) {
            _settings_color_menu = 0;
          }

          return true;
        }
#endif

#if ENV_INCLUDE_GPS == 1
        // GPS
        if (
          _settings_menu ==
          3 + HIVEFW_SETTINGS_COLOR_OFFSET
        ) {

          _task->toggleGPS();

          return true;
        }

        // DESLIGAR
        if (
          _settings_menu ==
          4 + HIVEFW_SETTINGS_COLOR_OFFSET
        ) {
#else
        // DESLIGAR
        if (
          _settings_menu ==
          3 + HIVEFW_SETTINGS_COLOR_OFFSET
        ) {
#endif

          _shutdown_init = true;

          return true;
        }

#if ENV_INCLUDE_GPS == 1
        // SAIR
        if (
          _settings_menu ==
          5 + HIVEFW_SETTINGS_COLOR_OFFSET
        ) {
#else
        // SAIR
        if (
          _settings_menu ==
          4 + HIVEFW_SETTINGS_COLOR_OFFSET
        ) {
#endif

          _settings_submenu = false;
          _settings_menu = 0;
          _settings_channel_submenu = false;
          _settings_channel_menu = 0;

          return true;
        }
      }

      return true;
    }

    // ========================================================
    // APLICAÇÕES MENU
    // ========================================================

    if (_page == HomePage::APPS && _apps_submenu) {

      // Número real de opções do menu APLICAÇÕES.
      const uint8_t apps_count = APPS_MENU_COUNT;

      if (c == KEY_NEXT || c == KEY_RIGHT) {

        _apps_menu =
          (_apps_menu + 1) % apps_count;

        return true;
      }

      if (c == KEY_PREV || c == KEY_LEFT) {

        _apps_menu =
          (_apps_menu + apps_count - 1)
          % apps_count;

        return true;
      }

      if (c == KEY_CANCEL ||
          c == KEY_SELECT) {

        _apps_submenu = false;
        _apps_menu = 0;
        _apps_view = APPS_VIEW_NONE;
        _apps_return = false;

        return true;
      }

      if (c == KEY_ENTER) {

        switch (_apps_menu) {

          case APPS_HOME_ASSISTANT:
            _apps_submenu = false;
            _apps_view = APPS_VIEW_HOME_ASSISTANT;
            _apps_return = true;

            _ha_submenu = true;

            loadHACommands();

            _ha_menu = 0;
            _ha_stage = HA_STAGE_MAIN;
            _ha_manage_menu = 0;
            _ha_action_menu = 0;
            _ha_delete_confirm = 0;
            _ha_edit_index = HA_EDIT_NEW;
            _ha_char_index = 3;

            _page = HomePage::INTERNAL_HOME_ASSISTANT;
            return true;

#if ENV_INCLUDE_GPS == 1
          case APPS_GPS:
            _apps_submenu = false;
            _apps_view = APPS_VIEW_GPS;
            _apps_return = true;

            _page = HomePage::INTERNAL_GPS;
            return true;
#endif

#if UI_SENSORS_PAGE == 1
          case APPS_SENSORS:
            _apps_submenu = false;
            _apps_view = APPS_VIEW_SENSORS;
            _apps_return = true;

            _page = HomePage::INTERNAL_SENSORS;
            return true;
#endif

          case APPS_CLOCK:
            _apps_submenu = false;
            _apps_view = APPS_VIEW_CLOCK;
            _apps_return = true;

            _page = HomePage::INTERNAL_CLOCK;
            return true;

          case APPS_EXIT:
            _apps_submenu = false;
            _apps_menu = 0;
            _apps_view = APPS_VIEW_NONE;
            _apps_return = false;
            return true;

          default:
            return true;
        }
      }

      return true;
    }

    // --------------------------------------------------------
    // SOS — lógica interna APPS_VIEW_SOS
    // --------------------------------------------------------

    if (_page == HomePage::SOS &&
        _apps_view == APPS_VIEW_SOS &&
        _sos_submenu) {

      // ------------------------------------------------------
      // CONFIRMAÇÃO SOS
      // ------------------------------------------------------

      if (_sos_confirm_submenu) {

        if (c == KEY_NEXT ||
            c == KEY_RIGHT ||
            c == KEY_PREV ||
            c == KEY_LEFT) {

          _sos_menu =
            (_sos_menu + 1) % 2;

          return true;
        }

        if (c == KEY_CANCEL ||
            c == KEY_SELECT) {

          _sos_confirm_submenu = false;
          _sos_menu = 0;

          return true;
        }

        if (c == KEY_ENTER) {

          // SIM
          if (_sos_menu == 0) {

            ChannelDetails channel;

            if (!getAppsChannel(channel)) {

              _task->showAlert(
                "Canal APPS não definido",
                1500
              );

              return true;
            }

            char command[64];

#if ENV_INCLUDE_GPS == 1
            LocationProvider* nmea = sensors.getLocationProvider();

            if (nmea != NULL && nmea->isValid()) {

              snprintf(
                command,
                sizeof(command),
                "!SOS %.4f %.4f",
                nmea->getLatitude() / 1000000.0,
                nmea->getLongitude() / 1000000.0
              );

            } else {
              snprintf(
                command,
                sizeof(command),
                "!SOS SEM GPS"
              );
            }
#else
            snprintf(
              command,
              sizeof(command),
              "!SOS SEM GPS"
            );
#endif

            bool success =
              the_mesh.sendGroupMessage(
                _rtc->getCurrentTime(),
                channel.channel,
                _node_prefs->node_name,
                command,
                strlen(command)
              );

            _task->notify(UIEventType::ack);

            _task->showAlert(
              success
                ? "SOS Enviado"
                : "Falha ao enviar",
              1200
            );

            // Depois de um envio bem sucedido, regressar
            // automaticamente à página principal SOS.
            //
            // Isto corresponde ao estado obtido pelo BACK,
            // evitando o gesto manual de 3 cliques.
            if (success) {

              _sos_submenu = false;
              _sos_confirm_submenu = false;
              _sos_menu = 0;

              _apps_view = APPS_VIEW_NONE;
              _apps_menu = 0;
              _apps_return = false;

              return true;
            }
          }

          // NÃO ou envio falhado:
          // regressar apenas ao ecrã principal SOS.
          _sos_confirm_submenu = false;
          _sos_menu = 0;

          return true;
        }

        return true;
      }

      // ------------------------------------------------------
      // ECRÃ SOS
      // ------------------------------------------------------

      if (c == KEY_ENTER) {

        _sos_menu = 0;
        _sos_confirm_submenu = true;

        return true;
      }

      if (c == KEY_CANCEL ||
          c == KEY_SELECT ||
          c == KEY_LEFT ||
          c == KEY_PREV) {

        _sos_submenu = false;
        _sos_confirm_submenu = false;
        _sos_menu = 0;

        _apps_view = APPS_VIEW_NONE;
        _apps_menu = 0;
        _apps_return = false;

        return true;
      }

      return true;
    }

    // --------------------------------------------------------
    // APLICAÇÕES -> DESCOBRIR NÓS
    // --------------------------------------------------------
    // Discovery ATIVO.
    // Não usa AdvertPath e não interfere em "REPETIDORES DESCOBERTOS".
    // --------------------------------------------------------


    if (_page == HomePage::APPS &&
        !_apps_submenu &&
        _apps_view == APPS_VIEW_DISCOVERY) {

      // 1 CLICK -> próximo resultado
      if (c == KEY_NEXT ||
          c == KEY_RIGHT) {

        refreshActiveDiscoveryNodes();

        if (_active_discovery_count > 0) {

          _active_discovery_menu =
            (_active_discovery_menu + 1)
            % _active_discovery_count;
        }

        return true;
      }

      // 2 CLICKS -> voltar para APPS
      if (c == KEY_PREV ||
          c == KEY_LEFT) {

        _page = HomePage::COMPANION;
        _companion_submenu = true;
        _companion_menu = 1;

        _apps_submenu = false;
        _apps_menu = 0;
        _apps_view = APPS_VIEW_NONE;
        _apps_return = false;

        _active_discovery_menu = 0;
        _active_discovery_count = 0;
        the_mesh.clearNodeDiscoveryResults();

        return true;
      }

      // LONG PRESS
      //
      // Sem resultados:
      //   iniciar nova pesquisa Discovery.
      //
      // Com resultado selecionado:
      //   pedir o nome real ao nó.
      if (c == KEY_ENTER) {

        refreshActiveDiscoveryNodes();

        if (_active_discovery_count == 0) {

          _active_discovery_menu = 0;

          the_mesh.clearNodeDiscoveryResults();

          if (!the_mesh.sendNodeDiscoveryReq()) {

            _task->showAlert(
              "Falha ao descobrir",
              1500
            );

          } else {

            _task->showAlert(
              "Pedido Enviado",
              1200
            );
          }

        } else {

          if (!the_mesh.requestNodeDiscoveryName(
                _active_discovery_menu
              )) {

            _task->showAlert(
              "Falha ao pedir nome",
              1500
            );

          } else {

            _task->showAlert(
              "A obter nome...",
              1200
            );
          }
        }

        return true;
      }

      // 3 CLICKS / CANCEL -> sair
      if (c == KEY_SELECT ||
          c == KEY_CANCEL) {

        _page = HomePage::COMPANION;
        _companion_submenu = true;
        _companion_menu = 1;

        _apps_submenu = false;
        _apps_menu = 0;
        _apps_view = APPS_VIEW_NONE;
        _apps_return = false;

        _active_discovery_menu = 0;
        _active_discovery_count = 0;
        the_mesh.clearNodeDiscoveryResults();

        return true;
      }

      return true;
    }

    // --------------------------------------------------------
    // APLICAÇÕES -> NÓS DESCOBERTOS
    // handler discovered nodes final
    // --------------------------------------------------------

    if (_page == HomePage::APPS &&
        !_apps_submenu &&
        _apps_view == APPS_VIEW_DISCOVERED) {

      // 1 CLICK -> próximo
      if (c == KEY_NEXT ||
          c == KEY_RIGHT) {

        refreshDiscoveredNodes();

        if (_discover_count > 0) {

          _discover_menu =
            (_discover_menu + 1)
            % _discover_count;
        }

        return true;
      }

      // 2 CLICKS -> voltar
      if (c == KEY_PREV ||
          c == KEY_LEFT) {

        _page = HomePage::COMPANION;
        _companion_submenu = true;
        _companion_menu = 2;

        _apps_submenu = false;
        _apps_menu = 0;
        _apps_view = APPS_VIEW_NONE;
        _apps_return = false;

        return true;
      }

      // 3 CLICKS / CANCEL -> sair
      if (c == KEY_SELECT ||
          c == KEY_CANCEL) {

        _page = HomePage::COMPANION;
        _companion_submenu = true;
        _companion_menu = 2;

        _apps_submenu = false;
        _apps_menu = 0;
        _apps_view = APPS_VIEW_NONE;
        _apps_return = false;

        return true;
      }

      return true;
    }

    // --------------------------------------------------------
    // APLICAÇÕES -> RELÓGIO
    //
    // O render continua sendo o bloco CLOCK original.
    // --------------------------------------------------------

    if (_page == HomePage::INTERNAL_CLOCK &&
        _apps_return) {

      // Qualquer ação de saída regressa diretamente à
      // aba principal APPS.

      if (c == KEY_CANCEL ||
          c == KEY_SELECT ||
          c == KEY_ENTER) {

        _page = HomePage::APPS;

        _apps_submenu = false;
        _apps_view = APPS_VIEW_NONE;
        _apps_menu = 0;
        _apps_return = false;

        return true;
      }

      return true;
    }

    // --------------------------------------------------------
    // APLICAÇÕES -> HOME ASSISTANT
    //
    // Zero comandos pré-definidos.
    // Todos são carregados do armazenamento HiveFW.
    // --------------------------------------------------------

    if (
      _page ==
        HomePage::INTERNAL_HOME_ASSISTANT &&
      _apps_return
    ) {

      // ======================================================
      // EDITOR
      // ======================================================

      if (
        _ha_stage == HA_STAGE_EDIT_NAME ||
        _ha_stage == HA_STAGE_EDIT_COMMAND
      ) {

        const char* charset =
          haEditorCharset();

        int charset_len =
          strlen(charset);

        if (
          c == KEY_NEXT ||
          c == KEY_RIGHT
        ) {

          _ha_char_index =
            (_ha_char_index + 1)
            % charset_len;

          return true;
        }

        if (
          c == KEY_PREV ||
          c == KEY_LEFT
        ) {

          _ha_char_index =
            (
              _ha_char_index +
              charset_len -
              1
            ) % charset_len;

          return true;
        }

        if (c == KEY_CANCEL) {

          cancelHAEditor();
          return true;
        }

        // SELECT / 3 cliques:
        // terminar o campo atual.
        if (c == KEY_SELECT) {

          if (
            _ha_stage ==
            HA_STAGE_EDIT_NAME
          ) {

            trimHAField(
              _ha_edit_name
            );

            if (
              _ha_edit_name[0] ==
              '\0'
            ) {

              _task->showAlert(
                "Nome vazio",
                1200
              );

              return true;
            }

            _ha_stage =
              HA_STAGE_EDIT_COMMAND;

            _ha_char_index = 3;

            return true;
          }

          saveHAEditor();

          return true;
        }

        // ENTER / long press:
        // inserir carácter ou executar função.
        if (c == KEY_ENTER) {

          char selected =
            charset[_ha_char_index];

          char* text =
            (_ha_stage ==
             HA_STAGE_EDIT_NAME)
              ? _ha_edit_name
              : _ha_edit_command;

          size_t capacity =
            (_ha_stage ==
             HA_STAGE_EDIT_NAME)
              ? sizeof(_ha_edit_name)
              : sizeof(_ha_edit_command);

          // APAGAR
          if (selected == '\b') {

            size_t len =
              strlen(text);

            if (len > 0)
              text[len - 1] = '\0';

            return true;
          }

          // CANCELAR
          if (selected == '\x1B') {

            cancelHAEditor();
            return true;
          }

          size_t len =
            strlen(text);

          if (
            len <
            capacity - 1
          ) {

            text[len] = selected;
            text[len + 1] = '\0';

          } else {

            _task->showAlert(
              "Limite atingido",
              1000
            );
          }

          return true;
        }

        return true;
      }


      // ======================================================
      // GERIR — LISTA
      // ======================================================

      if (
        _ha_stage ==
        HA_STAGE_MANAGE_LIST
      ) {

        if (_ha_command_count == 0) {

          _ha_stage =
            HA_STAGE_MAIN;

          _ha_menu = 0;

          return true;
        }

        int total =
          _ha_command_count + 1;

        if (
          c == KEY_NEXT ||
          c == KEY_RIGHT
        ) {

          _ha_manage_menu =
            (_ha_manage_menu + 1)
            % total;

          return true;
        }

        if (
          c == KEY_PREV ||
          c == KEY_LEFT
        ) {

          _ha_manage_menu =
            (
              _ha_manage_menu +
              total -
              1
            ) % total;

          return true;
        }

        if (
          c == KEY_CANCEL ||
          c == KEY_SELECT
        ) {

          _ha_stage =
            HA_STAGE_MAIN;

          _ha_menu =
            haManageIndex();

          return true;
        }

        if (c == KEY_ENTER) {

          // [ SAIR ]
          if (
            _ha_manage_menu >=
            _ha_command_count
          ) {

            _ha_stage =
              HA_STAGE_MAIN;

            _ha_menu =
              haManageIndex();

            return true;
          }

          _ha_edit_index =
            _ha_manage_menu;

          _ha_action_menu = 0;

          _ha_stage =
            HA_STAGE_MANAGE_ACTION;

          return true;
        }

        return true;
      }


      // ======================================================
      // GERIR — ALTERAR / LOCALIZAÇÃO / APAGAR
      // ======================================================

      if (
        _ha_stage ==
        HA_STAGE_MANAGE_ACTION
      ) {

#if ENV_INCLUDE_GPS == 1
        const uint8_t action_count = 4;
#else
        const uint8_t action_count = 3;
#endif

        if (
          c == KEY_NEXT ||
          c == KEY_RIGHT
        ) {

          _ha_action_menu =
            (_ha_action_menu + 1)
            % action_count;

          return true;
        }

        if (
          c == KEY_PREV ||
          c == KEY_LEFT
        ) {

          _ha_action_menu =
            (
              _ha_action_menu +
              action_count -
              1
            ) % action_count;

          return true;
        }

        if (
          c == KEY_CANCEL ||
          c == KEY_SELECT
        ) {

          _ha_stage =
            HA_STAGE_MANAGE_LIST;

          return true;
        }

        if (c == KEY_ENTER) {

          // ALTERAR
          if (_ha_action_menu == 0) {

            beginHAEdit(
              _ha_edit_index
            );

            return true;
          }

#if ENV_INCLUDE_GPS == 1

          // LOCALIZAÇÃO
          if (_ha_action_menu == 1) {

            toggleHALocation(
              _ha_edit_index
            );

            return true;
          }

          // APAGAR
          if (_ha_action_menu == 2) {

#else

          // APAGAR
          if (_ha_action_menu == 1) {

#endif

            _ha_delete_confirm = 0;

            _ha_stage =
              HA_STAGE_DELETE_CONFIRM;

            return true;
          }

          // VOLTAR
          _ha_stage =
            HA_STAGE_MANAGE_LIST;

          return true;
        }

        return true;
      }


      // ======================================================
      // CONFIRMAÇÃO APAGAR
      // ======================================================

      if (
        _ha_stage ==
        HA_STAGE_DELETE_CONFIRM
      ) {

        if (
          c == KEY_NEXT ||
          c == KEY_RIGHT ||
          c == KEY_PREV ||
          c == KEY_LEFT
        ) {

          _ha_delete_confirm =
            (_ha_delete_confirm + 1)
            % 2;

          return true;
        }

        if (
          c == KEY_CANCEL ||
          c == KEY_SELECT
        ) {

          _ha_stage =
            HA_STAGE_MANAGE_ACTION;

          _ha_delete_confirm = 0;

          return true;
        }

        if (c == KEY_ENTER) {

          if (_ha_delete_confirm == 0) {

            deleteHACommand(
              _ha_edit_index
            );

          } else {

            _ha_stage =
              HA_STAGE_MANAGE_ACTION;
          }

          _ha_delete_confirm = 0;

          return true;
        }

        return true;
      }


      // ======================================================
      // MENU PRINCIPAL DINÂMICO
      // ======================================================

      int total =
        haMainCount();

      if (total <= 0)
        total = 2;

      if (_ha_menu >= total)
        _ha_menu = 0;

      if (
        c == KEY_NEXT ||
        c == KEY_RIGHT
      ) {

        _ha_menu =
          (_ha_menu + 1)
          % total;

        return true;
      }

      if (
        c == KEY_PREV ||
        c == KEY_LEFT
      ) {

        _ha_menu =
          (
            _ha_menu +
            total -
            1
          ) % total;

        return true;
      }

      if (
        c == KEY_CANCEL ||
        c == KEY_SELECT
      ) {

        exitHomeAssistant();
        return true;
      }

      if (c == KEY_ENTER) {

        // COMANDO PERSONALIZADO
        if (
          _ha_menu <
          _ha_command_count
        ) {

          sendHACommand(
            _ha_menu
          );

          return true;
        }

        // ADICIONAR COMANDO
        if (
          _ha_menu ==
          haAddIndex()
        ) {

          if (
            _ha_command_count >=
            HIVEFW_HA_MAX_COMMANDS
          ) {

            _task->showAlert(
              "Lista cheia",
              1500
            );

            return true;
          }

          beginHAAdd();
          return true;
        }

        // GERIR COMANDOS
        if (
          _ha_command_count > 0 &&
          _ha_menu ==
          haManageIndex()
        ) {

          _ha_manage_menu = 0;

          _ha_stage =
            HA_STAGE_MANAGE_LIST;

          return true;
        }

        // [ SAIR ]
        if (
          _ha_menu ==
          haExitIndex()
        ) {

          exitHomeAssistant();
          return true;
        }
      }

      return true;
    }

    // ========================================================
    // MP-04 — RÁDIO
    //
    // MP-03.1 — Página RÁDIO
    //   ENTER -> MENU RÁDIO
    //   CANCEL/SELECT -> MENSAGENS
    //
    // MP-04.2 — MENU RÁDIO
    //   0 -> FREQUÊNCIA
    //   1 -> BANDWIDTH
    //   2 -> SPREADING FACTOR
    //   3 -> CODING RATE
    //   4 -> TX POWER
    //   5 -> PATH
    //   6 -> RX GAIN
    //   7 -> [ SAIR ] -> MENSAGENS
    //
    // O MENU RÁDIO contém directamente a configuração.
    // Não existe INFO RÁDIO nem CONFIG RÁDIO intermédio.
    // ========================================================

    if (_page == HomePage::COMPANION) {

      // ======================================================
      // INFO COMPANION — 12 páginas
      // ======================================================

      if (_companion_info_submenu) {

        if (c == KEY_NEXT || c == KEY_RIGHT) {

          _companion_info_page =
            (_companion_info_page + 1) % COMP_INFO_COUNT;

          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {

          _companion_info_page =
            (_companion_info_page + COMP_INFO_COUNT - 1) % COMP_INFO_COUNT;

          return true;
        }

        if (c == KEY_CANCEL || c == KEY_SELECT) {

          _companion_info_submenu = false;
          _companion_info_page = 0;

          return true;
        }

        return true;
      }

      // ======================================================
      // HOME COMPANION
      // ======================================================

      if (!_companion_submenu) {

        if (c == KEY_ENTER) {

          _companion_submenu = true;
          _companion_menu = 0;

          return true;
        }

        if (c == KEY_CANCEL || c == KEY_SELECT) {

          _companion_menu = 0;
          _companion_submenu = false;
          _companion_info_submenu = false;

          _page = HomePage::MESSAGES;

          return true;
        }

        if (c == KEY_NEXT || c == KEY_RIGHT) {

          _page =
            (_page + 1)
            % HomePage::Count;

          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {

          _page =
            (_page + HomePage::Count - 1)
            % HomePage::Count;

          return true;
        }

        return true;
      }

      // ======================================================
      // MENU COMPANION
      // ======================================================

      const uint8_t companion_count = COMP_MENU_COUNT;

      if (c == KEY_NEXT || c == KEY_RIGHT) {

        _companion_menu =
          (_companion_menu + 1)
          % companion_count;

        return true;
      }

      if (c == KEY_PREV || c == KEY_LEFT) {

        _companion_menu =
          (_companion_menu + companion_count - 1) % companion_count;

        return true;
      }

      if (c == KEY_CANCEL || c == KEY_SELECT) {

        _companion_menu = 0;
        _companion_submenu = false;
        _companion_info_submenu = false;

        _page = HomePage::MESSAGES;

        return true;
      }

      if (c == KEY_ENTER) {

        // INFO COMPANION
        if (_companion_menu == COMP_MENU_INFO) {

          _companion_info_page = 0;
          _companion_info_submenu = true;

          return true;
        }

        // --------------------------------------------------
        // SINCRONIZAR RELÓGIO
        //
        // Usa a última referência enviada pelo Companion/app.
        // É também o override manual para um RTC demasiado
        // adiantado para a janela automática de -60 s.
        // --------------------------------------------------

        if (_companion_menu == COMP_MENU_SYNC_CLOCK) {

          bool synced =
            the_mesh.syncClockFromCompanionTime();

          _task->notify(
            UIEventType::ack
          );

          _task->showAlert(
            synced
              ? "Relógio sincronizado"
              : "Ligue primeiro à app",
            1500
          );

          return true;
        }


#if ENV_INCLUDE_GPS == 1

        // --------------------------------------------------
        // SINCRONIZAR VIA GPS
        //
        // Equivalente funcional ao comando oficial:
        //
        //   gps sync
        //
        // syncTime() pede ao LocationProvider para aplicar
        // hora UTC do GPS ao RTC.
        // --------------------------------------------------

        if (_companion_menu == COMP_MENU_SYNC_GPS) {

          LocationProvider* location =
            _sensors->getLocationProvider();

          if (location == NULL) {

            _task->showAlert(
              "GPS indisponível",
              1500
            );

            return true;
          }

          if (!location->isEnabled()) {

            _task->showAlert(
              "Ative o GPS primeiro",
              1500
            );

            return true;
          }

          location->syncTime();

          _task->notify(
            UIEventType::ack
          );

          _task->showAlert(
            location->isValid()
              ? "GPS sync pedido"
              : "Aguardar fix GPS",
            1500
          );

          return true;
        }

#endif


        // DESCOBRIR REPETIDORES
        if (_companion_menu == COMP_MENU_DISCOVERY) {

          _apps_submenu = false;
          _apps_view = APPS_VIEW_DISCOVERY;
          _apps_return = true;

          _active_discovery_menu = 0;

          refreshActiveDiscoveryNodes();

          _page = HomePage::APPS;

          return true;
        }

        // REPETIDORES DESCOBERTOS
        if (_companion_menu == COMP_MENU_DISCOVERED) {

          _apps_submenu = false;
          _apps_view = APPS_VIEW_DISCOVERED;
          _apps_return = true;

          _discover_menu = 0;

          refreshDiscoveredNodes();

          _page = HomePage::APPS;

          return true;
        }

        // SAIR
        if (_companion_menu == COMP_MENU_EXIT) {

          _companion_menu = 0;
          _companion_submenu = false;
          _companion_info_submenu = false;

          _page = HomePage::MESSAGES;

          return true;
        }
      }

      return true;
    }

    if (_page == HomePage::RADIO) {

      // ======================================================
      // MP-04.1 — PÁGINA RÁDIO
      //
      // Página inicial do Rádio.
      // ENTER -> MENU RÁDIO
      // CANCEL/SELECT -> MENSAGENS
      // ======================================================

      if (!_radio_submenu) {

        if (c == KEY_ENTER) {

          _radio_submenu = true;
          _radio_menu = 0;

          return true;
        }

        if (c == KEY_CANCEL || c == KEY_SELECT) {

          _radio_menu = 0;
          _radio_submenu = false;

          _page = HomePage::MESSAGES;

          return true;
        }

        if (c == KEY_NEXT || c == KEY_RIGHT) {

          _page = (_page + 1) % HomePage::Count;

          if (_page == HomePage::RECENT) {
            _task->showAlert("Anúncios Recentes", 800);
          }

          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {

          _page = (_page + HomePage::Count - 1) % HomePage::Count;

          return true;
        }

        return true;
      }

      // ======================================================
      // MP-04.2 — MENU RÁDIO
      //
      // Menu directo de configuração.
      //
      // CANCEL/SELECT -> página RÁDIO
      // [ SAIR ] -> MENSAGENS
      // ======================================================

      if (_radio_submenu) {

        // ======================================================
        // EDITOR DE FREQUÊNCIA
        // ======================================================

        if (_radio_freq_edit) {

          if (
            c == KEY_NEXT ||
            c == KEY_RIGHT
          ) {

            _radio_freq_digits[
              _radio_freq_digit
            ]++;

            if (
              _radio_freq_digits[
                _radio_freq_digit
              ] > 9
            ) {
              _radio_freq_digits[
                _radio_freq_digit
              ] = 0;
            }

            return true;
          }

          if (
            c == KEY_PREV ||
            c == KEY_LEFT
          ) {

            if (
              _radio_freq_digits[
                _radio_freq_digit
              ] == 0
            ) {

              _radio_freq_digits[
                _radio_freq_digit
              ] = 9;

            } else {

              _radio_freq_digits[
                _radio_freq_digit
              ]--;
            }

            return true;
          }

          if (
            c == KEY_ENTER ||
            c == KEY_SELECT
          ) {

            if (_radio_freq_digit < 5) {

              _radio_freq_digit++;

              return true;
            }

            int mhz =
              _radio_freq_digits[0] * 100 +
              _radio_freq_digits[1] * 10 +
              _radio_freq_digits[2];

            int khz =
              _radio_freq_digits[3] * 100 +
              _radio_freq_digits[4] * 10 +
              _radio_freq_digits[5];

            float freq =
              (float)mhz +
              ((float)khz / 1000.0f);

            if (
              freq < 150.0f ||
              freq > 999.999f
            ) {

              _task->showAlert(
                "Frequencia invalida",
                1200
              );

              return true;
            }

            _node_prefs->freq = freq;

            the_mesh.savePrefs();

            radio_driver.setParams(
              _node_prefs->freq,
              _node_prefs->bw,
              _node_prefs->sf,
              _node_prefs->cr
            );

            _radio_freq_edit = false;
            _radio_freq_digit = 0;

            _task->notify(
              UIEventType::ack
            );

            return true;
          }

          if (c == KEY_CANCEL) {

            _radio_freq_edit = false;
            _radio_freq_digit = 0;

            return true;
          }

          return true;
        }

        // ======================================================
        // EDITOR TX POWER
        //
        // O limite vem de MAX_LORA_TX_POWER.
        // Não existe 22 dBm fixo.
        // ======================================================

        if (_radio_tx_edit) {

          const int max_tx =
            MAX_LORA_TX_POWER;

          if (
            c == KEY_NEXT ||
            c == KEY_RIGHT
          ) {

            _radio_tx_digits[
              _radio_tx_digit
            ]++;

            if (
              _radio_tx_digits[
                _radio_tx_digit
              ] > 9
            ) {

              _radio_tx_digits[
                _radio_tx_digit
              ] = 0;
            }

            int value =
              _radio_tx_digits[0] * 10 +
              _radio_tx_digits[1];

            if (value > max_tx) {

              _radio_tx_digits[0] =
                max_tx / 10;

              _radio_tx_digits[1] =
                max_tx % 10;
            }

            return true;
          }

          if (
            c == KEY_PREV ||
            c == KEY_LEFT
          ) {

            if (
              _radio_tx_digits[
                _radio_tx_digit
              ] == 0
            ) {

              _radio_tx_digits[
                _radio_tx_digit
              ] = 9;

            } else {

              _radio_tx_digits[
                _radio_tx_digit
              ]--;
            }

            int value =
              _radio_tx_digits[0] * 10 +
              _radio_tx_digits[1];

            if (value > max_tx) {

              _radio_tx_digits[0] =
                max_tx / 10;

              _radio_tx_digits[1] =
                max_tx % 10;
            }

            return true;
          }

          if (
            c == KEY_ENTER ||
            c == KEY_SELECT
          ) {

            if (_radio_tx_digit == 0) {

              _radio_tx_digit = 1;

              return true;
            }

            int value =
              _radio_tx_digits[0] * 10 +
              _radio_tx_digits[1];

            if (value > max_tx) {

              _radio_tx_digits[0] =
                max_tx / 10;

              _radio_tx_digits[1] =
                max_tx % 10;

              _task->showAlert(
                "TX invalido",
                1000
              );

              return true;
            }

            _node_prefs->tx_power_dbm =
              value;

            the_mesh.savePrefs();

            radio_driver.setTxPower(
              _node_prefs->tx_power_dbm
            );

            _radio_tx_edit = false;
            _radio_tx_digit = 0;

            _task->notify(
              UIEventType::ack
            );

            return true;
          }

          if (c == KEY_CANCEL) {

            _radio_tx_edit = false;
            _radio_tx_digit = 0;

            return true;
          }

          return true;
        }

        // ======================================================
        // EDITOR DAS OPÇÕES SIMPLES
        // BW / SF / CR / PATH / RX GAIN
        // ======================================================

        if (_radio_value_edit) {

          int total = 1;

          if (_radio_menu == 1)
            total = 10;
          else if (_radio_menu == 2)
            total = 8;
          else if (_radio_menu == 3)
            total = 4;
          else if (_radio_menu == 5)
            total = 3;
          else if (_radio_menu == 6)
            total = 2;
          else if (_radio_menu == 7)
            total = 15;

          if (
            c == KEY_NEXT ||
            c == KEY_RIGHT
          ) {

            _radio_value_index =
              (_radio_value_index + 1) % total;

            return true;
          }

          if (
            c == KEY_PREV ||
            c == KEY_LEFT
          ) {

            _radio_value_index =
              (_radio_value_index + total - 1) % total;

            return true;
          }

          if (c == KEY_ENTER) {

            if (_radio_menu == 1) {

              const float bw_values[] = {
                7.8f,
                10.4f,
                15.6f,
                20.8f,
                31.25f,
                41.7f,
                62.5f,
                125.0f,
                250.0f,
                500.0f
              };

              _node_prefs->bw =
                bw_values[_radio_value_index];

              radio_driver.setParams(
                _node_prefs->freq,
                _node_prefs->bw,
                _node_prefs->sf,
                _node_prefs->cr
              );

            } else if (_radio_menu == 2) {

              _node_prefs->sf =
                5 + _radio_value_index;

              radio_driver.setParams(
                _node_prefs->freq,
                _node_prefs->bw,
                _node_prefs->sf,
                _node_prefs->cr
              );

            } else if (_radio_menu == 3) {

              _node_prefs->cr =
                5 + _radio_value_index;

              radio_driver.setParams(
                _node_prefs->freq,
                _node_prefs->bw,
                _node_prefs->sf,
                _node_prefs->cr
              );

            } else if (_radio_menu == 5) {

              _node_prefs->path_hash_mode =
                _radio_value_index;

            } else if (_radio_menu == 6) {

              _node_prefs->rx_boosted_gain =
                _radio_value_index != 0;

              radio_driver.setRxBoostedGainMode(
                _node_prefs->rx_boosted_gain
              );

            } else if (_radio_menu == 7) {

              // ==================================================
              // PRESETS MESHCORE
              //
              // 0  AUSTRALIA
              // 1  AUSTRALIA NARROW
              // 2  AUSTRALIA SA/WA/QLD
              // 3  EU/UK NARROW
              // 4  EU/UK LONG RANGE
              // 5  EU/UK MEDIUM RANGE
              // 6  CZECH REPUBLIC
              // 7  EU 433MHZ
              // 8  NEW ZEALAND
              // 9  NEW ZEALAND NARROW
              // 10 PORTUGAL 433
              // 11 PORTUGAL 868
              // 12 SWITZERLAND
              // 13 USA/CANADA
              // 14 VIETNAM
              //
              // TX POWER / PATH / RX GAIN não são alterados.
              // ==================================================

              _node_prefs->freq =
                radio_presets[_radio_value_index].freq;

              _node_prefs->bw =
                radio_presets[_radio_value_index].bw;

              _node_prefs->sf =
                radio_presets[_radio_value_index].sf;

              _node_prefs->cr =
                radio_presets[_radio_value_index].cr;

              radio_driver.setParams(
                _node_prefs->freq,
                _node_prefs->bw,
                _node_prefs->sf,
                _node_prefs->cr
              );
            }

            the_mesh.savePrefs();

            if (_radio_menu == 7) {
              _task->showAlert(
                "Preset Selecionado",
                1000
              );
            }

            _radio_value_edit = false;
            _radio_value_index = 0;

            _task->notify(
              UIEventType::ack
            );

            return true;
          }

          if (c == KEY_CANCEL) {

            _radio_value_edit = false;
            _radio_value_index = 0;

            return true;
          }

          return true;
        }

        // ======================================================
        // NAVEGAÇÃO DO MENU RÁDIO
        // ======================================================

        if (
          c == KEY_NEXT ||
          c == KEY_RIGHT
        ) {

          _radio_menu =
            (_radio_menu + 1) % 9;

          return true;
        }

        if (
          c == KEY_PREV ||
          c == KEY_LEFT
        ) {

          _radio_menu =
            (_radio_menu + 8) % 9;

          return true;
        }

        if (
          c == KEY_CANCEL ||
          c == KEY_SELECT
        ) {

          _radio_submenu = false;
          _radio_menu = 0;

          return true;
        }

        if (c == KEY_ENTER) {

          // ----------------------------------------------------
          // FREQUÊNCIA
          // ----------------------------------------------------

          if (_radio_menu == 0) {

            int freq =
              (int)_node_prefs->freq;

            int khz =
              (int)(
                (
                  _node_prefs->freq -
                  (float)freq
                ) * 1000.0f + 0.5f
              );

            if (khz >= 1000) {
              freq++;
              khz = 0;
            }

            _radio_freq_digits[0] =
              (freq / 100) % 10;

            _radio_freq_digits[1] =
              (freq / 10) % 10;

            _radio_freq_digits[2] =
              freq % 10;

            _radio_freq_digits[3] =
              (khz / 100) % 10;

            _radio_freq_digits[4] =
              (khz / 10) % 10;

            _radio_freq_digits[5] =
              khz % 10;

            _radio_freq_digit = 0;
            _radio_freq_edit = true;

            return true;
          }

          // ----------------------------------------------------
          // BW / SF / CR
          // ----------------------------------------------------

          if (
            _radio_menu == 1 ||
            _radio_menu == 2 ||
            _radio_menu == 3
          ) {

            if (_radio_menu == 1) {

              const float bw_values[] = {
                7.8f,
                10.4f,
                15.6f,
                20.8f,
                31.25f,
                41.7f,
                62.5f,
                125.0f,
                250.0f,
                500.0f
              };

              _radio_value_index = 0;

              for (int i = 0; i < 10; i++) {

                if (
                  _node_prefs->bw ==
                  bw_values[i]
                ) {
                  _radio_value_index = i;
                  break;
                }
              }

            } else if (_radio_menu == 2) {

              int sf = _node_prefs->sf;

              if (sf < 5)
                sf = 5;

              if (sf > 12)
                sf = 12;

              _radio_value_index =
                sf - 5;

            } else {

              int cr = _node_prefs->cr;

              if (cr < 5)
                cr = 5;

              if (cr > 8)
                cr = 8;

              _radio_value_index =
                cr - 5;
            }

            _radio_value_edit = true;

            return true;
          }

          // ----------------------------------------------------
          // TX POWER
          // ----------------------------------------------------

          if (_radio_menu == 4) {

            int tx =
              _node_prefs->tx_power_dbm;

            if (tx < 0)
              tx = 0;

            if (
              tx > MAX_LORA_TX_POWER
            )
              tx = MAX_LORA_TX_POWER;

            _radio_tx_digits[0] =
              (tx / 10) % 10;

            _radio_tx_digits[1] =
              tx % 10;

            _radio_tx_digit = 0;
            _radio_tx_edit = true;

            return true;
          }

          // ----------------------------------------------------
          // PATH
          // ----------------------------------------------------

          if (_radio_menu == 5) {

            _radio_value_index =
              _node_prefs->path_hash_mode % 3;

            _radio_value_edit = true;

            return true;
          }

          // ----------------------------------------------------
          // RX GAIN
          // ----------------------------------------------------

          if (_radio_menu == 6) {

            _radio_value_index =
              _node_prefs->rx_boosted_gain ? 1 : 0;

            _radio_value_edit = true;

            return true;
          }

          // ----------------------------------------------------
          // PRESETS
          // ----------------------------------------------------

          if (_radio_menu == 7) {

            _radio_value_index = 0;

            // Começa pelo preset que corresponde à configuração actual.
            int current_preset = findRadioPreset(
              _node_prefs->freq,
              _node_prefs->bw,
              _node_prefs->sf,
              _node_prefs->cr
            );

            if (current_preset >= 0) {
              _radio_value_index = current_preset;
            }

            _radio_value_edit = true;

            return true;
          }

          // ----------------------------------------------------
          // SAIR
          // ----------------------------------------------------

          if (_radio_menu == 8) {

            _radio_submenu = false;
            _radio_menu = 0;

            _page = HomePage::MESSAGES;

            return true;
          }
        }

        return true;
      }
    }

    if (_page == HomePage::REPETIDOR) {

      // ======================================================
      // MP-05.1 — PÁGINA REPETIDOR
      // ======================================================

      if (!_repeater_submenu && !_repeater_info_submenu) {

        if (c == KEY_ENTER) {
          _repeater_submenu = true;
          _repeater_info_submenu = false;
          _repeater_duty_submenu = false;
          _repeater_menu = 0;
          return true;
        }

        if (c == KEY_CANCEL || c == KEY_SELECT) {
          _repeater_menu = 0;
          _repeater_info_submenu = false;
          _repeater_duty_submenu = false;
          _repeater_submenu = false;
          _page = HomePage::RADIO;
          return true;
        }

        // Navegação das páginas principais.
        // Nesta função o return false NÃO encaminha o evento
        // para a navegação abaixo, por isso tratamos aqui.
        if (c == KEY_NEXT || c == KEY_RIGHT) {
          _page =
            (_page + 1)
            % HomePage::Count;

          if (_page == HomePage::RECENT) {
            _task->showAlert(
              "Anúncios Recentes",
              800
            );
          }

          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {
          _page =
            (_page + HomePage::Count - 1)
            % HomePage::Count;

          return true;
        }

        return true;
      }

      // ======================================================
      // MP-05.2 — MENU REPETIDOR
      // ======================================================

      // ======================================================
      // MP-05.2.1 — HANDLER DUTY CYCLE
      // ======================================================

      if (_repeater_duty_submenu) {

        if (c == KEY_NEXT ||
            c == KEY_RIGHT) {

          if (_repeater_duty_value >= 50) {
            _repeater_duty_value = 10;
          } else {
            _repeater_duty_value++;
          }

          return true;
        }

        if (c == KEY_PREV ||
            c == KEY_LEFT) {

          if (_repeater_duty_value <= 10) {
            _repeater_duty_value = 50;
          } else {
            _repeater_duty_value--;
          }

          return true;
        }

        if (c == KEY_CANCEL ||
            c == KEY_SELECT) {

          _repeater_duty_submenu = false;

          return true;
        }

        if (c == KEY_ENTER) {

          _node_prefs->airtime_factor =
            airtimeFactorFromDutyCyclePercent(
              _repeater_duty_value
            );

          the_mesh.savePrefs();

          _repeater_duty_submenu = false;

          _task->notify(
            UIEventType::ack
          );

          _task->showAlert(
            "Duty Cycle guardado",
            1000
          );

          return true;
        }

        return true;
      }

      // ======================================================
      // MP-05.2.2 — HANDLER VIZINHOS
      //
      // NEXT / RIGHT -> vizinho seguinte
      // PREV / LEFT  -> vizinho anterior
      // SELECT/CANCEL -> voltar ao menu REPETIDOR
      // ======================================================

      if (_repeater_neighbours_submenu) {

        int neighbour_count =
          the_mesh.getRepeaterNeighbourCount();

        if (c == KEY_NEXT || c == KEY_RIGHT) {

          if (neighbour_count > 0) {
            _repeater_neighbour_menu =
              (_repeater_neighbour_menu + 1)
              % neighbour_count;
          }

          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {

          if (neighbour_count > 0) {
            _repeater_neighbour_menu =
              (_repeater_neighbour_menu + neighbour_count - 1)
              % neighbour_count;
          }

          return true;
        }

        if (c == KEY_SELECT || c == KEY_CANCEL) {

          _repeater_neighbour_menu = 0;
          _repeater_neighbours_submenu = false;

          return true;
        }

        return true;
      }

      // ======================================================
      // MP-05.2 — MENU REPETIDOR
      // ======================================================

      if (_repeater_submenu &&
          !_repeater_info_submenu &&
          !_repeater_neighbours_submenu &&
          !_repeater_duty_submenu) {

        if (c == KEY_NEXT || c == KEY_RIGHT) {
          _repeater_menu =
            (_repeater_menu + 1) % REPEATER_MENU_COUNT;
          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {
          _repeater_menu =
            (_repeater_menu + REPEATER_MENU_COUNT - 1) % REPEATER_MENU_COUNT;
          return true;
        }

        if (c == KEY_CANCEL || c == KEY_SELECT) {
          _repeater_menu = 0;
          _repeater_submenu = false;
          _repeater_info_submenu = false;
          _repeater_neighbours_submenu = false;
          _repeater_duty_submenu = false;
          return true;
        }

        if (c == KEY_ENTER) {

          // ------------------------------------------------
          // 1. REPETIDOR
          // ------------------------------------------------

          if (_repeater_menu == REPEATER_MENU_TOGGLE) {

            bool repeater_enable =
              !the_mesh.getNodePrefs()->isRepeatEn();

            the_mesh.getNodePrefs()->setRepeatEn(
              repeater_enable
            );

            the_mesh.savePrefs();

            _task->notify(UIEventType::ack);

            _task->showAlert(
              repeater_enable
                ? "Repetidor: ON"
                : "Repetidor: OFF",
              1000
            );

            return true;
          }

          // ------------------------------------------------
          // 2. AUTOADVERT
          // ------------------------------------------------

          if (_repeater_menu == REPEATER_MENU_AUTOADVERT) {

            bool auto_advert =
              !the_mesh.getNodePrefs()->isAutoAdvertEn();

            the_mesh.getNodePrefs()->setAutoAdvertEn(
              auto_advert
            );

            the_mesh.savePrefs();

            _task->notify(UIEventType::ack);

            _task->showAlert(
              auto_advert
                ? "AutoAdvert: ON"
                : "AutoAdvert: OFF",
              1000
            );

            return true;
          }

          // ------------------------------------------------
          // 3. DUTY CYCLE
          // ------------------------------------------------

          if (_repeater_menu == REPEATER_MENU_DUTY_CYCLE) {

            _repeater_duty_value =
              dutyCyclePercentFromAirtimeFactor(
                _node_prefs->airtime_factor
              );

            _repeater_duty_submenu = true;

            return true;
          }

          // ------------------------------------------------
          // 4. VIZINHOS
          // ------------------------------------------------

          if (_repeater_menu == REPEATER_MENU_NEIGHBOURS) {

            _repeater_neighbour_menu = 0;
            _repeater_neighbours_submenu = true;

            return true;
          }

          // ------------------------------------------------
          // 5. INFO REPETIDOR
          // ------------------------------------------------

          if (_repeater_menu == REPEATER_MENU_INFO) {

            _repeater_info_submenu = true;
            _repeater_stats_page = 0;

            return true;
          }

          // ------------------------------------------------
          // 6. SAIR
          // ------------------------------------------------

          if (_repeater_menu == REPEATER_MENU_EXIT) {

            _repeater_menu = 0;
            _repeater_info_submenu = false;
            _repeater_duty_submenu = false;
            _repeater_submenu = false;
            _page = HomePage::RADIO;

            return true;
          }
        }

        return true;
      }

      // ======================================================
      // MP-05.3 — SELECTOR DE ESTATÍSTICAS
      // ======================================================

      if (_repeater_info_submenu) {

        if (c == KEY_NEXT || c == KEY_RIGHT) {

          _repeater_stats_page =
            (_repeater_stats_page + 1) % REPEATER_INFO_PAGE_COUNT;

          return true;
        }

        if (c == KEY_PREV || c == KEY_LEFT) {

          _repeater_stats_page =
            (_repeater_stats_page + REPEATER_INFO_PAGE_COUNT - 1) % REPEATER_INFO_PAGE_COUNT;

          return true;
        }

        if (c == KEY_ENTER && _repeater_stats_page == REPEATER_INFO_PAGE_COUNT - 1) {

          bool share_location =
            the_mesh.getNodePrefs()->advert_loc_policy == ADVERT_LOC_SHARE;

          share_location = !share_location;

          the_mesh.getNodePrefs()->advert_loc_policy =
            share_location
              ? ADVERT_LOC_SHARE
              : ADVERT_LOC_NONE;

          the_mesh.savePrefs();

          _task->notify(UIEventType::ack);

          _task->showAlert(
            share_location
              ? "Localizacao ativa"
              : "Localizacao oculta",
            1200
          );

          return true;
        }

        if (c == KEY_CANCEL || c == KEY_SELECT) {

          _repeater_stats_page = 0;
          _repeater_info_submenu = false;
          _repeater_submenu = true;

          return true;
        }

        return true;
      }

      return true;
    }

#if ENV_INCLUDE_GPS == 1
    if (_page == HomePage::INTERNAL_GPS && _apps_return) {

      if (c == KEY_PREV || c == KEY_LEFT ||
          c == KEY_CANCEL || c == KEY_SELECT) {

        _page = HomePage::APPS;
        _apps_submenu = true;
        _apps_menu = 0;
        _apps_view = APPS_VIEW_NONE;
        _apps_return = false;

        return true;
      }

      if (c == KEY_ENTER) {
        _task->toggleGPS();
        return true;
      }

      return true;
    }
#endif

#if UI_SENSORS_PAGE == 1
    if (_page == HomePage::INTERNAL_SENSORS && _apps_return) {

      if (c == KEY_PREV || c == KEY_LEFT ||
          c == KEY_CANCEL || c == KEY_SELECT) {

        _page = HomePage::APPS;
        _apps_submenu = true;
        _apps_menu = 0;
        _apps_view = APPS_VIEW_NONE;
        _apps_return = false;

        return true;
      }

      if (c == KEY_ENTER) {
        next_sensors_refresh = 0;
        return true;
      }

      return true;
    }
#endif

    // ========================================================
    // NORMAL PAGE NAVIGATION
    // ========================================================

    if (c == KEY_LEFT || c == KEY_PREV) {

      _page =
        (_page + HomePage::Count - 1)
        % HomePage::Count;

      return true;
    }

    if (c == KEY_NEXT || c == KEY_RIGHT) {

      _page =
        (_page + 1)
        % HomePage::Count;

      if (_page == HomePage::RECENT) {
        _task->showAlert(
          "Anúncios Recentes",
          800
        );
      }

      return true;
    }

    // ========================================================
    // ENTER SMS
    // ========================================================

    if (c == KEY_ENTER && _page == HomePage::MESSAGES) {
      _sms_submenu = true;
      _sms_menu = 0;
      return true;
    }

    // ========================================================
    // ENTER HOME ASSISTANT
    // ========================================================

    if (
      c == KEY_ENTER &&
      _page ==
        HomePage::INTERNAL_HOME_ASSISTANT
    ) {

      _ha_submenu = true;

      loadHACommands();

      _ha_menu = 0;
      _ha_stage = HA_STAGE_MAIN;

      return true;
    }

    // ========================================================
    // ========================================================
    // ENTER SOS
    // ========================================================

    if (c == KEY_ENTER && _page == HomePage::SOS) {

      // Mantemos a infraestrutura SOS existente.
      _apps_submenu = false;
      _apps_menu = 0;
      _apps_view = APPS_VIEW_SOS;
      _apps_return = true;

      _sos_submenu = true;
      _sos_confirm_submenu = true;
      _sos_menu = 0;

      return true;
    }

    // ========================================================
    // ENTER APLICAÇÕES
    // ========================================================

    if (c == KEY_ENTER && _page == HomePage::APPS) {

      _apps_submenu = true;
      _apps_menu = 0;
      _apps_view = APPS_VIEW_NONE;
      _apps_return = true;

      return true;
    }

    // ENTER SETTINGS
    // ========================================================

    if (c == KEY_ENTER && _page == HomePage::SETTINGS) {
      _settings_submenu = true;
      _settings_menu = 0;
      _settings_advert_submenu = false;
      _settings_advert_menu = 0;
      return true;
    }





    return false;
  }};

class MsgPreviewScreen : public UIScreen {
  UITask* _task;
  mesh::RTCClock* _rtc;

  struct MsgEntry {
    uint32_t timestamp;
    uint8_t channel_hash[PATH_HASH_SIZE];
    char origin[62];
    char msg[78];
  };

  #define MAX_UNREAD_MSGS 32

  // ========================================================
  // Fila de mensagens / histórico
  // ========================================================

  MsgEntry unread[MAX_UNREAD_MSGS];

  int num_unread;

  // Índice da mensagem mais recente para notificações
  int unread_head;

  // Índice da mensagem mais recente do histórico
  int history_head;

  // Número de mensagens válidas armazenadas
  int history_count;

  // ========================================================
  // Estado do histórico por canal
  // ========================================================

  bool _history_mode;

  uint8_t _history_channel_hash[PATH_HASH_SIZE];

  char _history_channel_name[32];

  // 0 = mais recente
  int _history_position;

public:

  MsgPreviewScreen(
    UITask* task,
    mesh::RTCClock* rtc
  )
    : _task(task),
      _rtc(rtc),
      num_unread(0),
      unread_head(MAX_UNREAD_MSGS - 1),
      history_head(MAX_UNREAD_MSGS - 1),
      history_count(0),
      _history_mode(false),
      _history_position(0) {

    memset(
      _history_channel_hash,
      0,
      sizeof(_history_channel_hash)
    );

    memset(
      _history_channel_name,
      0,
      sizeof(_history_channel_name)
    );
  }

  // ========================================================
  // Receber e armazenar uma nova mensagem
  // ========================================================

  void addPreview(
    uint8_t path_len,
    const uint8_t* channel_hash,
    const char* from_name,
    const char* msg
  ) {

    // Avança o armazenamento circular
    history_head =
      (history_head + 1) % MAX_UNREAD_MSGS;

    // As notificações começam também pela mensagem mais recente
    unread_head = history_head;

    if (history_count < MAX_UNREAD_MSGS) {
      history_count++;
    }

    if (num_unread < MAX_UNREAD_MSGS) {
      num_unread++;
    }

    MsgEntry* p = &unread[history_head];

    p->timestamp =
      _rtc->getCurrentTime();

    if (channel_hash != NULL) {

      memcpy(
        p->channel_hash,
        channel_hash,
        PATH_HASH_SIZE
      );

    } else {

      memset(
        p->channel_hash,
        0,
        PATH_HASH_SIZE
      );
    }

    if (path_len == 0xFF) {

      sprintf(
        p->origin,
        "(D) %s:",
        from_name
      );

    } else {

      sprintf(
        p->origin,
        "(%d) %s:",
        (uint32_t) path_len,
        from_name
      );
    }

    StrHelper::strncpy(
      p->msg,
      msg,
      sizeof(p->msg)
    );
  }

  // ========================================================
  // Abrir histórico de um canal
  // ========================================================

  void openChannel(
    const uint8_t* channel_hash,
    const char* channel_name
  ) {

    _history_mode = true;

    _history_position = 0;

    memcpy(
      _history_channel_hash,
      channel_hash,
      PATH_HASH_SIZE
    );

    StrHelper::strncpy(
      _history_channel_name,
      channel_name,
      sizeof(_history_channel_name)
    );
  }

  // ========================================================
  // Contar mensagens pertencentes ao canal
  // ========================================================

  int countChannelMessages() {

    int count = 0;

    for (int n = 0; n < history_count; n++) {

      int idx =
        (history_head - n + MAX_UNREAD_MSGS)
        % MAX_UNREAD_MSGS;

      if (memcmp(
            unread[idx].channel_hash,
            _history_channel_hash,
            PATH_HASH_SIZE
          ) == 0) {

        count++;
      }
    }

    return count;
  }

  // ========================================================
  // Obter mensagem N do canal
  //
  // position 0 = mais recente
  // ========================================================

  MsgEntry* getChannelMessage(int position) {

    int found = 0;

    for (int n = 0; n < history_count; n++) {

      int idx =
        (history_head - n + MAX_UNREAD_MSGS)
        % MAX_UNREAD_MSGS;

      if (memcmp(
            unread[idx].channel_hash,
            _history_channel_hash,
            PATH_HASH_SIZE
          ) == 0) {

        if (found == position) {
          return &unread[idx];
        }

        found++;
      }
    }

    return NULL;
  }

  // ========================================================
  // RENDER
  // ========================================================

  int render(DisplayDriver& display) override {

    // ======================================================
    // HISTÓRICO DE CANAL
    // ======================================================

    if (_history_mode) {

      int total = countChannelMessages();

      // Nome do canal
      display.setColor(UIColor::corp_blue);

      display.drawTextCentered(
        display.width() / 2,
        0,
        _history_channel_name
      );

      display.drawRect(
        0,
        11,
        display.width(),
        1
      );

      if (total == 0) {

        display.setColor(
          UIColor::secondary_txt
        );

        display.drawTextCentered(
          display.width() / 2,
          30,
          "Sem mensagens"
        );

      } else {

        if (_history_position >= total) {
          _history_position = total - 1;
        }

        MsgEntry* p =
          getChannelMessage(_history_position);

        if (p != NULL) {

          // Remetente
          display.setCursor(0, 14);

          display.setColor(
            UIColor::secondary_txt
          );

          char filtered_origin[
            sizeof(p->origin)
          ];

          display.translateUTF8ToBlocks(
            filtered_origin,
            p->origin,
            sizeof(filtered_origin)
          );

          display.print(filtered_origin);

          // Mensagem
          display.setCursor(0, 25);

          display.setColor(
            UIColor::primary_txt
          );

          char filtered_msg[
            sizeof(p->msg)
          ];

          display.translateUTF8ToBlocks(
            filtered_msg,
            p->msg,
            sizeof(filtered_msg)
          );

          display.printWordWrap(
            filtered_msg,
            display.width()
          );

          // Contador
          char counter[20];

          sprintf(
            counter,
            "%d/%d",
            _history_position + 1,
            total
          );

          display.setColor(
            UIColor::secondary_txt
          );

          display.drawTextCentered(
            display.width() / 2,
            54,
            counter
          );
        }
      }

#if AUTO_OFF_MILLIS==0
      return 10000;
#else
      return 1000;
#endif
    }

    // ======================================================
    // NOTIFICAÇÃO NORMAL
    // ======================================================

    char tmp[16];

    display.setCursor(0, 0);
    display.setTextSize(1);

    display.setColor(
      UIColor::corp_blue
    );

    sprintf(
      tmp,
      "Unread: %d",
      num_unread
    );

    display.print(tmp);

    if (num_unread > 0) {

      MsgEntry* p =
        &unread[unread_head];

      int secs =
        _rtc->getCurrentTime() -
        p->timestamp;

      if (secs < 60) {

        sprintf(
          tmp,
          "%ds",
          secs
        );

      } else if (secs < 60 * 60) {

        sprintf(
          tmp,
          "%dm",
          secs / 60
        );

      } else {

        sprintf(
          tmp,
          "%dh",
          secs / (60 * 60)
        );
      }

      display.setCursor(
        display.width() -
        display.getTextWidth(tmp) -
        2,
        0
      );

      display.print(tmp);

      display.drawRect(
        0,
        11,
        display.width(),
        1
      );

      display.setCursor(0, 14);

      display.setColor(
        UIColor::secondary_txt
      );

      char filtered_origin[
        sizeof(p->origin)
      ];

      display.translateUTF8ToBlocks(
        filtered_origin,
        p->origin,
        sizeof(filtered_origin)
      );

      display.print(filtered_origin);

      display.setCursor(0, 25);

      display.setColor(
        UIColor::primary_txt
      );

      char filtered_msg[
        sizeof(p->msg)
      ];

      display.translateUTF8ToBlocks(
        filtered_msg,
        p->msg,
        sizeof(filtered_msg)
      );

      display.printWordWrap(
        filtered_msg,
        display.width()
      );
    }

#if AUTO_OFF_MILLIS==0
    return 10000;
#else
    return 1000;
#endif
  }

  // ========================================================
  // INPUT
  // ========================================================

  bool handleInput(char c) override {

    // ======================================================
    // HISTÓRICO
    // ======================================================

    if (_history_mode) {

      int total =
        countChannelMessages();

      // Próxima mensagem
      if (c == KEY_NEXT || c == KEY_RIGHT) {

        if (total > 0) {

          _history_position =
            (_history_position + 1) % total;
        }

        return true;
      }

      // Mensagem anterior
      if (c == KEY_PREV || c == KEY_LEFT) {

        if (total > 0) {

          _history_position =
            (_history_position + total - 1)
            % total;
        }

        return true;
      }

      // Sair do histórico
      if (c == KEY_ENTER ||
          c == KEY_CANCEL) {

        _history_mode = false;

        _task->gotoHomeScreen();

        return true;
      }

      return true;
    }

    // ======================================================
    // NOTIFICAÇÕES
    // ======================================================

    if (c == KEY_NEXT || c == KEY_RIGHT) {

      if (num_unread > 0) {

        unread_head =
          (unread_head + MAX_UNREAD_MSGS - 1)
          % MAX_UNREAD_MSGS;

        num_unread--;

        if (num_unread == 0) {
          _task->gotoHomeScreen();
        }
      }

      return true;
    }

    if (c == KEY_ENTER) {

      num_unread = 0;

      _task->gotoHomeScreen();

      return true;
    }

    return false;
  }
};


void UITask::gotoChannelMessages(uint8_t channel_index) {

  ChannelDetails channel;

  if (!the_mesh.getChannel(
        channel_index,
        channel
      )) {

    return;
  }

  ((MsgPreviewScreen*) channel_messages)->openChannel(
    channel.channel.hash,
    channel.name
  );

  setCurrScreen(channel_messages);
}


void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _auto_off = millis() + AUTO_OFF_MILLIS;

#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif
#if defined(PIN_USER_BTN_ANA)
  analog_btn.begin();
#endif

  _node_prefs = node_prefs;

  if (_display != NULL) {
    _display->turnOn();
  }

#ifdef PIN_BUZZER
  buzzer.begin();
  buzzer.quiet(_node_prefs->buzzer_quiet);
  buzzer.startup();
#endif

#ifdef PIN_VIBRATION
  vibration.begin();
#endif

  ui_started_at = millis();
  _alert_expiry = 0;

  splash = new SplashScreen(this);
  home = new HomeScreen(this, &rtc_clock, sensors, node_prefs);
  msg_preview = new MsgPreviewScreen(this, &rtc_clock);
  channel_messages = msg_preview;
  setCurrScreen(splash);
}

void UITask::showAlert(const char* text, int duration_millis) {
  strcpy(_alert, text);
  _alert_expiry = millis() + duration_millis;
}

void UITask::notify(UIEventType t) {
#if defined(PIN_BUZZER)
switch(t){
  case UIEventType::contactMessage:
    // gemini's pick
    buzzer.play("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
    break;
  case UIEventType::channelMessage:
    buzzer.play("kerplop:d=16,o=6,b=120:32g#,32c#");
    break;
  case UIEventType::ack:
    buzzer.play("ack:d=32,o=8,b=120:c");
    break;
  case UIEventType::roomMessage:
  case UIEventType::newContactMessage:
  case UIEventType::none:
  default:
    break;
}
#endif

#ifdef PIN_VIBRATION
  // Trigger vibration for all UI events except none
  if (t != UIEventType::none) {
    vibration.trigger();
  }
#endif
}


void UITask::msgRead(int msgcount) {
  _msgcount = msgcount;
  if (msgcount == 0) {
    gotoHomeScreen();
  }
}

void UITask::newMsg(uint8_t path_len, const uint8_t* channel_hash, const char* from_name, const char* text, int msgcount) {
  _msgcount = msgcount;

  if (channel_hash != NULL && !hasConnection()) {
    HomeScreen* home_ptr = (HomeScreen*) home;
    home_ptr->addUnreadChannelMessage(channel_hash);
  }

  ((MsgPreviewScreen *) msg_preview)->addPreview(path_len, channel_hash, from_name, text);
  setCurrScreen(msg_preview);

  if (_display != NULL) {
    if (!_display->isOn() && !hasConnection()) {
      _display->turnOn();
    }
    if (_display->isOn()) {
    _auto_off = millis() + AUTO_OFF_MILLIS;  // extend the auto-off timer
    _next_refresh = 100;  // trigger refresh
    }
  }
}

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  int cur_time = millis();
  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;
      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }
      next_led_change = cur_time + last_led_increment;
    } else {
      led_state = 0;
      next_led_change = cur_time + LED_CYCLE_MILLIS - last_led_increment;
    }
    digitalWrite(PIN_STATUS_LED, led_state == LED_STATE_ON);
  }
#endif
}

void UITask::setCurrScreen(UIScreen* c) {
  curr = c;
  _next_refresh = 100;
}

/*
  hardware-agnostic pre-shutdown activity should be done here
*/
void UITask::shutdown(bool restart){

  #ifdef PIN_BUZZER
  /* note: we have a choice here -
     we can do a blocking buzzer.loop() with non-deterministic consequences
     or we can set a flag and delay the shutdown for a couple of seconds
     while a non-blocking buzzer.loop() plays out in UITask::loop()
  */
  buzzer.shutdown();
  uint32_t buzzer_timer = millis(); // fail-safe shutdown
  while (buzzer.isPlaying() && (millis() - 2500) < buzzer_timer)
    buzzer.loop();

  #endif // PIN_BUZZER

  if (restart) {
    _board->reboot();
  } else {
    // Power off board including radio, display, GPS and components
    _board->powerOff();
  }
}

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  return user_btn.isPressed();
#else
  return false;
#endif
}

void UITask::loop() {
  char c = 0;
#if UI_HAS_JOYSTICK
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);  // REVISIT: could be mapped to different key code
  }
  ev = joystick_left.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_LEFT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_LEFT);
  }
  ev = joystick_right.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_RIGHT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_RIGHT);
  }
  ev = back_btn.check();
  if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#elif defined(PIN_USER_BTN)
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_NEXT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_PREV);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#endif
#if defined(UI_HAS_ROTARY_INPUT)
  RotaryInputEvent rotaryEv = rotary_input.poll();
  if (c == 0 && _display != NULL && _display->isOn()) {
    if (rotaryEv == RotaryInputEvent::Next) {
      c = KEY_NEXT;
    } else if (rotaryEv == RotaryInputEvent::Prev) {
      c = KEY_PREV;
    }
  }
#endif
#if defined(PIN_USER_BTN_ANA)
  if (abs(millis() - _analogue_pin_read_millis) > 10) {
    int ev = analog_btn.check();
    if (ev == BUTTON_EVENT_CLICK) {
      c = checkDisplayOn(KEY_NEXT);
    } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      c = handleLongPress(KEY_ENTER);
    } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
      c = handleDoubleClick(KEY_PREV);
    } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
      c = handleTripleClick(KEY_SELECT);
    }
    _analogue_pin_read_millis = millis();
  }
#endif
#if defined(BACKLIGHT_BTN)
  if (millis() > next_backlight_btn_check) {
    bool touch_state = digitalRead(PIN_BUTTON2);
#if defined(DISP_BACKLIGHT)
    digitalWrite(DISP_BACKLIGHT, !touch_state);
#elif defined(EXP_PIN_BACKLIGHT)
    expander.digitalWrite(EXP_PIN_BACKLIGHT, !touch_state);
#endif
    next_backlight_btn_check = millis() + 300;
  }
#endif

  if (c != 0 && curr) {
    curr->handleInput(c);
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 100;  // trigger refresh
  }

  userLedHandler();

#ifdef PIN_BUZZER
  if (buzzer.isPlaying())  buzzer.loop();
#endif

  if (curr) curr->poll();

  if (_display != NULL && _display->isOn()) {
    if (millis() >= _next_refresh && curr) {
      _display->startFrame();
      int delay_millis = curr->render(*_display);
      if (millis() < _alert_expiry) {  // render alert popup
        _display->setTextSize(1);
        int y = _display->height() / 3;
        int p = _display->height() / 32;
        _display->setColor(UIColor::popup_bkg);
        _display->fillRect(p, y, _display->width() - p*2, y);
        _display->setColor(UIColor::popup_txt);  // draw box border
        _display->drawRect(p, y, _display->width() - p*2, y);
        _display->drawTextCentered(_display->width() / 2, y + p*3, _alert);
        _next_refresh = _alert_expiry;   // will need refresh when alert is dismissed
      } else {
        _next_refresh = millis() + delay_millis;
      }
      _display->endFrame();
    }
#if AUTO_OFF_MILLIS > 0
#ifdef KEEP_DISPLAY_ON_USB
    // Opt-in: refresh the auto-off deadline while externally powered, so the
    // timer counts from the moment external power is removed. Off by default
    // because OLED panels burn in quickly; only enable for LCD targets or
    // where the display is replaceable.
    if (board.isExternalPowered()) {
      _auto_off = millis() + AUTO_OFF_MILLIS;
    }
#endif
    if (millis() > _auto_off) {
      _display->turnOff();
    }
#endif
  }

#ifdef PIN_VIBRATION
  vibration.loop();
#endif

#ifdef AUTO_SHUTDOWN_MILLIVOLTS
  if (millis() > next_batt_chck) {
    uint16_t milliVolts = getBattMilliVolts();
    if (milliVolts > 0 && milliVolts < AUTO_SHUTDOWN_MILLIVOLTS) {
      if(!board.isExternalPowered()) {
        if (_display != NULL) {
          _display->startFrame();
          _display->setTextSize(2);
          _display->setColor(UIColor::warning_txt);
          _display->drawTextCentered(_display->width() / 2, 20, "Low Battery.");
          _display->drawTextCentered(_display->width() / 2, 40, "Shutting Down!");
          _display->endFrame();
          if (_display->isEink() == false) { delay(3000); }
        }
        shutdown();
      }
    }
    next_batt_chck = millis() + 8000;
  }
#endif
}

char UITask::checkDisplayOn(char c) {
  if (_display != NULL) {
    if (!_display->isOn()) {
      _display->turnOn();   // turn display on and consume event
      c = 0;
    }
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 0;  // trigger refresh
  }
  return c;
}

char UITask::handleLongPress(char c) {
  if (millis() - ui_started_at < 8000) {   // long press in first 8 seconds since startup -> CLI/rescue
    the_mesh.enterCLIRescue();
    c = 0;   // consume event
  }
  return c;
}

char UITask::handleDoubleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: double-click triggered");
  checkDisplayOn(c);
  return c;
}

char UITask::handleTripleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: triple click triggered");
  checkDisplayOn(c);
  toggleBuzzer();
  return c;
}

bool UITask::getGPSState() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        return !strcmp(_sensors->getSettingValue(i), "1");
      }
    }
  }
  return false;
}

void UITask::toggleGPS() {
    if (_sensors != NULL) {
    // toggle GPS on/off
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        if (strcmp(_sensors->getSettingValue(i), "1") == 0) {
          _sensors->setSettingValue("gps", "0");
          _node_prefs->gps_enabled = 0;
          notify(UIEventType::ack);
        } else {
          _sensors->setSettingValue("gps", "1");
          _node_prefs->gps_enabled = 1;
          notify(UIEventType::ack);
        }
        the_mesh.savePrefs();
        showAlert(_node_prefs->gps_enabled ? "GPS: Enabled" : "GPS: Disabled", 800);
        _next_refresh = 0;
        break;
      }
    }
  }
}

void UITask::toggleBuzzer() {
    // Toggle buzzer quiet mode
  #ifdef PIN_BUZZER
    if (buzzer.isQuiet()) {
      buzzer.quiet(false);
      notify(UIEventType::ack);
    } else {
      buzzer.quiet(true);
    }
    _node_prefs->buzzer_quiet = buzzer.isQuiet();
    the_mesh.savePrefs();
    showAlert(buzzer.isQuiet() ? "Buzzer: OFF" : "Buzzer: ON", 800);
    _next_refresh = 0;  // trigger refresh
  #endif
}
