#include <WifiCredObtainer.h>
#include "debug.h"

#define MAX_CLIENTS 4	// ESP32 supports up to 10 but I have not tested it yet
#define WIFI_CHANNEL 6	// 2.4ghz channel 6 https://en.wikipedia.org/wiki/List_of_WLAN_channels#2.4_GHz_(802.11b/g/n/ax)

String wifiSSID, wifiPassword;
const String localIPURL = "http://4.3.2.1";	 // a string version of the local IP with http, used for redirecting clients to your webpage 
static String urlDecode(String str);
bool isPasswordProvided = false;
const char index_html[] PROGMEM = R"=====(
    <!DOCTYPE html> <html>
    <head>
      <title>WiFi Config</title>
      <style>
        body {
          font-family: sans-serif;
          background: linear-gradient(135deg, #6e7dff, #44d9e1);
          color: #fff;
          text-align: center;
          margin: 0;
          padding: 0;
        }
        .logo {
          font-size: 50px;
          font-weight: bold;
          color: #fff;
          margin-top: 50px;
          text-shadow: 2px 2px 10px rgba(0,0,0,0.2);
        }
        .container {
          width: 90%;
          max-width: 500px;
          background: rgba(255,255,255,0.85);
          border-radius: 10px;
          padding: 30px;
          box-shadow: 0 4px 15px rgba(116, 13, 13, 0.15);
          margin: auto;
          margin-top: 50px;
        }
        h2 {
          color: #444;
          font-size: 28px;
          margin-bottom: 10px;
        }
        p {
          font-size: 18px;
          color: #666;
          margin-bottom: 30px;
        }
        label {
          font-size: 20px;
          color: #333;
          text-align: left;
          display: block;
          margin-top: 10px;
          margin-bottom: 5px;
        }
        .input-field {
          padding: 12px;
          margin: 10px 0;
          width: 100%;
          max-width: 400px;
          border-radius: 5px;
          border: 2px solid #ddd;
          background-color: #fff;
          font-size: 18px;
          color: #333;
          box-sizing: border-box;
          transition: border 0.3s ease, background-color 0.3s ease;
        }
        .input-field:focus {
          border-color: #4CAF50;
          background-color: #f4fdf2;
          outline: none;
        }
        .button {
          background-color: #4CAF50;
          border: none;
          color: white;
          padding: 16px 32px;
          font-size: 20px;
          cursor: pointer;
          border-radius: 5px;
          transition: background-color 0.3s ease, transform 0.2s ease;
        }
        .button:hover {
          background-color: #45a049;
          transform: scale(1.05);
        }
        .button:active {
          background-color: #388e3c;
          transform: scale(0.98);
        }
        .footer {
          margin-top: 40px;
          font-size: 14px;
          color: #ccc;
        }
        .footer a {
          color: #44d9e1;
          text-decoration: none;
        }
        .footer a:hover {
          text-decoration: underline;
        }
        @media (max-width: 600px) {
          .container {
            width: 90%;
            padding: 20px;
          }
          .logo {
            font-size: 40px;
          }
          h2 {
            font-size: 24px;
          }
        }
      </style>
    </head>
    <body>
      <div class="logo">The Amazing Wifi Watch</div>
      <div class="container">
        <h2>Configure Your Wi-Fi</h2>
        <p>Enter your Wi-Fi details below to set up your device.</p>
        <form action="/submit" method="GET">
          <label for="ssid">Wi-Fi SSID:</label>
          <input type="text" id="ssid" name="ssid" class="input-field" required>
          
          <label for="password">Wi-Fi Password:</label>
          <input type="password" id="password" name="password" class="input-field" required>
          
          <input type="submit" value="Send" class="button">
        </form>
      </div>
      <div class="footer">
        <p>Powered by Udi & Noam Shorer | The Amazing WiFi Watch</p>
      </div>
    </body>
  </html>
  )=====";



void WifiCredObtainer::setUpWebserver(AsyncWebServer &server, const IPAddress &localIP) 
{
	//======================== Webserver ========================
	// WARNING IOS (and maybe macos) WILL NOT POP UP IF IT CONTAINS THE WORD "Success" https://www.esp8266.com/viewtopic.php?f=34&t=4398
	// SAFARI (IOS) IS STUPID, G-ZIPPED FILES CAN'T END IN .GZ https://github.com/homieiot/homie-esp8266/issues/476 this is fixed by the webserver serve static function.
	// SAFARI (IOS) there is a 128KB limit to the size of the HTML. The HTML can reference external resources/images that bring the total over 128KB
	// SAFARI (IOS) popup browser has some severe limitations (javascript disabled, cookies disabled)

	// Required
	server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });	// windows 11 captive portal workaround
	server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });								// Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32 :)

	// Background responses: Probably not all are Required, but some are. Others might speed things up?
	// A Tier (commonly used by modern systems)
	server.on("/generate_204", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });		   // android captive portal redirect
	server.on("/redirect", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			   // microsoft redirect
	server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });  // apple call home
	server.on("/canonical.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });	   // firefox captive portal call home
	server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });					   // firefox captive portal call home
	server.on("/ncsi.txt", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			   // windows call home

	// return 404 to webpage icon
	server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });	// webpage icon

	// Serve Basic HTML Page
	// Root page
    server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
    response->addHeader("Cache-Control", "public,max-age=31536000");
    request->send(response);
    LOG_INFO("Served Basic HTML Page");
    });

    // Form submission handler
    server.on("/submit", HTTP_GET, [&wifiSSID, &wifiPassword, &isPasswordProvided](AsyncWebServerRequest *request) 
    {
        if (request->hasParam("ssid") && request->hasParam("password")) {
            wifiSSID = request->getParam("ssid")->value();
            wifiPassword = request->getParam("password")->value();
            LOG_INFO("Received credentials: SSID [%s] Password: [%s]", wifiSSID.c_str(), wifiPassword.c_str());

            request->send(200, "text/html", "<html><body style='font-family: Arial, sans-serif; background-color: #f0f8ff; color: #333;'>"
                        "<h1 style='color: #1e90ff; text-align: center; font-size: 48px;'>Wi-Fi Credentials Received</h1>"
                        "<p style='font-size: 36px; text-align: center; color: #4682b4;'>Thank you! The credentials have been saved successfully.</p>"
                        "<footer style='text-align: center; margin-top: 20px; font-size: 16px; color: #777;'>"
                        "<p>Powered by Udi and Noam Shorer</p>"
                        "</footer>"
                        "</body></html>");
            isPasswordProvided = true;
        } 
        else 
        {
            request->send(400, "text/plain", "Missing SSID or Password.");
        }
    });

	// the catch all
	server.onNotFound([&isPasswordProvided](AsyncWebServerRequest *request) 
    {
		request->redirect(localIPURL);
		LOG_INFO("onnotfound [%s] [%s] sent redirect to ", request->host().c_str(), request->url().c_str(), localIPURL.c_str()); // This gives some insight into whatever was being requested on the serial monitor
	});
}

//===========================================================================================================

WifiCredObtainer::WifiCredObtainer(const char* configWifiName) : CaptivePortal(configWifiName, WifiCredObtainer::setUpWebserver)
{
    //  setUpWebserver(this->server, localIP);
    //  server.begin();

    //  Serial.print("\n");
    //  Serial.print("Startup Time:");	// should be somewhere between 270-350 for Generic ESP32 (D0WDQ6 chip, can have a higher startup time on first boot)
    //  Serial.println(millis());
    //  Serial.print("\n");
}

void WifiCredObtainer::run(String &wifiName, String &password, int timeout) 
{
    CaptivePortal::run(wifiName.c_str());
    long unsigned int endMilis = millis() + timeout;
    while (endMilis > millis() && !isPasswordProvided)
    {
        this->processNextRequest();	 
	    #define DNS_INTERVAL 30
        vTaskDelay(DNS_INTERVAL / portTICK_PERIOD_MS);
        if (isPasswordProvided)
        {
            wifiName = wifiSSID;
            password = wifiPassword;
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            return;
        }

    }
}

static String urlDecode(String str) 
{
    String decoded = "";
    char c;
    int i, ii;
    for (i = 0; i < str.length(); i++) 
    {
        if (str[i] == '%') 
        {
            sscanf(str.substring(i + 1, i + 3).c_str(), "%2x", &ii);
            c = (char)ii;
            decoded += c;
            i = i + 2;
        } else if (str[i] == '+') 
        {
            decoded += ' ';  // '+' becomes a space
        } 
        else if (str[i] == ' ') 
        {
            return decoded;
        } 
        else 
        {
            decoded += str[i];
        }
    }
    return decoded;
}



