#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "A1-D575B0";
const char* password = "DDXJCJPDBF";
WiFiServer server(80);

String PC_IP = "192.168.1.100:8080";//out current IP
String tasksEndpoint = "tasks?user=Maj";//where to request to get tasks
String backEnd = "http://" + PC_IP + "/";//easier requesting

LiquidCrystal_I2C lcd(0x27, 20, 4);  // set the LCD address to 0x27 for a 20 chars and 4 line display
//lcd.setCursor(col,row);
//lcd.print("Text!");
//lcd.clear();

//variables for real data and comparission data (when updating)
JsonDocument data;
JsonDocument newData;

//pins
int buttonUp = 25;
int buttonDown = 5;
int buttonOk = 26;

//variables for automatic updating every x seconds
int currentMillis = 0;
int prevMillis = 0;
int waitTime = 5000;//how many milliseconds to wait before requesting again
int millisActionOffset = 5000; //when touching the button, there is waitTimt + millisActionOffset before it resends a request
//random variables
int currentPlace = 0;//current index where we are on tasks
int dataSize = 0;//size of data we requested and are displaying

//Button variables
bool prevButtonDown = 0;//data for tracking clicking of buttons
bool prevButtonUp = 0;
bool prevButtonOk = 0;
int debounceDelay = 100;//filtering button delay

//connect to WiFi
void setupWifi(){
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected!");
  server.begin();
}

//get data about our tasks, return JSON object
JsonDocument getData(){
  JsonDocument doc;
  HTTPClient http;

  http.begin(backEnd + tasksEndpoint);//make connection
  int httpResponseCode = http.GET();//send get request

  if(httpResponseCode == 200){//if response is "OK"
    String payload = http.getString();//get string of json file
    //Serial.println(payload);//for debugging
    deserializeJson(doc, payload);//string into a json object
    dataSize = doc.size();//get size of payload(it's an array)
  }

  else{
    Serial.print("Response code: ");//when shit goes wrong
    Serial.print(httpResponseCode);
  }

  return doc;//return the json object
}

//shows a list of tasks, one after another
void showData(JsonDocument inputData, int start = 0){
  lcd.clear();
  lcd.setCursor(0,0);
  int line = 0;
  for(int i = 0 + start; i < 4 + start; i++){
      if(inputData.size() >= i ){
        if(!inputData[i]){
          return;
        }
        String taskName = inputData[i]["taskName"];
        String points = String(inputData[i]["points"]);
        lcd.setCursor(0, line);
        lcd.print(String(i) + "." + taskName + ": " + points);
        line++;
      }
    }
}

//shows each task fullscreen, details about tasks too.
void showSingleTask(JsonDocument inputData, int start = 0){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(String(start + 1) + ". Task:");//show order of tasks
  lcd.setCursor(17, 0);
  lcd.print(String(start + 1) + "/" + String(dataSize));//shows how many tasks there are
  lcd.setCursor(0,1);

  String taskName = String(inputData[start]["taskName"]);
  lcd.print(taskName.length() < 20 ? taskName : taskName.substring(0,19));//is it's t
  if(taskName.length() > 20){
    lcd.setCursor(19, 1);//this part handles the - character at the end of the line when splitting words
    lcd.print("-");
    lcd.setCursor(0, 2);
    lcd.print(taskName.substring(19, taskName.length() < 40 ? taskName.length() : 39));//prints the substring that's longer than the screen on the new line
    if(taskName.length() > 40){
      lcd.setCursor(17, 2);
      lcd.print("...");
    }
  }

  lcd.setCursor(0,3);
  lcd.print("Points: " + String(inputData[start]["points"]));//shows the amount of points in the left bottom corner
  bool completed = inputData[start]["finished"];//
  String word = completed ? "DONE" : "INCOMPLETE";//if task is completed, show DONE, otherwise INCOMPLETE
  int len = word.length();
  lcd.setCursor(20 - len, 3);//show in right bottom corner
  lcd.print(word);
}

//increases current place and re-renders screeen
void currentPlaceUp(){
  if(currentPlace < dataSize - 1){//checks if it's in range
    currentPlace++;
    showSingleTask(data, currentPlace);
    prevMillis = millis() + millisActionOffset;//makes the timer reset, gives you more time to look at it
  }
}

void currentPlaceDown(){
  if(currentPlace > 0){
    currentPlace--;
    //showData(data, currentPlace);
    showSingleTask(data, currentPlace);
    prevMillis = millis() + millisActionOffset;
  }
}

void handleOkButton(){
  HTTPClient http;
  String user = data[currentPlace]["user"];
  String points = data[currentPlace]["points"];
  String taskName = data[currentPlace]["taskName"];
  String finished = data[currentPlace]["finished"] ? "false" : "true";//since json has to get true or false... also you have to turn it around to change it.
  http.begin(backEnd + "tasks");
  http.addHeader("Content-Type", "application/json");
  String requestedJSON = "{\"user\": \"" + String(user) + "\", \"points\": " + String(points) + ", \"taskName\": \"" + String(taskName) + "\", \"finished\": " + finished + "}";

  //Serial.println(requestedJSON);
  int httpResponseCode = http.PUT(requestedJSON);//Send a put request
  if(httpResponseCode == 200){//if the data is accepted and updated, update everything.
    data = getData();
    showSingleTask(data, currentPlace);
  }
  prevMillis = millis() + millisActionOffset;//reset timer
}

//handles button click action
void handleSingleButton(int digitalPin, bool &previousState, void (*handlerFunction)()){
  bool buttonState = !digitalRead(digitalPin);
  if(buttonState && !previousState){
    previousState = true;
    delay(debounceDelay);//adds debounce delay, way better performance on clicking (no multiclick fuckery)
  }
  else if(!buttonState && previousState){
    previousState = false;
    handlerFunction();
  }
}

//handles the button logic for all buttons
void handleButtons(){
  handleSingleButton(buttonDown, prevButtonDown, currentPlaceDown);
  handleSingleButton(buttonUp, prevButtonUp, currentPlaceUp);
  handleSingleButton(buttonOk, prevButtonOk, handleOkButton);
}

void setup(){
  Serial.begin(115200);
  pinMode(buttonUp, INPUT_PULLUP);
  pinMode(buttonDown, INPUT_PULLUP);
  pinMode(buttonOk, INPUT_PULLUP);

  lcd.init();        
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Connecting...");//while connecting to the wifi
  setupWifi();

  lcd.setCursor(0,0);
  lcd.print("Connected to WiFi!");

  data = getData();//initial request to the backend
  showSingleTask(data, currentPlace);
}

void loop(){
  handleButtons();//well... handles.... buttons....
  currentMillis = millis();//checks for current time
   
  if(currentMillis - prevMillis > waitTime){//compares current time with previously requested time
    prevMillis = currentMillis;//updates previously requested time to now

    newData = getData();
    if((data != newData) || (currentPlace != 0)){//compares data if there's any difference and updates if there is.
      currentPlace = 0;
      data = newData;
      showSingleTask(data, currentPlace);
    }
  }
}


