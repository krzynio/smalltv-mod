// StockPages.h — pages that look and work like the stock GeekMagic firmware's:
// the firmware update form (/update) and the Wi-Fi setup page (/network.html),
// so updating or joining a network feels the same before and after the switch.
// Written for this firmware (no stock assets): same layout, fields and texts.
#pragma once
#include <Arduino.h>

// GET /update. The form posts back to /update with the file in "firmware",
// exactly like the stock page, so the stock image can be flashed back the same way.
static const char STOCK_UPDATE_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang='en'>
<head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'/>
</head>
<body>
<form method='POST' action='' enctype='multipart/form-data'>
PLEASE CHECK PRODUCT MODEL BEFORE UPDATE!!!<p>
1. Choose firmware file.<p>
2. Click Update.<p>
3. Wait, do nothing.<p>
Firmware:<br>
<input type='file' accept='.bin,.bin.gz' name='firmware'>
<input type='submit' value='Update'>
</form>
</body>
</html>)HTML";

static const char STOCK_UPDATE_OK[] PROGMEM =
    "<META http-equiv=\"refresh\" content=\"15;URL=/\">Update Success! Rebooting...";

// GET /network.html: scan list with signal strength, name, password, save.
// Talks to /wifi.json?q=1 and /wifisave?s=&p= like the stock page does.
static const char STOCK_NETWORK_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=0.8, minimal-ui">
<meta name="theme-color" content="#36393E">
<title>WiFi</title>
<style>
body{margin:0;background:#36393e;color:#dcddde;font:16px/1.5 -apple-system,Segoe UI,Roboto,sans-serif}
nav{width:80%;margin:0 auto;font-size:25px;text-align:center;padding:10px 0}
nav a{color:#fff;text-decoration:none;margin:0 10px}
.container{max-width:640px;margin:0 auto;padding:0 16px 32px}
h1{font-size:22px;margin:24px 0 10px;color:#fff}
label{display:block;margin-top:12px}
input,select{width:100%;box-sizing:border-box;height:44px;margin:6px 0;padding:0 12px;border:0;border-radius:4px;background:#2f3136;color:#fff;font-size:15px}
.rowf{display:flex;gap:10px;align-items:center}
.rowf select{flex:1}
button{height:40px;padding:0 18px;border:0;border-radius:4px;background:#5865f2;color:#fff;font-size:15px;text-transform:uppercase;cursor:pointer}
.red{color:#f04747}.bold{font-weight:700}.warm{color:orange}
.popup{position:fixed;top:16px;left:50%;transform:translateX(-50%);padding:10px 18px;border-radius:6px;color:#fff;display:none}
</style>
</head>
<body>
<nav><a href="/network.html">Network</a><a href="/">Settings</a><a href="/update">Update</a></nav>
<div class="container">
<div id="popup" class="popup"></div>
<h1>1. Scan WiFi</h1>
<div class="rowf"><select id="wifi-select"><option value="">Scanning...</option></select><button id="rescan">RESCAN</button></div>
<p class="red bold">Please select WiFi with strong signal to connect, 2.4G only, 5G is not supported.</p>
<label for="ssid">WiFi Name:</label><input type="text" id="ssid" placeholder="Select or Input Your WiFi Name">
<label for="pwd">WiFi Password:</label><input type="password" id="pwd" placeholder="Input password">
<p><button id="save">save</button></p>
<p class="warm">Warm reminder: After successfully connecting to the Internet, the "GIFTV" hotspot will be turned off, and the new IP address will be displayed on the screen when it is turned on. Please connect your phone to the same WIFI, then open a web browser and enter the new IP address to change settings.</p>
</div>
<script>
var $=function(i){return document.getElementById(i)};
function popup(t,ms,c){var p=$('popup');p.textContent=t;p.style.background=c;p.style.display='block';setTimeout(function(){p.style.display='none'},ms)}
function scan(){var s=$('wifi-select');s.disabled=true;s.innerHTML='<option>Scanning...</option>';
fetch('wifi.json?q=1').then(function(r){return r.json()}).then(function(res){s.disabled=false;s.innerHTML='';
var o=document.createElement('option');o.value='';o.textContent='Click Here to Select WiFi';s.appendChild(o);
var aps=(res.aps||[]).sort(function(a,b){return b.r-a.r});
if(!aps.length){o=document.createElement('option');o.value='';o.textContent='No WiFi Available';s.appendChild(o)}
aps.forEach(function(ap){o=document.createElement('option');o.value=ap.ss;
var lvl=ap.r>=70?'Strong':ap.r>=40?'Medium':'Weak';o.textContent=lvl+': '+ap.ss+' ('+ap.r+'%)';s.appendChild(o)})
}).catch(function(){s.disabled=false;s.innerHTML='<option value="">Load failed</option>'})}
$('wifi-select').onchange=function(){$('ssid').value=this.value;$('pwd').value='';$('pwd').focus()};
$('rescan').onclick=scan;
$('save').onclick=function(){fetch('/wifisave?s='+encodeURIComponent($('ssid').value)+'&p='+encodeURIComponent($('pwd').value))
.then(function(r){return r.text()}).then(function(t){if(t=='OK')popup('WiFi info saved, the device will try reboot to connect.',3000,'#02a601');else popup('Save failed!',2000,'#dc0d04')})
.catch(function(){popup('WiFi info saved, the device will try reboot to connect.',3000,'#02a601')})};
scan();
</script>
</body>
</html>)HTML";
