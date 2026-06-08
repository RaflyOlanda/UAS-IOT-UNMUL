# Smart Waste Management Monitoring (SWMM) - Sistem Monitoring Tempat Sampah Pintar Berbasis IoT

Repositori ini dibuat untuk memenuhi tugas Projek Akhir Praktikum Internet of Things (IoT), Program Studi Informatika, Universitas Mulawarman (2026).

---

## Anggota Kelompok

| Nama | NIM |
| :--- | :--- |
| Christian Farrel A. P. | 2309106032 |
| Zulfikar Heriansyah | 2309106033 |
| Muhammad Rafly Pratama O. | 2309106043 |

---

## Judul Projek Akhir

**Smart Waste Management Monitoring (SWMM)**

---

## Deskripsi Projek

Projek ini merupakan sistem Internet of Things (IoT) berupa tempat sampah pintar (*Smart Waste Management Monitoring*) yang mampu melakukan pemantauan kondisi di dalam tempat sampah dan kapasitasnya secara *real-time*, serta dilengkapi dengan sistem buka tutup otomatis tanpa sentuhan (*zero-touch*) berbasis mikrokontroler ESP32.

Sistem ini menggunakan beberapa sensor yang terdiri dari sensor ultrasonik ganda (untuk mendeteksi objek dan kapasitas sampah), sensor DHT11 (suhu dan kelembaban), serta sensor MQ-2 (gas/asap). Data dari sensor dikirim menggunakan protokol MQTT menuju Antares IoT Platform untuk penyimpanan data (*logging*) dan monitoring, sekaligus terhubung dengan Telegram Bot untuk notifikasi otomatis dan kontrol jarak jauh.

Fitur utama pada sistem ini meliputi:

1. **Monitoring Lingkungan Real-Time (Multi-Sensor)**
   * Pemantauan sisa kapasitas tempat sampah (%).
   * Pemantauan suhu di dalam tempat sampah (°C).
   * Pemantauan tingkat kelembaban (%).
   * Pemantauan konsentrasi gas/polutan (PPM).

2. **Sistem Buka Tutup Otomatis (Zero-Touch)**
   * Tutup tempat sampah akan terbuka secara otomatis ketika sensor ultrasonik eksternal mendeteksi tangan/objek dalam radius ≤ 5 cm.
   * Sistem akan menutup kembali secara otomatis setelah beberapa detik.

3. **Integrasi Antares IoT (MQTT)**
   * Pengiriman data sensor secara otomatis menggunakan protokol MQTT.
   * Dashboard monitoring untuk memantau kondisi sistem secara terpusat.

4. **Integrasi Telegram Bot (Alert & Remote Control)**
   * **Notifikasi Otomatis:** Mengirimkan pesan otomatis jika parameter lingkungan melewati batas tertentu, seperti suhu ≥ 40°C, kelembaban ≥ 70% RH, gas ≥ 600 PPM, atau kapasitas ≥ 80%.
   * **Kontrol Manual Dua Arah:** Mendukung penggunaan perintah teks:
     * `/start` atau `/help` - Menampilkan panduan penggunaan.
     * `/open` - Membuka tutup tempat sampah dari jarak jauh.
     * `/close` - Menutup tutup tempat sampah dari jarak jauh.
     * `/status` - Menampilkan kondisi sensor terkini.

---

## Pembagian Tugas

| Nama | Deskripsi Tugas |
| :--- | :--- |
| **Christian Farrel A. P.** | Merancang skematik dan membangun perangkat keras (*hardware*) IoT, serta melakukan pengujian komponen. |
| **Zulfikar Heriansyah** | Melakukan pengujian sistem, kalibrasi batas sensor (*threshold*), dan membantu perancangan logika program. |
| **Muhammad Rafly Pratama O.** | Menyiapkan integrasi platform IoT (Antares & Telegram), memrogram mikrokontroler, dan melakukan pengujian pengiriman data. |

---

## Komponen yang Digunakan

**Perangkat Keras (Hardware):**
* 1x ESP32 DEVKIT V1 (Mikrokontroler Utama)
* 2x Sensor Ultrasonik HC-SR04 (Eksternal & Internal)
* 1x Sensor DHT11 (Sensor Suhu & Kelembaban)
* 1x Sensor Gas MQ-2 (Pendeteksi Gas/Asap)
* 1x Motor Servo (Penggerak Tutup)
* 1x Powerbank 5V DC (Power Supply)
* 1x Breadboard / Project Board
* Kabel Jumper (Male-to-Male, Female-to-Female, Male-to-Female)
* Wadah Tempat Sampah Miniatur

**Platform dan Software:**
* Arduino IDE (C++ Framework)
* Antares IoT Platform (MQTT Broker & Dashboard)
* Telegram API (Bot UI)

---

## Cara Kerja Sistem

### 1. Deteksi Kapasitas (Ultrasonik Internal)
Sensor ultrasonik internal mengukur jarak permukaan sampah di dalam wadah. Nilai jarak tersebut kemudian diproses oleh ESP32 untuk dikonversi menjadi persentase kapasitas tempat sampah.

### 2. Monitoring Lingkungan (DHT11 & MQ-2)
Sensor DHT11 memantau suhu dan kelembaban udara secara terus-menerus, sedangkan sensor MQ-2 digunakan untuk mendeteksi adanya gas atau asap di dalam tempat sampah.

### 3. Sistem Otomatis Tanpa Sentuhan (Ultrasonik Eksternal & Servo)
Sensor ultrasonik eksternal berfungsi untuk mendeteksi keberadaan tangan atau objek pengguna. Jika objek terdeteksi pada jarak tertentu (*threshold* ≤ 5 cm), ESP32 akan mengirimkan sinyal PWM ke motor servo untuk membuka tutup tempat sampah secara otomatis sehingga pengguna tidak perlu menyentuhnya secara langsung.

### 4. Pengiriman Data & Notifikasi
Seluruh data sensor dikirimkan ke broker MQTT (Antares) secara berkala. Selain itu, sistem juga membaca pesan dari Telegram. Jika salah satu sensor mendeteksi kondisi abnormal, seperti kapasitas penuh atau suhu tinggi, sistem akan mengirimkan notifikasi otomatis ke Telegram pengguna.

---

## Video Demonstrasi

Klik tautan di bawah ini untuk melihat video demonstrasi cara kerja sistem SWMM:

🎥 **[Tonton Video Demonstrasi di Sini](https://youtu.be/sV9NxRGtoNI?si=jpkwx1wAazdzuTUo)**

---

## Board Schematic & Wiring

![Board Schematic](https://github.com/RaflyOlanda/UAS-IOT-UNMUL/blob/main/SWMM_schematic%20(1).png)

> *Desain skematik dirakit menggunakan simulator Wokwi.*
