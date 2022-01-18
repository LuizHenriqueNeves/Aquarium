#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <NTPClient.h>
#include <WiFiUDP.h>

//--------  Definições  ------------
#define ledBranca 5
#define pinCooler 13
#define ledUV     4
#define ledRoyal  16

#define minPot          0
#define minPotCooler    665 //65%
#define maxPot          1023
#define maxPotLedBranco 440 // 43%


//------- Portal Web ----------------
const char* WEBSERVER_HEADER_KEYS[] = {"User-Agent"}; // Headers do Servidor Web
ESP8266WebServer server(80); // Classe WebServer para ESP8266
DNSServer dnsServer; // DNS Server

//--------- Arquivo Json ------------
const size_t JSON_SIZE = JSON_OBJECT_SIZE(10) + 130;

//--------- Horas -------------------
WiFiUDP udp;//Cria um objeto "UDP".
NTPClient ntp(udp, "br.pool.ntp.org", -3 * 3600, 60000);

//---------  Variáveis  --------------
int lights_manha     = 0,  //the start of the morning
    lights_dia       = 1140, //end of morning and start of the day
    lights_tarde     = 1320, //end of day and start of the evening
    lights_noite     = 0, //end of the evening and start of the night
    lights_madrugada = 0,
    horas = 0;
int HRacao1 = 0, MRacao1 = 0,
    HRacao2 = 0, MRacao2 = 0,
    HLLed   = 0, MLLed   = 0,
    HDLed   = 0, MDLed   = 0,
    HNoite  = 0, MNoite  = 0;
int intensPwmBranca = 0,
    intensPwmRoyal  = 0,
    intensPwmUV     = 0;
int intensidade = 358;// 35%

boolean ledOnOff = false;

char ssid[20] = ""; // Rede WiFi
char pw[20] = "";// Senha da Rede WiFi

//----------------------  Funções  -----------------------------//
//-------------------------//
//      Inicia Horas       //
//-------------------------//
void setupNTP() {
  //Inicializa o client NTP
  ntp.begin();

  //Espera pelo primeiro update online
  Serial.println("Esperando por atualização das Horas..");
  while (!ntp.update()) {
    Serial.print(".");
    ntp.forceUpdate();
    delay(500);
  }
  Serial.println();
  Serial.println("Hora atualizada!");
}


//---------------------------------//
//     Config. horas do usuario    //
//---------------------------------//

void ajustaHorario() {
  lights_manha = ((HLLed * 100) + MLLed);
  lights_noite = ((HDLed * 100) + MDLed);
  lights_madrugada = ((HNoite * 100) + MNoite);
}


//---------------------------------//
//        Cria military time       //
//---------------------------------//
//create military time output like [0000,2400)

void militaryTime() {
  horas = ((ntp.getHours() * 100) + ntp.getMinutes());
}


//---------------------------------//
//   Ajusta formato endereco IP    //
//---------------------------------//

String ipStr(const IPAddress &ip) {
  // Retorna IPAddress em formato "n.n.n.n"
  String sFn = "";
  for (byte bFn = 0; bFn < 3; bFn++) {
    sFn += String((ip >> (8 * bFn)) & 0xFF) + ".";
  }
  sFn += String(((ip >> 8 * 3)) & 0xFF);
  return sFn;
}


//---------------------------------//
// acressenta zeros a esquerda str //
//---------------------------------//
String escrever (int h) {
  String result;
  if (h < 10) {
    result = '0' + String(h);
    return result;
  }
  return String(h);
}


//-------------------------//
//     Horas em String     //
//-------------------------//
int qtdLedLigado() {
  int x = 0;
  if (intensPwmBranca != 0)
    x++;
  if (intensPwmRoyal != 0)
    x++;
  if (intensPwmUV != 0)
    x++;
  if (x == 0)
    return 1;
  else
    return x;
}

String strNivel() {
  int med = map((intensPwmBranca + intensPwmRoyal + intensPwmUV) / qtdLedLigado(),
                minPot, maxPot, 0, 100);
  return String(med) + "%";
}


//------------------------------------//
// Definção da intensidade das luzes  //
//------------------------------------//

void intensidadeLuz() {
  int b, r, u;
  if (lights_manha == 0 || lights_noite == 0 || lights_madrugada == 0 ) {
    intensPwmBranca     =  minPot;
    intensPwmRoyal      =  minPot;
    intensPwmUV         =  minPot;
    analogWrite(pinCooler, minPot);
    analogWrite(ledBranca, intensPwmBranca);
    analogWrite(ledRoyal,  intensPwmRoyal);
    analogWrite(ledUV,     intensPwmUV);
  }
  else if ( horas >= lights_manha && horas < lights_dia) { // manhã
    b =  map( horas, lights_manha, lights_dia, minPot, maxPotLedBranco);
    r =  map( horas, lights_manha, lights_dia, minPot, 255); //25%
    u =  map( horas, lights_manha, lights_dia, minPot, 358); //35%
    if (intensPwmBranca != b || intensPwmRoyal != r || intensPwmUV != u) {
      intensPwmBranca = b;
      intensPwmRoyal  = r;
      intensPwmUV     = u;
      analogWrite(ledBranca, intensPwmBranca);
      analogWrite(ledRoyal,  intensPwmRoyal);
      analogWrite(ledUV,     intensPwmUV);
      analogWrite(pinCooler, map( horas, lights_manha, lights_dia, minPotCooler, maxPot));
    }
  }
  else if ( horas >= lights_dia && horas < lights_tarde) { //meio dia
    if (maxPotLedBranco != intensPwmBranca || 307 != intensPwmRoyal || 409 != intensPwmUV) {
      intensPwmBranca     =  maxPotLedBranco;
      intensPwmRoyal      =  255; //25%
      intensPwmUV         =  358; //35%
      analogWrite(pinCooler, maxPot);
      analogWrite(ledBranca, intensPwmBranca);
      analogWrite(ledRoyal,  intensPwmRoyal);
      analogWrite(ledUV,     intensPwmUV);
    }
  }
  else if ( horas >= lights_tarde && horas < lights_noite) { //tarde
    b =  map( horas, lights_tarde, lights_noite, maxPotLedBranco, 30); //3%
    r =  map( horas, lights_tarde, lights_noite, 255, 51); //5%-25%
    u =  map( horas, lights_tarde, lights_noite, 358, 20); //2%-35%
    if (intensPwmBranca != b || intensPwmRoyal != r || intensPwmUV != u) {
      intensPwmBranca = b;
      intensPwmRoyal  = r;
      intensPwmUV     = u;
      analogWrite(ledBranca, intensPwmBranca);
      analogWrite(ledRoyal,  intensPwmRoyal);
      analogWrite(ledUV,     intensPwmUV);
      analogWrite(pinCooler, map( horas, lights_noite, lights_tarde, minPotCooler, maxPot));
    }
  }
  else if ( horas >= lights_noite && horas <= lights_madrugada) { //noite
    b =  map( horas, lights_noite, lights_madrugada, 30, minPot); //3%
    r =  map( horas, lights_noite, lights_madrugada, 51, minPot); //5%
    u =  map( horas, lights_noite, lights_madrugada, 20, minPot); //2%
    if (intensPwmBranca != b || intensPwmRoyal != r || intensPwmUV != u) {
      intensPwmBranca = b;
      intensPwmRoyal  = r;
      intensPwmUV     = u;
      analogWrite(ledBranca, intensPwmBranca);
      analogWrite(ledRoyal,  intensPwmRoyal);
      analogWrite(ledUV,     intensPwmUV);
      analogWrite(pinCooler, minPotCooler); //60%
    }
  }
  else {
    b =  minPot;
    r =  minPot;
    u =  minPot;
    if (intensPwmBranca != b || intensPwmRoyal != r || intensPwmUV != u) {
      intensPwmBranca = b;
      intensPwmRoyal  = r;
      intensPwmUV     = u;
      analogWrite(ledBranca, intensPwmBranca);
      analogWrite(ledRoyal,  intensPwmRoyal);
      analogWrite(ledUV,     intensPwmUV);
      analogWrite(pinCooler, minPot);
    }
  }
}


//---------------------------------//
//        Botao ligar Luz          //
//---------------------------------//

String ligaLed(const String x) {
  if (x.toInt() == ledOnOff) {
    return x;
  } else {
    ledOnOff = x.toInt();
    if (ledOnOff == true) {
      for (int x = 0; x <= intensidade; x = x + 3) {
        intensPwmBranca      =  x;
        intensPwmRoyal       =  map( x, minPot, maxPot, minPot, 635); //62%
        intensPwmUV          =  map( x, minPot, maxPot, minPot, 635); //62%
        analogWrite(ledBranca,  intensPwmBranca);
        analogWrite(ledRoyal,   intensPwmRoyal);
        analogWrite(ledUV,      intensPwmUV);
        analogWrite(pinCooler,  map( x, minPot, maxPot, minPotCooler, maxPot));
        delay(10);
      }
      Serial.println("Ligou a luz");
      return String(ledOnOff);
    } else if (ledOnOff == false) {
      for (int x = intensidade; x >= 0; x = x - 3) {
        intensPwmBranca     =  x;
        intensPwmRoyal      =  map( x, minPot, maxPot, minPot, 635); //62%
        intensPwmUV         =  map( x, minPot, maxPot, minPot, 635); //62%
        analogWrite(ledBranca, intensPwmBranca);
        analogWrite(ledRoyal,  intensPwmRoyal);
        analogWrite(ledUV,     intensPwmUV);
        analogWrite(pinCooler, map( x, minPot, maxPot, minPotCooler, maxPot));
        delay(10);
      }
      Serial.println("Desligou a luz");
      delay(1000);
      intensidadeLuz();
      return String(ledOnOff);
    } else {
      return "";
    }
  }
}

void handleDT() {
  server.send(200, "text/plain", ligaLed(server.arg("ref")));
}

void av(int valor, int ref) {
  int med = (intensPwmBranca + intensPwmRoyal + intensPwmUV) / qtdLedLigado();
  analogWrite(ref, valor);
  analogWrite(pinCooler, map( med, minPot, maxPot, minPotCooler, maxPot));
  server.send(200, "text/plain", "");
}

void handleAV() {
  if (ledOnOff == true) {
    switch (server.arg("ref").toInt()) {
      case 1:
        intensPwmBranca = map( server.arg("val").toInt(), minPot, 100, minPot, maxPot);
        av(intensPwmBranca, ledBranca);
        break;
      case 2:
        intensPwmRoyal = map( server.arg("val").toInt(), minPot, 100, minPot, maxPot);
        av(intensPwmRoyal, ledRoyal);
        break;
      case 3:
        intensPwmUV = map( server.arg("val").toInt(), minPot, 100, minPot, maxPot);
        av(intensPwmUV, ledUV);
        break;
    }
  }
  server.send(200, "text/plain", "");
}


//---------------------------------//
// Config. de gravacao de arquivo  //
//---------------------------------//

boolean configSave() {
  // Grava configuração
  StaticJsonDocument<JSON_SIZE> jsonConfig;

  File file = SPIFFS.open(F("/Config.json"), "w+");
  if (file) {
    // Atribui valores ao JSON e grava
    jsonConfig["HRacao1"] = HRacao1;
    jsonConfig["MRacao1"] = MRacao1;
    jsonConfig["HRacao2"] = HRacao2;
    jsonConfig["MRacao2"] = MRacao2;
    jsonConfig["HLLed"]   = HLLed;
    jsonConfig["MLLed"]   = MLLed;
    jsonConfig["HDLed"]   = HDLed;
    jsonConfig["MDLed"]   = MDLed;
    jsonConfig["HNoite"]  = HNoite;
    jsonConfig["MNoite"]  = MNoite;

    serializeJsonPretty(jsonConfig, file);
    file.close();

    Serial.println("Gravando configurações!");
    serializeJsonPretty(jsonConfig, Serial);
    return true;
  }
  return false;
}


//---------------------------------//
//  Definicao configuração padrão  //
//---------------------------------//
void  configReset() {
  ledOnOff = false;
  HRacao1 = 0;
  MRacao1 = 0;
  HRacao2 = 0;
  MRacao2 = 0;
  HLLed   = 0;
  MLLed   = 0;
  HDLed   = 0;
  MDLed   = 0;
  HNoite  = 0;
  MNoite  = 0;
  configSave();
}


//---------------------------------//
//  Config. de leitura de arquivo  //
//---------------------------------//

boolean configRead() {
  // Lê configuração
  StaticJsonDocument<JSON_SIZE> jsonConfig;

  File file = SPIFFS.open(F("/Config.json"), "r");
  if (deserializeJson(jsonConfig, file)) {
    configReset(); // Falha na leitura, assume valores padrão
    Serial.println("Falha ao ler CONFIGURAÇÕES, assumindo valores padrão.");
    return false;
  } else {
    // Sucesso na leitura
    HRacao1 = jsonConfig["HRacao1"];
    MRacao1 = jsonConfig["MRacao1"];
    HRacao2 = jsonConfig["HRacao2"];
    MRacao2 = jsonConfig["MRacao2"];
    HLLed   = jsonConfig["HLLed"];
    MLLed   = jsonConfig["MLLed"];
    HDLed   = jsonConfig["HDLed"];
    MDLed   = jsonConfig["MDLed"];
    HNoite  = jsonConfig["HNoite"];
    MNoite  = jsonConfig["MNoite"];

    file.close();

    Serial.println(F("Lendo configurações!"));
    serializeJsonPretty(jsonConfig, Serial);
    return true;
  }
}


//---------------------------------//
//   Requisiçao Web - Pagina Home  //
//---------------------------------//

void handleHome() {
  // Home
  File file = SPIFFS.open(F("/index.htm"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    // Atualiza conteúdo dinâmico
    s.replace(F("#HNoite#")   , escrever (HNoite));
    s.replace(F("#MNoite#")   , escrever (MNoite));
    s.replace(F("#HLLed#")    , escrever (HLLed));
    s.replace(F("#MLLed#")    , escrever (MLLed));
    s.replace(F("#HDLed#")    , escrever (HDLed));
    s.replace(F("#MDLed#")    , escrever (MDLed));
    s.replace(F("#sysIP#")    , ipStr(WiFi.status() == WL_CONNECTED ? WiFi.localIP() : WiFi.softAPIP()));
    s.replace(F("#clientIP#") , ipStr(WiFi.softAPIP()));//ipStr(server.client().remoteIP()));
    s.replace(F("#hora#")     , escrever(ntp.getHours()) + ":" + escrever(ntp.getMinutes()));
    s.replace(F("#nivel#")    , strNivel());


    // Envia dados
    server.send(200, F("text/html"), s);
    Serial.println("Home - Cliente: " + ipStr(server.client().remoteIP()) +
                   (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  } else {
    server.send(500, F("text/plain"), F("Home - ERROR 500"));
    Serial.println(F("Home - ERRO lendo arquivo"));
  }
}

//-----------------------------------------//
//  Requisiçao Web - Pagina Conf. Horario  //
//-----------------------------------------//

void handleConfigHorario() {
  // Config
  File file = SPIFFS.open(F("/index.htm"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    // Atualiza conteúdo dinâmico
    s.replace(F("#HNoite#")  , String(HNoite));
    s.replace(F("#MNoite#")  , String(MNoite));
    s.replace(F("#HLLed#")    , String(HLLed));
    s.replace(F("#MLLed#")    , String(MLLed));
    s.replace(F("#HDLed#")    , String(HDLed));
    s.replace(F("#MDLed#")    , String(MDLed));

    // Send data
    server.send(200, F("text/html"), s);
    Serial.println("Config - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("Config - ERROR 500"));
    Serial.println(F("Config - ERRO lendo arquivo"));
  }
}


//-----------------------------------------------//
//  Requisiçao Web - Salva Pagina Conf. Horario  //
//-----------------------------------------------//

void handleConfigSaveHorario() {
  if (server.args() == 7) {

    // Grava HNoite
    HNoite = server.arg("HNoite").toInt();
    MNoite = server.arg("MNoite").toInt();

    // Grava HLLed
    HLLed = server.arg("HLLed").toInt();
    MLLed = server.arg("MLLed").toInt();

    // Grava HDLed
    HDLed = server.arg("HDLed").toInt();
    MDLed = server.arg("MDLed").toInt();

    if (HNoite  > 24 || MNoite > 60  ||
        HLLed   > 24 || MLLed  > 60  ||
        HDLed   > 24 || MDLed  > 60 ) {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Erro ao informar as horas.');history.back()</script></html>"));
    } else {
      // Grava configuração
      if (configSave()) {
        server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração salva.');history.back()</script></html>"));
        ajustaHorario();
        intensidadeLuz();
        Serial.println("Configuração Salva!");
      } else {
        server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha ao salvar configuração.');history.back()</script></html>"));
        Serial.println(F("ConfigSave - ERRO ao salvar Config"));
      }
    }
  } else {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Erro de parâmetros.');history.back()</script></html>"));
  }
}


//------------------------------------------//
//  Requisiçao Web - Pagina conf. WIFI      //
//------------------------------------------//

void handleConfig() {
  // Config
  File file = SPIFFS.open(F("/index.htm"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    // Atualiza conteúdo dinâmico
    s.replace(F("#ssid#")   , ssid);
    // Send data
    server.send(200, F("text/html"), s);
    Serial.println("Config - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("Config - ERROR 500"));
    Serial.println(F("Config - ERRO lendo arquivo"));
  }
}


//------------------------------------------//
//  Requisiçao Web - Pagina Salva Config.   //
//------------------------------------------//

void handleConfigSave() {
  // Grava Config
  // Verifica número de campos recebidos
  // ESP8266 gera o campo adicional "plain" via post
  if (server.args() == 3) {
    String s;

    // Grava ssid
    s = server.arg("ssid");
    s.trim();
    if (s != "")
      strlcpy(ssid, s.c_str(), sizeof(ssid));

    // Grava pw
    s = server.arg("pw");
    s.trim();
    if (s != "")
      strlcpy(pw, s.c_str(), sizeof(pw)); // Atualiza senha, se informada

    // Grava configuração
    if (configSave()) {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração salva. Reiniciando!');history.back()</script></html>"));
      Serial.println("ConfigSave - Cliente: " + ipStr(server.client().remoteIP()));
      delay(1000);
      ESP.restart();
    } else {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha ao salvar configuração.');history.back()</script></html>"));
      Serial.println(F("ConfigSave - ERRO ao salvar Config"));
    }
  } else {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Erro de parâmetros.');history.back()</script></html>"));
  }
}


//----------------------------//
//  Reset das Configuracoes   //
//----------------------------//

void handleReconfig() {
  File file = SPIFFS.open(F("/reconfig.htm"), "r");
  if (file) {
    server.streamFile(file, F("text/html"));
    file.close();
    // Reinicia Config
    configReset();
  }
  // Grava configuração
  if (configSave()) {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração reiniciada.');window.location = '/'</script></html>"));
    Serial.println("Reconfig - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha reiniciando configuração.');history.back()</script></html>"));
    Serial.println(F("Reconfig - ERRO reiniciando Config"));
  }
}


//-----------//
//  Reboot   //
//-----------//

void handleReboot() {
  // Reboot
  File file = SPIFFS.open(F("/reboot.htm"), "r");
  if (file) {
    server.streamFile(file, F("text/html"));
    file.close();
    Serial.println("Reboot - Cliente: " + ipStr(server.client().remoteIP()));
    delay(100);
    ESP.restart();
  } else {
    server.send(500, F("text/plain"), F("Reboot - ERROR 500"));
    Serial.println(F("Reboot - ERRO lendo arquivo"));
  }
}


//-----------------------------------//
//  Definição de cache para 2 dias   //
//-----------------------------------//

void handleStream(const String f, const String mime) {
  // Processa requisição de arquivo
  File file = SPIFFS.open("/" + f, "r");
  if (file) {
    // Define cache para 2 dias
    server.sendHeader(F("Cache-Control"), F("public, max-age=172800"));
    server.streamFile(file, mime);
    file.close();
  } else {
    server.send(500, F("text/plain"), F("Stream - ERROR 500"));
  }
}


//-------------------//
//  CSS Pagina Web   //
//-------------------//

void handleCSS() {
  // CSS
  handleStream(F("main.css"), F("text/css"));
}

void handleLogo() {
  // Logo
  handleStream(F("favicon.png"), F("image/png"));
}

void handlenoscript() {
  handleStream(F("noscript.css"), F("text/css"));
}

//-----------------------//
//     JS Pagina Web     //
//-----------------------//

void handlejquery() {
  // Arquivo js
  handleStream(F("jquery.min.js"), F("application/javascript"));
}

void handlebreakpoints() {
  handleStream(F("breakpoints.min.js"), F("application/javascript"));
}

void handlemain() {
  handleStream(F("main.js"), F("application/javascript"));
}

void handleutil() {
  handleStream(F("util.js"), F("application/javascript"));
}
