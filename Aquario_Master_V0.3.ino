/********************************************
   Projeto Aquario automático
   25/04/2020 - Luiz Henrique Soares Neves
 ********************************************/

#ifndef ESP8266
// Aceita apenas ESP8266
#error Placa inválida, apenas ESP8266
#endif

// ---------  Bibliotecas  ----------------
#include "funcoes.h"

//------------  Variáveis  -----------------
int contUpdate = 0,
    cont       = 0;

// --------  Setup  -----------
void setup() {

  Serial.begin(115200);
  Serial.println("Iniciando..");

  //---  Inicializa SPIFFS  ---
  if (SPIFFS.begin())
    Serial.println("SPIFFS Ok");
  else
    Serial.println("SPIFFS Falha");

  configRead(); // Lê configuração

  //---  WiFi Access Point  ---
  // Configura WiFi para ESP8266
  WiFi.hostname("AquarioCiclideos");
  WiFi.softAP("AquarioCiclideos", "654123");

  Serial.println("WiFi AP " + String("AquarioCiclideos") + " - IP " + ipStr(WiFi.softAPIP()));

  // Habilita roteamento DNS
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(53, "*", WiFi.softAPIP());

  //---  Iniciando conexão com WiFi  ---
  WiFi.begin(ssid, pw);
  Serial.println("Conectando-se ao WiFi: " + String(ssid));
  byte b = 0;
  while (WiFi.status() != WL_CONNECTED && b < 40) {
    b++;
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    // WiFi Station conectado
    Serial.println("WiFi conectado");
  } else {
    Serial.println(F("WiFi não conectado"));
  }
  Serial.println("Endereço IP da conexao local: " + WiFi.localIP());

  //---  Define funções de callback do processo  ---
  // Início
  ArduinoOTA.setHostname("Aqua-Ciclideos");
  ArduinoOTA.onStart([]() {
    String s;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      // Atualizar sketch
      s = "Sketch";
    } else { // U_SPIFFS
      // Atualizar SPIFFS
      s = "SPIFFS";
      // SPIFFS deve ser finalizada
      SPIFFS.end();
    }
    Serial.println("Iniciando OTA - " + s);
  });
  // Fim
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA Concluído.");
  });
  // Progresso
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.print(progress * 100 / total);
    Serial.print(" ");
  });
  // Falha
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.print("Erro " + String(error) + " ");
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Falha de autorização");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Falha de inicialização");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Falha de conexão");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Falha de recebimento");
    } else if (error == OTA_END_ERROR) {
      Serial.println("Falha de finalização");
    } else {
      Serial.println("Falha desconhecida");
    }
  });
  // Inicializa OTA
  ArduinoOTA.begin();
  Serial.println("Atualização via OTA disponível.");

  // WebServer
  server.on(F("/configSaveHorario")      , handleConfigSaveHorario);
  server.on(F("#edHorario")              , handleConfigHorario);
  server.on(F("#config")                 , handleConfig);
  server.on(F("/configSave")             , handleConfigSave);
  server.on(F("/reconfig")               , handleReconfig);
  server.on(F("/reboot")                 , handleReboot);
  server.on(F("/main.css")               , handleCSS);
  server.on(F("/jquery.min.js")          , handlejquery);
  server.on(F("/breakpoints.min.js")     , handlebreakpoints);
  server.on(F("/main.js")                , handlemain);
  server.on(F("/dt")                     , handleDT);
  server.on(F("/av")                     , handleAV);
  server.on(F("/favicon.png")            , handleLogo);
  server.on(F("/util.js")                , handleutil);
  server.on(F("/noscript.css")           , handlenoscript);

  server.onNotFound(handleHome);
  server.collectHeaders(WEBSERVER_HEADER_KEYS, 1);
  server.begin();

  setupNTP(); // Recebe horas atualizada

  //-----  Atualiza as horas  -------
  ajustaHorario();
  militaryTime();
  intensidadeLuz();
  cont = ntp.getMinutes();
}


// -----------  Loop  ---------------
void loop() {
  //----- Ajusta relogio interno e atualiza intensidade da luz ----
  if (ntp.getMinutes() != cont) {
    cont = ntp.getMinutes();
    militaryTime();
    if (ledOnOff == false)
      intensidadeLuz();
    contUpdate++;
  }

  //----- Update Horas ----
  if (contUpdate == 59) {
    ntp.update();
    contUpdate = 0;
  }
  ArduinoOTA.handle();
  dnsServer.processNextRequest(); // DNS
  server.handleClient(); // Web
  yield();
}
