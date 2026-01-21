#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// =================== SETTINGS ===================
const char* ssid = "1234";          
const char* password = "12345678";  
String cameraIP = "10.117.127.213";      

// =================== PINS ===================
const int IN1 = 13; const int IN2 = 12; 
const int IN3 = 14; const int IN4 = 27;
const int laserPin = 4;   
const int servoPanPin = 18;
const int servoTiltPin = 19;

Servo servoPan; Servo servoTilt;
int panAngle = 90; int tiltAngle = 90;
int flashVal = 0; 
bool laserState = false;

WebServer server(80);

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);
  
  ESP32PWM::allocateTimer(0); ESP32PWM::allocateTimer(1);
  servoPan.setPeriodHertz(50); servoTilt.setPeriodHertz(50);
  servoPan.attach(servoPanPin, 500, 2400); servoTilt.attach(servoTiltPin, 500, 2400);
  servoPan.write(panAngle); servoTilt.write(tiltAngle);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(laserPin, OUTPUT); digitalWrite(laserPin, LOW); 
  stopCar(); 

  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  
  server.enableCORS(true);
  server.on("/", handleRoot);
  server.on("/car", handleCar);
  server.on("/camera", handleCamera);
  server.on("/laser_toggle", handleLaserToggle);
  server.begin();
}

void loop() { server.handleClient(); }

// =================== CONTROLS ===================
void stopCar() { digitalWrite(IN1, 0); digitalWrite(IN2, 0); digitalWrite(IN3, 0); digitalWrite(IN4, 0); }

void handleCar() {
  String dir = server.arg("dir");
  if (dir == "stop") stopCar();
  else if (dir == "F") { digitalWrite(IN1, 1); digitalWrite(IN2, 0); digitalWrite(IN3, 1); digitalWrite(IN4, 0); }
  else if (dir == "B") { digitalWrite(IN1, 0); digitalWrite(IN2, 1); digitalWrite(IN3, 0); digitalWrite(IN4, 1); }
  else if (dir == "L") { digitalWrite(IN1, 0); digitalWrite(IN2, 1); digitalWrite(IN3, 1); digitalWrite(IN4, 0); }
  else if (dir == "R") { digitalWrite(IN1, 1); digitalWrite(IN2, 0); digitalWrite(IN3, 0); digitalWrite(IN4, 1); }
  server.send(200);
}

void handleCamera() {
  String dir = server.arg("dir");
  if (dir == "up") tiltAngle -= 6; else if (dir == "down") tiltAngle += 6;
  else if (dir == "left") panAngle += 6; else if (dir == "right") panAngle -= 6;
  else if (dir == "center") { panAngle = 90; tiltAngle = 90; }
  panAngle = constrain(panAngle, 0, 180); tiltAngle = constrain(tiltAngle, 0, 180);
  servoPan.write(panAngle); servoTilt.write(tiltAngle);
  server.send(200);
}

void handleLaserToggle() {
  laserState = !laserState;
  digitalWrite(laserPin, laserState ? HIGH : LOW);
  server.send(200, "text/plain", laserState ? "ON" : "OFF");
}

// =================== INTERFACE ===================
void handleRoot() {
  // استخدام حاوية نصية قوية جداً لمنع الأخطاء
  String html = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>PRO ROVER AI</title>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/tracking.js/1.1.3/tracking-min.js"></script>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/tracking.js/1.1.3/data/face-min.js"></script>
  <style>
    body { background: #0a0a0a; color: #00d2ff; font-family: 'Courier New', monospace; text-align: center; margin: 0; user-select: none; }
    .vid-frame { position: relative; width: 480px; height: 360px; margin: 0 auto; border-bottom: 2px solid #00d2ff; background: #000; }
    #camStream, #canvas { position: absolute; top: 0; left: 0; width: 100%; height: 100%; object-fit: fill; }
    .status-box { font-size: 16px; font-weight: bold; margin: 8px; height: 20px; color: #ffeb3b; text-shadow: 0 0 5px #ffeb3b; }
    .info-txt { font-size: 10px; color: #666; margin-bottom: 5px; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; padding: 10px; max-width: 600px; margin: 0 auto; }
    .panel { background: #111; border: 1px solid #333; border-radius: 15px; padding: 10px; }
    .btn { width: 55px; height: 55px; background: #222; border: 1px solid #444; color: #fff; border-radius: 12px; margin: 3px; font-size: 18px; cursor: pointer; touch-action: manipulation; }
    .btn:active, .btn.active { background: #00d2ff; color: #000; transform: scale(0.95); }
    .tool-btn { width: 100%; padding: 10px; margin: 5px 0; background: #222; color: white; border: 1px solid #444; border-radius: 8px; cursor: pointer; }
    .tool-btn.active { background: #00d2ff; color: #000; }
  </style>
</head>
<body>
  
  <div class="vid-frame">
    <img id="camStream" src="http://XX_IP_XX:81/stream" crossorigin="anonymous" width="480" height="360">
    <canvas id="canvas" width="480" height="360"></canvas>
  </div>
  
  <div class="status-box" id="aiStatus">MANUAL MODE</div>
  <div class="info-txt">CONTROLS: MOUSE | TOUCH | KEYBOARD [WASD] | CONTROLLER</div>

  <div class="grid">
    <div class="panel">
      <div style="color:#aaa; font-size:12px; margin-bottom:5px;">DRIVE (WASD)</div>
      <button class="btn" id="keyW" onmousedown="mv('F')" onmouseup="st()" ontouchstart="mv('F')" ontouchend="st()">▲</button><br>
      <button class="btn" id="keyA" onmousedown="mv('L')" onmouseup="st()" ontouchstart="mv('L')" ontouchend="st()">◀</button>
      <button class="btn" id="keyX" onmousedown="st()" style="background:#f44336">■</button>
      <button class="btn" id="keyD" onmousedown="mv('R')" onmouseup="st()" ontouchstart="mv('R')" ontouchend="st()">▶</button><br>
      <button class="btn" id="keyS" onmousedown="mv('B')" onmouseup="st()" ontouchstart="mv('B')" ontouchend="st()">▼</button>
    </div>
    
    <div class="panel">
      <div style="color:#aaa; font-size:12px; margin-bottom:5px;">CAM (ARROWS)</div>
      <button class="btn" id="keyUp" onclick="cm('up')">▲</button><br>
      <button class="btn" id="keyLeft" onclick="cm('left')">◀</button>
      <button class="btn" id="keyEnter" onclick="cm('center')" style="font-size:12px">●</button>
      <button class="btn" id="keyRight" onclick="cm('right')">▶</button><br>
      <button class="btn" id="keyDown" onclick="cm('down')">▼</button>
    </div>
  </div>

  <div style="max-width:600px; margin:0 auto; padding:10px;">
    <button id="btnAI" class="tool-btn" onclick="toggleAI()">[SPACE] AI FACE DETECTION</button>
    <div style="display:flex; gap:10px;">
      <button id="keyR" class="tool-btn" onclick="lsr()">[R] LASER</button>
      <button id="keyL" class="tool-btn" onclick="fls()">[L] FLASH</button>
    </div>
  </div>

  <script>
    const camIP = "XX_IP_XX";
    let flash = 0; 
    let ai = false; 
    let task = null;

    function s(u) { fetch(u).catch(e => console.log(e)); }
    function mv(d) { s('/car?dir='+d); }
    function st() { s('/car?dir=stop'); }
    function cm(d) { s('/camera?dir='+d); }
    function lsr() { s('/laser_toggle'); }
    function fls() { flash = (flash + 51) % 306; if(flash>255) flash=0; fetch('http://'+camIP+'/control?var=led_intensity&val='+flash); }

    document.addEventListener('keydown', e => {
      if(e.repeat) return;
      const k = e.key.toLowerCase();
      if(k==='w') { mv('F'); hl('keyW'); }
      if(k==='s') { mv('B'); hl('keyS'); }
      if(k==='a') { mv('L'); hl('keyA'); }
      if(k==='d') { mv('R'); hl('keyD'); }
      if(k==='x') { st(); hl('keyX'); }
      
      if(e.key==='ArrowUp') { cm('up'); hl('keyUp'); }
      if(e.key==='ArrowDown') { cm('down'); hl('keyDown'); }
      if(e.key==='ArrowLeft') { cm('left'); hl('keyLeft'); }
      if(e.key==='ArrowRight') { cm('right'); hl('keyRight'); }
      if(e.key==='Enter') { cm('center'); hl('keyEnter'); }

      if(k==='r') { lsr(); hl('keyR'); }
      if(k===' ') { toggleAI(); hl('btnAI'); }
      if(k==='l') { fls(); hl('keyL'); }
    });

    document.addEventListener('keyup', e => {
      if(['w','a','s','d'].includes(e.key.toLowerCase())) st();
      remHl();
    });

    function hl(id) { 
      const el = document.getElementById(id); 
      if(el) el.classList.add('active'); 
    }
    function remHl() { 
      document.querySelectorAll('.btn, .tool-btn').forEach(b => b.classList.remove('active')); 
      if(ai) document.getElementById('btnAI').classList.add('active');
    }

    function toggleAI() {
      ai = !ai;
      const stBox = document.getElementById('aiStatus');
      const btn = document.getElementById('btnAI');
      if(ai) {
        stBox.innerText = "SEARCHING FACE...";
        stBox.style.color = "#ffeb3b";
        btn.classList.add('active');
        startTracking();
      } else {
        stBox.innerText = "MANUAL MODE";
        stBox.style.color = "#ffeb3b";
        btn.classList.remove('active');
        if(task) task.stop();
        document.getElementById('canvas').getContext('2d').clearRect(0,0,480,360);
      }
    }

    function startTracking() {
      const tracker = new tracking.ObjectTracker('face');
      tracker.setInitialScale(4);
      tracker.setStepSize(2);
      tracker.setEdgesDensity(0.1);

      task = tracking.track('#camStream', tracker);

      tracker.on('track', function(event) {
        const ctx = document.getElementById('canvas').getContext('2d');
        ctx.clearRect(0, 0, 480, 360);

        if (event.data.length === 0) {
          document.getElementById('aiStatus').innerText = "SEARCHING FACE...";
          document.getElementById('aiStatus').style.color = "#ffeb3b";
        } else {
          document.getElementById('aiStatus').innerText = "FACE DETECTED!";
          document.getElementById('aiStatus').style.color = "#00ff00";

          event.data.forEach(function(rect) {
            ctx.strokeStyle = '#00ff00';
            ctx.lineWidth = 4;
            ctx.strokeRect(rect.x, rect.y, rect.width, rect.height);
            
            ctx.fillStyle = "#00ff00";
            ctx.font = "12px Courier";
            ctx.fillText("TARGET", rect.x, rect.y - 5);
            
            const cx = 240;
            const cy = 180;
            const threshold = 50;

            if(rect.x + rect.width/2 < cx - threshold) cm('left');
            else if(rect.x + rect.width/2 > cx + threshold) cm('right');
            
            if(rect.y + rect.height/2 < cy - threshold) cm('up');
            else if(rect.y + rect.height/2 > cy + threshold) cm('down');
          });
        }
      });
    }
  </script>
</body>
</html>
)=====";
  
  html.replace("XX_IP_XX", cameraIP);
  server.send(200, "text/html", html);
}