// Smart Notice Board with ESP8266 & Dot Matrix LED Display
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <cstring>

// Turn on debug statements to the serial output
#define  DEBUG  0

#if  DEBUG
#define PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTX(x) Serial.println(x, HEX)
#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTX(x)
#endif

// if DEBUG 1 then use below
//char ssid;

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN    15 // or SS

// HARDWARE SPI
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// WiFi Server object and parameters
WiFiServer server(80);

// Scrolling parameters
uint8_t frameDelay = 35;  // default frame delay value
textEffect_t  scrollEffect = PA_SCROLL_LEFT;

// Global message buffers shared by Wifi and Scrolling functions
#define BUF_SIZE  512
char curMessage[BUF_SIZE];
char newMessage[BUF_SIZE];
bool newMessageAvailable = false;

const char WebResponse[] = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";

const char WebPage[] =
"<!DOCTYPE html>\n"
"<html lang='en'>\n"
"<head>\n"
"    <meta charset='UTF-8'>\n"
"    <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
"    <title>Smart Notice Board</title>\n"
"\n"
"    <script>\n"
"        function validateLogin() {\n"
"            var username = document.getElementById('username').value;\n"
"            var password = document.getElementById('password').value;\n"
"\n"
"            if (username === 'admin' && password === 'admin') {\n"
"                document.getElementById('loginContainer').style.display = 'none';\n"
"                document.getElementById('controlContainer').style.display = 'block';\n"
"                return false;\n"
"            } else {\n"
"                alert('Invalid username or password. Please try again.');\n"
"                return false;\n"
"            }\n"
"        }\n"
"\n"
"        strLine = '';\n"
"        function SendData(event) {\n"
"            event.preventDefault();\n"
"            nocache = '/&nocache=' + Math.random() * 1000000;\n"
"            var request = new XMLHttpRequest();\n"
"            strLine = '&MSG=' + document.getElementById('data_form').Message.value;\n"
"            strLine = strLine + '/&SD=' + document.getElementById('data_form').ScrollType.value;\n"
"            strLine = strLine + '/&I=' + document.getElementById('data_form').Invert.value;\n"
"            strLine = strLine + '/&SP=' + document.getElementById('data_form').Speed.value;\n"
"            strLine = strLine + '/&SJ=' + document.getElementById('data_form').Intensity.value;\n"
"            request.open('GET', strLine + nocache, false);\n"
"            request.send(null);\n"
"        }\n"
"\n"
"        function wificontrol(event) {\n"
"            event.preventDefault();\n"
"            nocache = '/&nocache=' + Math.random() * 1000000;\n"
"       		 var request = new XMLHttpRequest();\n"
"        		 request.open('GET', '/eraseWiFiCredentials' + nocache, false);\n"
"        		 request.send(null);\n"
"        }\n"
"\n"
"    </script>\n"
"\n"
"    <style>\n"
"        /* Add your custom styles here */\n"
"        body {\n"
"            font-family: Arial, sans-serif;\n"
"            background-color: #f4f4f4;\n"
"        }\n"
"\n"
"        .container {\n"
"            max-width: 600px;\n"
"            margin: 50px auto;\n"
"            padding: 20px;\n"
"            background-color: #fff;\n"
"            border-radius: 8px;\n"
"            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);\n"
"        }\n"
"\n"
"        label {\n"
"            display: block;\n"
"            margin-bottom: 8px;\n"
"        }\n"
"\n"
"        input[type='text'], input[type='password'], input[type='number'] {\n"
"            width: 100%;\n"
"            padding: 10px;\n"
"            margin-bottom: 15px;\n"
"            box-sizing: border-box;\n"
"        }\n"
"\n"
"        input[type='radio'] {\n"
"            margin-right: 5px;\n"
"        }\n"
"\n"
"        input[type='range'] {\n"
"            width: 100%;\n"
"            margin-bottom: 15px;\n"
"        }\n"
"\n"
"        button {\n"
"            background-color: #4caf50;\n"
"            color: #fff;\n"
"            padding: 10px 15px;\n"
"            border: none;\n"
"            border-radius: 4px;\n"
"            cursor: pointer;\n"
"       }\n"
"\n"
"        button:hover {\n"
"            background-color: #45a049;\n"
"        }\n"
"\n"
"       #buttonContainer {\n"
"            display: flex;\n"
"            justify-content: space-between;\n"
"        }\n"
"\n"
"        #buttonContainer button {\n"
"            margin: 0; /* Remove default margin */\n"
"        }\n"
"\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"\n"
"    <div class='container' id='loginContainer'>\n"
"        <h2>Login</h2>\n"
"        <form id='loginForm' onsubmit='return validateLogin()'>\n"
"            <label for='username'>Username:</label>\n"
"            <input type='text' id='username' name='username' required><br>\n"
"            <label for='password'>Password:</label>\n"
"            <input type='password' id='password' name='password' required><br>\n"
"            <button type='submit' value='Login'>Login</button>\n"
"        </form>\n"
"    </div>\n"
"\n"
"    <div class='container' id='controlContainer' style='display: none;'>\n"
"        <h2>Smart Notice Board</h2>\n"
"        <!--<form id='controlForm' onsubmit='return submitData()'>-->\n"
"        <form id='data_form' name='frmText'>\n"
"            <label for='message'>Message:</label>\n"
"            <input type='text' id='message' name='Message' maxlength='255' required>\n"
"\n"
"            <label>Text Style:</label>\n"
"            <label><input type='radio' name='Invert' value='0' checked> Normal</label>\n"
"            <label><input type='radio' name='Invert' value='1'> Invert</label>\n"
"            <br><br>\n"
"\n"
"            <label>Scrolling Direction:</label>\n"
"            <label><input type='radio' name='ScrollType' value='L' checked> Left</label>\n"
"            <label><input type='radio' name='ScrollType' value='R'> Right</label>\n"
"           <br><br>\n"
"\n"
"            <label for='scrollSpeed'>Scroll Speed:</label>\n"
"            <div style='display: flex; justify-content: space-between;'>\n"
"            <span style='text-align: left;'>Fast</span>\n"
"            <span style='text-align: right;'>Slow</span>\n"
"            </div>\n"
"            <input type='range' id='scrollSpeed' name='Speed' min='0' max='200' value='200'>\n"
"            <br><br>\n"
"\n"
"            <label for='displayIntensity'>Display Brightness:</label>\n"
"            <div style='display: flex; justify-content: space-between;'>\n"
"            <span style='text-align: left;'>Low</span>\n"
"            <span style='text-align: right;'>High</span>\n"
"            </div>\n"
"            <input type='range' id='displayIntensity' name='Intensity' min='1' max='15' value='0'>\n"
"            <br><br>\n"
"\n"
"            <div id='buttonContainer'>\n"
"            <button type='submit' value='Send Data' onclick='SendData(event)'>Send Data</button>\n"
"            <button type='button' id='wifiConfigButton' onclick='wificontrol(event)'>WiFi AP-Setup</button>\n"
"            </div>\n"
"        </form>\n"
"    </div>\n"
"</body>\n"
"</html>\n";

const char *err2Str(wl_status_t code)
{
  switch (code)
  {
    case WL_IDLE_STATUS:    return ("IDLE");           break; // WiFi is in process of changing between statuses
    case WL_NO_SSID_AVAIL:  return ("NO_SSID_AVAIL");  break; // case configured SSID cannot be reached
    case WL_CONNECTED:      return ("CONNECTED");      break; // successful connection is established
    case WL_CONNECT_FAILED: return ("CONNECT_FAILED"); break; // password is incorrect
    case WL_DISCONNECTED:   return ("CONNECT_FAILED"); break; // module is not configured in station mode
    default: return ("??");
  }
}

uint8_t htoi(char c)
{
  c = toupper(c);
  if ((c >= '0') && (c <= '9')) return (c - '0');
  if ((c >= 'A') && (c <= 'F')) return (c - 'A' + 0xa);
  return (0);
}

void getData(char *szMesg, uint16_t len)
// Message may contain data for:
// New text (/&MSG=)
// Scroll direction (/&SD=)
// Invert (/&I=)
// Speed (/&SP=)
{
  char *pStart, *pEnd;      // pointer to start and end of text

  // check text message
  pStart = strstr(szMesg, "/&MSG=");
  if (pStart != NULL)
  {
    char *psz = newMessage;

    pStart += 6;  // skip to start of data
    pEnd = strstr(pStart, "/&");

    if (pEnd != NULL)
    {
      while (pStart != pEnd)
      {
        if ((*pStart == '%') && isxdigit(*(pStart + 1)))
        {
          // replace %xx hex code with the ASCII character
          char c = 0;
          pStart++;
          c += (htoi(*pStart++) << 4);
          c += htoi(*pStart++);
          *psz++ = c;
        }
        else
          *psz++ = *pStart++;
      }

      *psz = '\0'; // terminate the string
      newMessageAvailable = (strlen(newMessage) != 0);
      PRINT("\nNew Msg: ", newMessage);
    }
  }

  // check scroll direction
  pStart = strstr(szMesg, "/&SD=");
  if (pStart != NULL)
  {
    pStart += 5;  // skip to start of data

    PRINT("\nScroll direction: ", *pStart);
    scrollEffect = (*pStart == 'R' ? PA_SCROLL_RIGHT : PA_SCROLL_LEFT);
    P.setTextEffect(scrollEffect, scrollEffect);
    P.displayReset();
  }

  // check invert
  pStart = strstr(szMesg, "/&I=");
  if (pStart != NULL)
  {
    pStart += 4;  // skip to start of data

    PRINT("\nInvert mode: ", *pStart);
    P.setInvert(*pStart == '1');
  }

  // check speed
  pStart = strstr(szMesg, "/&SP=");
  if (pStart != NULL)
  {
    pStart += 5;  // skip to start of data

    int16_t speed = atoi(pStart);
    PRINT("\nSpeed: ", P.getSpeed());
    P.setSpeed(speed);
    frameDelay = speed;
  }

  // brightness
  pStart = strstr(szMesg, "/&SJ=");
  if (pStart != NULL)
  {
    pStart += 5;  // skip to start of data

    int16_t Intensity = atoi(pStart);
    PRINT("\nIntensity: ", P.getIntensity());
    P.setIntensity(Intensity);
  }
}

void handleWiFi(void)
{
  static enum { S_IDLE, S_WAIT_CONN, S_READ, S_EXTRACT, S_RESPONSE, S_DISCONN } state = S_IDLE;
  static char szBuf[1024];
  static uint16_t idxBuf = 0;
  static WiFiClient client;
  static uint32_t timeStart;

  switch (state)
  {
    case S_IDLE:   // initialise
      PRINTS("\nS_IDLE");
      idxBuf = 0;
      state = S_WAIT_CONN;
      break;

    case S_WAIT_CONN:   // waiting for connection
      {
        client = server.available();
        if (!client) break;
        if (!client.connected()) break;

#if DEBUG
        char szTxt[20];
        sprintf(szTxt, "%03d.%03d.%03d.%03d", client.remoteIP()[0], client.remoteIP()[1], client.remoteIP()[2], client.remoteIP()[3]);
        PRINT("\nNew client @ ", szTxt);
#endif

        timeStart = millis();
        state = S_READ;
      }
      break;

    case S_READ: // get the first line of data
      PRINTS("\nS_READ ");

      while (client.available())
      {
        char c = client.read();

        if ((c == '\r') || (c == '\n'))
        {
          szBuf[idxBuf] = '\0';
          client.flush();
          PRINT("\nRecv: ", szBuf);
          state = S_EXTRACT;
        }
        else
          szBuf[idxBuf++] = (char)c;
      }
      if (millis() - timeStart > 1000)
      {
        PRINTS("\nWait timeout");
        state = S_DISCONN;
      }
      break;

    case S_EXTRACT: // extract data
      PRINTS("\nS_EXTRACT");
      // Extract the string from the message if there is one
      getData(szBuf, BUF_SIZE);
      state = S_RESPONSE;
      break;

    case S_RESPONSE: // send the response to the client
      PRINTS("\nS_RESPONSE");
      // Return the response to the client (web page)
      client.print(WebResponse);
      client.print(WebPage);
      state = S_DISCONN;
      break;

    case S_DISCONN: // disconnect client
      PRINTS("\nS_DISCONN");
      client.flush();
      client.stop();
      state = S_IDLE;
      break;

    default:  state = S_IDLE;
  }

  // wifi re-config button click
    if (strstr(szBuf, "/eraseWiFiCredentials") != NULL) {
        eraseWiFiCredentials();
        state = S_DISCONN;
        return;
    }
}


// wifi re-config function
void eraseWiFiCredentials() {
  WiFi.disconnect(true);
  ESP.eraseConfig();
  Serial.println("[INFO] WiFi credentials are erased.");
  ESP.restart();
}

void setup()
{
  Serial.begin(115200);
  PRINTS("\n[MD_Parola WiFi Message Display]\nType a message for the scrolling display from your internet browser");

  P.begin();
  // Set the intensity (brightness) of the display (0-15)
  P.setIntensity(0);
  P.displayClear();
  P.displaySuspend(false);

  P.displayScroll(curMessage, PA_LEFT, scrollEffect, frameDelay);

  curMessage[0] = newMessage[0] = '\0';

  WiFiManager wifiManager;
  wifiManager.autoConnect("SmartNoticeBoard");

  // Connect to and initialise WiFi network
  PRINT("\nConnecting to ", ssid);

  while (WiFi.status() != WL_CONNECTED)
  {
    PRINT("\n", err2Str(WiFi.status()));
    delay(500);
  }
  PRINTS("\nWiFi connected");

  // Start the server
  server.begin();
  PRINTS("\nServer started");

  // Set up first message as the IP address
  sprintf(curMessage, "Smart notice board ip address : %03d.%03d.%03d.%03d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  PRINT("\nAssigned IP ", curMessage);
}

void loop()
{
  handleWiFi();
  
  if (newMessageAvailable) {
    P.displayClear();
    P.displaySuspend(false);
    P.displayScroll(newMessage, PA_LEFT, scrollEffect, frameDelay);
    strcpy(curMessage, newMessage);
    newMessageAvailable = false;
    P.displayReset();
  }

  // Continue displaying the current message
  if (P.displayAnimate()) {
    P.displayReset();
  }
}