
// ========== BLYNK =========
#define BLYNK_TEMPLATE_ID "TMPL21-00MbYE"
#define BLYNK_TEMPLATE_NAME "BENEDICTO SECURE HOMES"
#define BLYNK_AUTH_TOKEN "5eF1JsYKFPnZblY8d7jOScuQFAOLCDmi"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <BlynkSimpleEsp32.h>

// ========== Wi-Fi ==========
const char* ssid     = "Muhumuza";
const char* password = "79355553";

// ========== GPIO Pins ==========
const uint8_t PIR_PIN    = 4;
const uint8_t LED_PIN    = 13;
const uint8_t BUZZER_PIN = 27;

// ========== Alarm Logic ==========
bool alarmArmed   = true;
bool alarmActive  = false;
bool inCooldown   = false;
unsigned long alarmStart = 0UL;
unsigned long cooldownStart = 0UL;
const unsigned long ALARM_DURATION = 10000;
const unsigned long COOLDOWN_DURATION = 5000;

unsigned long lastIpPrint = 0UL;
const unsigned long IP_INTERVAL = 10000;

bool loggedIn = false;
const String USERNAME = "admin";
const String PASSWORD = "1234";

WebServer server(80);
BlynkTimer timer;

// ========== Blynk Virtual Pin V1 ==========
BLYNK_WRITE(V1) {
  alarmArmed = param.asInt();
  Serial.println(alarmArmed ? "🔒 Alarm Armed via Blynk" : "🔕 Alarm Disarmed via Blynk");

  // Reset all outputs and states if disarmed
  if (!alarmArmed) {
    alarmActive = false;
    inCooldown = false;
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ========== Web Pages ==========
void handleMotionStatus() {
  bool m = digitalRead(PIR_PIN) == HIGH;
  String resp = m
    ? "<span style='color:#e67e22;font-weight:bold;'>Motion Detected</span>"
    : "<span style='color:#888;'>No Motion</span>";
  server.send(200, "text/html", resp);
}

void handleLoginPage() {
  String html = R"rawliteral(
    <!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
    <title>BENEDICTO SECURE SYSTEMS $ Alarm</title><style>
      body{font-family:sans-serif;text-align:center;padding:40px;background:#f2f2f2;}
      form{background:#fff;padding:20px;border-radius:8px;box-shadow:0 0 10px rgba(0,0,0,.2);width:90%;max-width:320px;margin:auto;}
      input{width:90%;margin:10px 0;padding:8px;font-size:1em;}
      button{padding:10px 20px;font-size:1em;background:#27ae60;color:#fff;border:none;border-radius:5px;}
    </style></head><body>
      <h2>Login Required</h2>
      <form method="POST" action="/login">
        <input type="text" name="user" placeholder="Username" required >
        <input type="password" name="pass" placeholder="Password" required >
        <button type="submit">Login</button>
      </form>
    </body></html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void handleLoginSubmit() {
  if (server.hasArg("user") && server.hasArg("pass")) {
    String u = server.arg("user");
    String p = server.arg("pass");
    if (u == USERNAME && p == PASSWORD) {
      loggedIn = true;
      server.sendHeader("Location", "/dashboard", true);
      server.send(302, "text/plain", "");
      return;
    }
  }
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleDashboard() {
  if (!loggedIn) {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
    return;
  }

  bool motion = digitalRead(PIR_PIN) == HIGH;
  String status  = alarmArmed ? "ARMED"   : "DISARMED";
  String motionText = motion ? "Motion Detected" : "No Motion";
  String motionCls  = motion ? "motion" : "no-motion";

  String html = R"rawliteral(
    <!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
    <title>BENEDICTO SECURE SYSTEMS Dashboard</title><style>
      body{font-family:sans-serif;text-align:center;padding:30px;background:#f4f4f4;}
      .card{display:inline-block;padding:20px;background:#fff;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}
      .status, .motion{margin:15px;font-size:1.2em;}
      .alarm-armed{color:#c9302c;} .alarm-disarmed{color:#5cb85c;}
      .motion{font-size:1.4em;color:#e67e22;} .motion.no-motion{color:#888;}
      .btn-arm{background:#5cb85c;} .btn-disarm{background:#dc3545;}
      button{padding:12px 25px;font-size:1em;color:#fff;border:none;border-radius:6px;cursor:pointer;margin-top:15px;}
      .btn-arm:hover{background:#449d44;} .btn-disarm:hover{background:#c12e2a;}
    </style>
    <script>
      setInterval(()=>{
        fetch('/motion_status').then(r=>r.text()).then(txt=>document.getElementById('mstat').innerHTML = txt);
      }, 1000);
    </script></head><body><div class="card">
      <h1>BENEDICTO SECURE SYSTEMS $ ALARM Dashboard</h1>
      <div class="status">
        System is <span class="alarm-%STATUS_CLASS%">%STATUS%</span>
      </div>
      <div class="motion" id="mstat">%MOTION_TEMP%</div>
      <form action="/toggle" method="POST">
        <button class="btn-%BUTTON_CLASS%">%BUTTON_TEXT%</button>
      </form>
    </div></body></html>
  )rawliteral";

  html.replace("%STATUS_CLASS%", alarmArmed ? "armed" : "disarmed");
  html.replace("%STATUS%",       status);
  html.replace("%BUTTON_CLASS%", alarmArmed ? "disarm" : "arm");
  html.replace("%BUTTON_TEXT%",  alarmArmed ? "Disarm Alarm" : "Arm Alarm");
  html.replace("%MOTION_TEMP%",  motionText);

  server.send(200, "text/html", html);
}

void handleToggle() {
  if (!loggedIn) {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
    return;
  }
  alarmArmed = !alarmArmed;
  alarmActive = false;
  inCooldown = false;
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  server.sendHeader("Location", "/dashboard", true);
  server.send(302, "text/plain", "");
  Serial.println(alarmArmed ? "🔓 System Armed (Web)" : "🔐 System Disarmed (Web)");
}

// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi connected");

  // Start Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
  timer.setInterval(500L, [](){
    Blynk.run();
  });

  // Web server routes
  server.on("/",              HTTP_GET,  handleLoginPage);
  server.on("/login",         HTTP_POST, handleLoginSubmit);
  server.on("/dashboard",     HTTP_GET,  handleDashboard);
  server.on("/toggle",        HTTP_POST, handleToggle);
  server.on("/motion_status", HTTP_GET,  handleMotionStatus);
  server.begin();

  Serial.println("📡 Web Dashboard: http://" + WiFi.localIP().toString());
}

// ========== Main Loop ==========
void loop() {
  server.handleClient();
  timer.run();

  unsigned long now = millis();
  if (now - lastIpPrint >= IP_INTERVAL) {
    Serial.println("📡 Dashboard IP: " + WiFi.localIP().toString());
    lastIpPrint = now;
  }

  bool motion = digitalRead(PIR_PIN);

  if (alarmArmed) {
    if (!alarmActive && !inCooldown && motion) {
      alarmActive = true;
      alarmStart = now;
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);
      Serial.println("🚨 Motion Detected! Alarm ON");

      Blynk.logEvent("motion_alert", "🚨 Motion detected at home!");
    }

    if (alarmActive && now - alarmStart >= ALARM_DURATION) {
      alarmActive = false;
      inCooldown = true;
      cooldownStart = now;
      digitalWrite(LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("🕒 Alarm paused (cooldown)");
    }

    if (inCooldown && now - cooldownStart >= COOLDOWN_DURATION) {
      inCooldown = false;
      Serial.println("🔁 Cooldown over, ready again");
    }
  } else {
    alarmActive = false;
    inCooldown = false;
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

