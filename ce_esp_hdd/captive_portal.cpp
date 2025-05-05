#include "WiFi.h"
#include <Preferences.h>

#include <WebServer.h>
#include <DNSServer.h>

#include "defs.h"

DNSServer dnsServer;
WebServer server(80);

extern Preferences preferences;
extern String ssid;
extern String password;

bool newSettingsSaved;

const char index_html[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>CosmosEx Captive Portal</title>
  <style>
    body { font-family: sans-serif; margin: 0; padding: 0; background: #f4f4f4; display: flex; justify-content: center; align-items: center; height: 100vh; }
    .card { background: white; padding: 30px 20px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); width: 90%; max-width: 400px; }
    h1 { font-size: 22px; margin-bottom: 10px; text-align: center; }
    p { font-size: 16px; text-align: center; margin-bottom: 20px; }
    label { font-size: 16px; display: block; margin-bottom: 5px; }
    input[type="text"], input[type="password"] { width: 100%; padding: 12px; font-size: 16px; margin-bottom: 20px; border: 1px solid #ccc; border-radius: 6px; box-sizing: border-box; }
    button { width: 100%; padding: 12px; font-size: 16px; background: #007bff; color: white; border: none; border-radius: 6px; cursor: pointer; }
    button:hover { background: #0056b3; }
  </style>
</head>
<body>
  <div class="card">
    <h1>Welcome to the CosmosEx Captive Portal</h1>
    <p>Please fill in your wifi settings below.</p>
    <form method="POST" action="/save">
      <label for="ssid">SSID</label>
      <input type="text" id="ssid" name="ssid" required />
      <label for="password">Password</label>
      <input type="password" id="password" name="password" required />
      <button type="submit">Save</button>
    </form>
  </div>
</body>
</html>
    )rawliteral";

const char saved_html[] = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <title>CosmosEx Captive Portal</title>
    </head>
    <body>
        <h1>Your wifi credentials have been saved.</h1>
        <p>Please wait a moment for your device to reconnect.</p>
    </body>
    </html>
    )rawliteral";

void handleIndex(void)
{
    server.send(200, "text/html", index_html);
}

void handleSave(void)
{
    if (server.hasArg("ssid") && server.hasArg("password")) {
        ssid = server.arg("ssid");
        password = server.arg("password");

        Serial.print("handleSave() - ssid: ");
        Serial.print(ssid);
        Serial.print(", password: ");
        Serial.println(password);

        // store credentials to preferences
        preferences.begin("credentials", PREFERENCES_RW_MODE);
        preferences.putString("ssid", ssid.c_str());
        preferences.putString("password", password.c_str());
        preferences.end();

        // mark that we now have new settings
        newSettingsSaved = true;

        server.send(200, "text/html", saved_html);
    } else {
        Serial.print("handleSave() - no ssid and/or password in request");
        server.send(200, "text/html", index_html);
    }
}

void handleNotFound(void)
{
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

void runCaptivePortal(void)
{
    Serial.println("runCaptivePortal() - now starting");

    // switch to access point mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP("CosmosEx AP");

    // start dns and web server
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.setTTL(300);
    dnsServer.start(53, "*", WiFi.softAPIP());

    server.on("/", handleIndex);
    server.on("/save", handleSave);
    server.onNotFound(handleNotFound);

    server.begin();

    Serial.println("Captive Portal started");

    // user didn't save the new settings yet
    newSettingsSaved = false;

    // handle requests until settings saved
    while(!newSettingsSaved)
    {
        dnsServer.processNextRequest();
        server.handleClient();
    }

    Serial.println("Captive Portal - settings saved, will restart");

    // give enough time to serve last page with success message
    delay(1000);

    ESP.restart();
    delay(1000);
}
