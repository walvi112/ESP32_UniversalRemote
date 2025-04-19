# ESP32 Universal Remote  
A universal remote control powered by ESP32.

<p align="center">  
  <img src="/Firmware_UniversalRemote/demo.jpeg" width="300px">  
</p>

## Features

### 🔧 Hardware  
- IR receiver for learning new IR codes  
- 7 IR emitters for all-angle transmission

### 💻 Firmware  
- Web server to send/add new IR commands  
- mDNS service broadcasts the web server at [`remote.local`](http://remote.local)  
- Supports up to 5 different TV command sets  
- Supports up to 50 different IR protocols  
- Easy Wi-Fi setup via AP mode

---

## 🚀 Setup

### 📶 Wi-Fi Setup  
1. On startup, press the **user button** to enter AP mode  
2. Connect to the `esp32remote` Wi-Fi network  
3. In your browser, go to [`remote.local`](http://remote.local)  
4. Enter your Wi-Fi information and click `Submit`

<p align="center">  
  <img src="/Firmware_UniversalRemote/login_demo.jpeg" width="300px">  
</p>

---

### 🔄 Send/Add IR Code

#### ➕ Add IR Code  
1. After setting the Wi-Fi network, go to [`remote.local`](http://remote.local)  
2. Select **Add TV** from the dropdown and choose a remote ID  
3. Click the **Register** button — the LED will start blinking  
4. Point your TV remote at the IR receiver and press a key  
5. If successful, the LED stops blinking; if not, it times out after 5 seconds

<p align="center">  
  <img src="/Firmware_UniversalRemote/addTV_demo.jpeg" width="300px">  
</p>

#### 📤 Send IR Code  
1. Select **TV Remote** and choose a remote ID from the dropdown  
2. Press the key you want to send

---

### 🐞 Debugging

- Use a serial monitor with **baud rate: 115200** to view logs  
- You can also send serial commands to the device

#### 🔧 Serial Commands  
| Command | Description |
|--------|-------------|
| `led on` | Turn on the LED |
| `led off` | Turn off the LED |
| `send ir _protocol _address _command` | Send IR code (all values in decimal). Refer to `irmpprotocols.h` for `_protocol` values |
| `set wifi _ssid+_pwd` | Set Wi-Fi SSID and password |
| `add tv ir _ir_code _remote_id` | Add new IR command to `_remote_id`. LED will blink while waiting for input |
| `reset wifi` | Enter AP mode (same as pressing user button) |
| `restart` | Restart the device |
