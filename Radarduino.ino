/*
 * RADARDUINO
 * Radar implementado con arduino y controlado por un servidor web
 * 
 * Autores:
 * Belver Prieto, Manuel
 * Maseli Martín, Vanesa
 * Santiago Corral, María
 */

//LCD library
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>

//Password and ssid
#define ssid "Radarduino"
#define pass "arduinomega"

//Constants for LCD and I2C
#define I2C_ADDR    0x3F  // Memory Address for I2C PCF8574AT 
#define  LED_OFF  0
#define  LED_ON  1

//LCD instance
//                    addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(I2C_ADDR, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

//Digital pins for laser diodes
#define LASERPin1 26
#define LASERPin2 28

//Analogic pins for LDR photorresistors
#define LDRPin1 A0
#define LDRPin2 A1

//Digital pins for matrix
#define matriz_on 46
#define no_blue 48
#define no_red 49
#define no_green 47

//Some constants
#define threshold 500 //For LDR 
#define margen 10     //For speed

//Program variables
int input1;
int input2;
boolean coche=false;
unsigned long tiempo1;
unsigned long tiempo2; //globales ambas
unsigned long diferencia;
String http_request ="";
String line="";

//Control variables
int vel_max=50;
unsigned long espacio =10000; //10m
int encendido=0;

// String array with orders to initialize Wifi module ESP8266
String ordenes[]={
  "AT+RST",                         //Reset
  "AT+CWMODE=3",                    //1 = Sta, 2 = AP, 3 = both
  ("AT+CWJAP=%s,%s",ssid,pass),     //Join AP with ssid and pass
  "AT+CIFSR",                       //Get IP address
  "AT+CIPMUX=1",                    //Set multiple conection
  "AT+CIPSERVER=1,80",              //Set as server (mode=1 (multiple connection), port=80)
  "END"                             //Para reconocer el fin de los comandos AT
};

/******************************************************************************
 * 
 * SETUP AND LOOP
 * 
 ******************************************************************************/

void setup(){ 
  Serial1.begin(115200);
  Serial.begin(115200);
  setup_radar();
  setup_lcd();  
  setup_matrix();
  setup_wifi();
  delay (1000);
}

void loop(){
  checkWifi();
  if (encendido==1){
    checkLasers();
    if (coche){
      unsigned long velocidad = calculo_velocidad(espacio,diferencia);
      int coef=coefVel(velocidad);
      show_speed(velocidad, coef);
    }
  }
}

/************************************************
 *                                              *
 * Setup methods                                *
 *                                              *
 ************************************************/
void setup_radar(){
  pinMode(LASERPin1, OUTPUT);
  pinMode(LDRPin1, INPUT);
  pinMode(LASERPin2, OUTPUT);
  pinMode(LDRPin2, INPUT);
  digitalWrite(LASERPin1, LOW);
  digitalWrite(LASERPin2, LOW);
}
void setup_lcd(){
  lcd.begin (16,2);  // inicializar lcd 
  lcd.setBacklight(LED_OFF);
  lcd.clear();
}
void setup_matrix(){
  pinMode(matriz_on, OUTPUT);
  pinMode(no_blue, OUTPUT);
  pinMode(no_red, OUTPUT);
  pinMode(no_green, OUTPUT);
  
  digitalWrite(matriz_on, HIGH);
  digitalWrite(no_blue, HIGH);
  digitalWrite(no_red, HIGH);
  digitalWrite(no_green, HIGH);
}

//Executes the array of ordes to initialize the wifi and waits for answers
void setup_wifi(){
  int index = 0;
  while(ordenes[index] != "END"){  
      Serial1.println(ordenes[index++]);
      while (true){
        String s = get_line_wifi();
        if ( s!= "") Serial.println(s);
        if ( s.startsWith("no change"))   
          break;
        if ( s.startsWith("OK"))   
          break;
        if ( s.startsWith("ready"))   
          break;  
      }
      Serial.println("....................");
    }
}

/**********************************************************************************
 * 
 * Loop methods
 * 
 **********************************************************************************/
//---------------------Radar-------------------------------
//Check if there is a car passing though the lasers
void checkLasers(){
  input1=analogRead(LDRPin1);
  input2=analogRead(LDRPin2);
  if(input1<threshold){
    tiempo2=0;
    tiempo1=micros();
  }
  if(input2<threshold){
    tiempo2=micros();
  }
  if(tiempo1!=0&&tiempo2!=0){ //If a car had cross
    Serial.println("Coche detectado");
    diferencia = tiempo2-tiempo1;
    tiempo1=0;
    tiempo2=0;
    coche=true;
    Serial.println("--------------");
  }
}

//---------------LCD and matrix methods-----------------
//Show speed in LCD display and matrix
void show_speed(long vel, int coef) {
  Serial.println("Distancia --------------  360000");
  Serial.print("Coche salida ----  Tiempo ");
  Serial.println(diferencia);
  Serial.print(vel);
  Serial.println(" km/h");
  Serial.println("--------------------------");
  Serial.println();
  encender_LED(vel,coef);
  matrix(coef);
  apagar_LED();  
  coche=false;
  diferencia=0;
}
void apagar_LED(){
  lcd.setBacklight(LED_OFF);
  lcd.clear();
}
void encender_LED(long vel, int coef){
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(0, 0);
  String lcd_input="";
  lcd_input+="Velocidad: ";
  lcd_input+=vel;
  lcd_input+=" km/h";
  lcd.print(lcd_input);
  lcd_input="";
  lcd.setCursor(0, 1);
  lcd_input+=coef;
  lcd_input+="% vel max";
  lcd.print(lcd_input);
  lcd_input="";
}

//Matrix
void matrix(int coef) {
  if(coef<(100-margen)){
    green();
  }
  else if (coef>(100+margen)){
    red();
  }
  else{
    yellow();
  }
}

void red(){
  long tiempo;
  tiempo=micros();
  do{
    digitalWrite(no_red, LOW);
    digitalWrite(no_red, HIGH);
  }while(micros()<(tiempo+2000000));
}

void green(){
  long tiempo;
  tiempo=micros();
  do{
    digitalWrite(no_green, LOW);
    digitalWrite(no_green, HIGH);
  }while(micros()<(tiempo+2000000));
}

void yellow(){
  long tiempo;
  tiempo=micros();
  do{
    digitalWrite(no_green, LOW);
    digitalWrite(no_green, HIGH);
    digitalWrite(no_red, LOW);
    digitalWrite(no_red,HIGH);
  }while(micros()<(tiempo+2000000));
}


//-----------------Wifi--------------------------
void checkWifi(){
  while (Serial1.available() >0 ){  
    char c =Serial1.read();
    if (c!='\n')
      line+=c;
    else{
      if (line.startsWith("+IPD")){
        http_request=line;
      }
      line="";
    }
  }
  if (http_request!=""){ //Si termina con un salto de linea
    Serial.println(http_request);
      if (http_request.length()<35){ //+IPD,0,379:GET / HTTP/1.1
        Serial.println("Enviada Web Request");
        webserver();
        delay(500);
      }
      else{                          //+IPD,0,418:GET /?maxspeed=120&distance=1000&encendido=1 HTTP/1.1
        Serial.println(http_request);
        Serial.println("Recibida respuesta de formulario");
        parser(http_request);
        webserver();
      }
    http_request="";
    Serial.println("---------------------------");
  }
}
//Send a webpage with a form to control Radarduino
void webserver() {   
  String code="";
  code+="<!DOCTYPE HTML>";
  code+="<html>";
  code+="<head>";
  code+="<title>Radarduino</title>";
  code+="<link rel=\"icon\" type=\"img/ico\" href=\"https://cdn.sstatic.net/Sites/arduino/img/favicon.ico?v=9b9d74e64022\">";
  code+="<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\">";
  code+="</head>";
  code+="<body class=\"container\"><h1>Radarduino v3.1</h1>";
    code+="<div class=\"col-md-6 col-md-offset-3 col-xs-6 col-xs-offset-3\">";
      code+="<form class=\"form\" action=\"/\" method=\"GET\">";
      code+="<fieldset>";
        code+="<legend>Radarduino config:</legend>";
      code+="<div class=\"form-group\">";
        code+="<label for=\"number\">Max speed: (km/h)</label>";
        code+="<input class=\"form-control\" type=\"number\" name=\"maxspeed\" min=\"0\" value=\"";
        code+=vel_max;
        code+="\">";
      code+="</div>";
      code+="<div class=\"form-group\">";
        code+="<label for=\"distance\">Distance: (mm)</label>";
        code+="<input class=\"form-control\" type=\"text\" name=\"distance\" min=\"0\" value=\"";
        code+=espacio;
        code+="\">";
      code+="</div>";
      code+="<div class= \"row col-md-6 col-md-offset-3\">";
        code+="<div class=\"col-md-3 col-xs-3\">OFF</div>";
        code+="<div class=\"col-md-6 col-xs-6\"><input type=\"range\" name=\"encendido\" min=\"0\" max=\"1\" value=\"";
        code+=encendido;
        code+="\"></div>";
        code+="<div class=\"col-md-3 col-xs-3\">ON</div>";
      code+="</div>";
      code+="<br><br><input class=\"btn btn-lg btn-primary btn-block\" type=\"submit\" value=\"Enviar\">";
      code+="</fieldset>";
      code+="</form>";
    code+="</div>";
  code+="</body>";
  code+="</html>";
  
  //Send to client
  Serial1.print("AT+CIPSEND=0,");              // AT+CIPSEND=0, length
  Serial1.println(code.length());
  if (Serial1.find(">")){ //https://github.com/espressif/ESP8266_AT/wiki/CIPSEND
    Serial1.println(code);
    delay(50); 
    Serial.println("Enviada web");
    while ( Serial1.available() > 0 ) {
      if (  Serial1.find("SEND OK") ) {   // Busca el OK en la respuesta del wifi
        break;
      }
    }
  }
  Serial1.println("AT+CIPCLOSE=0"); 
}

//Parses the information of the form
void parser(String info){
  //maxspeed=sp%20distance=dist%20encendido=power
  int pos1=info.indexOf("maxspeed");
  int pos2=info.indexOf("distance");
  int pos3=info.indexOf("encendido");
  long sp=info.substring(pos1+9,pos2-1).toInt();
  long dist=info.substring(pos2+9,pos3-1).toInt();
  int power=info.substring(pos3+10).toInt();
  vel_max = sp;
  espacio = dist;
  encendido = power;
  if (encendido==0){ //OFF
    digitalWrite(LASERPin1,LOW);
    digitalWrite(LASERPin2,LOW);
  }
  else{ //ON
    digitalWrite(LASERPin1,HIGH);
    digitalWrite(LASERPin2,HIGH);
  }
  Serial.println("-----------------------------");
  Serial.println("La velocidad elegida es "+sp);
  Serial.println("La distancia elegida es "+dist);
  Serial.println("Encendido "+power);
  Serial.println("-----------------------------");
}
//Gets next line of text
String get_line_wifi(){
  String S = "" ;
  if (Serial1.available()){
    char c = Serial1.read(); ;
    while ( c != '\n' ){            //Hasta que el caracter sea intro
        S = S + c ;
        c = Serial1.read();
    }
    return( S ) ;
  }
}

//------------------Mathematics methods-----------------------
//Calculate speed in km/h with space in mm and time in microseconds
unsigned long calculo_velocidad(unsigned long e, unsigned long t) {
  /*
     V=e/t=[km]/[h]
        [km]=[mm]/10e6
        [s]=[micros]/10e6
        [horas]=[s]/3600

        V= ([mm]/10e6)/(([micros]/10e6)/3600)
        V= [mm]/([micros]/3600)
        V= ([mm]*3600)/[micros]

  */
  unsigned long resultado = (e*3600) / t;
  return resultado;
}

//Calcula el porcentaje de velocidad a la que circula el coche sobre la velocidad máxima
int coefVel(long velocidad){
  return velocidad*100/vel_max; //120/100=120 //120/120=100 //140/120=116.6666 //80/100=80
}

