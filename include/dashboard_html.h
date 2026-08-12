#ifndef DASHBOARD_HTML_H
#define DASHBOARD_HTML_H

const char dashboard_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CYD Photo Frame</title>
<style>
body { font-family: 'Inter', system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; box-sizing: border-box; }
.card { background: #181825; border-radius: 12px; padding: 40px 30px; width: 100%; max-width: 400px; box-shadow: 0 8px 30px rgba(0,0,0,0.3); border: 1px solid #313244; text-align: center; }
h1 { color: #cba6f7; margin-top: 0; margin-bottom: 5px; font-weight: 700; }
p.subtitle { color: #a6adc8; margin-top: 0; margin-bottom: 30px; font-size: 15px; }
.btn { display: flex; align-items: center; justify-content: center; text-decoration: none; width: 100%; padding: 14px; border-radius: 8px; color: #11111b; font-size: 16px; font-weight: bold; cursor: pointer; transition: transform 0.2s, background 0.2s; margin-bottom: 15px; box-sizing: border-box; }
.btn:active { transform: scale(0.98); }
.btn-upload { background: #a6e3a1; }
.btn-upload:hover { background: #94e2d5; }
.btn-settings { background: #89b4fa; }
.btn-settings:hover { background: #b4befe; }
.btn-screenshot { background: #f5c2e7; }
.btn-screenshot:hover { background: #f5e0dc; }
.btn-ota { background: #fab387; }
.btn-ota:hover { background: #f9e2af; }
.btn-reset { background: #f38ba8; margin-top: 30px; }
.btn-reset:hover { background: #eba0ac; }
</style>
</head>
<body>
<div class='card'>
<h1>CYD Photo Frame</h1>
<p class='subtitle'>Version %APP_VERSION%</p>

<a href="/upload" class="btn btn-upload">🖼️ Manage Photos</a>
<a href="/settings" class="btn btn-settings">⚙️ Device Settings</a>
<a href="/screenshot" class="btn btn-screenshot" target="_blank">📸 View Screenshot</a>
<a href="/update" class="btn btn-ota">🔄 Firmware Update</a>

<a href="/reset" class="btn btn-reset" onclick="return confirm('Are you sure you want to factory reset this device? All settings and Wi-Fi credentials will be erased and the device will restart in AP Setup mode. This cannot be undone.');">⚠️ Factory Reset</a>

<p style="margin-top: 25px; font-size: 13px; color: #6c7086;">Built for %DEVICE_NAME% | <a href="https://github.com/nicholaswilde/cyd-photo-frame" target="_blank" style="color: #89b4fa; text-decoration: none;">GitHub</a></p>
</div>
</body>
</html>
)=====";

#endif
