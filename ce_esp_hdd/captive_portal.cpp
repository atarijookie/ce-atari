#include "WiFi.h"
#include <Preferences.h>

#include <ESPAsyncWebServer.h>      // install ZIP library for AsyncWebServer, because the Arduino version fails compilation
#include <DNSServer.h>

#include "defs.h"

DNSServer dnsServer;
AsyncWebServer server(80);

extern Preferences preferences;
extern String ssid;
extern String password;

bool newSettingsSaved;

const char index_html[] = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>CosmosEx Captive Portal</title>
    </head>
    <body>
      <h1>Welcome to the CosmosEx Captive Portal</h1>
      <p>Please fill in your wifi settings below.</p>
        <form method="POST" action="/save">
            <label for="ssid">SSID:</label><br>
            <input type="text" id="ssid" name="ssid" required><br><br>
            <label for="password">Password:</label><br>
            <input type="password" id="password" name="password" required><br><br>
            <button type="submit">Save</button>
        </form>
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

class CaptivePortalHandler : public AsyncWebHandler
{
public:
    CaptivePortalHandler() {}
    virtual ~CaptivePortalHandler() {}

    bool canHandle(AsyncWebServerRequest *request)
    {
        return request->url() == "/";
    }

    void handleRequest(AsyncWebServerRequest *request)
    {
        // POST to /save handled here
        if (request->method() == HTTP_POST && request->url() == "/save")
        {
            // get ssid from request
            if(request->hasParam("ssid", true))
            {
                ssid = request->getParam("ssid", true)->value();
            }

            // get password from request
            if(request->hasParam("password", true))
            {
                password = request->getParam("password", true)->value();
            }

            // store credentials to preferences
            preferences.begin("credentials", PREFERENCES_RW_MODE);
            preferences.putString("ssid", ssid.c_str());
            preferences.putString("password", password.c_str());
            preferences.end();

            // mark that we now have new settings
            newSettingsSaved = true;

            request->send(200, "text/html", saved_html);
        }

        // in any other case - return index.html
        request->send(200, "text/html", index_html);
    }
};

void runCaptivePortal(void)
{
    // user didn't save the new settings yet
    newSettingsSaved = false;

    // switch to access point mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP("CosmosEx AP");

    // configure web server
    server.addHandler(new CaptivePortalHandler()).setFilter(ON_AP_FILTER);

    server.onNotFound([&](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });

    // start dns and web server
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.setTTL(300);
    dnsServer.start(53, "*", WiFi.softAPIP());

    server.begin();

    Serial.println("Captive Portal started");

    // handle requests until settings saved
    while(!newSettingsSaved)
    {
        dnsServer.processNextRequest();
    }

    Serial.println("Captive Portal stoped");

    // give enough time to serve last page with success message
    delay(1000);

    ESP.restart();
    delay(1000);
}
