#include "../drivers/sdcard.h"
#include "../include/uart.h"
#include "../include/spi.h"

#include <avr/io.h>
#include <util/delay.h>

// Makronaredbe za upravljanje Chip Select (CS) linijom
#define SD_CS_LOW()  (PORTB &= ~(1 << PB2))
#define SD_CS_HIGH() (PORTB |= (1 << PB2))

static uint8_t high_capacity_card = 0;

// Deklaracije internih funkcija
static uint8_t sdcard_command(uint8_t cmd, uint32_t arg, uint8_t crc);

// Šalje prazne taktove dok je kartica neaktivna (CS=HIGH)
static void sdcard_clock_idle(uint8_t cycles)
{
    SD_CS_HIGH();
    while (cycles--) {
        spi_transfer(0xFF);
    }
}

// Šalje naredbu na SD karticu i vraća R1 odgovor (ostavlja CS LOW ako je odgovor uspješan!)
static uint8_t sdcard_command(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t response;

    // Osiguraj kratki visoki rub prije svake naredbe za sinkronizaciju automata kartice
    SD_CS_HIGH();
    spi_transfer(0xFF);
    
    // Aktiviraj karticu
    SD_CS_LOW();

    // Slanje 6 bajtova naredbe
    spi_transfer((uint8_t)(0x40 | cmd));
    spi_transfer((uint8_t)(arg >> 24));
    spi_transfer((uint8_t)(arg >> 16));
    spi_transfer((uint8_t)(arg >> 8));
    spi_transfer((uint8_t)arg);
    spi_transfer(crc);

    // Čekanje na R1 odgovor (najviše 10 pokušaja)
    // SD kartica drži MSB na 1 (0xFF) dok je zaposlena, odgovor počinje s 0 na MSB poziciji
    for (uint8_t i = 0; i < 10; i++) {
        response = spi_transfer(0xFF);
        if (!(response & 0x80)) {
            return response; // Vraća odgovor, CS ostaje LOW za potrebe daljnjeg čitanja (npr. R7/OCR)
        }
    }
    
    // Ako nema odgovora, otpusti liniju
    SD_CS_HIGH();
    spi_transfer(0xFF);
    return 0xFF;
}

// Pomoćna funkcija za eksplicitno zatvaranje transakcije i oslobađanje MISO linije
static inline void sdcard_end_cmd(void)
{
    SD_CS_HIGH();
    spi_transfer(0xFF);
}

sdcard_err_t sdcard_init_debug(void)
{
    uint8_t response = 0xFF;
    uint8_t ocr[4];
    uint8_t r55;

    spi_init();
    spi_set_slow(); // SD kartica mora startati na frekvenciji između 100kHz i 400kHz
    _delay_ms(20);
    
    // Šalje barem 74+ dummy takta na početku da se interna logika kartice stabilizira
    sdcard_clock_idle(80);

    // --- 1. KORAK: CMD0 (Reset u SPI mod) ---
    uart_puts("[SD] CMD0...\r\n");
    for (uint8_t i = 0; i < 20; i++) {
        response = sdcard_command(0, 0, 0x95);
        sdcard_end_cmd(); // Obavezno zatvori transakciju nakon CMD0
        
        uart_puts("[SD] CMD0 resp=0x");
        uart_putc("0123456789ABCDEF"[response >> 4]);
        uart_putc("0123456789ABCDEF"[response & 0x0F]);
        uart_puts("\r\n");
        
        if (response == 0x01) break;
        _delay_ms(10);
    }
    if (response != 0x01) return SD_ERR_CMD0;

    // --- 2. KORAK: CMD8 (Provjera napona i verzije kartice) ---
    uart_puts("[SD] CMD8...\r\n");
    response = sdcard_command(8, 0x000001AAUL, 0x87);

    uart_puts("[SD] CMD8 resp=0x");
    uart_putc("0123456789ABCDEF"[response >> 4]);
    uart_putc("0123456789ABCDEF"[response & 0x0F]);
    uart_puts("\r\n");

    if (response == 0x01) {
        // Budući da je CMD8 vratio 0x01, CS je ostao LOW. Sada čitamo preostala 4 bajta R7 odgovora:
        for (uint8_t i = 0; i < 4; i++) {
            ocr[i] = spi_transfer(0xFF);
        }
        sdcard_end_cmd(); // Tek sada gasimo CS i oslobađamo sabirnicu

        uart_puts("[SD] CMD8 R7: ");
        for (uint8_t i = 0; i < 4; i++) {
            uart_putc("0123456789ABCDEF"[ocr[i] >> 4]);
            uart_putc("0123456789ABCDEF"[ocr[i] & 0x0F]);
            uart_putc(' ');
        }
        uart_puts("\r\n");
        
        // Provjera eho uzorka (bajt 3 mora biti 0x01, bajt 4 mora biti 0xAA)
        if (ocr[2] != 0x01 || ocr[3] != 0xAA) {
            return SD_ERR_CMD8_VOLT;
        }
        high_capacity_card = 1; // Kartica je SDv2 (potencijalno SDHC/SDXC)
    } else {
        sdcard_end_cmd(); // Isključi CS ako CMD8 nije uspio
        uart_puts("[SD] CMD8 illegal — SDv1 ili MMC\r\n");
        high_capacity_card = 0;
    }

    // --- 3. KORAK: Petlja ACMD41 (Pokretanje interne inicijalizacije kartice) ---
    uart_puts("[SD] ACMD41...\r\n");
    for (uint16_t i = 0; i < 500; i++) {

    /* Obavezni idle taktovi između iteracija — spec zahtijeva ≥ 8 */
    SD_CS_HIGH();
    spi_transfer(0xFF);
    spi_transfer(0xFF);
    spi_transfer(0xFF);

    r55 = sdcard_command(55, 0, 0x01);
    sdcard_end_cmd();

    /* Neke kartice ne odgovaraju na CMD55 ali prihvate ACMD41 —
       šalji ACMD41 bez obzira na r55, samo ne ako je kartica potpuno tiha */
    if (r55 != 0xFF) {
        response = sdcard_command(
            41,
            high_capacity_card ? 0x40000000UL : 0,
            0x01
        );
        sdcard_end_cmd();

        if (response == 0x00) {
            uart_puts("[SD] ACMD41 OK after ");
            uart_putu16(i);
            uart_puts(" iter\r\n");
            break;
        }
    }

    /* Log samo svakih 50 iteracija (ne 250) da lakše vidiš napredak */
    if ((i % 50) == 0) {
        uart_puts("[SD] ACMD41 wait r55=0x");
        uart_putc("0123456789ABCDEF"[r55 >> 4]);
        uart_putc("0123456789ABCDEF"[r55 & 0x0F]);
        uart_puts(" resp=0x");
        uart_putc("0123456789ABCDEF"[response >> 4]);
        uart_putc("0123456789ABCDEF"[response & 0x0F]);
        uart_puts("\r\n");
    }

    _delay_ms(10);  /* ← s 2ms na 10ms */
}
    if (response != 0x00) return SD_ERR_ACMD41;

    // --- 4. KORAK: CMD58 (Čitanje OCR registra radi provjere je li kartica SDHC ili SDSC) ---
    uart_puts("[SD] CMD58...\r\n");
    response = sdcard_command(58, 0, 0x01);
    if (response == 0x00) {
        // Čitanje 4 bajta OCR registra dok je CS još LOW
        for (uint8_t i = 0; i < 4; i++) {
            ocr[i] = spi_transfer(0xFF);
        }
        sdcard_end_cmd(); // Zatvori transakciju
        
        // Ako je bit 30 u OCR-u postavljen (0x40 u prvom bajtu), kartica je SDHC/SDXC
        high_capacity_card = (ocr[0] & 0x40) ? 1 : 0;
        
        uart_puts("[SD] OCR=");
        for (uint8_t i = 0; i < 4; i++) {
            uart_putc("0123456789ABCDEF"[ocr[i] >> 4]);
            uart_putc("0123456789ABCDEF"[ocr[i] & 0x0F]);
            uart_putc(' ');
        }
        uart_puts(high_capacity_card ? "SDHC\r\n" : "SDSC\r\n");
    } else {
        sdcard_end_cmd();
        return SD_ERR_CMD58;
    }

    // --- 5. KORAK: CMD16 (Postavljanje duljine bloka na 512 bajtova - obavezno za SDSC) ---
    response = sdcard_command(16, 512, 0x01);
    sdcard_end_cmd();
    if (response != 0x00 && !high_capacity_card) return SD_ERR_CMD16;

    // Inicijalizacija je gotova, prebaci SPI na maksimalnu brzinu rada
    spi_set_fast();
    return SD_ERR_NONE;
}

// Standardna produkcijska inicijalizacijska funkcija (bez UART ispisa)
uint8_t sdcard_init(void)
{
    uint8_t response = 0xFF;
    uint8_t ocr[4];
    uint8_t r55;

    spi_init();
    spi_set_slow();
    sdcard_clock_idle(80);

    for (uint8_t i = 0; i < 20; i++) {
        response = sdcard_command(0, 0, 0x95);
        sdcard_end_cmd();
        if (response == 0x01) break;
        _delay_ms(10);
    }
    if (response != 0x01) return 0;

    response = sdcard_command(8, 0x000001AAUL, 0x87);
    if (response == 0x01) {
        for (uint8_t i = 0; i < 4; i++) ocr[i] = spi_transfer(0xFF);
        sdcard_end_cmd();
        if (ocr[2] != 0x01 || ocr[3] != 0xAA) return 0;
        high_capacity_card = 1;
    } else {
        sdcard_end_cmd();
        high_capacity_card = 0;
    }

    for (uint16_t i = 0; i < 1000; i++) {
        r55 = sdcard_command(55, 0, 0x01);
        sdcard_end_cmd();
        if (r55 > 0x01) { _delay_ms(2); continue; }
        
        response = sdcard_command(41, high_capacity_card ? 0x40000000UL : 0, 0x01);
        sdcard_end_cmd();
        if (response == 0x00) break;
        _delay_ms(2);
    }
    if (response != 0x00) return 0;

    response = sdcard_command(58, 0, 0x01);
    if (response == 0x00) {
        for (uint8_t i = 0; i < 4; i++) ocr[i] = spi_transfer(0xFF);
        high_capacity_card = (ocr[0] & 0x40) ? 1 : 0;
    }
    sdcard_end_cmd();

    response = sdcard_command(16, 512, 0x01);
    sdcard_end_cmd();
    if (response == 0x00 || high_capacity_card) {
        spi_set_fast();
        return 1;
    }
    return 0;
}

uint8_t sdcard_write_block(uint32_t block, const uint8_t *data)
{
    // SDHC koristi adresiranje po blokovima (0, 1, 2...), dok SDSC koristi bajtne adrese (0, 512, 1024...)
    uint32_t address = high_capacity_card ? block : (block * 512UL);

    uint8_t response = sdcard_command(24, address, 0x01);
    if (response != 0x00) { sdcard_end_cmd(); return 0; }

    // Pošalji "Start Block" token za jednostruki blok zapisa (0xFE)
    spi_transfer(0xFE);
    
    // Slanje 512 bajtova podataka bloka
    for (uint16_t i = 0; i < 512; i++) {
        spi_transfer(data[i]);
    }
    
    // Slanje 2 bajta lažnog CRC-a (u SPI načinu rada se ignorira)
    spi_transfer(0xFF);
    spi_transfer(0xFF);

    // Čitanje odgovora na zapis podataka (Data Response Token)
    response = spi_transfer(0xFF);
    if ((response & 0x1F) != 0x05) { 
        sdcard_end_cmd(); 
        return 0; // Podaci su odbijeni (CRC ili Write Error)
    }

    // Čekanje da kartica završi interni upis (dok god radi, drži MISO na LOW nivou)
    uint16_t timeout = 0xFFFF;
    while (spi_transfer(0xFF) == 0x00) {
        if (--timeout == 0) { 
            sdcard_end_cmd(); 
            return 0; // Timeout greška pri upisu
        }
    }

    sdcard_end_cmd();
    return 1; // Uspješan upis
}