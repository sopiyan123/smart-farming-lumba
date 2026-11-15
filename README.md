# Lumba-Lumba: Smart Farming Berbasis ESP32

Proyek pemantauan tanaman dengan keran air otomatis yang digerakkan motor servo. ESP32 membaca kondisi tanah, udara, dan tandon air, lalu mengatur servo, pompa, dan alarm. Data alat juga bisa dilihat lewat dashboard web.

**Link penting**

- Dashboard: https://relaxed-bonbon-84653d.netlify.app
- Simulasi Wokwi: https://wokwi.com/projects/480661311179185153
- Repositori GitHub: https://github.com/sopiyan123/smart-farming-lumba

---

## Tim Pembuat

1. Kheysiera Nanda Kusuma
2. Maria Nini Adelya Watokolah
3. Rizki Jaya
4. Muhammad Dzaky Maulanaa

---

## Tentang Proyek

Menyiram tanaman terlalu sedikit atau terlalu banyak sama-sama merugikan. Karena itu kami membuat alat yang menyiram sesuai kelembapan tanah. Inovasi kami adalah motor servo sebagai penggerak keran, jadi keran tidak perlu dibuka dan ditutup dengan tangan.

---

## Alur Kerja Alat

| Kondisi | Pompa | Servo (keran) | Buzzer |
|---|---|---|---|
| Tanah kering (di bawah 40%), air ada | Menyala | Buka (90°) | Diam |
| Tanah basah (di atas 60%) | Mati | Tutup (0°) | Diam |
| Tanah 40% sampai 60% | Tetap | Tetap | Diam |
| Tandon kosong (jarak lebih dari 15 cm) | Mati | Tutup (0°) | Bunyi |

Penjelasan singkat:

- Rentang 40% sampai 60% dibuat supaya pompa tidak hidup-mati terus ketika nilai sensor naik turun sedikit (histeresis).
- Jika sensor jarak tidak membaca apa pun, tandon dianggap kosong agar pompa tetap aman.
- Isi tandon dihitung dari jarak air: 15 cm dianggap kosong, 3 cm dianggap penuh.
- LCD bergantian menampilkan dua halaman tiap 3 detik: kelembapan tanah dan status pompa, lalu suhu, kelembapan udara, dan isi tandon.
- Data dikirim ke dashboard lewat MQTT tiap 3 detik.

---

## Daftar Komponen dan Pin

| Komponen | Fungsi | Pin |
|---|---|---|
| ESP32 DevKit C | Pusat kendali | - |
| Sensor kelembapan tanah (di simulasi: potensiometer) | Membaca kelembapan tanah | GPIO 34 |
| DHT22 | Suhu dan kelembapan udara | GPIO 15 |
| HC-SR04 | Mengukur jarak permukaan air di tandon | TRIG GPIO 5, ECHO GPIO 18 |
| Motor servo | Membuka dan menutup keran | GPIO 27 |
| Modul relay | Saklar untuk pompa | GPIO 26 |
| LED merah + resistor 1220 ohm | Penanda pompa menyala (lewat kontak relay) | Tanpa GPIO |
| Buzzer | Alarm saat tandon kosong | GPIO 25 |
| LCD I2C 16x2 (alamat 0x27) | Menampilkan status alat | SDA GPIO 21, SCL GPIO 22 |

---

## Software yang Dipakai

- **Visual Studio Code**: tempat menulis dan mengelola kode.
- **PlatformIO**: ekstensi untuk mengatur board, library, compile, dan upload ke ESP32.
- **Wokwi Simulator**: menguji rangkaian dan program secara virtual sebelum dipasang di alat asli.
- **EMQX Cloud**: broker MQTT untuk mengirim data lewat internet.
- **Netlify**: tempat dashboard web dipublikasikan.
- **Git dan GitHub**: menyimpan dan membagikan kode.

Library terpasang otomatis lewat `platformio.ini`: DHT sensor library, Adafruit Unified Sensor, LiquidCrystal_I2C, ESP32Servo, dan PubSubClient.

---

## Susunan Folder

| Folder atau file | Isi |
|---|---|
| `src/main.cpp` | Program utama ESP32 |
| `include/` | File kredensial `secrets.h` (tidak ikut diunggah) |
| `website/index.html` | Halaman dashboard |
| `diagram.json`, `wokwi.toml` | Pengaturan simulasi Wokwi |
| `platformio.ini` | Pengaturan board dan library |

---

## Langkah Menjalankan

**Cara 1: simulasi di Wokwi**

1. Buka https://wokwi.com/projects/480661311179185153
2. Klik Start Simulation.
3. Geser potensiometer (kelembapan tanah) dan ubah jarak sensor HC-SR04. Perhatikan servo, pompa, LED, dan buzzer yang bereaksi.

**Cara 2: di alat asli lewat VS Code**

1. Clone repo ini, lalu buka foldernya di VS Code.
2. Salin `include/secrets.example.h` menjadi `include/secrets.h`, lalu isi data WiFi dan akun MQTT.
3. Klik ikon centang di bar bawah (Build), lalu ikon panah (Upload).
4. Klik ikon colokan (Serial Monitor) dengan kecepatan 115200 untuk melihat laporan alat.

**Melihat data lewat dashboard**

1. Buka https://relaxed-bonbon-84653d.netlify.app
2. Pastikan alat atau simulasi sedang menyala dan terhubung ke internet.
3. Data sensor dan status alat tampil di halaman, dan frekuensi buzzer bisa diatur dari sana.

---

## Jalur Data MQTT

| Topik | Arah | Isi |
|---|---|---|
| `smartfarm/lumba/data` | ESP32 ke broker | Data sensor (JSON) |
| `smartfarm/lumba/event` | ESP32 ke broker | Catatan kejadian |
| `smartfarm/lumba/status` | ESP32 ke broker | online atau offline |
| `smartfarm/lumba/set/buzzer_freq` | Broker ke ESP32 | Frekuensi buzzer (Hz) |

---

## Catatan Pengembangan

- Sensor kelembapan tanah perlu dikalibrasi sesuai media tanam yang dipakai.
- Sudut buka dan tutup servo disesuaikan dengan bentuk tuas keran.
- Di alat asli, sensor tanah sebaiknya ditenagai 3,3 V karena input ADC ESP32 tidak boleh melebihi 3,3 V.
- Simulasi belum sepenuhnya sama dengan kondisi di perangkat fisik, jadi pengujian langsung tetap diperlukan.