#include <SoftwareSerial.h>

//#include <DallasTemperature.h>      // https://github.com/milesburton/Arduino-Temperature-Control-Library

#include <stdint.h>


//  ----------------------------------------- НАЗНАЧАЕМ ВЫВОДЫ для платок до 1.7.6 (c Arduino Pro Mini) ------------------------------

SoftwareSerial SIM800(27, 26); // для старых плат начиная с версии RX,TX
#define SIM_PWR 23
#define SIM_RST 5
#define SIM_PWKEY 4

#define ONE_WIRE_BUS 10      // пин датчика DS18B20, https://github.com/PaulStoffregen/OneWire
#define FIRST_P_Pin 8        // на реле первого положения замка зажигания с 8-го пина ардуино
#define SECOND_P 9           // на реле зажигания, через транзистор с 9-го пина ардуино

#define MODE_RESET 0 // ЖДЕМ SMS Ready, шлем "AT+CLIP=1;+DDET=1" ставм MODE_RESET_MODEM
#define MODE_RESET_TIMER 60
#define MODE_RESET_MODEM 1 // Ждем Ок, переходим
#define MODE_RESET_MODEM_TIMER 60
#define MODE_MODEM_INIT 2
#define MODE_MODEM_INIT_TIMER 30
#define MODE_MODEM_OK 3
#define MODE_MODEM_OK_TIMER 30
#define MODE_INIT_GPRS 4
#define MODE_INIT_GPRS_TIMER 30
#define MODE_INIT_GPRS_SETCONTYPE 5
#define MODE_INIT_GPRS_SETCONTYPE_TIMER 30
#define MODE_INIT_GPRS_OK 6
#define MODE_INIT_GPRS_OK_TIMER 30
#define MODE_INIT_GPRS_SETAPN 9
#define MODE_INIT_GPRS_SETAPN_TIMER 30
#define MODE_INIT_GPRS_CONNECT 10
#define MODE_INIT_GPRS_CONNECT_TIMER 30
#define MODE_CONNECT_MQTT 7
#define MODE_CONNECT_MQTT_TIMER 30
#define MODE_MQTT_CONNECTED 8
#define MODE_MQTT_CONNECTED_TIMER 120
#define MODE_SETFULL_FUNCTIONALITY 11
#define MODE_SETFULL_FUNCTIONALITY_TIMER 10

/*  ----------------------------------------- НАСТРОЙКИ MQTT брокера---------------------------------------------------------   */
const char MQTT_user[9] = "kgltdagp";           // api.cloudmqtt.com > Details > User
const char MQTT_pass[15] = "KeIN2CNFNLHp";      // api.cloudmqtt.com > Details > Password
const char MQTT_type[7] = "MQIsdp";             // тип протокола НЕ ТРОГАТЬ !
const char MQTT_CID[8] = "STAREX";              // уникальное имя устройства в сети MQTT
const String MQTT_SERVER = "m15.cloudmqtt.com"; // api.cloudmqtt.com > Details > Server  сервер MQTT брокера
const String PORT = "12319";                    // api.cloudmqtt.com > Details > Port    порт MQTT брокера НЕ SSL !
const String version = "MQTT|01/11/2019";
/*  ----------------------------------------- ИНДИВИДУАЛЬНЫЕ НАСТРОЙКИ !!!---------------------------------------------------------   */
const String call_phone = "+79202544485"; // телефон входящего вызова  для управления DTMF
const String APN = "internet.mts.ru";     // тчка доступа выхода в интернет вашего сотового оператора

/*  ----------------------------------------- ДАЛЕЕ НЕ ТРОГАЕМ ---------------------------------------------------------------   */
//float Vstart = 13.20;                        // порог распознавания момента запуска по напряжению
int mode = MODE_RESET;
int modeTimer = MODE_RESET_TIMER;
String pin = ""; // строковая переменная набираемого пинкода
//float TempDS[11];                           // массив хранения температуры c рахных датчиков
float Vbat, V_min; // переменная хранящая напряжение бортовой сети
//float m = 68.01;                            // делитель для перевода АЦП в вольты для резистров 39/11kOm
float m = 57.701915071; //58.3402489626556;                            // делитель для перевода АЦП в вольты для резистров 39/10kOm
unsigned long Time1, Time2 = 0;
int Timer1 = 10, Timer2 = 10, count, error_CF, error_C, defaultTimer1 = 10, defaultTimer2 = 10;
int interval = 1; // интервал тправки данных на сервер после загрузки ардуино
int connecttry = 0;
int sendtry = 0;
bool relay1 = false, relay2 = false; // переменная состояния режим прогрева двигателя
bool ring = false;                   // флаг момента снятия трубки
bool broker = false;                 // статус подклюлючения к брокеру
bool Security = false;               // состояние охраны после подачи питания
String LOC = "";

void (*resetFunc)(void) = 0; //declare reset function @ address 0

void setup()
{
  pinMode(SIM_RST, OUTPUT);
  pinMode(SIM_PWR, OUTPUT);
  pinMode(SIM_PWKEY, OUTPUT);
  
  
  pinMode(FIRST_P_Pin, OUTPUT);
  pinMode(SECOND_P, OUTPUT);

  delay(100);
  Serial.begin(115200); //скорость порта
  
  SIM800.begin(9600); //скорость связи с модемом
  Serial.println(version);
  Serial.println("starting");
  SIM800_reset();
}

void SIM800_reset()
{
  broker = false;
  digitalWrite(SIM_PWR, HIGH);
  digitalWrite(SIM_PWKEY, LOW);  
  digitalWrite(SIM_RST, LOW);
  delay(400);
  digitalWrite(SIM_RST, HIGH); // перезагрузка модема
  sendtry = 0;
  mode = MODE_RESET;
  modeTimer = MODE_RESET_TIMER;
  Serial.println("Modem reset");
  SIM800.println("AT");
}

void loop()
{

  if (SIM800.available())
    resp_modem(); // если что-то пришло от SIM800 в Ардуино отправляем для разбора
  if (Serial.available())
    resp_serial(); // если что-то пришло от Ардуино отправляем в SIM800
  if (millis() > Time2 + 60000)
  {
    Time2 = millis();
    modeTimer--;
    bool timerchgflag = false;
    if (relay1 == true && Timer1 > 0)
      Timer1--, Serial.print("Тм:"), Serial.println(Timer1), timerchgflag = true;
    ;
    if (relay2 == true && Timer2 > 0)
      Timer2--, Serial.print("Тм:"), Serial.println(Timer2), timerchgflag = true;
    ;
    if (timerchgflag == true)
    {
      MQTT_PUB_ALL();
    }
    if (modeTimer < 1){
      Serial.println("Mode: "+String(mode)+" timer reached");
      SIM800_reset();
      }
      
  }

  if (millis() > Time1 + 10000)
    Time1 = millis(), detection(); // выполняем функцию detection () каждые 10 сек
//  if (relay1 == true && digitalRead(STOP_Pin) == 1)
//    relay1stop(); // для платок 1,7,2
//  if (relay2 == true && digitalRead(STOP_Pin) == 1)
//    relay2stop(); // для платок 1,7,2
}

void relay1start()
{ // программа запуска двигателя
  Serial.println("R1 ON");
  Timer1 = defaultTimer1; // устанавливаем таймер
  pushRelay1();
  relay1 = true;
  MQTT_PUB_ALL();
  interval = 1;
}
void relay1stop()
{ // программа остановки прогрева двигателя
  if (!(defaultTimer1 == 30 && Timer1 == 0))
  {
    pushRelay1();
  }

  relay1 = false, Timer1 = defaultTimer1;
  Serial.println("R1 OFF");
  MQTT_PUB_ALL();
}

void pushRelay1()
{
  digitalWrite(FIRST_P_Pin, HIGH);
  delay(800);
  digitalWrite(FIRST_P_Pin, LOW);
}

void relay2start()
{ // программа запуска двигателя
  Serial.println("R2 ON");
  Timer1 = defaultTimer1; // устанавливаем таймер
  digitalWrite(SECOND_P, HIGH);
  relay2 = true;
  MQTT_PUB_ALL();
  interval = 1;
}

void relay2stop()
{ // программа остановки прогрева двигателя
  digitalWrite(SECOND_P, LOW), delay(100);
  relay2 = false, Timer2 = defaultTimer2;
  Serial.println("R2 OFF");
  MQTT_PUB_ALL();
}

//float VoltRead()
//{ // замеряем напряжение на батарее и переводим значения в вольты
//  float ADCC = analogRead(BAT_Pin);
//  float realadcc = ADCC;
//  ADCC = ADCC / m;
//  Serial.print("АКБ: "), Serial.print(ADCC), Serial.print("V "), Serial.println(realadcc);
//  if (ADCC < V_min)
//    V_min = ADCC;
//  return (ADCC);
//} // переводим попугаи в вольты

void detection()
{ // условия проверяемые каждые 10 сек

  //  Serial.print("Инт:"), Serial.println(interval);

  if (relay1 == true && Timer1 < 1)
    relay1stop(); // остановка прогрева если закончился отсчет таймера
  if (relay2 == true && Timer2 < 1)
    relay2stop(); // остановка прогрева если закончился отсчет таймера

  interval--;
  if (interval == 2)
  {
    getLocation();
  }
  if (interval < 1 && broker == false)
  {
    Serial.println("Connect to MQTT Broker.");
    connecttry++;
    if (connecttry > 5)
    {
      connecttry = 0;
      Serial.println("Reset");
      SIM800_reset();
    }
    interval = 1;
    // delay(30); // подключаемся к GPRS
  }
  if (interval < 1)
    interval = 6, MQTT_PUB_ALL();
}

void playDtmf()
{
  SIM800.println("AT+VTD=1;+VTS=1");
  delay(400);
  SIM800.println("AT+VTD=1;+VTS=0");
}

void resp_serial()
{ // ---------------- ТРАНСЛИРУЕМ КОМАНДЫ из ПОРТА В МОДЕМ ----------------------------------
  String at = "";
  int k = 0;
  while (Serial.available())
    k = Serial.read(), at += char(k), delay(1);
  SIM800.println(at), at = "";
}

void MQTT_FloatPub(const char topic[15], float val, int x)
{
  char st[10];
  dtostrf(val, 0, x, st), MQTT_PUB(topic, st);
}

void MQTT_CONNECT()
{
  if (connecttry >= 10)
  {
    Serial.println("RSTALL");
    resetFunc();
  }
  Serial.println("MQTT_CONNECT");
  SIM800.println("AT+CIPSEND"), delay(100);

  SIM800.write(0x10); // маркер пакета на установку соединения
  SIM800.write(strlen(MQTT_type) + strlen(MQTT_CID) + strlen(MQTT_user) + strlen(MQTT_pass) + 12);
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_type)), SIM800.write(MQTT_type);   // тип протокола
  SIM800.write(0x03), SIM800.write(0xC2), SIM800.write((byte)0), SIM800.write(0x3C); // просто так нужно
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_CID)), SIM800.write(MQTT_CID);     // MQTT  идентификатор устройства
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_user)), SIM800.write(MQTT_user);   // MQTT логин
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_pass)), SIM800.write(MQTT_pass);   // MQTT пароль

  MQTT_PUB("C5/status", "Подключено"); // пакет публикации
  MQTT_SUB("C5/comand");               // пакет подписки на присылаемые команды
  MQTT_SUB("C5/comandrelay1");         // пакет подписки на присылаемые команды
  MQTT_SUB("C5/comandrelay2");         // пакет подписки на присылаемые команды
  MQTT_SUB("C5/settimer1");            // пакет подписки на присылаемые значения таймера
  MQTT_SUB("C5/settimer2");            // пакет подписки на присылаемые значения таймера1
  SIM800.write(0x1A);
  broker = true;
  interval = 6;
  connecttry++;
} // маркер завершения пакета

void MQTT_PUB(const char MQTT_topic[35], const char MQTT_messege[35])
{ // пакет на публикацию

  SIM800.write(0x30), SIM800.write(strlen(MQTT_topic) + strlen(MQTT_messege) + 2);
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_topic)), SIM800.write(MQTT_topic); // топик
  SIM800.write(MQTT_messege);
} // сообщение

void MQTT_SUB(const char MQTT_topic[15])
{ // пакет подписки на топик
  Serial.println("MQTT_SUB");
  SIM800.write(0x82), SIM800.write(strlen(MQTT_topic) + 5);         // сумма пакета
  SIM800.write((byte)0), SIM800.write(0x01), SIM800.write((byte)0); // просто так нужно
  SIM800.write(strlen(MQTT_topic)), SIM800.write(MQTT_topic);       // топик
  SIM800.write((byte)0);
}

void getLocation()
{
  SIM800.println("AT+CIPGSMLOC=1,1");
}
void MQTT_PUB_ALL()
{
  Vbat = 0; //VoltRead(); // замеряем напряжение на батарее
  if (sendtry > 2)
  {
    Serial.println("Reset by sendtry");
    SIM800_reset();
  }
  if (broker == true)
  {
    sendtry++;
    Serial.println("MQTT_PUB_ALL sendtry=" + String(sendtry));
    SIM800.println("AT+CIPSEND"), delay(200); // если не "влезает" "ALREADY CONNECT"
    MQTT_FloatPub("C5/vbat", Vbat, 2);
    MQTT_FloatPub("C5/timer1", Timer1, 0);
    MQTT_FloatPub("C5/timer2", Timer2, 0);
    MQTT_PUB("C5/security", Security ? "lock1" : "lock0");
    MQTT_PUB("C5/relay1", relay1 ? "start" : "stop");
    MQTT_PUB("C5/relay2", relay2 ? "start" : "stop");
    MQTT_PUB("C5/location", LOC.c_str());
    MQTT_FloatPub("C5/uptime", millis() / 3600000, 0);
    MQTT_FloatPub("C5/C", error_C, 0);
    MQTT_FloatPub("C5/CF", error_CF, 0);
    SIM800.write(0x1A);
    interval = 6;
    modeTimer = MODE_CONNECT_MQTT_TIMER;
  }
  else
  {
    interval = 1;
    init_gprs();
  }
}

void init_gprs()
{
  mode = MODE_INIT_GPRS;
  modeTimer = MODE_INIT_GPRS_TIMER;
  SIM800.println("AT+SAPBR=2,1");
}

void resp_modem()
{ //------------------ АНЛИЗИРУЕМ БУФЕР ВИРТУАЛЬНОГО ПОРТА МОДЕМА------------------------------
  String at = "";
  //    while (SIM800.available()) at = SIM800.readString();  // набиваем в переменную at
  int k = 0;
  while (SIM800.available())
    k = SIM800.read(), at += char(k), delay(1);
  Serial.println("mode:"+String(mode)+"resp:" + String(at));

  switch (mode)
  {
  case MODE_RESET:
  {
    if (at.indexOf("SMS Ready") > -1 || at.indexOf("NO CARRIER") > -1)
    {
      Serial.println("Get: SMS READY. Send: AT+CLIP=1;+DDET=1");
      SIM800.println("AT+CLIP=1;+DDET=1"); // Активируем АОН и декодер DTMF
      mode = MODE_RESET_MODEM;
      modeTimer = MODE_RESET_MODEM_TIMER;
    }
    break;
  }
  case MODE_RESET_MODEM:
  {
    if (at.indexOf("OK") > -1)
    {
      init_gprs();
    } else {
      Serial.println("RESET_MODEM ERROR");
    }
    
  }

  case MODE_INIT_GPRS:
  {
    if (at.indexOf("+SAPBR: 1,3") > -1)
    {
      /// bearer is closed, set CONTYPE
      SIM800.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
      mode = MODE_INIT_GPRS_SETCONTYPE;
      modeTimer = MODE_INIT_GPRS_SETCONTYPE_TIMER;
    }
    if (at.indexOf("DEACT") > -1){
      
    }
  }
  case MODE_INIT_GPRS_SETCONTYPE:
  {
    if (at.indexOf("OK") > -1)
    {
      SIM800.println("AT+SAPBR=3,1, \"APN\",\"" + APN + "\""); // , delay (500);
      mode = MODE_INIT_GPRS_SETAPN;
      modeTimer = MODE_INIT_GPRS_SETAPN_TIMER;
    }
  }
  case MODE_INIT_GPRS_SETAPN:
  {
    if (at.indexOf("OK") > -1)
    {
      SIM800.println("AT+SAPBR=1,1"); // устанавливаем соеденение open bearer
      mode = MODE_INIT_GPRS_CONNECT;
      modeTimer = MODE_INIT_GPRS_CONNECT_TIMER;
    }
  }
  case MODE_INIT_GPRS_CONNECT:
  {
    if (at.indexOf("OK") > -1)
    {
      SIM800.println("AT+CIPSTART=\"TCP\",\"" + MQTT_SERVER + "\",\"" + PORT + "\""), delay(200);
      mode = MODE_CONNECT_MQTT;
      modeTimer = MODE_CONNECT_MQTT_TIMER;
    }
  }
  case MODE_CONNECT_MQTT:
  {
    if (at.indexOf("CONNECT FAIL") > -1)
    {
      SIM800.println("AT+CFUN=1,1"), error_CF++, delay(1000), interval = 3; // костыль 1
      mode=MODE_SETFULL_FUNCTIONALITY;
      modeTimer=MODE_SETFULL_FUNCTIONALITY_TIMER;
    }
    else if (at.indexOf("CLOSED") > -1)
    {
      SIM800.println("AT+CFUN=1,1"), error_C++, delay(1000), interval = 3; // костыль 2
      mode=MODE_SETFULL_FUNCTIONALITY;
      modeTimer=MODE_SETFULL_FUNCTIONALITY_TIMER;
    }
    else if (at.indexOf("+CME ERROR:") > -1)
    {
      error_CF++; // костыль 4
      if (error_CF > 5)
      {
        error_CF = 0, SIM800_reset();
      }
    }
    else if (at.indexOf("CONNECT OK") > -1)
    {
      MQTT_CONNECT();
    }
    else if (at.indexOf("SEND OK"))
    {
      mode = MODE_MQTT_CONNECTED;
      modeTimer = MODE_MQTT_CONNECTED_TIMER;
    }
  }
  case MODE_SETFULL_FUNCTIONALITY:{
    if (at.indexOf("OK") > -1)
    {
      init_gprs();
    }
    if(at.indexOf("+CME ERROR")>-1){
      Serial.println("+CFUN error!");
      SIM800_reset();
    }
  }
  case MODE_MQTT_CONNECTED:
  {
    if (at.indexOf("SEND OK") > -1)
    {
      sendtry--;
      Serial.println("Sended data");
    }
    else if (at.indexOf("C5/settimer1", 4) > -1)
    {
      if (relay1 == true)
      {
        Timer1 = at.substring(at.indexOf("") + 16, at.indexOf("") + 19).toInt();
        if (Timer1 > 30)
          Timer1 = 30;
      }
      else
      {
        defaultTimer1 = at.substring(at.indexOf("") + 16, at.indexOf("") + 19).toInt();
        if (defaultTimer1 > 30)
          defaultTimer1 = 30;
        Timer1 = defaultTimer1;
      }
      MQTT_PUB_ALL();
    }
    else if (at.indexOf("C5/settimer2", 4) > -1)
    {
      if (relay2)
      {
        Timer2 = at.substring(at.indexOf("") + 16, at.indexOf("") + 19).toInt();
      }
      else
      {
        defaultTimer2 = at.substring(at.indexOf("") + 16, at.indexOf("") + 19).toInt();
        Timer2 = defaultTimer2;
      }
      MQTT_PUB_ALL();
    }
    else if (at.indexOf("C5/comandbalans", 4) > -1)
    {
      SIM800.println("AT+CUSD=1,\"*100#\""); // запрос баланса
    }
    else if (at.indexOf("C5/comandrssi", 4) > -1)
    {
      SIM800.println("AT+CSQ"); // запрос уровня сигнала
    }
    else if (at.indexOf("C5/comandlocation", 4) > -1)
    {
      SIM800.println("AT+CIPGSMLOC=1,1"); // запрос локации
    }
    else if (at.indexOf("C5/comandrelay1stop", 4) > -1)
    {
      relay1stop(); // команда остановки прогрева
    }
    else if (at.indexOf("C5/comandrelay1start", 4) > -1)
    {
      relay1start(); // команда запуска прогрева
    }
    else if (at.indexOf("C5/comandrelay2stop", 4) > -1)
    {
      relay2stop(); // команда остановки прогрева
    }
    else if (at.indexOf("C5/comandrelay2start", 4) > -1)
    {
      relay2start(); // команда запуска прогрева
    }
    else if (at.indexOf("C5/comandRefresh", 4) > -1)
    {
      MQTT_PUB_ALL();
      interval = 6;
      at = "";
    }
  }
  
default: {
  if (at.indexOf("+CLIP: \"" + call_phone + "\",") > -1)
  {
    delay(200);
    SIM800.println("AT+DDET=1");
    delay(200);
    SIM800.println("ATA");
  }
  else if (at.indexOf("+CIPSEND") > -1 && at.indexOf("ERROR") > -1)
  {
    Serial.println("Connetion break error. Reset modem.");
    error_C++;
    SIM800_reset();
  }
  else if (at.indexOf("+DTMF: ") > -1)
  {
    String key = at.substring(at.indexOf("") + 9, at.indexOf("") + 10);
    pin = pin + key;
    if (pin.indexOf("*") > -1)
      pin = "";
  }
  else if (at.indexOf("+CIPGSMLOC: 0,") > -1)
  {
    LOC = at.substring(26, 35) + "," + at.substring(16, 25);
  }

  else if (at.indexOf("+CUSD:") > -1)
  {
    String BALANS = at.substring(26);
    SIM800.println("AT+CIPSEND"), delay(200);
    MQTT_PUB("C5/ussd", BALANS.c_str()), SIM800.write(0x1A);
  }

  else if (at.indexOf("+CSQ:") > -1)
  {
    String RSSI = at.substring(at.lastIndexOf(":") + 1, at.lastIndexOf(",")); // +CSQ: 31,0
    SIM800.println("AT+CIPSEND"), delay(200);
    MQTT_PUB("C5/rssi", RSSI.c_str()), SIM800.write(0x1A);
  }
  else if (at.indexOf("ALREAD") > -1)
  {
    if (broker)
    {

      MQTT_PUB_ALL();
      interval = 6;
    }
    else
    {
      mode = MODE_CONNECT_MQTT;
      modeTimer = MODE_CONNECT_MQTT_TIMER;
      MQTT_CONNECT();
    }
  }
  if (pin.indexOf("11") > -1)
  {
    pin = "",
    playDtmf();
    relay1start();
  }
  if (pin.indexOf("12") > -1)
  {
    pin = "",
    playDtmf();
    relay2start();
  }
  else if (pin.indexOf("777") > -1)
  {
    pin = "",
    SIM800.println("ATH0");
    Serial.println("Model reset by user");
    error_C++;
    SIM800_reset();
  }
  else if (pin.indexOf("00") > -1)
  {
    pin = "",
    delay(1500),
    playDtmf(),
    relay1stop(),
    relay2stop(),
    SIM800.println("ATH0");
  }
  else if (pin.indexOf("#") > -1)
  {
    pin = "", SIM800.println("ATH0");
  }
}
}
}
