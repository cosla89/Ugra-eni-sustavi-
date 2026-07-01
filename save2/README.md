# Arduino Nano, pure C mikroklimatski logger

Ovo je AVR-GCC projekt za ATmega328P koji koristi C bez Arduino frameworka.
Implementira temu: sustav za dugorocno prikupljanje mikroklimatskih uvjeta.

## Sto radi

- cita BME280 temperaturu, relativnu vlagu i atmosferski tlak
- cita BH1750 osvjetljenje u luxima
- cita vrijeme s DS3231 RTC modula ako je dostupan; bez RTC-a koristi BOOT+sekunde
- prikazuje trenutna mjerenja na SSD1306 OLED zaslonu
- prikazuje trend tlaka: gore, dolje ili stabilno
- zapisuje CSV zapis na SD karticu preko SPI protokola
- salje isti zapis na serijski monitor na 9600 8N1

## Spajanje

I2C uredaji koriste standardne Nano pinove:

- SDA: A4
- SCL: A5
- BME280: 0x76
- BH1750: 0x23
- DS3231: 0x68
- SSD1306: 0x3C

SD modul koristi hardverski SPI:

- CS: D10 / PB2
- MOSI: D11 / PB3
- MISO: D12 / PB4
- SCK: D13 / PB5

## SD zapis

Logger zapisuje jedan CSV zapis u jedan raw SD blok od 512 bajtova, pocevsi od bloka 2048.
Ovo namjerno ne koristi FAT datotecni sustav kako bi projekt ostao mali i bez vanjskih biblioteka.
Kartica ce se zato najlakse citati raw dump alatom ili dodatnim programom koji cita sektore od bloka 2048.

Format zapisa:

```csv
timestamp,temp_c,humidity_pct,pressure_pa,lux,trend
BOOT+10s,24.52,45.31,101245,320,UP
```

Interval mjerenja je u `src/main.c` kroz `LOG_INTERVAL_MS`.
Trenutno je postavljen na 10 sekundi radi lakseg testiranja.
DS3231 nije obavezan za testiranje. Ako ne radi ili nema bateriju, serial ispis pokazuje RTC=OFF, a timestamp je oblika BOOT+10s.

## Kako pokrenuti

Treba ti:

- `avr-gcc`
- `avr-libc`
- `make`
- `avrdude`
- USB-serial adapter ili USBasp

### Build

```bash
make
```

### Flash

U `Makefile` podesi `PORT` na svoj COM port, pa:

```bash
make flash
```

### Serijski monitor

Otvori terminal na `9600 8N1`.


