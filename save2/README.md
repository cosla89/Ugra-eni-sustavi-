# Mikroklimatski logger na Arduino Nano platformi

Projekt predstavlja ugradeni sustav za prikupljanje mikroklimatskih podataka. Sustav koristi Arduino Nano s ATmega328P mikrokontrolerom, senzore za temperaturu, tlak, osvjetljenje i realno vrijeme te microSD modul za lokalnu pohranu podataka.

Program je pisan u C jeziku bez Arduino frameworka. Kompajliranje se izvodi pomocu `avr-gcc`, `make` alata i `avrdude` programa za upload na mikrokontroler.

## Funkcionalnosti

- mjerenje temperature i atmosferskog tlaka pomocu BMP280/BME280 senzora
- mjerenje osvjetljenja pomocu BH1750 senzora
- citanje stvarnog vremena pomocu DS3231 RTC modula
- izracun trenda tlaka: `UP`, `DOWN` ili `STABLE`
- zapis podataka na microSD karticu
- ispis podataka na Serial Monitor preko UART komunikacije
- periodicko mjerenje svakih 10 sekundi

## Koristene komponente

- Arduino Nano, ATmega328P, 16 MHz
- BMP280 ili BME280 senzor
- BH1750 senzor osvjetljenja
- DS3231 RTC modul
- LOLIN microSD v1.2.0 modul
- microSD kartica
- breadboard i jumper zice

## Komunikacijski protokoli

Projekt koristi vise komunikacijskih protokola:

- I2C za BMP/BME280, BH1750 i DS3231
- SPI za microSD karticu
- UART za ispis podataka na Serial Monitor

## Spajanje

### I2C senzori

```text
SDA -> A4
SCL -> A5
VCC -> 3.3V ili 5V, ovisno o modulu
GND -> GND
```

I2C adrese:

```text
BMP/BME280 -> 0x76 ili 0x77
BH1750    -> 0x23
DS3231    -> 0x68
```

### microSD modul

```text
LOLIN D4 / CS   -> Arduino D10
LOLIN D5 / SCK  -> Arduino D13
LOLIN D6 / MISO -> Arduino D12
LOLIN D7 / MOSI -> Arduino D11
GND             -> GND
3V3             -> 3.3V
```

Napomena: oznake `D4`, `D5`, `D6` i `D7` na LOLIN modulu ne predstavljaju iste pinove kao na Arduino Nano plocici. U ovom projektu CS signal je spojen na Arduino D10.

## Format zapisa

Podaci se oblikuju kao CSV zapis:

```csv
timestamp,temp_c,pressure_pa,lux,trend
2026-07-01 22:15:10,24.52,101245,320,STABLE
```

Znacenje stupaca:

- `timestamp`: vrijeme mjerenja iz DS3231 modula ili uptime ako RTC nije dostupan
- `temp_c`: temperatura u stupnjevima Celzija
- `pressure_pa`: atmosferski tlak u paskalima
- `lux`: osvjetljenje u luxima
- `trend`: smjer promjene tlaka

## Pokretanje projekta

U PowerShellu otvoriti projektni direktorij:

```powershell
cd "C:\Users\Korisnik\Desktop\save2"
```

Dodati AVR toolchain i MinGW u PATH:

```powershell
$env:Path="C:\Users\Korisnik\Desktop\avr8-gnu-toolchain-4.0.0.52-win32.any.x86_64\avr8-gnu-toolchain-win32_x86_64\bin;C:\Users\Korisnik\Desktop\mingw\bin;" + $env:Path
```

Kompajliranje:

```powershell
mingw32-make
```

Upload na Arduino:

```powershell
mingw32-make flash
```

Serial Monitor treba biti postavljen na:

```text
9600 baud
```

Ako upload javi gresku `Access is denied`, potrebno je zatvoriti Serial Monitor jer zauzima COM port.

## Struktura projekta

```text
save2/
├── Makefile
├── README.md
├── drivers/
├── include/
├── src/
│   ├── main.c
│   ├── bme280.c
│   ├── bh1750.c
│   ├── ds3231.c
│   ├── sdcard.c
│   ├── spi.c
│   ├── i2c.c
│   └── uart.c
└── build/
```

## Napomena o SD zapisu

Projekt zapisuje podatke direktno u sektore SD kartice, pocevsi od bloka 2048. Ne koristi se FAT datotecni sustav, pa se na racunalu ne mora pojaviti standardna `.csv` datoteka. Ovaj pristup smanjuje slozenost programa i memorijsko opterecenje mikrokontrolera.

