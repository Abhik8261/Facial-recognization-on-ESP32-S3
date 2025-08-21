# Facial Recognition on ESP32-S3 (UniHIKER K10)

This project shows how a **deep learning facial recognition model** can run directly on a low-power **ESP32-S3 device (UniHIKER K10)**.  
It is part of my master's dissertation, where I explore how **TinyML** can bring AI to small, resource-constrained devices.  

---

## 🚀 What it Does
- Uses the UniHIKER K10’s built-in **camera** to capture faces.  
- Saves the captured images to an **SD card**.  
- Converts images into a **96×96 grayscale format** that the AI model understands.  
- Runs a **trained Edge Impulse model** directly on the device.  
- Shows the recognition results on the K10’s screen.  
- Buttons make it simple to use:
  - **Button A → Capture an image**  
  - **Button B → Run inference on the latest image**

---

## ⚙️ Hardware
- UniHIKER K10 (ESP32-S3 with onboard LCD and camera)  
- MicroSD card (formatted as FAT32)  
- USB-C cable to connect with laptop  

---

## 🛠️ Software
- Arduino IDE **1.8.19 (or below)**  
- ESP32 board package  
- Edge Impulse SDK (from exported model)  
- UniHIKER K10 device library  


## 📖 How to Use
1. Clone this repository.  
2. Open the `uihike.ino` file in Arduino IDE.  
3. Insert a formatted SD card into the UniHIKER K10.  
4. Upload the sketch to the board.  
5. Use the buttons:  
   - **Press A** → Takes a picture and stores it on the SD card.  
   - **Press B** → Runs the AI model on the most recent image and displays the result.  

---

## 📊 Workflow
The process is simple:  

**Capture → Preprocess → Store → Run AI model → Display result**  

---

## ✅ Current Status
- The system is working end-to-end.  
- Capturing and saving images works reliably.  
- The model runs on-device and shows classification results.  
- Ongoing work: improving dataset quality and model accuracy.  

---

## 📌 Notes
- If results don’t look correct, the issue is usually the **dataset size/quality**, not the pipeline.  
- The model should be trained with enough diverse images per person/class to perform well.  

---

## 👨‍💻 Author
Developed by **Abhinav**  
Master’s Dissertation Project: *Deep Learning with TinyML on ESP32-S3 (UniHIKER K10)*  

---

## 📖 License
This project is open-source under the MIT License.  
Feel free to use and adapt it with attribution.  
