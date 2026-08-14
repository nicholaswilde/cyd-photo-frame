#ifndef SETTINGS_HTML_H
#define SETTINGS_HTML_H

const char settings_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CYD Photo Frame - Settings</title>
<style>
body { font-family: 'Inter', system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: flex-start; min-height: 100vh; box-sizing: border-box; }
.card { background: #181825; border-radius: 12px; padding: 30px; width: 100%; max-width: 500px; box-shadow: 0 8px 30px rgba(0,0,0,0.3); border: 1px solid #313244; }
h2 { color: #f5c2e7; margin-top: 0; margin-bottom: 20px; font-weight: 600; text-align: center; }
label { display: block; margin-bottom: 8px; color: #a6adc8; font-size: 14px; }
select, input[type='text'], input[type='password'], input[type='number'] { width: 100%; padding: 12px; margin-bottom: 20px; border-radius: 6px; border: 1px solid #45475a; background: #313244; color: #cdd6f4; font-size: 16px; box-sizing: border-box; }
select:focus, input:focus { outline: none; border-color: #f5c2e7; }
input[type='checkbox'] { width: auto; margin-right: 10px; margin-bottom: 0; transform: scale(1.2); }
.slider-group { display: flex; align-items: center; gap: 15px; margin-bottom: 20px; }
.slider-group input[type='range'] { flex-grow: 1; margin: 0; cursor: pointer; }
.slider-group input[type='number'] { width: 80px; margin-bottom: 0; flex-shrink: 0; }
.checkbox-group { display: flex; align-items: center; margin-bottom: 10px; }
.checkbox-group label { margin-bottom: 0; margin-top: 2px; }
button { width: 100%; padding: 12px; background: #cba6f7; border: none; border-radius: 6px; color: #11111b; font-size: 16px; font-weight: bold; cursor: pointer; transition: background 0.2s; margin-top: 10px; }
button:hover { background: #f5c2e7; }
.section-title { color: #89b4fa; font-size: 18px; margin-top: 20px; margin-bottom: 15px; border-bottom: 1px solid #313244; padding-bottom: 5px; }
.btn-back { display: block; margin-top: 15px; color: #a6adc8; text-decoration: none; font-size: 14px; text-align: center; transition: color 0.2s; }
.btn-back:hover { color: #cdd6f4; }
</style>
</head>
<body>
<div class='card'>
<h2 style="margin-bottom: 5px;">CYD Photo Frame</h2>
<p style="text-align: center; color: #a6adc8; margin-top: 0; margin-bottom: 20px; font-size: 14px;">Version %APP_VERSION%</p>
<form method='POST' action='/settings/save'>

<div class='section-title' style='margin-top: 0;'>Display & UI</div>
<label for='theme_flavor' title='Select the color palette for the user interface'>Theme (Catppuccin)</label>
<select id='theme_flavor' name='theme_flavor'>
    <option value='0' %THEME_MOCHA%>Mocha</option>
    <option value='1' %THEME_MACCHIATO%>Macchiato</option>
    <option value='2' %THEME_FRAPPE%>Frappe</option>
    <option value='3' %THEME_LATTE%>Latte</option>
</select>

<label for='screen_orientation' title='Set the rotation of the screen'>Screen Orientation</label>
<select id='screen_orientation' name='screen_orientation'>
    <option value='0' %ORIENT_0%>Portrait (0°)</option>
    <option value='1' %ORIENT_1%>Landscape (90°)</option>
    <option value='2' %ORIENT_2%>Portrait Rev (180°)</option>
    <option value='3' %ORIENT_3%>Landscape Rev (270°)</option>
</select>

<label for='brightness' title='Adjust the display backlight brightness'>Screen Brightness (%)</label>
<div class='slider-group'>
    <input type='range' id='brightness_slider' min='10' max='100' value='%BRIGHTNESS%' oninput='document.getElementById("brightness").value = this.value'>
    <input type='number' id='brightness' name='brightness' min='10' max='100' value='%BRIGHTNESS%' oninput='document.getElementById("brightness_slider").value = this.value'>
</div>

<div class='checkbox-group'>
    <input type='checkbox' id='auto_brightness' name='auto_brightness' value='1' %AUTO_BRIGHTNESS%>
    <label for='auto_brightness' title='Automatically adjust brightness based on time of day'>Auto Brightness</label>
</div>

<div class='checkbox-group'>
    <input type='checkbox' id='inactivity_sleep' name='inactivity_sleep' value='1' %INACTIVITY_SLEEP%>
    <label for='inactivity_sleep' title='Turn off screen after a period of inactivity'>Inactivity Sleep</label>
</div>

<div class='section-title'>Slideshow Options</div>
<label for='slideshow_interval' title='Time before switching to the next photo'>Slideshow Interval</label>
<select id='slideshow_interval' name='slideshow_interval'>
    <option value='2' %DELAY_2%>2 Seconds</option>
    <option value='5' %DELAY_5%>5 Seconds</option>
    <option value='10' %DELAY_10%>10 Seconds</option>
    <option value='15' %DELAY_15%>15 Seconds</option>
    <option value='30' %DELAY_30%>30 Seconds</option>
    <option value='60' %DELAY_60%>60 Seconds</option>
</select>

<div class='checkbox-group'>
    <input type='checkbox' id='random_mode' name='random_mode' value='1' %RANDOM_MODE%>
    <label for='random_mode' title='Display photos in a random order'>Randomize Photos</label>
</div>

<div class='checkbox-group'>
    <input type='checkbox' id='show_filename' name='show_filename' value='1' %SHOW_FILENAME%>
    <label for='show_filename' title='Display the current file name on the screen'>Show Filename</label>
</div>

<div class='checkbox-group'>
    <input type='checkbox' id='bypass_opt' name='bypass_opt' value='1' %BYPASS_OPT%>
    <label for='bypass_opt' title='Load images directly without resizing optimization'>Bypass Image Optimization</label>
</div>

<div class='section-title'>System Features</div>
<div class='checkbox-group'>
    <input type='checkbox' id='led_enabled' name='led_enabled' value='1' %LED_ENABLED%>
    <label for='led_enabled' title='Enable or disable the onboard RGB LED'>RGB LED Enabled</label>
</div>

<label for='led_brightness' title='Adjust the brightness of the RGB LED'>LED Brightness (%)</label>
<div class='slider-group'>
    <input type='range' id='led_brightness_slider' min='0' max='100' value='%LED_BRIGHTNESS%' oninput='document.getElementById("led_brightness").value = this.value'>
    <input type='number' id='led_brightness' name='led_brightness' min='0' max='100' value='%LED_BRIGHTNESS%' oninput='document.getElementById("led_brightness_slider").value = this.value'>
</div>

<div class='checkbox-group'>
    <input type='checkbox' id='wifi_enabled' name='wifi_enabled' value='1' %WIFI_ENABLED%>
    <label for='wifi_enabled' title='Enable WiFi to connect to the network'>WiFi Enabled</label>
</div>

<div class='checkbox-group'>
    <input type='checkbox' id='mqtt_enabled' name='mqtt_enabled' value='1' %MQTT_ENABLED% onchange='toggleMqttSettings()'>
    <label for='mqtt_enabled' title='Enable MQTT to publish device state and receive commands'>MQTT Enabled</label>
</div>

<div id='mqtt_settings' style='display: none; margin-left: 20px; border-left: 2px solid #313244; padding-left: 15px; margin-bottom: 20px;'>
    <label for='mqtt_server' title='Hostname or IP address of the MQTT broker'>MQTT Server</label>
    <input type='text' id='mqtt_server' name='mqtt_server' value='%MQTT_SERVER%'>
    
    <label for='mqtt_port' title='Port number for the MQTT broker (default: 1883)'>MQTT Port</label>
    <input type='number' id='mqtt_port' name='mqtt_port' value='%MQTT_PORT%'>
    
    <label for='mqtt_user' title='Username for MQTT authentication (leave blank if none)'>MQTT Username</label>
    <input type='text' id='mqtt_user' name='mqtt_user' value='%MQTT_USER%'>
    
    <label for='mqtt_password' title='Password for MQTT authentication (leave blank if none)'>MQTT Password</label>
    <input type='password' id='mqtt_password' name='mqtt_password' value='%MQTT_PASSWORD%'>
    
    <label for='mqtt_base_topic' title='Base topic prefix for MQTT (e.g. home/living_room/photo_frame/)'>MQTT Base Topic</label>
    <input type='text' id='mqtt_base_topic' name='mqtt_base_topic' value='%MQTT_BASE_TOPIC%'>
</div>

<div class='checkbox-group'>
    <input type='checkbox' id='static_ip_enabled' name='static_ip_enabled' value='1' %STATIC_IP_ENABLED% onchange='toggleStaticIpSettings()'>
    <label for='static_ip_enabled' title='Use a static IP address instead of DHCP'>Static IP Enabled</label>
</div>

<div id='static_ip_settings' style='display: none; margin-left: 20px; border-left: 2px solid #313244; padding-left: 15px; margin-bottom: 20px;'>
    <label for='static_ip' title='Static IP address (e.g. 192.168.1.100)'>IP Address</label>
    <input type='text' id='static_ip' name='static_ip' value='%STATIC_IP%'>
    <label for='static_gw' title='Gateway IP address (e.g. 192.168.1.1)'>Gateway</label>
    <input type='text' id='static_gw' name='static_gw' value='%STATIC_GW%'>
    <label for='static_sn' title='Subnet mask (e.g. 255.255.255.0)'>Subnet Mask</label>
    <input type='text' id='static_sn' name='static_sn' value='%STATIC_SN%'>
    <label for='static_dns' title='DNS server IP address (e.g. 8.8.8.8)'>DNS Server</label>
    <input type='text' id='static_dns' name='static_dns' value='%STATIC_DNS%'>
</div>

<button type='submit'>Save Settings & Reboot</button>
</form>
<a href="/" class="btn-back">&larr; Back to Dashboard</a>
<p style="margin-top: 25px; margin-bottom: 0; font-size: 13px; color: #6c7086; text-align: center;">Built for %DEVICE_NAME% | <a href="https://github.com/nicholaswilde/cyd-photo-frame" target="_blank" style="color: #89b4fa; text-decoration: none;">GitHub</a></p>
</div>
<script>
function toggleMqttSettings() {
    var cb = document.getElementById('mqtt_enabled');
    var div = document.getElementById('mqtt_settings');
    if (cb && div) {
        div.style.display = cb.checked ? 'block' : 'none';
    }
}
function toggleStaticIpSettings() {
    var cb = document.getElementById('static_ip_enabled');
    var div = document.getElementById('static_ip_settings');
    if (cb && div) {
        div.style.display = cb.checked ? 'block' : 'none';
    }
}
window.onload = function() {
    toggleMqttSettings();
    toggleStaticIpSettings();
};
</script>
</body>
</html>
)=====";

#endif // SETTINGS_HTML_H
