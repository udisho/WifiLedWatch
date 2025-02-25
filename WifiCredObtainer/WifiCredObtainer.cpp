#include <WifiCredObtainer.h>


WifiCredObtainer::WifiCredObtainer(const char* configWifiName) : server(80)
{
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  IPAddress local_IP(10, 10, 10, 10);  // Set the static IP
  IPAddress gateway(10, 10, 10, 1);    // Set the gateway
  IPAddress subnet(255, 255, 255, 0);  // Set the subnet mask

  WiFi.softAP(configWifiName);
  if (true != WiFi.softAPConfig(local_IP, gateway, subnet)) {
    Serial.println("Failed to set static IP");
    }
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  this->server.begin();
}



static String urlDecode(String str);

const char* htmlForm = R"rawliteral(
  <html><head><style>body{font-family:sans-serif;background:linear-gradient(135deg,#6e7dff,#44d9e1);color:#fff;text-align:center;margin:0;padding:0}.logo{font-size:50px;font-weight:bold;color:#fff;margin-top:50px;text-shadow:2px 2px 10px rgba(0,0,0,0.2)}.container{width:90%;max-width:500px;background:rgba(255,255,255,0.85);border-radius:10px;padding:30px;box-shadow:0 4px 15px rgba(0,0,0,0.15);margin:auto;margin-top:50px}h2{color:#444;font-size:28px;margin-bottom:10px}p{font-size:18px;color:#666;margin-bottom:30px}label{font-size:20px;color:#333;text-align:left;display:block;margin-top:10px;margin-bottom:5px}.input-field{padding:12px;margin:10px 0;width:100%;max-width:400px;border-radius:5px;border:2px solid #ddd;background-color:#fff;font-size:18px;color:#333;box-sizing:border-box;transition:border 0.3s ease,background-color 0.3s ease}.input-field:focus{border-color:#4CAF50;background-color:#f4fdf2;outline:none}.button{background-color:#4CAF50;border:none;color:white;padding:16px 32px;font-size:20px;cursor:pointer;border-radius:5px;transition:background-color 0.3s ease,transform 0.2s ease}.button:hover{background-color:#45a049;transform:scale(1.05)}.button:active{background-color:#388e3c;transform:scale(0.98)}.footer{margin-top:40px;font-size:14px;color:#ccc}.footer a{color:#44d9e1;text-decoration:none}.footer a:hover{text-decoration:underline}@media (max-width:600px){.container{width:90%;padding:20px}.logo{font-size:40px}h2{font-size:24px}}</style></head><body><div class="logo">The Amazing Wifi Watch</div><div class="container"><h2>Configure Your Wi-Fi</h2><p>Enter your Wi-Fi details below to set up your device.</p><form action="/submit" method="GET"><label for="ssid">Wi-Fi SSID:</label><input type="text" id="ssid" name="ssid" class="input-field" required><label for="password">Wi-Fi Password:</label><input type="password" id="password" name="password" class="input-field" required><input type="submit" value="Send" class="button"></form></div><div class="footer"><p>Powered by Udi & Noam Shorer | The Amazing WiFi Watch</p></div></body></html>

)rawliteral";


void WifiCredObtainer::run(String &wifiName, String &password, int timeout) {
    long unsigned int endMilis = millis() + timeout;
    bool hasClientFinished = false;
    while (endMilis > millis() && !hasClientFinished)
    {
        WiFiClient client = server.available();  // Listen for incoming clients
        if (client) {  // If a client connects
        Serial.println("New Client Connected");

        String request = "";  // Store the client's HTTP request

        // Read the client's request
        while (client.available()) {
        char c = client.read();
        request += c;
        }

        // Print the request to the Serial Monitor (for debugging)
        Serial.println(request);

        // Serve the HTML form to the client
        if (request.indexOf("GET / ") >= 0) {
        client.print(htmlForm);
        }

        // If form is submitted (when the path is /submit)
        else if (request.indexOf("GET /submit?ssid=") >= 0) {
        // Parse SSID and password from the URL
        int ssidStart = request.indexOf("ssid=") + 5;
        int ssidEnd = request.indexOf("&", ssidStart);
        wifiName = request.substring(ssidStart, ssidEnd);

        int passwordStart = request.indexOf("password=") + 9;
        password = request.substring(passwordStart);
        password = urlDecode(password);
        // Print out the SSID and password (for debugging purposes)
        Serial.println("Wi-Fi SSID: " + wifiName);
        Serial.println("Wi-Fi Password: " + password);

        // Send a simple confirmation page to the client
        // Send a simple confirmation page to the client
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");
        client.println();  // This blank line is critical to end the HTTP header section properly
        client.println("<html><body style='font-family: Arial, sans-serif; background-color: #f0f8ff; color: #333;'>"
                        "<h1 style='color: #1e90ff; text-align: center; font-size: 48px;'>Wi-Fi Credentials Received</h1>"
                        "<p style='font-size: 36px; text-align: center; color: #4682b4;'>Thank you! The credentials have been saved successfully.</p>"
                        "<footer style='text-align: center; margin-top: 20px; font-size: 16px; color: #777;'>"
                        "<p>Powered by Udi and Noam Shorer</p>"
                        "</footer>"
                        "</body></html>");
        }

        // Close the client connection
        client.stop();
        Serial.println("Client Disconnected");
        hasClientFinished = true;
    }
}
}

static String urlDecode(String str) {
  String decoded = "";
  char c;
  int i, ii;
  for (i = 0; i < str.length(); i++) {
    if (str[i] == '%') {
      sscanf(str.substring(i + 1, i + 3).c_str(), "%2x", &ii);
      c = (char)ii;
      decoded += c;
      i = i + 2;
    } else if (str[i] == '+') {
      decoded += ' ';  // '+' becomes a space
    } else if (str[i] == ' ') {
      return decoded;
    } else {
      decoded += str[i];
    }
  }
  return decoded;
}



