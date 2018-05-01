  /**
   data - folder for files
 * */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <CreateTimer.h>

#define DBG_OUTPUT_PORT Serial

#include "credentials.h"

#define DHTPIN D4
#define DHTTYPE DHT22
DHT_Unified dht(DHTPIN, DHTTYPE);

CreateTimer SensorTimer;

ESP8266WebServer server(8080);
//holds the current upload
File fsUploadFile;

//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

String getContentType(String filename) {
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) {
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload() {
  if (server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    DBG_OUTPUT_PORT.print("handleFileUpload Name: "); DBG_OUTPUT_PORT.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile)
      fsUploadFile.close();
    DBG_OUTPUT_PORT.print("handleFileUpload Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void handleFileDelete() {
  if (server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_OUTPUT_PORT.println("handleFileDelete: " + path);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate() {
  if (server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_OUTPUT_PORT.println("handleFileCreate: " + path);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if (file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  DBG_OUTPUT_PORT.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}

bool isAuth() {
  return server.authenticate(WWW_USERNAME, WWW_PASSWORD);
}

// Hack for failed DHT readings
float lastTemp = 0;
float lastHumid = 0;
float temp = 0.0;
float humid = 0.0;
int signalStrength;

void updateSensorsData() {
  DBG_OUTPUT_PORT.println("Updating sensors data...");

  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    DBG_OUTPUT_PORT.println("Error reading temperature!");
    temp = lastTemp;
  }
  else {
    temp = event.temperature;
    lastTemp = temp;
    DBG_OUTPUT_PORT.print("Temperature: ");
    DBG_OUTPUT_PORT.print(event.temperature);
    DBG_OUTPUT_PORT.println(" *C");
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    DBG_OUTPUT_PORT.println("Error reading humidity!");
    humid = lastHumid;
  }
  else {
    humid = event.relative_humidity;
    lastHumid = humid;
    DBG_OUTPUT_PORT.print("Humidity: ");
    DBG_OUTPUT_PORT.print(event.relative_humidity);
    DBG_OUTPUT_PORT.println("%");
  }

  signalStrength = WiFi.RSSI();
}

void setup(void) {
  SensorTimer.Start(5000UL);

  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.print("\n");
  DBG_OUTPUT_PORT.setDebugOutput(true);
  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    DBG_OUTPUT_PORT.printf("\n");
  }


  //WIFI INIT
  DBG_OUTPUT_PORT.printf("Connecting to %s\n", WIFI_NAME);
  if (String(WiFi.SSID()) != String(WIFI_NAME)) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DBG_OUTPUT_PORT.print(".");
  }
  DBG_OUTPUT_PORT.println("");
  DBG_OUTPUT_PORT.print("Connected! IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.localIP());

  MDNS.begin(HOST);
  DBG_OUTPUT_PORT.print("Open http://");
  DBG_OUTPUT_PORT.print(HOST);
  DBG_OUTPUT_PORT.println(".local/edit to see the file browser");

  // Start DHT sensor
  dht.begin();

  // Listener for basic auth
  ArduinoOTA.begin();

  //SERVER INIT
  //list directory
  server.on("/list", HTTP_GET, []() {
    if (isAuth()) {
      handleFileList();
    } else {
      return server.requestAuthentication();
    }
  });
  //load editor
  server.on("/edit", HTTP_GET, []() {
    if (isAuth()) {
      if (!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound");
    } else {
      return server.requestAuthentication();
    }
  });
  //create file
  server.on("/edit", HTTP_PUT, []() {
    if (isAuth()) {
      handleFileCreate();
    } else {
      return server.requestAuthentication();
    }
  });
  //delete file
  server.on("/edit", HTTP_DELETE, []() {
    if (isAuth()) {
      handleFileDelete();
    } else {
      return server.requestAuthentication();
    }
  });
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    if (isAuth()) {
      server.send(200, "text/plain", "");
    } else {
      return server.requestAuthentication();
    }
  }, []() {
    if (isAuth()) {
      handleFileUpload();
    } else {
      return server.requestAuthentication();
    }
  });

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, []() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"temp\":" + String(temp);
    json += ", \"humid\":" + String(humid);
    json += ", \"sig\":" + String(signalStrength);
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });
  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");

}

void loop(void) {
  ArduinoOTA.handle();
  server.handleClient();

  // Debounce for sensors data updates
  if (SensorTimer.Once()) {
    updateSensorsData();
    SensorTimer.Start(5000UL);
  }
}
