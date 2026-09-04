#include "UITask.h"
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
    const char* firmware_name = "Companion&Repeater";
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

    // FIRMWARE_BUILD_DATE pode vir como:
    // "Sep 04 2026" ou "Set 04 2026"
    sscanf(
      FIRMWARE_BUILD_DATE,
      "%3s %d %d",
      build_month,
      &build_day,
      &build_year
    );

    // Normalizar abreviatura do mês para português.
    if (strcmp(build_month, "Jan") == 0)
      strcpy(build_month, "JAN");
    else if (strcmp(build_month, "Feb") == 0)
      strcpy(build_month, "FEV");
    else if (strcmp(build_month, "Mar") == 0)
      strcpy(build_month, "MAR");
    else if (strcmp(build_month, "Apr") == 0)
      strcpy(build_month, "ABR");
    else if (strcmp(build_month, "May") == 0)
      strcpy(build_month, "MAI");
    else if (strcmp(build_month, "Jun") == 0)
      strcpy(build_month, "JUN");
    else if (strcmp(build_month, "Jul") == 0)
      strcpy(build_month, "JUL");
    else if (strcmp(build_month, "Aug") == 0)
      strcpy(build_month, "AGO");
    else if (strcmp(build_month, "Sep") == 0)
      strcpy(build_month, "SET");
    else if (strcmp(build_month, "Oct") == 0)
      strcpy(build_month, "OUT");
    else if (strcmp(build_month, "Nov") == 0)
      strcpy(build_month, "NOV");
    else if (strcmp(build_month, "Dec") == 0)
      strcpy(build_month, "DEZ");

    snprintf(
      build_date,
      sizeof(build_date),
      "%02d %s %04d",
      build_day,
      build_month,
      build_year
    );

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

class HomeScreen : public UIScreen {
  enum HomePage {
    FIRST,
    RECENT,
    MESSAGES,
    RADIO,
    SETTINGS,
    APPS,
    Count,    // keep as last

    // Estados internos das APPS.
    INTERNAL_HOME_ASSISTANT,
    INTERNAL_CLOCK,
    INTERNAL_GPS,
    INTERNAL_SENSORS
  };

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  uint8_t _page;
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

  uint8_t _settings_advert_menu;
  bool _settings_advert_submenu;
  bool _settings_confirm;
  uint8_t _settings_confirm_menu;
  uint8_t _ha_menu;
  bool _ha_submenu;
  uint8_t _ha_confirm;
  bool _ha_confirm_submenu;

  // APLICAÇÕES
  // 0 = menu
  // 1 = relógio
  // 2 = Home Assistant
  // 3 = GPS
  // 4 = Sensores
  // 5 = Descobrir Repetidores
  // 6 = Repetidores Descobertos
  uint8_t _apps_menu;
  uint8_t _apps_view;
  bool _apps_submenu;
  bool _apps_return;

  // Discovery ativo — INDEPENDENTE de AdvertPath.
  static const uint8_t ACTIVE_DISCOVERY_MAX_NODES = 8;
  NodeDiscoveryResult _active_discovery_nodes[ACTIVE_DISCOVERY_MAX_NODES];
  uint8_t _active_discovery_count;
  uint8_t _active_discovery_menu;

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


  void renderBatteryIndicator(DisplayDriver& display, uint16_t batteryMilliVolts) {
#ifndef BATT_MIN_MILLIVOLTS
#define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
#define BATT_MAX_MILLIVOLTS 4200
#endif

    const int minMilliVolts = BATT_MIN_MILLIVOLTS;
    const int maxMilliVolts = BATT_MAX_MILLIVOLTS;

    int batteryPercentage =
      ((batteryMilliVolts - minMilliVolts) * 100) /
      (maxMilliVolts - minMilliVolts);

    if (batteryPercentage < 0) batteryPercentage = 0;
    if (batteryPercentage > 100) batteryPercentage = 100;

    char batteryText[24];
    snprintf(
      batteryText,
      sizeof(batteryText),
      "%d%% %.2fV",
      batteryPercentage,
      batteryMilliVolts / 1000.0f
    );

    display.setColor(UIColor::secondary_txt);
    display.setTextSize(1);
    display.drawTextCentered(display.width() - 24, 1, batteryText);
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
      _settings_advert_menu(0), _settings_advert_submenu(false),
      _settings_confirm(false), _settings_confirm_menu(0),
      _ha_menu(0), _ha_submenu(false),
      _ha_confirm(0), _ha_confirm_submenu(false),
       _apps_menu(0), _apps_view(0), _apps_submenu(false), _apps_return(false),
       _active_discovery_count(0), _active_discovery_menu(0),
       _discover_count(0), _discover_menu(0),
       _shutdown_init(false), sensors_lpp(200) {
    _sms_text[0] = '\0';
    memset(&_sms_recipient, 0, sizeof(_sms_recipient));

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

  void drawSelectedMenuText(
    DisplayDriver& display,
    int centerX,
    int y,
    const char* text
  ) {
    // Simula negrito desenhando o texto duas vezes,
    // com deslocamento horizontal de 1 pixel.
    display.drawTextCentered(centerX, y, text);
    display.drawTextCentered(centerX + 1, y, text);
  }

  void renderSectionHome(
    DisplayDriver& display,
    const uint8_t* icon,
    uint8_t icon_width,
    uint8_t icon_height,
    const char* title
  ) {

    const int centerX = display.width() / 2;
    const int iconY = 15;
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

    int textCenterX = centerX;

    if (strcmp(title, "DEFINIÇÕES") == 0 ||
        strcmp(title, "APLICAÇÕES") == 0) {
      textCenterX += 7;
    }

    display.drawTextCentered(
      textCenterX,
      titleY,
      title
    );
  }

  int render(DisplayDriver& display) override {
    display.setColor(UIColor::title_bkg);
    display.fillRect(0, 0, display.width(), 12);
    char tmp[80];
    // node name
    display.setTextSize(1);
    display.setColor(UIColor::title_txt);
    char filtered_name[sizeof(_node_prefs->node_name)];
    display.translateUTF8ToBlocks(filtered_name, _node_prefs->node_name, sizeof(filtered_name));
    display.setCursor(0, 2);
    display.print(filtered_name);

    // battery voltage
    renderBatteryIndicator(display, _task->getBattMilliVolts());

    // curr page indicator
    if (UIColor::title_bkg == UIColor::window_bkg) {
      display.setColor(UIColor::title_txt);
    } else {
      display.setColor(UIColor::title_bkg);
    }
    int y = 14;
    int x = display.width() / 2 - 5 * (HomePage::Count-1);
    for (uint8_t i = 0; i < HomePage::Count; i++, x += 10) {
      if (i == _page) {
        display.fillRect(x-1, y-1, 4, 4);
      } else {
        display.fillRect(x, y, 2, 2);
      }
    }

    if (_page == HomePage::FIRST) {
      display.setColor(UIColor::primary_txt);
      display.setTextSize(2);
      sprintf(tmp, "Mensagens: %d", _task->getMsgCount());
      display.drawTextCentered(display.width() / 2, 22, tmp);

      #ifdef WIFI_SSID
        IPAddress ip = WiFi.localIP();
        snprintf(tmp, sizeof(tmp), "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, 54, tmp);
      #endif
      if (_task->hasConnection()) {
        display.setColor(UIColor::warning_txt);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, 43, "< Ligado >");

      } else if (the_mesh.getBLEPin() != 0) { // BT pin
        display.setColor(UIColor::warning_txt);
        display.setTextSize(2);
        sprintf(tmp, "Pin:%d", the_mesh.getBLEPin());
        display.drawTextCentered(display.width() / 2, 43, tmp);
      }
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

      display.drawTextCentered(
        display.width() / 2 - 42,
        40,
        ">"
      );

      drawSelectedMenuText(
        display,
        display.width() / 2 + 8,
        40,
        items[_sms_new_menu]
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

      display.drawTextCentered(
        display.width() / 2 - 42,
        40,
        ">"
      );

      drawSelectedMenuText(
        display,
        display.width() / 2 + 8,
        40,
        items[_sms_target_menu]
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

          display.drawTextCentered(
            display.width() / 2 - 42,
            40,
            ">"
          );

          display.drawTextEllipsized(
            8,
            34,
            display.width() - 12,
            contact.name
          );
        }

      } else {

        display.drawTextCentered(
          display.width() / 2 - 42,
          40,
          ">"
        );

        display.drawTextCentered(
          display.width() / 2 + 8,
          40,
          "[ SAIR ]"
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

      display.drawTextCentered(
        display.width() / 2 - 42,
        40,
        ">"
      );

      drawSelectedMenuText(
        display,
        display.width() / 2 + 8,
        40,
        actions[_sms_action_menu]
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

      display.drawTextCentered(
        display.width() / 2 - 42,
        54,
        ">"
      );

      drawSelectedMenuText(
        display,
        display.width() / 2 + 8,
        54,
        confirms[_sms_confirm]
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

      if (_sms_channel_menu < count) {

        int found = 0;
        ChannelDetails selected;
        bool valid = false;

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

          display.drawTextCentered(
            display.width() / 2 - 42,
            40,
            ">"
          );

          display.drawTextEllipsized(
            8,
            34,
            display.width() - 12,
            selected.name
          );
        }

      } else {

        display.drawTextCentered(
          display.width() / 2 - 42,
          40,
          ">"
        );

        display.drawTextCentered(
          display.width() / 2 + 8,
          40,
          "[ SAIR ]"
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

      display.drawTextCentered(
        display.width() / 2 - 42,
        40,
        ">"
      );

      display.drawTextEllipsized(
        8,
        34,
        display.width() - 12,
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
          display.drawTextCentered(display.width() / 2 - 42, y, ">");
          display.drawTextCentered(display.width() / 2 + 8, y, sms_items[i]);
        } else {
          display.setColor(UIColor::secondary_txt);
          display.drawTextCentered(display.width() / 2, y, sms_items[i]);
        }
      }
    }

  } else if (_page == HomePage::RADIO) {
      display.setColor(UIColor::primary_txt);
      display.setTextSize(1);
      // freq / sf
      display.setCursor(0, 20);
      sprintf(tmp, "FQ: %06.3f   SF: %d", _node_prefs->freq, _node_prefs->sf);
      display.print(tmp);

      display.setCursor(0, 31);
      sprintf(tmp, "BW: %03.2f     CR: %d", _node_prefs->bw, _node_prefs->cr);
      display.print(tmp);

      // tx power,  noise floor
      display.setCursor(0, 42);
      sprintf(tmp, "TX: %ddBm", _node_prefs->tx_power_dbm);
      display.print(tmp);
      display.setCursor(0, 53);
      sprintf(tmp, "Noise floor: %d", radio_driver.getNoiseFloor());
      display.print(tmp);
    } else if (_page == HomePage::SETTINGS) {

      if (!_settings_submenu) {

        renderSectionHome(
          display,
          settings_icon,
          64, 32,
          "DEFINIÇÕES"
        );

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

      } else {

        const char* settings_items[] = {
          "BLUETOOTH",
          "ANUNCIAR NÓ",
          "DESLIGAR",
          "[ SAIR ]"
        };

        display.setColor(UIColor::primary_txt);
        display.setTextSize(1);

        display.drawTextCentered(
          display.width() / 2,
          38,
          settings_items[_settings_menu]
        );
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

    } else if (_page == HomePage::APPS) {

      // ======================================================
      // APLICAÇÕES
      // ======================================================

      // ------------------------------------------------------
      // APLICAÇÕES -> DESCOBRIR NÓS
      // ------------------------------------------------------
      // Discovery ATIVO. Não usa AdvertPath.
      // ------------------------------------------------------

      if (!_apps_submenu && _apps_view == 5) {

        refreshActiveDiscoveryNodes();

        display.setColor(UIColor::primary_txt);
        display.setTextSize(1);

        if (_active_discovery_count == 0) {

          // Linha reservada intencionalmente vazia.

          display.drawTextCentered(
            display.width() / 2,
            37,
            "Prima demoradamente"
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
          // Isto NÃO altera "Repetidores Descobertos".
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

          display.setColor(UIColor::secondary_txt);

          display.drawTextCentered(
            display.width() / 2,
            42,
            keyText
          );

          char infoText[32];

          const char* typeText =
            (result.node_type == ADV_TYPE_REPEATER)
              ? "REPEATER"
              : (result.node_type == ADV_TYPE_ROOM)
                ? "ROOM"
                : "NODE";

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

      else if (!_apps_submenu && _apps_view == 6) {

        refreshDiscoveredNodes();

        display.setColor(UIColor::primary_txt);
        display.setTextSize(1);

        display.drawTextCentered(
          display.width() / 2,
          16,
          "Repetidores Descobertos"
        );

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
          "APLICAÇÕES"
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
          "Sensores",
#endif
          "Relógio",
          "Descobrir Repetidores",
          "Repetidores Descobertos",
          "[ SAIR ]"
        };

        display.setColor(UIColor::primary_txt);
        display.setTextSize(1);

        display.drawTextCentered(
        display.width() / 2 - 42,
        34,
        ">"
      );

      drawSelectedMenuText(
        display,
        display.width() / 2 + 8,
        34,
        apps_items[_apps_menu]
      );
      }

    } else if (_page == HomePage::INTERNAL_HOME_ASSISTANT) {

      if (!_ha_submenu) {

        display.setColor(UIColor::corp_blue);
        display.drawXbm(
          (display.width() - 32) / 2,
          15,
          home_assistant_icon,
          32,
          32
        );

        display.setColor(UIColor::primary_txt);
        display.setTextSize(1);
        display.drawTextCentered(
          display.width() / 2,
          55,
          "Home Assistant"
        );

      } else if (_ha_confirm_submenu) {

        display.setColor(UIColor::primary_txt);
        display.setTextSize(1);

        display.drawTextCentered(
          display.width() / 2,
          18,
          "Abrir Prédio?"
        );

        const char* confirm_items[] = {
          "SIM",
          "NÃO"
        };

        display.drawTextCentered(
        display.width() / 2 - 42,
        40,
        ">"
      );

      drawSelectedMenuText(
        display,
        display.width() / 2 + 8,
        40,
        confirm_items[_ha_confirm]
      );

      } else {

        display.setColor(UIColor::primary_txt);
        display.setTextSize(1);

        const char* ha_items[] = {
          "Abrir Prédio",
          "[ SAIR ]"
        };

        display.drawTextCentered(
        display.width() / 2 - 42,
        34,
        ">"
      );

      drawSelectedMenuText(
        display,
        display.width() / 2 + 8,
        34,
        ha_items[_ha_menu]
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
      display.setTextSize(2);
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

      // ======================================================
      // SUBMENU ANUNCIAR NÓ
      // ======================================================

      if (_settings_advert_submenu) {

        if (c == KEY_NEXT) {
          _settings_advert_menu =
            (_settings_advert_menu + 1) % 3;
          return true;
        }

        if (c == KEY_PREV) {
          _settings_advert_menu =
            (_settings_advert_menu + 2) % 3;
          return true;
        }

        if (c == KEY_CANCEL) {
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
      // MENU DE DEFINIÇÕES
      // ======================================================

      if (c == KEY_NEXT) {
        _settings_menu = (_settings_menu + 1) % 4;
        return true;
      }

      if (c == KEY_PREV) {
        _settings_menu = (_settings_menu + 3) % 4;
        return true;
      }

      if (c == KEY_CANCEL) {
        _settings_submenu = false;
        _settings_menu = 0;
        _settings_advert_submenu = false;
        _settings_advert_menu = 0;
        return true;
      }

      if (c == KEY_ENTER) {

        // BLUETOOTH
        if (_settings_menu == 0) {
          bool bluetooth_enable = !_task->isBluetoothEnabled();

          if (bluetooth_enable) {
            _task->enableBluetooth();
          } else {
            _task->disableBluetooth();
          }

          _task->notify(UIEventType::ack);
          _task->showAlert(
            bluetooth_enable ? "Bluetooth ON" : "Bluetooth OFF",
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

        // DESLIGAR
        if (_settings_menu == 2) {
          _shutdown_init = true;
          return true;
        }

        // SAIR
        if (_settings_menu == 3) {
          _settings_submenu = false;
          _settings_menu = 0;
          return true;
        }
      }

      return true;
    }

    // ========================================================
    // APLICAÇÕES MENU
    // ========================================================

    if (_page == HomePage::APPS && _apps_submenu) {

      // Número real de opções.
      int apps_count = 5;
      // HOME ASSISTANT
      // RELÓGIO
      // DESCOBRIR REPETIDORES
      // REPETIDORES DESCOBERTOS
      // SAIR

#if ENV_INCLUDE_GPS == 1
      apps_count++;
#endif

#if UI_SENSORS_PAGE == 1
      apps_count++;
#endif

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
        _apps_view = 0;
        _apps_return = false;

        return true;
      }

      if (c == KEY_ENTER) {

        int app_index = 0;

        // HOME ASSISTANT
        if (_apps_menu == app_index++) {

          _apps_submenu = false;
          _apps_view = 2;
          _apps_return = true;

          _ha_submenu = true;
          _ha_menu = 0;
          _ha_confirm = 0;
          _ha_confirm_submenu = false;

          _page = HomePage::INTERNAL_HOME_ASSISTANT;

          return true;
        }

#if ENV_INCLUDE_GPS == 1
        // GPS
        if (_apps_menu == app_index++) {

          _apps_submenu = false;
          _apps_view = 3;
          _apps_return = true;

          _page = HomePage::INTERNAL_GPS;

          return true;
        }
#endif

#if UI_SENSORS_PAGE == 1
        // SENSORES
        if (_apps_menu == app_index++) {

          _apps_submenu = false;
          _apps_view = 4;
          _apps_return = true;

          _page = HomePage::INTERNAL_SENSORS;

          return true;
        }
#endif

        // RELÓGIO
        if (_apps_menu == app_index++) {

          _apps_submenu = false;
          _apps_view = 1;
          _apps_return = true;

          _page = HomePage::INTERNAL_CLOCK;

          return true;
        }

        // DESCOBRIR REPETIDORES
        if (_apps_menu == app_index++) {

          _apps_submenu = false;
          _apps_view = 5;
          _apps_return = true;

          _discover_menu = 0;
          refreshDiscoveredNodes();

          return true;
        }

        // REPETIDORES DESCOBERTOS
        if (_apps_menu == app_index++) {

          _apps_submenu = false;
          _apps_view = 6;
          _apps_return = true;

          _discover_menu = 0;
          refreshDiscoveredNodes();

          return true;
        }

        // SAIR
        if (_apps_menu == app_index++) {

          _apps_submenu = false;
          _apps_menu = 0;
          _apps_view = 0;
          _apps_return = false;

          return true;
        }
      }

      return true;
    }

    // --------------------------------------------------------
    // APLICAÇÕES -> DESCOBRIR NÓS
    // --------------------------------------------------------
    // Discovery ATIVO.
    // Não usa AdvertPath e não interfere em "Repetidores Descobertos".
    // --------------------------------------------------------

    if (_page == HomePage::APPS &&
        !_apps_submenu &&
        _apps_view == 5) {

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

        _page = HomePage::APPS;

        _apps_submenu = false;
        _apps_menu = 0;
        _apps_view = 0;
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
              "A procurar nos...",
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

        _page = HomePage::APPS;

        _apps_submenu = false;
        _apps_menu = 0;
        _apps_view = 0;
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
        _apps_view == 6) {

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

        _page = HomePage::APPS;

        _apps_submenu = false;
        _apps_menu = 0;
        _apps_view = 0;
        _apps_return = false;

        return true;
      }

      // 3 CLICKS / CANCEL -> sair
      if (c == KEY_SELECT ||
          c == KEY_CANCEL) {

        _page = HomePage::APPS;

        _apps_submenu = false;
        _apps_menu = 0;
        _apps_view = 0;
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
        _apps_view = 0;
        _apps_menu = 0;
        _apps_return = false;

        return true;
      }

      return true;
    }

    // --------------------------------------------------------
    // APLICAÇÕES -> HOME ASSISTANT
    // --------------------------------------------------------

    if (_page == HomePage::INTERNAL_HOME_ASSISTANT &&
        _apps_return) {

      if (_ha_confirm_submenu) {

        if (c == KEY_NEXT ||
            c == KEY_RIGHT ||
            c == KEY_PREV ||
            c == KEY_LEFT) {

          _ha_confirm =
            (_ha_confirm + 1) % 2;

          return true;
        }

        if (c == KEY_CANCEL ||
            c == KEY_SELECT) {

          _ha_confirm_submenu = false;
          return true;
        }

        if (c == KEY_ENTER) {

          if (_ha_confirm == 0) {

            ChannelDetails channel;
            bool found = false;

            for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {

              if (the_mesh.getChannel(i, channel) &&
                  strcmp(channel.name, "HIVE") == 0) {

                found = true;
                break;
              }
            }

            if (!found) {

              _task->showAlert(
                "Canal HIVE não encontrado",
                1500
              );

              return true;
            }

            const char* command =
              "!portapredio";

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
                ? "Comando enviado"
                : "Falha ao enviar",
              1200
            );

            return true;
          }

          _ha_confirm_submenu = false;
          return true;
        }

        return true;
      }

      if (c == KEY_NEXT ||
          c == KEY_RIGHT) {

        _ha_menu =
          (_ha_menu + 1) % 2;

        return true;
      }

      if (c == KEY_PREV ||
          c == KEY_LEFT) {

        _ha_menu =
          (_ha_menu + 1) % 2;

        return true;
      }

      if (c == KEY_CANCEL ||
          c == KEY_SELECT ||
          c == KEY_ENTER) {

        _page = HomePage::APPS;

        _apps_submenu = false;
        _apps_menu = 0;
        _apps_view = 0;
        _apps_return = false;

        _ha_submenu = false;
        _ha_confirm_submenu = false;

        return true;
      }

      if (c == KEY_ENTER) {

        if (_ha_menu == 0) {

          _ha_confirm = 0;
          _ha_confirm_submenu = true;

          return true;
        }

        if (_ha_menu == 1) {

          _page = HomePage::APPS;

          _apps_submenu = false;
          _apps_menu = 0;
          _apps_view = 0;
          _apps_return = false;

          _ha_submenu = false;
          _ha_confirm_submenu = false;

          return true;
        }
      }

      return true;
    }

    // ========================================================
    // APLICAÇÕES -> GPS / SENSORES
    // Voltar ao menu APPS antes da navegação geral.
    // ========================================================

#if ENV_INCLUDE_GPS == 1
    if (_page == HomePage::INTERNAL_GPS && _apps_return) {

      if (c == KEY_NEXT || c == KEY_RIGHT ||
          c == KEY_PREV || c == KEY_LEFT ||
          c == KEY_CANCEL || c == KEY_SELECT) {

        _page = HomePage::APPS;
        _apps_submenu = true;
        _apps_menu = 0;
        _apps_view = 0;
        _apps_return = false;

        return true;
      }
    }
#endif

#if UI_SENSORS_PAGE == 1
    if (_page == HomePage::INTERNAL_SENSORS && _apps_return) {

      if (c == KEY_NEXT || c == KEY_RIGHT ||
          c == KEY_PREV || c == KEY_LEFT ||
          c == KEY_CANCEL || c == KEY_SELECT) {

        _page = HomePage::APPS;
        _apps_submenu = true;
        _apps_menu = 0;
        _apps_view = 0;
        _apps_return = false;

        return true;
      }
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

    if (c == KEY_ENTER && _page == HomePage::INTERNAL_HOME_ASSISTANT) {
      _ha_submenu = true;
      _ha_menu = 0;
      _ha_confirm_submenu = false;
      return true;
    }

    // ========================================================
    // ========================================================
    // ENTER APLICAÇÕES
    // ========================================================

    if (c == KEY_ENTER && _page == HomePage::APPS) {

      _apps_submenu = true;
      _apps_menu = 0;
      _apps_view = 0;
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

#if ENV_INCLUDE_GPS == 1
    if (_page == HomePage::INTERNAL_GPS && _apps_return) {

      if (c == KEY_PREV || c == KEY_LEFT ||
          c == KEY_CANCEL || c == KEY_SELECT) {

        _page = HomePage::APPS;
        _apps_submenu = true;
        _apps_menu = 0;
        _apps_view = 0;
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
        _apps_view = 0;
        _apps_return = false;

        return true;
      }

      if (c == KEY_ENTER) {
        _task->toggleGPS();
        next_sensors_refresh = 0;
        return true;
      }

      return true;
    }
#endif

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
