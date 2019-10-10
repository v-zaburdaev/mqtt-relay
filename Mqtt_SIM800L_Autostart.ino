
#include <SoftwareSerial.h>
//#include <DallasTemperature.h>      // https://github.com/milesburton/Arduino-Temperature-Control-Library

//  ----------------------------------------- НАЗНАЧАЕМ ВЫВОДЫ для платок до 1.7.6 (c Arduino Pro Mini) ------------------------------

SoftwareSerial SIM800(7, 6);                // для старых плат начиная с версии RX,TX
#define ONE_WIRE_BUS 10                      // пин датчика DS18B20, https://github.com/PaulStoffregen/OneWire
#define FIRST_P_Pin  8                      // на реле первого положения замка зажигания с 8-го пина ардуино
#define SECOND_P     9                      // на реле зажигания, через транзистор с 9-го пина ардуино
#define STARTER_Pin  12                     // на реле стартера, через транзистор с 12-го пина ардуино
#define Lock_Pin     4                     // реле на кнопку "заблокировать дверь"
#define Unlock_Pin   11                     // реле на кнопку "разаблокировать дверь"
#define LED_Pin      13                     // на светодиод (моргалку) 6-й транзистор
#define BAT_Pin      A0                     // на батарею, через делитель напряжения 39кОм / 11 кОм
#define Feedback_Pin A1                     // на провод от замка зажигания для обратной связи по проводу ON
#define STOP_Pin     A2                     // на концевик педали тормоза для отключения режима прогрева
#define PSO_Pin      A3                     // на прочие датчики через делитель 39 kOhm / 11 kΩ
#define K5           A5                     // на плате не реализован, снимать сигнал с ардуинки
#define IMMO         A4                     // на плате не реализован, снимать сигнал с ардуинк
#define RESET_Pin    5                      // аппаратная перезагрузка модема, по сути не задействован

//  ----------------------------------------- НАЗНАЧАЕМ ВЫВОДЫ для платок от 5.3.0  (c Atmega328 на самой плате)---------------------
/*
  SoftwareSerial SIM800(4, 5);                // для новых плат начиная с 5.3.0 пины RX,TX
  #define ONE_WIRE_BUS A5                     // пин датчика DS18B20, библиотека тут https://github.com/PaulStoffregen/OneWire
  #define FIRST_P_Pin  10                     // на реле K1 на плате ПОТРЕБИТЕЛИ
  #define SECOND_P     12                     // на реле К3 на плате ЗАЖИГАНИЕ
  #define STARTER_Pin  11                     // на реле К2 на плате СТАРТЕР
  #define IMMO         9                      // на реле K4 на плате под иммобилайзер
  #define K5           8                      // на реле K5  внешнее под различные нужды, програмно не реализован
  #define Lock_Pin     6                      // на реле K6 внешнее на кнопку "заблокировать дверь"
  #define Unlock_Pin   7                      // на реле K7 внешнее на кнопку "разаблокировать дверь"
  #define LED_Pin      13                     // на светодиод на плате
  #define STOP_Pin     A0                     // вход IN3 на концевик педали тормоза для отключения режима прогрева
  #define PSO_Pin      A1                     // вход IN4  на прочие датчики через делитель 39 kOhm / 11 kΩ
  #define PSO_F        A2                     // обратная связь по реле K1, проверка на ключ в замке
  #define RESET_Pin    A3                     // аппаратная перезагрузка модема, по сути не задействован
  #define BAT_Pin      A7                     // внутри платы соединен с +12, через делитель напряжения 39кОм / 11 кОм
  #define Feedback_Pin A6                     // обратная связь по реле K3, проверка на включенное зажигание
*/

//OneWire oneWire(ONE_WIRE_BUS);
//DallasTemperature sensors(&oneWire);
/*  ----------------------------------------- НАСТРОЙКИ MQTT брокера---------------------------------------------------------   */
const char MQTT_user[10] = "kgltdagp";      // api.cloudmqtt.com > Details > User
const char MQTT_pass[15] = "KeIN2CNFNLHp";  // api.cloudmqtt.com > Details > Password
const char MQTT_type[15] = "MQIsdp";        // тип протокола НЕ ТРОГАТЬ !
const char MQTT_CID[15] = "STAREX";        // уникальное имя устройства в сети MQTT
String MQTT_SERVER = "m15.cloudmqtt.com";   // api.cloudmqtt.com > Details > Server  сервер MQTT брокера
String PORT = "12319";                      // api.cloudmqtt.com > Details > Port    порт MQTT брокера НЕ SSL !
/*  ----------------------------------------- ИНДИВИДУАЛЬНЫЕ НАСТРОЙКИ !!!---------------------------------------------------------   */
String call_phone =  "+79202544485";       // телефон входящего вызова  для управления DTMF
//String call_phone2 = "+375000000001";       // телефон для автосброса могут работать не корректно
//String call_phone3 = "+375000000002";       // телефон для автосброса
//String call_phone4 = "+375000000003";       // телефон для автосброса
String APN = "internet.mts.ru";             // тчка доступа выхода в интернет вашего сотового оператора
enum mode {
  RESET,
  SIMINIT,

}
/*  ----------------------------------------- ДАЛЕЕ НЕ ТРОГАЕМ ---------------------------------------------------------------   */
//float Vstart = 13.20;                        // порог распознавания момента запуска по напряжению
String pin = "";                            // строковая переменная набираемого пинкода
//float TempDS[11];                           // массив хранения температуры c рахных датчиков
float Vbat, V_min;                                // переменная хранящая напряжение бортовой сети
//float m = 68.01;                            // делитель для перевода АЦП в вольты для резистров 39/11kOm
float m = 57.701915071; //58.3402489626556;                            // делитель для перевода АЦП в вольты для резистров 39/10kOm
unsigned long Time1, Time2 = 0;
int Timer1 = 10, Timer2 = 10, count, error_CF, error_C, defaultTimer1=10, defaultTimer2=10;
int interval = 1;                           // интервал тправки данных на сервер после загрузки ардуино
bool relay1 = false, relay2=false;          // переменная состояния режим прогрева двигателя
bool ring = false;                          // флаг момента снятия трубки
bool broker = false;                        // статус подклюлючения к брокеру
bool Security = false;                      // состояние охраны после подачи питания
String LOC="";

void setup() {
  pinMode(RESET_Pin, OUTPUT);
  pinMode(FIRST_P_Pin, OUTPUT);
  pinMode(SECOND_P,    OUTPUT);
  pinMode(LED_Pin,     OUTPUT);
  pinMode(3, INPUT_PULLUP);                 //  для плат до 1.7.2 с оптопарами
  pinMode(2, INPUT_PULLUP);                 //  для плат до 1.7.2 с оптопарами

  delay(100);
  Serial.begin(9600);                       //скорость порта
  SIM800.begin(9600);                       //скорость связи с модемом
  Serial.println("MQTT |06/10/2019");
  SIM800_reset();
}



void SIM800_reset() {
  broker=false;
  digitalWrite(RESET_Pin, LOW);
  delay(400);
  digitalWrite(RESET_Pin, HIGH);
  SIM800.println("AT+CLIP=1;+DDET=1;+CFUN=1,1;"); // Активируем АОН и декодер DTMF
  delay(100);
}                        // перезагрузка модема

// функция дергания реле блокировки/разблокировки дверей с паузой "удержания кнопки" в 0,5 сек.
void blocking (bool st) {
  digitalWrite(st ? Lock_Pin : Unlock_Pin, HIGH), delay(500), digitalWrite(st ? Lock_Pin : Unlock_Pin, LOW), Security = st, Serial.println(st ? "На охране" : "Открыто");
}
void callback()     {
  SIM800.println("ATD" + call_phone + ";"),    delay(3000); // обратный звонок при появлении напряжения на входе IN1
}

void loop() {

  if (SIM800.available())  resp_modem();                                    // если что-то пришло от SIM800 в Ардуино отправляем для разбора
  if (Serial.available())  resp_serial();                                 // если что-то пришло от Ардуино отправляем в SIM800
  if (millis() > Time2 + 60000) {
    Time2 = millis();
    bool timerchgflag=false;
    if (relay1==true && Timer1 > 0 ) Timer1--, Serial.print("Тм:"), Serial.println (Timer1), timerchgflag=true;;
    if (relay2==true && Timer2 > 0 ) Timer2--, Serial.print("Тм:"), Serial.println (Timer2), timerchgflag=true;;
    if(timerchgflag==true){
      MQTT_PUB_ALL();
      }
  }

  if (millis() > Time1 + 10000) Time1 = millis(), detection();             // выполняем функцию detection () каждые 10 сек
  if (relay1 == true &&  digitalRead(STOP_Pin) == 1) relay1stop();         // для платок 1,7,2
  if (relay2 == true &&  digitalRead(STOP_Pin) == 1) relay2stop();         // для платок 1,7,2

}



void relay1start() {                                              // программа запуска двигателя
    Serial.println("Relay 1 ON");
    Timer1 = defaultTimer1;                                                     // устанавливаем таймер
    pushRelay1();
    relay1=true;
    MQTT_PUB_ALL();
    Serial.println ("OUT"), interval = 1;
}
void relay1stop() {                                // программа остановки прогрева двигателя
  if(!(defaultTimer1==30 && Timer1==0)){
    pushRelay1();
  } 
  
  relay1 = false, Timer1 = defaultTimer1;
  Serial.println ("Relay 1 OFF");
  MQTT_PUB_ALL();
}

void pushRelay1(){
    digitalWrite(FIRST_P_Pin, HIGH);
    delay (800);
    digitalWrite(FIRST_P_Pin, LOW);
  }

void relay2start() {                                              // программа запуска двигателя
    Serial.println("Relay 2 ON");
    Timer1 = defaultTimer1;                                                     // устанавливаем таймер
    digitalWrite(SECOND_P, HIGH);
    relay2=true;
    MQTT_PUB_ALL();
    interval = 1;
}

void relay2stop() {                                // программа остановки прогрева двигателя
  digitalWrite(SECOND_P,    LOW), delay (100);
  relay2 = false, Timer2 = defaultTimer2;
  Serial.println ("Relay 2 OFF");
  MQTT_PUB_ALL();
}



float VoltRead()    {                                             // замеряем напряжение на батарее и переводим значения в вольты
  float ADCC = analogRead(BAT_Pin);
  float realadcc = ADCC;
  ADCC = ADCC / m ;
  Serial.print("АКБ: "), Serial.print(ADCC), Serial.print("V "), Serial.println(realadcc) ;
  if (ADCC < V_min) V_min = ADCC;
  return (ADCC);
}                  // переводим попугаи в вольты





void detection() {                                                // условия проверяемые каждые 10 сек


//  Serial.print("Инт:"), Serial.println(interval);

  if (relay1 == true && Timer1 < 1)    relay1stop();     // остановка прогрева если закончился отсчет таймера
  if (relay2 == true && Timer2 < 1)    relay2stop();     // остановка прогрева если закончился отсчет таймера
  
  interval--;
  if (interval==2) {getLocation();}
    if (interval < 1 && broker==false) {
      Serial.println("Connect to MQTT Broker.");
      interval = 1, SIM800.println("AT+SAPBR=2,1"), delay (30);   // подключаемся к GPRS
    }
    if (interval < 1) interval = 6, MQTT_PUB_ALL();
    
}

void playDtmf(){
    SIM800.println("AT+VTD=1;+VTS=1");
    delay(400);
    SIM800.println("AT+VTD=1;+VTS=0");
  }



void resp_serial () {    // ---------------- ТРАНСЛИРУЕМ КОМАНДЫ из ПОРТА В МОДЕМ ----------------------------------
  String at = "";
  int k = 0;
  while (Serial.available()) k = Serial.read(), at += char(k), delay(1);
  SIM800.println(at), at = "";
}


void  MQTT_FloatPub (const char topic[15], float val, int x) {
  char st[10];
  dtostrf(val, 0, x, st), MQTT_PUB (topic, st);
}

void MQTT_CONNECT () {
  Serial.println("MQTT_CONNECT");
  SIM800.println("AT+CIPSEND"), delay (100);

  SIM800.write(0x10);                                                              // маркер пакета на установку соединения
  SIM800.write(strlen(MQTT_type) + strlen(MQTT_CID) + strlen(MQTT_user) + strlen(MQTT_pass) + 12);
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_type)), SIM800.write(MQTT_type); // тип протокола
  SIM800.write(0x03), SIM800.write(0xC2), SIM800.write((byte)0), SIM800.write(0x3C); // просто так нужно
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_CID)),  SIM800.write(MQTT_CID);  // MQTT  идентификатор устройства
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_user)), SIM800.write(MQTT_user); // MQTT логин
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_pass)), SIM800.write(MQTT_pass); // MQTT пароль

  MQTT_PUB ("C5/status", "Подключено");                                            // пакет публикации
  MQTT_SUB ("C5/comand");                                                          // пакет подписки на присылаемые команды
  MQTT_SUB ("C5/comandrelay1");                                                          // пакет подписки на присылаемые команды
  MQTT_SUB ("C5/comandrelay2");                                                          // пакет подписки на присылаемые команды
  MQTT_SUB ("C5/settimer1");                                                        // пакет подписки на присылаемые значения таймера
  MQTT_SUB ("C5/settimer2");                                                        // пакет подписки на присылаемые значения таймера1
  SIM800.write(0x1A);
  broker = true;
  interval=6;
}                                         // маркер завершения пакета

void  MQTT_PUB (const char MQTT_topic[35], const char MQTT_messege[35]) {          // пакет на публикацию

  SIM800.write(0x30), SIM800.write(strlen(MQTT_topic) + strlen(MQTT_messege) + 2);
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_topic)), SIM800.write(MQTT_topic); // топик
  SIM800.write(MQTT_messege);
}                                                  // сообщение

void  MQTT_SUB (const char MQTT_topic[15]) {                                       // пакет подписки на топик
  Serial.println("MQTT_SUB");
  SIM800.write(0x82), SIM800.write(strlen(MQTT_topic) + 5);                        // сумма пакета
  SIM800.write((byte)0), SIM800.write(0x01), SIM800.write((byte)0);                // просто так нужно
  SIM800.write(strlen(MQTT_topic)), SIM800.write(MQTT_topic);                      // топик
  SIM800.write((byte)0);
}

void getLocation(){
  SIM800.println("AT+CIPGSMLOC=1,1");
  }
void MQTT_PUB_ALL(){
    Vbat = VoltRead();                                            // замеряем напряжение на батарее
    if(broker==true){
      Serial.println("MQTT_PUB_ALL");
      SIM800.println("AT+CIPSEND"), delay (200); // если не "влезает" "ALREADY CONNECT"
      MQTT_FloatPub ("C5/vbat",     Vbat, 2);
      MQTT_FloatPub ("C5/timer1",    Timer1, 0);
      MQTT_FloatPub ("C5/timer2",    Timer2, 0);
      MQTT_PUB      ("C5/security", Security ? "lock1" : "lock0");
      MQTT_PUB      ("C5/relay1",   relay1 ? "start" : "stop");
      MQTT_PUB      ("C5/relay2",   relay2 ? "start" : "stop");
      MQTT_PUB      ("C5/location", LOC.c_str());
      MQTT_FloatPub ("C5/uptime",   millis() / 3600000, 0);
      MQTT_FloatPub ("C5/C", error_C, 0);
      MQTT_FloatPub ("C5/CF", error_CF, 0);
      SIM800.write(0x1A);
      interval=6;
    } else {
      interval = 1, SIM800.println("AT+SAPBR=2,1"), delay (20);
    }
  }

void resp_modem () {    //------------------ АНЛИЗИРУЕМ БУФЕР ВИРТУАЛЬНОГО ПОРТА МОДЕМА------------------------------
  String at = "";
  //    while (SIM800.available()) at = SIM800.readString();  // набиваем в переменную at
  int k = 0;
  while (SIM800.available()) k = SIM800.read(), at += char(k), delay(1);
  Serial.println  ("resp:"+String(at));
  if (at.indexOf("+CLIP: \"" + call_phone + "\",") > -1) {
    delay(200);
    SIM800.println("AT+DDET=1");
    delay(200);
    SIM800.println("ATA");
  } 
  else if(at.indexOf("+CIPSEND")>-1 && at.indexOf("ERROR")>-1){
        Serial.println("Connetion break error. Reset modem.");
        error_C++;
        SIM800_reset();
  } else if (at.indexOf("+DTMF: ")  > -1)        {
    String key = at.substring(at.indexOf("") + 9, at.indexOf("") + 10);
    pin = pin + key;
    if (pin.indexOf("*") > -1 ) pin = "";
  }
  else if (at.indexOf("SMS Ready") > -1 || at.indexOf("NO CARRIER") > -1 ) {
    SIM800.println("AT+CLIP=1;+DDET=1"); // Активируем АОН и декодер DTMF
  }
  /*  -------------------------------------- проверяем соеденеиние с ИНТЕРНЕТ, конектимся к серверу------------------------------------------------------- */
  else if (at.indexOf("+SAPBR: 1,3") > -1)                                  {
    SIM800.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""); //, delay(200);
  }
  else if (at.indexOf("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\r\nOK") > -1)    {
    SIM800.println("AT+SAPBR=3,1, \"APN\",\"" + APN + "\"");// , delay (500);
  }
  else if (at.indexOf("AT+SAPBR=3,1, \"APN\",\"" + APN + "\"\r\r\nOK") > -1 )   {
    SIM800.println("AT+SAPBR=1,1"), interval = 1, delay (200) ; // устанавливаем соеденение
  }
  else if (at.indexOf("+SAPBR: 1,1") > -1 )        {
    delay (200),  
    SIM800.println("AT+CIPSTART=\"TCP\",\"" + MQTT_SERVER + "\",\"" + PORT + "\""), delay (200);
  }
  else if (at.indexOf("CONNECT FAIL") > -1 )       {
    SIM800.println("AT+CFUN=1,1"), error_CF++, delay (1000), interval = 3 ; // костыль 1
  }
  else if (at.indexOf("CLOSED") > -1 )             {
    SIM800.println("AT+CFUN=1,1"), error_C++,  delay (1000), interval = 3 ; // костыль 2
  }
  else if (at.indexOf("+CME ERROR:") > -1 )        {
    error_CF++;  // костыль 4
    if (error_CF > 5) {
      error_CF = 0, SIM800_reset();
    }
  }
  else if (at.indexOf("CONNECT OK") > -1)          {
    MQTT_CONNECT();
  }


  else if (at.indexOf("+CIPGSMLOC: 0,") > -1   )   {
    LOC = at.substring(26, 35) + "," + at.substring(16, 25);
  }

  else if (at.indexOf("+CUSD:") > -1   )           {
    String BALANS = at.substring(26);
    SIM800.println("AT+CIPSEND"), delay (200);
    MQTT_PUB ("C5/ussd", BALANS.c_str()), SIM800.write(0x1A);
  }

  else if (at.indexOf("+CSQ:") > -1   )            {
    String RSSI = at.substring(at.lastIndexOf(":") + 1, at.lastIndexOf(",")); // +CSQ: 31,0
    SIM800.println("AT+CIPSEND"), delay (200);
    MQTT_PUB ("C5/rssi", RSSI.c_str()), SIM800.write(0x1A);
  }
  else if (at.indexOf("ALREAD") > -1) {
    if(broker){
        MQTT_PUB_ALL();
        interval=6;
      } else {
        MQTT_CONNECT();
      }
  }



  else if (at.indexOf("C5/comandlock1", 4) > -1 )      {
    blocking(1), attachInterrupt(1, callback, FALLING); // команда постановки на охрану и включения прерывания по датчику вибрации
  }
  else if (at.indexOf("C5/comandlock0", 4) > -1 )      {
    blocking(0), detachInterrupt(1); // команда снятия с хораны и отключения прерывания на датчик вибрации
  }
  else if (at.indexOf("C5/settimer1", 4) > -1 )         {
    if(relay1==true) {
      Timer1 =at.substring(at.indexOf("") + 16, at.indexOf("") + 19).toInt();
      if(Timer1>30) Timer1=30;
    } else {
      defaultTimer1 = at.substring(at.indexOf("") + 16, at.indexOf("") + 19).toInt();
      if(defaultTimer1>30) defaultTimer1 =30;
      Timer1 = defaultTimer1;
      }
      MQTT_PUB_ALL();
    }
  else if (at.indexOf("C5/settimer2", 4) > -1 )         {
    if(relay2) {
      Timer2 = at.substring(at.indexOf("") + 16, at.indexOf("") + 19).toInt();
    } else {
      defaultTimer2 = at.substring(at.indexOf("") + 16, at.indexOf("") + 19).toInt();
      Timer2 = defaultTimer2;
      }
      MQTT_PUB_ALL();
    }
  else if (at.indexOf("C5/comandbalans", 4) > -1 )     {
    SIM800.println("AT+CUSD=1,\"*100#\"");  // запрос баланса
  }
  else if (at.indexOf("C5/comandrssi", 4) > -1 )       {
    SIM800.println("AT+CSQ");  // запрос уровня сигнала
  }
  else if (at.indexOf("C5/comandlocation", 4) > -1 )   {
    SIM800.println("AT+CIPGSMLOC=1,1");  // запрос локации
  }
  else if (at.indexOf("C5/comandrelay1stop", 4) > -1 )       {
    relay1stop();  // команда остановки прогрева
  }
  else if (at.indexOf("C5/comandrelay1start", 4) > -1 )      {
    relay1start();  // команда запуска прогрева
  }
  else if (at.indexOf("C5/comandrelay2stop", 4) > -1 )       {
    relay2stop();  // команда остановки прогрева
  }
  else if (at.indexOf("C5/comandrelay2start", 4) > -1 )      {
    relay2start();  // команда запуска прогрева
  }
  else if (at.indexOf("C5/comandRefresh", 4) > -1 )    { 
    // Serial.println ("Команда обнвления");
    MQTT_PUB_ALL();
    interval = 6; // швырнуть данные на сервер и ждать 60 сек
    at = "";
  }                                                  // Возвращаем ответ можема в монитор порта , очищаем переменную

  if (pin.indexOf("11") > -1 ) {
    pin = "", 
    playDtmf();
    relay1start();
  }
  if (pin.indexOf("12") > -1 ) {
    pin = "", 
    playDtmf();
    relay2start();
  }
  else if (pin.indexOf("777") > -1 ) {
    pin = "", 
    SIM800.println("ATH0");
    Serial.println("Model reset by user");
    error_C++;
    SIM800_reset();
  }  
  else if (pin.indexOf("00") > -1 ) {
    pin = "", 
    delay(1500), 
    playDtmf(),
    relay1stop(),
    relay2stop(),
    SIM800.println("ATH0");
  } else if (pin.indexOf("#")   > -1 ) {
    pin = "", SIM800.println("ATH0");
  }

}


//void blocking (bool st) {digitalWrite(Lock_Pin, st ? HIGH : LOW), Security = st, Serial.println(st ? "На охране":"Открыто");} // функция удержания реле блокировки/разблокировки на выходе out4
