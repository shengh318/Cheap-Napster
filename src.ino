#include <mpu6050_esp32.h>
#include<string.h>
#include <TFT_eSPI.h> 
#include <SPI.h>
#include <WiFi.h> //Connect to WiFi Network


const uint8_t IDLE = 0;
const uint8_t GET_SONG = 1;
const uint8_t PLAY_SONG = 2;
const uint8_t STOP_LOOP = 3;
const uint8_t UP_SONG_NUM = 4;
const uint8_t MAKE = 5;
const uint8_t MAKE_FREQ = 6;
const uint8_t MAKE_NOTE = 7;
const uint8_t STORE_NOTE = 8;

uint8_t state;

TFT_eSPI tft = TFT_eSPI();  

const int RESPONSE_TIMEOUT = 6000; //ms to wait for response from host
const int GETTING_PERIOD = 2000; //periodicity of getting a number fact.
const int BUTTON_TIMEOUT = 1000; //button timeout in milliseconds
const uint16_t IN_BUFFER_SIZE = 1000; //size of buffer to hold HTTP request
const uint16_t OUT_BUFFER_SIZE = 1000; //size of buffer to hold HTTP response
char request_buffer[IN_BUFFER_SIZE]; //char array buffer to hold HTTP request
char response_buffer[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP response
char host[] = "iesc-s3.mit.edu";



char network[] = "MIT";
char password[] = "";

uint8_t scanning = 0;//set to 1 if you'd like to scan for wifi networks (see below):
/* Having network issues since there are 50 MIT and MIT_GUEST networks?. Do the following:
    When the access points are printed out at the start, find a particularly strong one that you're targeting.
    Let's say it is an MIT one and it has the following entry:
   . 4: MIT, Ch:1 (-51dBm)  4:95:E6:AE:DB:41
   Do the following...set the variable channel below to be the channel shown (1 in this example)
   and then copy the MAC address into the byte array below like shown.  Note the values are rendered in hexadecimal
   That is specified by putting a leading 0x in front of the number. We need to specify six pairs of hex values so:
   a 4 turns into a 0x04 (put a leading 0 if only one printed)
   a 95 becomes a 0x95, etc...
   see starting values below that match the example above. Change for your use:
   Finally where you connect to the network, comment out 
     WiFi.begin(network, password);
   and uncomment out:
     WiFi.begin(network, password, channel, bssid);
   This will allow you target a specific router rather than a random one!
*/
uint8_t channel = 1; //network channel on 2.4 GHz
byte bssid[] = {0x04, 0x95, 0xE6, 0xAE, 0xDB, 0x41}; //6 byte MAC address of AP you're targeting.


// Minimalist C Structure for containing a musical "riff"
//can be used with any "type" of note. Depending on riff and pauses,
// you may need treat these as eighth or sixteenth notes or 32nd notes...sorta depends
struct Riff {
  double notes[1024]; //the notes (array of doubles containing frequencies in Hz. I used https://pages.mtu.edu/~suits/notefreqs.html
  int length; //number of notes (essentially length of array.
  float note_period; //the timing of each note in milliseconds (take bpm, scale appropriately for note (sixteenth note would be 4 since four per quarter note) then
};

//Kendrick Lamar's HUMBLE
// needs 32nd notes to sound correct...song is 76 bpm, so that's 100.15 ms per 32nd note though the versions on youtube vary by a few ms so i fudged it
Riff humble = {
  {
    311.13, 311.13, 0 , 0, 311.13, 311.13, 0, 0,  //beat 1
    0, 0, 0, 0, 329.63, 329.63, 0, 0,             //beat 2
    311.13, 311.13, 0, 0, 207.65, 0, 207.65, 0,   //beat 3
    0, 0, 207.65, 207.65, 329.63, 329.63, 0, 0    //beat 4
  }, 32, 100.15 //32 32nd notes in measure, 100.15 ms per 32nd note
};


//Beyonce aka Sasha Fierce's song Formation off of Lemonade. Don't have the echo effect
// needs 16th notes and two measures to sound correct...song is 123 bpm, so that's around 120.95 ms per 16th note though the versions on youtube
// vary even within the same song quite a bit, so sorta I matched to her video for the song.
Riff formation = {{
    261.63, 0, 261.63 , 0,   0, 0, 0, 0, 261.63, 0, 0, 0, 0, 0, 0, 0, //measure 1 Y'all haters corny with that illuminati messssss
    311.13, 0, 311.13 , 0,   0, 0, 0, 0, 311.13, 0, 0, 0, 0, 0, 0, 0 //measure 2 Paparazzi catch my fly and my cocky freshhhhhhh
  }, 32, 120.95 //32 32nd notes in measure, 120.95 ms per 32nd note
};

//Justin Bieber's Sorry:
// only need the 16th notes to make sound correct. 100 beats (notes) per minute in song means 150 ms per 16th note
// riff starts right at the doo doo do do do do doo part rather than the 2-ish beats leading up to it. That way you
// can go right into the good part with the song. Sorry if that's confusing.
Riff sorry = {{ 1046.50, 1244.51 , 1567.98, 0.0, 1567.98, 0.0, 1396.91, 1244.51, 1046.50, 0, 0, 0, 0, 0, 0, 0}, 16, 150};


//create a song_to_play Riff that is one of the three ones above. 
Riff song_to_play = formation;  //select one of the riff songs


const uint32_t READING_PERIOD = 150; //milliseconds
double MULT = 1.059463094359; //12th root of 2 (precalculated) for note generation
double A_1 = 55; //A_1 55 Hz  for note generation
const uint8_t NOTE_COUNT = 97; //number of notes set at six octaves from

//buttons for today 
uint8_t BUTTON1 = 45;
uint8_t BUTTON2 = 39;
uint8_t BUTTON3 = 34;
uint8_t BUTTON4 = 38;

//pins for LCD and AUDIO CONTROL
uint8_t LCD_CONTROL = 21;
uint8_t AUDIO_TRANSDUCER = 14;

//PWM Channels. The LCD will still be controlled by channel 0, we'll use channel 1 for audio generation
uint8_t LCD_PWM = 0;
uint8_t AUDIO_PWM = 1;

MPU6050 imu; //imu object called, appropriately, imu

int song_number = 1;

void print_IDLE_screen(){
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0,1);
  tft.println("IDLE");
  tft.println("-------------------");
  char print_array[100]; 
  sprintf(print_array, "GETTING SONG: %d", song_number);
  tft.println(print_array);
  tft.println("-------------------");
  tft.println("1 -- K. Lamar");
  tft.println("2 -- Bey");
  tft.println("3 -- lilDuino");
  tft.println("4 -- lilDuino");
  tft.println("5 -- lilDuino");
  tft.println("-------------------");
  tft.println("Port 38 to change");
  tft.println("Port 39 to loop");
  tft.println("Port 45 to make");
}

void setup() {
  Serial.begin(115200);
  while (!Serial); // wait for Serial to show up
  Wire.begin();
  delay(50); //pause to make sure comms get set up
  if (imu.setupIMU(1)) {
    Serial.println("IMU Connected!");
  } else {
    Serial.println("IMU Not Connected :/");
    Serial.println("Restarting");
    ESP.restart(); // restart the ESP (proper way)
  }
  tft.init(); //initialize the screen
  tft.setRotation(2); //set rotation for our layout
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  //Serial.printf("Frequencies:\n"); //print out your frequencies as you make them to help debugging


  //print out your accelerometer boundaries as you make them to help debugging
  // Serial.printf("Accelerometer thresholds:\n");
  // tft.println("Accelerometer thresholds:\n");
  //fill in accel_thresholds with appropriate accelerations from -1 to +1
  //start new_note as at middle A or thereabouts.
  //new_note = note_freqs[NOTE_COUNT - NOTE_COUNT / 2]; //set starting note to be middle of range.
  if (scanning){
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0) {
      Serial.println("no networks found");
    } else {
      Serial.print(n);
      Serial.println(" networks found");
      for (int i = 0; i < n; ++i) {
        Serial.printf("%d: %s, Ch:%d (%ddBm) %s ", i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "");
        uint8_t* cc = WiFi.BSSID(i);
        for (int k = 0; k < 6; k++) {
          Serial.print(*cc, HEX);
          if (k != 5) Serial.print(":");
          cc++;
        }
        Serial.println("");
      }
    }
  }
  delay(100); //wait a bit (100 ms)

  //if using regular connection use line below:
  WiFi.begin(network, password);
  //if using channel/mac specification for crowded bands use the following:
  //WiFi.begin(network, password, channel, bssid);
  uint8_t count = 0; //count used for Wifi check times
  Serial.print("Attempting to connect to ");
  Serial.println(network);
  while (WiFi.status() != WL_CONNECTED && count<6) {
    delay(500);
    Serial.print(".");
    count++;
  }
  delay(2000);
  if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
    Serial.println("CONNECTED!");
    Serial.printf("%d:%d:%d:%d (%s) (%s)\n",WiFi.localIP()[3],WiFi.localIP()[2],
                                            WiFi.localIP()[1],WiFi.localIP()[0], 
                                          WiFi.macAddress().c_str() ,WiFi.SSID().c_str());
    delay(500);
  } else { //if we failed to connect just Try again.
    Serial.println("Failed to Connect :/  Going to restart");
    Serial.println(WiFi.status());
    ESP.restart(); // restart the ESP (proper way)
  }

  //four pins needed: two inputs, two outputs. Set them up appropriately:
  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(BUTTON3, INPUT_PULLUP);
  pinMode(BUTTON4, INPUT_PULLUP);
  pinMode(AUDIO_TRANSDUCER, OUTPUT);
  pinMode(LCD_CONTROL, OUTPUT);

  //set up AUDIO_PWM which we will control in this lab for music:
  ledcSetup(AUDIO_PWM, 200, 12);//12 bits of PWM precision
  ledcWrite(AUDIO_PWM, 0); //0 is a 0% duty cycle for the NFET
  ledcAttachPin(AUDIO_TRANSDUCER, AUDIO_PWM);

  //set up the LCD PWM and set it to 
  pinMode(LCD_CONTROL, OUTPUT);
  ledcSetup(LCD_PWM, 100, 12);//12 bits of PWM precision
  ledcWrite(LCD_PWM, 0); //0 is a 0% duty cycle for the PFET...increase if you'd like to dim the LCD.
  ledcAttachPin(LCD_CONTROL, LCD_PWM);


  //play A440 for testing.
  //comment OUT this line for the latter half of the lab.
  //ledcWriteTone(AUDIO_PWM, 440); //COMMENT THIS OUT AFTER VERIFYING WORKING
  delay(2000);
  //ledcWriteTone(AUDIO_PWM, 0); //COMMENT THIS OUT AFTER VERIFYING WORKING
  state=IDLE;
  print_IDLE_screen();
}

void play_riff(Riff);

Riff got_song;

char note_period[100];
char without_note_period[1000];
char char_holder[100];
double notes_holder[1024];
int store_index = 0;
int running_index = 0;
int length_of_song = 0;
bool first_freq = true;

void get_songs(int song_id){


  sprintf(request_buffer, "GET http://iesc-s3.mit.edu/esp32test/limewire?song_id=%d HTTP/1.1\r\n", song_id);

  strcat(request_buffer, "Host: iesc-s3.mit.edu\r\n"); //add more to the end

  strcat(request_buffer, "\r\n"); //add blank line!

  //submit to function that performs GET.  It will return output using response_buffer char array

  do_http_GET(host, request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);

  //Serial.println(response_buffer); //print to serial monitor
  
  structure_song(response_buffer);


}

void structure_song(char* response_buffer){
  memset(note_period, 0, 100);
  memset(without_note_period, 0, 1000);
  memset(char_holder, 0, 100);
  memset(notes_holder, 0, 1024);
  store_index = 0;
  running_index = 0;
  length_of_song = 0;
  first_freq = true;

  char* first_occurence = strchr(response_buffer, '&');
  int index = first_occurence - response_buffer;
  for (int i = 0; i< index; i++){
    note_period[i] = response_buffer[i];
  }

  Serial.println("Note Period below");
  //Serial.println((note_period));
  //Serial.println(atof(note_period));
  got_song.note_period = atof(note_period);
  Serial.println(got_song.note_period);
  Serial.println("-----------");

  for (int i=index; response_buffer[i] != '\0'; i++){
      without_note_period[i-index] = response_buffer[i];
  }

  // Serial.println("String Below");
  // Serial.println(without_note_period);
  // Serial.println("-----------");

  

  //TODO:
  // 1. make sure the the without_note_period is outputting the correct items - done
  // 2. how do we "append" stuff to the end of a double array list -> figured out
  // 2a. How do we convert a char array to a float? -> atof

  // atof does not work since it is not a const char -> update, it kind of worked for the notes frequncies, but still not working for the note freq?????
  //
  //MAKE SURE THAT WE ARE PRINTING THE RIGHT STUFF. THE PRINTING ON THIS IS KIND OF DOO DOO.
  // WE ARE GETTING THE RIGHT VALUES!!!

  // 3. after we append stuff to double array, create Riff and set song_to_play as that Riff that we just created
  //SUCCESSFULLY MADE THE GOT_SONG_RIFF

  for (int i= 1; without_note_period[i] != '\0'; i++){
    if (without_note_period[i] != ','){
      if (first_freq){
        length_of_song = length_of_song + 1;
        first_freq = false;
      }
      char_holder[running_index] = without_note_period[i];
      running_index = running_index + 1;
    }
    if (without_note_period[i] == ','){
      //Serial.println(char_holder);
      //Serial.println(atof(char_holder));
      length_of_song = length_of_song + 1;
      notes_holder[store_index] = atof(char_holder);
      store_index = store_index + 1;
      memset(char_holder, 0, 100);
      running_index = 0;
    }

    if (without_note_period[i] == '\0'){
      length_of_song = length_of_song + 1;
      Serial.println(char_holder);
      notes_holder[store_index] = atof(char_holder);
      memset(char_holder, 0, 100);
      running_index = 0;
    }
  }
  

  // Serial.println("-----NOTES BELOW FLOAT----------");
  // for (int i =0; i < 64; i++){
  //   Serial.println(notes_holder[i]);
  // }

  //got_song.notes = notes_holder;
  got_song.length = length_of_song;
  //Serial.println(got_song.length);

  for (int i =0; i< length_of_song; i++){
    got_song.notes[i] = notes_holder[i];
  }

  Serial.println("-------------GOT_SONGS_NOTES-----------------");
  for (int i =0; i < got_song.length; i++){
    Serial.println(got_song.notes[i]);
  }
  
}
/*----------------------------------
 * char_append Function:
 * Arguments:
 *    char* buff: pointer to character array which we will append a
 *    char c: 
 *    uint16_t buff_size: size of buffer buff
 *    
 * Return value: 
 *    boolean: True if character appended, False if not appended (indicating buffer full)
 */
uint8_t char_append(char* buff, char c, uint16_t buff_size) {
        int len = strlen(buff);
        if (len>buff_size) return false;
        buff[len] = c;
        buff[len+1] = '\0';
        return true;
}

/*----------------------------------
 * do_http_GET Function:
 * Arguments:
 *    char* host: null-terminated char-array containing host to connect to
 *    char* request: null-terminated char-arry containing properly formatted HTTP GET request
 *    char* response: char-array used as output for function to contain response
 *    uint16_t response_size: size of response buffer (in bytes)
 *    uint16_t response_timeout: duration we'll wait (in ms) for a response from server
 *    uint8_t serial: used for printing debug information to terminal (true prints, false doesn't)
 * Return value:
 *    void (none)
 */
void do_http_GET(char* host, char* request, char* response, uint16_t response_size, uint16_t response_timeout, uint8_t serial){

  WiFiClient client; //instantiate a client object

  if (client.connect(host, 80)) { //try to connect to host on port 80

    if (serial) Serial.print(request);//Can do one-line if statements in C without curly braces

    client.print(request);

    memset(response, 0, response_size); //Null out (0 is the value of the null terminator '\0') entire buffer

    uint32_t count = millis();

    while (client.connected()) { //while we remain connected read out data coming back

      client.readBytesUntil('\n',response,response_size);

      if (serial) Serial.println(response);

      if (strcmp(response,"\r")==0) { //found a blank line!

        break;

      }

      memset(response, 0, response_size);

      if (millis()-count>response_timeout) break;

    }

    memset(response, 0, response_size); 

    count = millis();

    while (client.available()) { //read out remaining text (body of response)

      char_append(response,client.read(),OUT_BUFFER_SIZE);

    }

    if (serial) Serial.println(response);

    client.stop();

    if (serial) Serial.println("-----------"); 

  }else{

    if (serial) Serial.println("connection failed :/");

    if (serial) Serial.println("wait 0.5 sec...");

    client.stop();

  }

}           

//make the riff player. Play whatever the current riff is (specified by struct instance song_to_play)
//function can be blocking (just for this lab :) )



void print_GET_screen(){
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0,1);
  tft.println("GETTING SONG");
}

void print_LOOP_screen(){
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0,1);
  tft.println("LOOPING");
  tft.println("HOLD DOWN Port 34 to stop loop");
}

void print_STOP_screen(){
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0,1);
  tft.println("STOPPING");
}


void play_riff(Riff *riff) {
  
  for (int i =0; i < riff->length; i++){
    Serial.println(riff->notes[i]);
    ledcWriteTone(AUDIO_PWM, riff->notes[i]);
    delay(riff->note_period);
  }

  ledcWriteTone(AUDIO_PWM, 0);
}

// TODO: 2/28 HAVE THE USER CREATE A SONG AND UPLOAD IT!!!!!!!!

bool get_song_first_time = true;

double made_notes[1024];
float made_note_freq;

float running_note_freq = 10.00;
double running_note = 0.00;

int running_store_index = 0;

char artist[] = "PIKACHU";

char float_holder[100];

void post_reporter_fsm() {
  memset(request_buffer, 0, 1000);

  char body[5000]; //for body
  sprintf(body,"{\"artist\":\"%s\", \"song\":\"%.2f&",artist, running_note_freq);//generate body, posting to User, 1 step
  
  for (int i=0; i < running_store_index+1; i++){
    float place_holder = round(made_notes[i]*100.0)/100.0;
    if (i!=running_store_index){
      if (place_holder == 0.00){
        sprintf(float_holder, "0,");
      }
      else{
        sprintf(float_holder, "%.2f,", place_holder);          
      }
      strcat(body, float_holder);
      memset(float_holder, 0, 100);      
    }
    else{
      if (place_holder == 0.00){
        sprintf(float_holder, "0\"}");
      }
      else{
        sprintf(float_holder, "%.2f\"}", place_holder);
      }
      strcat(body, float_holder);
      memset(float_holder, 0, 100);
    }
  }




  int body_len = strlen(body); //calculate body length (for header reporting)
  sprintf(request_buffer,"POST http://iesc-s3.mit.edu/esp32test/limewire HTTP/1.1\r\n");
  strcat(request_buffer,"Host: iesc-s3.mit.edu\r\n");
  strcat(request_buffer,"Content-Type: application/json\r\n");
  sprintf(request_buffer+strlen(request_buffer),"Content-Length: %d\r\n", body_len); //append string formatted to end of request buffer
  strcat(request_buffer,"\r\n"); //new line from header to body
  strcat(request_buffer,body); //body
  strcat(request_buffer,"\r\n"); //new line
  Serial.println(request_buffer);
  do_http_GET("iesc-s3.mit.edu", request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT,1);
  Serial.println(response_buffer); //viewable in Serial Terminal
 }


void print_MAKE_screen(){
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0,1);
  tft.println("MAKE SONG");
  tft.println("--Curr. Freq----");
  tft.println(running_note_freq);
  tft.println("--Total Notes---");
  tft.println(running_store_index);
  tft.println("------------------");
  tft.println("Port 39 for freq");
  tft.println("Port 38 for note");
  tft.println("Port 34 for POST");
}

void print_FREQ_screen(){
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0,1);
  tft.println("------MADE_FREQ------");
  tft.println(running_note_freq);
  tft.println("----------------");
  tft.println("Hold port 38 to inc.");
  tft.println("Port 34 to Dec.");
  tft.println("Port 45 to go back");
}

void print_NOTE_screen(){
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0,1);
  tft.println("------MADE_NOTE------");
  tft.println(running_note);
  tft.println("----------------");
  tft.println("Hold port 34 to inc.");
  tft.println("Port 45 to go back");
  tft.println("Port 39 to store note");
}

void loop() {
  uint8_t button_1 = digitalRead(BUTTON1); // 45
  uint8_t play_song_button = digitalRead(BUTTON2); // 39
  uint8_t button_3 = digitalRead(BUTTON3); //34
  uint8_t button_4 = digitalRead(BUTTON4); //38

  switch(state){
    case IDLE:
      running_note_freq = 10.00;
      running_note = 0.00;
      running_store_index = 0;
      
      memset(made_notes, 0, 1024);
      
      if (play_song_button == 0){
        Serial.println("GOING TO PLAY SONG");
        print_LOOP_screen();
        state = PLAY_SONG;
      }

      if (button_4 == 0){
        state = UP_SONG_NUM;
      }

      if (button_1 == 0){
        print_MAKE_screen();
        Serial.println("GOING TO MAKE");
        state = MAKE;
      }
    break;

    case MAKE:
      running_note = 0.00;
      if (button_3 == 0){
        Serial.println("GOING TO IDLE");
        
        print_IDLE_screen();
        if (running_store_index > 0){
          post_reporter_fsm();
        }
        state = IDLE;
      }

      if (play_song_button == 0){
        Serial.println("GOING TO MAKE FREQ");
        print_FREQ_screen();
        state = MAKE_FREQ;
      }

      if (button_4 == 0){
        Serial.println("GOING TO MAKE NOTE");
        print_NOTE_screen();
        state = MAKE_NOTE;
      }
    break;

    case MAKE_NOTE:
      //can't use button 4 here again
      if (button_1 == 0){
        Serial.println("RETURN TO MAKE");
        state = MAKE;
        print_MAKE_screen(); 
      }

      if (button_3 == 0){
        if (running_note <= 13900){
          running_note = running_note + 5.5;
          print_NOTE_screen();
        }
      }

      if (play_song_button == 0){
        Serial.println("STORING_NOTE");
        state = STORE_NOTE;
      }


    break;

    case STORE_NOTE:
      if (play_song_button == 1){
        if (running_store_index <= 1023){
          made_notes[running_store_index] = running_note;
          running_store_index = running_store_index + 1;
          Serial.println(running_store_index);
          running_note = 0.00;
          
          for (int i=0; i <= running_store_index; i++){
            Serial.println(made_notes[i]);
          }

          print_NOTE_screen();
          state = MAKE_NOTE;            
        }
        else{
          print_NOTE_screen();
          tft.println("NOT ENOUGH SPACE");
          state = MAKE_NOTE;
        }
      } 
    break;

    case MAKE_FREQ:
      if (button_4 == 0){
        if (running_note_freq <= 200){ // capped at 200.5 because I want to :)
          running_note_freq = running_note_freq + 0.5;
          Serial.println(running_note_freq);
          print_FREQ_screen();
        }
      }

      if (button_3 == 0){
        if (running_note_freq >= 10.5){ // capped at 10 because I want to :)
          running_note_freq = running_note_freq - 0.5;
          Serial.println(running_note_freq);
          print_FREQ_screen();
        }
      }

      if (button_1 == 0){
        Serial.println("RETURN TO MAKE");
        state = MAKE;
        print_MAKE_screen();        
      }
    break;

    case UP_SONG_NUM:
      Serial.println("ONE UP SONG NUMBER");
      if (button_4 == 1){
        Serial.println("BUTTON 4 RELEASED");
        if (song_number == 5){
          
          song_number = 1;
          Serial.println(song_number);
          print_IDLE_screen();
          Serial.println("GOING TO IDLE");
          state = IDLE;
        }
        else{

          song_number = song_number + 1;
          Serial.println(song_number);
          print_IDLE_screen();
          Serial.println("GOING TO IDLE");
          state = IDLE;
        }
      }
      
    break;

    case PLAY_SONG:
      if (play_song_button == 1){
        if (get_song_first_time){
          get_songs(song_number);
          get_song_first_time = false;
        }
        
        struct Riff *pointer;
        pointer = &got_song;

        play_riff(pointer);
        Serial.println("GOING TO CHECK LOOP");
        state = STOP_LOOP;
      }
    break;

    case STOP_LOOP:
      if (button_3 == 0){
        Serial.println("GOING TO IDLE");
        print_STOP_screen();
        print_IDLE_screen();
        get_song_first_time = true;
        state = IDLE;
      }
      else{
        Serial.println("PLAYING SONG");
        print_LOOP_screen();
        state = PLAY_SONG;
      }
    break;
  }
}
