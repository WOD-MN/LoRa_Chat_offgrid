#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

// Heltec LoRa32 Pinouts
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DI0 26
#define BAND 915E6  // Set your frequency

// OLED display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET 16
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// SDA and SCL Pins for OLED I2C
#define SDA_PIN 4
#define SCL_PIN 15

// WiFi AP Configuration
const char* ssid = "LoRaChat_AP";
const char* password = "12345678";
WiFiServer server(80);

String incomingMessage = "";
String outgoingMessage = "";

unsigned long displayTime = 10000;  // 10 seconds for displaying AP and IP
unsigned long lastDisplayChange = 0;
bool displayAP = true;
bool messageUpdated = false;

String urlDecode(String input) {
  String decoded = "";
  char tempChar;
  int hexValue;
  
  for (int i = 0; i < input.length(); i++) {
    if (input[i] == '%') {
      sscanf(input.substring(i + 1, i + 3).c_str(), "%x", &hexValue);
      tempChar = (char)hexValue;
      decoded += tempChar;
      i += 2;  // Skip the next two characters as they are part of the hex code
    } else if (input[i] == '+') {
      decoded += ' ';  // Replace '+' with space
    } else {
      decoded += input[i];
    }
  }
  
  return decoded;
}

void setup() {
  Serial.begin(9600);

  // Initialize OLED
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed!");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Starting LoRa Chat...");
  display.display();

  // Initialize LoRa
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("LoRa initialization failed!");
    display.println("LoRa Failed!");
    display.display();
    while (true);
  }
  display.println("LoRa Initialized");
  display.display();

  // Initialize Wi-Fi AP
  WiFi.softAP(ssid, password);
  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP Address: ");
  Serial.println(ip);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi AP Ready");
  display.print("IP: ");
  display.println(ip.toString());
  display.display();

  lastDisplayChange = millis();  // Start the timer
  server.begin();
  Serial.println("Web server started...");
}

void loop() {
  if (millis() - lastDisplayChange > displayTime && displayAP) {
    displayAP = false;
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("LoRa Chat");
    display.display();
    lastDisplayChange = millis();
  }

  if (!displayAP) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("LoRa Chat");
    
    if (incomingMessage.length() > 0) {
      display.setCursor(0, 20);
      display.print("Received: ");
      display.println(incomingMessage);
    }

    if (outgoingMessage.length() > 0) {
      display.setCursor(0, 40);
      display.print("Sent: ");
      display.println(outgoingMessage);
    }

    display.display();
  }

  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    client.flush();
    
    if (request.startsWith("GET /send?message=")) {
      int messageStart = request.indexOf('=') + 1;
      int messageEnd = request.indexOf(' ', messageStart);
      outgoingMessage = urlDecode(request.substring(messageStart, messageEnd));
      LoRa.beginPacket();
      LoRa.print(outgoingMessage);
      LoRa.endPacket();
      client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nMessage Sent: ");
      client.print(outgoingMessage);
      client.stop();
    } else {
      client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
      client.print("<html><head><style>");
      client.print("body { font-family: Arial, sans-serif; background-color: #f4f4f9; padding: 20px; }");
      client.print("h1 { text-align: center; color: #333; }");
      client.print(".chat-box { max-width: 600px; margin: 0 auto; padding: 20px; background-color: #fff; border-radius: 10px; border: 1px solid #ddd; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); }");
      client.print(".messages { max-height: 300px; overflow-y: auto; margin-bottom: 20px; }");
      client.print(".message { padding: 10px; margin-bottom: 10px; border-radius: 5px; background-color: #e0f7fa; }");
      client.print(".message.sent { background-color: #c8e6c9; text-align: right; }");
      client.print(".message.received { background-color: #f1f8e9; text-align: left; }");
      client.print(".input-box { display: flex; }");
      client.print(".input-box input { flex-grow: 1; padding: 10px; border: 1px solid #ddd; border-radius: 5px; }");
      client.print(".input-box button { padding: 10px 20px; background-color: #4CAF50; color: white; border: none; border-radius: 5px; cursor: pointer; }");
      client.print(".input-box button:hover { background-color: #45a049; }");
      client.print("</style></head><body>");
      client.print("<div class='chat-box'>");
      client.print("<h1>LoRa Chat</h1>");
      client.print("<div class='messages' id='messages'></div>");
      client.print("<form class='input-box' action='/send' method='GET' id='messageForm'>");
      client.print("<input type='text' name='message' id='messageInput' placeholder='Enter your message...' required>");
      client.print("<button type='submit'>Send</button>");
      client.print("</form>");
      client.print("</div>");
      client.print("<script>");
      client.print("let messagesDiv = document.getElementById('messages');");
      client.print("let message = '");
      client.print(incomingMessage);
      client.print("';");
      client.print("if (message) {");
      client.print("  let msgDiv = document.createElement('div');");
      client.print("  msgDiv.classList.add('message', 'received');");
      client.print("  msgDiv.textContent = 'Receiver: ' + message;");
      client.print("  messagesDiv.appendChild(msgDiv);");
      client.print("  messagesDiv.scrollTop = messagesDiv.scrollHeight;");
      client.print("}");
      client.print("document.getElementById('messageForm').onsubmit = function(event) {");
      client.print("  event.preventDefault();");
      client.print("  let msg = document.getElementById('messageInput').value;");
      client.print("  let msgDiv = document.createElement('div');");
      client.print("  msgDiv.classList.add('message', 'sent');");
      client.print("  msgDiv.textContent = 'Sender: ' + msg;");
      client.print("  messagesDiv.appendChild(msgDiv);");
      client.print("  messagesDiv.scrollTop = messagesDiv.scrollHeight;");
      client.print("  fetch('/send?message=' + encodeURIComponent(msg));");
      client.print("  document.getElementById('messageInput').value = '';");
      client.print("}");
      client.print("</script>");
      client.print("</body></html>");
      client.stop();
    }
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    incomingMessage = "";
    while (LoRa.available()) {
      incomingMessage += (char)LoRa.read();
    }
    Serial.print("Received: ");
    Serial.println(incomingMessage);
  }
}
