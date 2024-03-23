#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h> // Library for I2C communication
#include <SPI.h>  // not used here, but needed to prevent a RTClib compile error
#include "RTClib.h"
#include <EEPROM.h>

// Object creation
RTC_DS3231 RTC;

/*NodeMcu Local Network SSID & Password */
const char* ssid = "WasserCraft";
const char* password = "";

/*IP Address details */
IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

// Server Port
ESP8266WebServer server(80);


// Global / fixed variables
int Total_Relays = 2;
int Total_Days = 7;


void setup() {
  
  // Comms Intialization
  EEPROM.begin(4096);
  Serial.begin(115200);
  RTC.begin();  // Init RTC
  
  /* Create Local Wifi Network to get Wifi Creds*/
  create_local_network();

  //Pin Config
  pinMode(D0,OUTPUT);
  digitalWrite(D0,HIGH);
  
  // First time setup only
  //EEPROM.put(0,0);
  //EEPROM.commit();

}

void loop() {
  server.handleClient();
  DateTime now = RTC.now();
  int day = String(now.dayOfTheWeek()).toInt();
  int time = (String(now.hour())+String((now.minute()<10)?("0"+String(now.minute())):(now.minute()))).toInt();
  Serial.println(String(time)+"->"+String(day));
  relay_operations(time, day);
}

void relay_operations(int time, int day){
  //DateTime now = RTC.now();
  //int day = String(now.dayOfTheWeek()).toInt();
  //int time = (String(now.hour())+String(now.minute())).toInt();
  int TotalSchedules = 0;
  EEPROM.get(0, TotalSchedules);

  for(int i = 1;i<=TotalSchedules;i++){
    int Start_addr =  5 + (24 * (i -1));
    int start_hr ;
    int Start_min;
    int end_hr;
    int End_min ;
    int relay_code;
    int day_code;
    int ScheduleNo;


    EEPROM.get(Start_addr,start_hr);
    EEPROM.get(Start_addr+4,Start_min);
    EEPROM.get(Start_addr+8,end_hr);
    EEPROM.get(Start_addr+12,End_min);
    EEPROM.get(Start_addr+16,day_code);
    EEPROM.get(Start_addr+20,relay_code);

    String start_min = Start_min < 10 ? ("0" + String(Start_min)) : String(Start_min);
    String end_min = End_min < 10 ? ("0" + String(End_min)) : String(End_min);

    String Str_DayCode = BuildDaysCode(day_code);
    String Str_RelayCode = BuildRelayCode(relay_code);
    char DayCode[8];
    char RelayCode[Total_Relays+1];
    Str_DayCode.toCharArray(DayCode, 8);
    Str_RelayCode.toCharArray(RelayCode, Total_Relays+1);

    if( (String(start_hr)+String(start_min)).toInt()  >= (String(end_hr)+String(end_min)).toInt() ){
      if( ( time >= (String(start_hr)+String(start_min)).toInt() && time > (String(end_hr)+String(end_min)).toInt() ) || ( time < (String(start_hr)+String(start_min)).toInt() && time <= (String(end_hr)+String(end_min)).toInt() ) ){
        if ( time >= (String(start_hr)+String(start_min)).toInt() && time > (String(end_hr)+String(end_min)).toInt() ){
          if(String(DayCode[day]).toInt()==1){
            for(int j=0;j<Total_Relays;j++){
              if(String(RelayCode[j]).toInt()==1){
                Serial.println("Relay " + String(j+1) + "  On For Schedule "+String(i));
                digitalWrite(D0,LOW);
              }
            }
          }
        }
        else if ( time < (String(start_hr)+String(start_min)).toInt() && time <= (String(end_hr)+String(end_min)).toInt() ){
          int dayPtr;
          if(day-1 < 0 ){
            dayPtr = 7+(day-1);
          }
          else{
            dayPtr = (day-1);
          }
          if(String(DayCode[dayPtr]).toInt()==1){
            for(int j=0;j<Total_Relays;j++){
              if(String(RelayCode[j]).toInt()==1){
                Serial.println("Relay " + String(j+1));
                Serial.println("Schedule "+String(i+1));
                digitalWrite(D0,LOW);
              }
            }
          }
        }
      }
      else if( ( time < (String(start_hr)+String(start_min)).toInt() && time > (String(end_hr)+String(end_min)).toInt() ) ){
        Serial.println("Relay Off For Schedule"+String(i));
        digitalWrite(D0,HIGH);
      }
    }
    else if( (String(start_hr)+String(start_min)).toInt()  <= (String(end_hr)+String(end_min)).toInt() ){
      if( ( time > (String(start_hr)+String(start_min)).toInt() && time > (String(end_hr)+String(end_min)).toInt() ) || ( time < (String(start_hr)+String(start_min)).toInt() && time < (String(end_hr)+String(end_min)).toInt() ) ){
        Serial.println("Relay off For Schedule"+String(i));
        digitalWrite(D0,HIGH);
      }
      else if( ( time >= (String(start_hr)+String(start_min)).toInt() && time <= (String(end_hr)+String(end_min)).toInt() ) ){
        //Serial.println("Relay On For Schedule"+(i+1));
        if(String(DayCode[day]).toInt()==1){
            for(int j=0;j<Total_Relays;j++){
              if(String(RelayCode[j]).toInt()==1){
                Serial.println("Relay On " + String(j+1));
                Serial.println("Schedule "+String(i));
                digitalWrite(D0,LOW);
              }
            }
          }
      }
    }


  }


}



String BuildDaysCode(int day_code){
  String Day_Cod = "";
  for(int i=0; i<7-String(day_code).length();i++){
    Day_Cod += "0";
  }
  Day_Cod += String(day_code);
  return Day_Cod;
}

String BuildRelayCode(int relay_code){
  String Relay_Cod = "";
  for(int i=0; i<Total_Relays-String(relay_code).length();i++){
    Relay_Cod += "0";
  }
  Relay_Cod += String(relay_code);
  return Relay_Cod;
}

void create_local_network(){
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  // Request handler
  server.on("/", handle_OnConnect);
  server.on("/Schedule_Count", handle_Schedule_Count);
  server.on("/Relay_Count", handle_Relay_Count);
  server.on("/Schedule_Details", handle_Schedule_Details);
  server.on("/Create_Schedule", handle__Create_Schedules);
  server.on("/Delete_Schedule", handel_delete_schedule);
  server.onNotFound(handle_NotFound);

  server.begin();
}

void handle_OnConnect() {
  String year = server.arg("year");
  String month = server.arg("month");
  String date = server.arg("date");
  String hr = server.arg("hr");
  String min = server.arg("min");
  String sec = server.arg("sec");
  RTC.adjust(DateTime(year.toInt(), month.toInt(), date.toInt(), hr.toInt(), min.toInt(), sec.toInt()));
  server.send(200, "text/plain", "Connected To Server");
  Serial.println("Connected"); 
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
   Serial.println("Not Connected"); 
}

// Get total Number of Relays
void handle_Schedule_Count(){
  server.send(200, "text/plain", String(EEPROM.read(0)));
}

void handle_Relay_Count(){
  server.send(200, "text/plain", String(Total_Relays));
}

// Get Schedule details
void handle__Create_Schedules() {
  String ReqType = server.arg("ReqType");
  String start_hr = server.arg("Start_hr");
  String start_min = server.arg("Start_min");
  String end_hr = server.arg("End_hr");
  String end_min = server.arg("End_min");
  String relay_code = server.arg("Relay_code");
  String day_code = server.arg("Day_code");

  Serial.println(start_hr+""+start_min+""+relay_code);

  int Schedules = ReqType.toInt();
  if(ReqType == "I"){
    EEPROM.get(0, Schedules);
    Serial.println(Schedules);
    Serial.println(sizeof(Schedules));
    Schedules += 1;
    EEPROM.put(0,Schedules);
  }



    int Start_addr =  5 + (24 * (Schedules-1));

      Serial.println(Start_addr);
      EEPROM.put(Start_addr,start_hr.toInt());
      EEPROM.put(Start_addr+4,start_min.toInt());
      EEPROM.put(Start_addr+8,end_hr.toInt());
      EEPROM.put(Start_addr+12,end_min.toInt());
      EEPROM.put(Start_addr+16,day_code.toInt());
      EEPROM.put(Start_addr+20,relay_code.toInt());

      EEPROM.commit();

      
  //}


  server.send(200, "text/plain", "OK");
}


// Create Schedules and save in EEPROM
void create_schedule(){

}

void handle_Schedule_Details(){
  String ScheduleNo = server.arg("Schedule_no");
  String resp = "";

  if (ScheduleNo == "A"){
    resp = "12:12;12:12;false,false,false,false,false,false,false;";
    for(int i = 0; i<Total_Relays; i++){
      resp += "false";
      if (i < Total_Relays-1){
        resp += ",";
      }
      else{
        resp += ";";
      }
    }
  }
  else{
    Serial.println("Reached");
    resp = Read_Schedules(ScheduleNo.toInt());
  }

  server.send(200, "text/plain", resp);

  

}



String Read_Schedules(int Schedule_No ){

  int Start_addr =  5 + (24 * (Schedule_No -1));
  int start_hr ;
  int start_min;
  int end_hr;
  int end_min ;
  int relay_code;
  int day_code;
  int ScheduleNo;


  EEPROM.get(Start_addr,start_hr);
  EEPROM.get(Start_addr+4,start_min);
  EEPROM.get(Start_addr+8,end_hr);
  EEPROM.get(Start_addr+12,end_min);
  EEPROM.get(Start_addr+16,day_code);
  EEPROM.get(Start_addr+20,relay_code);  

  String Day_Cod = "";
  for(int i=0; i<7-String(day_code).length();i++){
    Day_Cod += "0";
  }
  Day_Cod += String(day_code);
  String Day_Bool_Code = "";
  for(int i=0;i<7;i++){
    if(String(Day_Cod[i])=="0"){
      if(i==6){
        Day_Bool_Code += "false";
      }
      else{
        Day_Bool_Code += "false,";
      }
    }
    else if(String(Day_Cod[i])=="1"){
      if(i==6){
        Day_Bool_Code += "true";
      }
      else{
        Day_Bool_Code += "true,";
      }
    }
  }

  String Relay_Cod = "";
  for(int i=0; i<Total_Relays -String(relay_code).length();i++){
    Relay_Cod += "0";
  }
  Relay_Cod += String(relay_code);
  String Relay_Bool_Code = "";
  for(int i=0;i<Total_Relays;i++){
    if(String(Relay_Cod[i])=="0"){
      if(i==Total_Relays-1){
        Relay_Bool_Code += "false";
      }
      else{
        Relay_Bool_Code += "false,";
      }
    }
    else if(String(Relay_Cod[i])=="1"){
      if(i==Total_Relays-1){
        Relay_Bool_Code += "true";
      }
      else{
        Relay_Bool_Code += "true,";
      }
    }
  }

  String resp = String(start_hr)+":"+String(start_min)+";"+String(end_hr)+":"+String(end_min)+";"+Day_Bool_Code+";"+Relay_Bool_Code+";";

  Serial.println(resp);

  return resp;
}


void handel_delete_schedule(){
   String ScheduleNo = server.arg("ScheduleNo");
   int TotalSchedule;
   EEPROM.get(0, TotalSchedule);
   for(int i=ScheduleNo.toInt();i<=TotalSchedule;i++){  
      if(i<TotalSchedule){
        int Start_addr2 =  5 + (24 * (i));
        int start_hr2 ;
        int start_min2;
        int end_hr2;
        int end_min2 ;
        int relay_code2;
        int day_code2;
        int Start_addr1 =  5 + (24 * (i -1));

        EEPROM.get(Start_addr2,start_hr2);
        EEPROM.get(Start_addr2+4,start_min2);
        EEPROM.get(Start_addr2+8,end_hr2);
        EEPROM.get(Start_addr2+12,end_min2);
        EEPROM.get(Start_addr2+16,day_code2);
        EEPROM.get(Start_addr2+20,relay_code2);

        EEPROM.put(Start_addr1,start_hr2);
        EEPROM.put(Start_addr1+4,start_min2);
        EEPROM.put(Start_addr1+8,end_hr2);
        EEPROM.put(Start_addr1+12,end_min2);
        EEPROM.put(Start_addr1+16,day_code2);
        EEPROM.put(Start_addr1+20,relay_code2);

        EEPROM.commit();
      }
   }

  int Schedules;
  EEPROM.get(0, Schedules);
  Schedules = Schedules - 1;
  EEPROM.put(0,Schedules);
  EEPROM.commit();
  server.send(200, "text/plain", "Done");
}





