

#include "RunTimeConfigureEngine.h"
#include "ConfigManager.h"
#include "Common.h"
#include "debug.h"
//#include "UdiWifiClockManager.h"
RunTimeConfigEngine::RunTimeConfigEngine(const char* configWifiName) : CaptivePortal(configWifiName, setUpWebserver)
{

}


int RunTimeLastTimeUpdate = 0;

void RunTimeConfigEngine::run(const char* configWifiName)
{
   
    bool isCaptivePortalRunning = false;
    while (1)
    {
        int currTimeInterval = millis() / TIME_UPDATE_INTERVAL;
        if (currTimeInterval  != RunTimeLastTimeUpdate)
        {
            LOG_INFO("It is time to update sutting down captive portal");
            RunTimeLastTimeUpdate = currTimeInterval;
            WiFi.mode(WIFI_MODE_STA);
            vTaskDelay(15000 / portTICK_PERIOD_MS);
            isCaptivePortalRunning = false;
            continue;
        }

        #define DNS_INTERVAL 30

        if (!isCaptivePortalRunning) {
            LOG_INFO("Opening captive portal...");
            WiFi.mode(WIFI_AP_STA);  // Always set it before running captive portal
            CaptivePortal::run(configWifiName);
            isCaptivePortalRunning = true;
        }


        if (hasConnectedClients()) {
            // Serve requests / stay responsive
            this->processNextRequest();
            vTaskDelay(DNS_INTERVAL / portTICK_PERIOD_MS);
        } else {
            // Idle mode
            vTaskDelay(3000 / portTICK_PERIOD_MS);  // Sleep longer
        }

      /* if (hasConfigurationProvided)
        {
            LOG_INFO("delivering page \n");

            // change config
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            //return;
        }*/

    }
}

String generateColorOptionsHTML() {
    String html = "<div style='display:flex;flex-wrap:wrap;justify-content:center;'>";
  
    for (int i = 0; i < ledColorOptionsCount; ++i) {
      html += "<label style='margin:5px;'>";
      html += "<input type='radio' name='color' value='" + String(i) + "'>";
      html += "<div style='width:40px;height:40px;border-radius:50%;background-color:" + String(ledColorOptions[i].name) + ";'></div>";
      html += "</label>";
    }
  
    html += "</div>";
    return html;
  }

  
  String getMainPage() {
    String html = "<!DOCTYPE html><html><head><title>LED Config</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;text-align:center;background:#f0f0f0;padding:20px;}</style></head><body>";
    html += "<h1>Configure LED</h1>";
    html += "<form action='/submit' method='GET'>";
  
    // Brightness
    html += "<label>Brightness: <input type='range' name='brightness' min='0' max='100' value='" + String(gSelectedBrightness) + "'></label><br><br>";
  
    // Time Format
    html += "<label for='timeFormat'>Time Format:</label><br>";
    html += "<select name='timeFormat' id='timeFormat'>";
    html += "<option value='24'" + String(gTimeFormat == 24 ? " selected" : "") + ">24-Hour</option>";
    html += "<option value='12'" + String(gTimeFormat == 12 ? " selected" : "") + ">12-Hour (AM/PM)</option>";
    html += "</select><br><br>";
  
    // Color Options
    html += "<p>Select Color:</p>";
    html += generateColorOptionsHTML();
  
    html += "<br><input type='submit' value='Apply' style='margin-top:20px; padding:10px 20px;'>";
    html += "</form></body></html>";
    return html;
  }
  

  void RunTimeConfigEngine::setUpWebserver(AsyncWebServer &server, const IPAddress &localIP) {
    // Build the local IP URL for redirects
    String localIPURL = "http://" + localIP.toString();

    // Captive portal handlers for various OSes
    server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });
    server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });

    server.on("/generate_204", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
    server.on("/redirect", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
    server.on("/hotspot-detect.html", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
    server.on("/canonical.html", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
    server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });
    server.on("/ncsi.txt", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });

    server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });

    // Serve main page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", getMainPage());
        response->addHeader("Cache-Control", "public,max-age=31536000");
        request->send(response);
        LOG_INFO("Served Config Page");
    });

    // Handle settings form submission
    server.on("/submit", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("brightness")) {
            gSelectedBrightness = request->getParam("brightness")->value().toInt();
            LOG_INFO("Updated brightness: %d", gSelectedBrightness);
            gCfgMgr.setBrightness(gSelectedBrightness);
            gGenericDisplay->ChangeBrightness(gSelectedBrightness);

        }

        if (request->hasParam("color")) {
            int colorIndex = request->getParam("color")->value().toInt();
            if (colorIndex >= 0 && colorIndex < ledColorOptionsCount) {
                gSelectedColor = ledColorOptions[colorIndex].color;
                LOG_INFO("Updated color: %s (index %d)", ledColorOptions[colorIndex].name, colorIndex);
                gCfgMgr.setColorIndex(colorIndex);
                gGenericDisplay->ChangeColor(getLedColorOptionByIndex(colorIndex));
            }
        }

        if (request->hasParam("timeFormat")) {
            int format = request->getParam("timeFormat")->value().toInt();
            if (format == 12 || format == 24) {
                gTimeFormat = format;
                LOG_INFO("Updated time format: %d-hour", gTimeFormat);
                gCfgMgr.setTimeFormat(gTimeFormat);  
            } else {
                LOG_WARN("Invalid time format received: %s", request->getParam("timeFormat")->value().c_str());
            }
        }

        request->send(200, "text/html",
            "<html><body style='font-family: Arial, sans-serif; background-color: #f0f8ff; color: #333;'>"
            "<h1 style='color: #1e90ff; text-align: center; font-size: 48px;'>Settings Saved</h1>"
            "<p style='font-size: 36px; text-align: center; color: #4682b4;'>Thank you! Your preferences have been updated.</p>"
            "<footer style='text-align: center; margin-top: 20px; font-size: 16px; color: #777;'>"
            "<p>Powered by Udi and Noam Shorer</p>"
            "</footer>"
            "</body></html>");
    });

    // Redirect any unknown routes
    server.onNotFound([localIPURL](AsyncWebServerRequest *request) {
        request->redirect(localIPURL);
        LOG_INFO("%s %s %s %s %s" "onNotFound ", request->host().c_str(), request->url().c_str(), " sent redirect to ", localIPURL.c_str());
    });
}

