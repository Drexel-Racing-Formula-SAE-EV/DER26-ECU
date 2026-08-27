#!/usr/bin/env python3
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
main_c = (root / "Core/Src/main.c").read_text()
ioc = (root / "DER26-ECU.ioc").read_text()
spi = (root / "FATFS/Target/user_diskio_spi.c").read_text()
ffconf = (root / "FATFS/Target/ffconf.h").read_text()
service = (root / "Core/Src/ext_drivers/sdcard_service.c").read_text()
errors = []

def require(text, pattern, label, regex=False):
    ok = re.search(pattern, text, re.MULTILINE | re.DOTALL) if regex else pattern in text
    if not ok:
        errors.append(label)

require(main_c, "hspi6.Init.DataSize = SPI_DATASIZE_8BIT;", "SPI6 must use 8-bit frames")
require(main_c, "hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;", "SPI6 startup prescaler must be /256")
require(main_c, "HAL_GPIO_WritePin(SPI6_NSS_GPIO_Port, SPI6_NSS_Pin, GPIO_PIN_SET);", "SD CS must initialize high")
require(ioc, "SPI6.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_256", ".ioc must retain SPI6 /256 startup rate")
require(ioc, "SPI6.DataSize=SPI_DATASIZE_8BIT", ".ioc must retain 8-bit SPI6")
require(spi, "SPI_BAUDRATEPRESCALER_256", "low-level SD driver must retain /256 slow clock")
require(spi, "SPI_BAUDRATEPRESCALER_8", "low-level SD driver must retain /8 fast clock")
require(spi, "CS_HIGH();\n\tHAL_Delay(2u);", "card startup must begin with CS high")
require(spi, "for (n = 10u; n != 0u; n--) xchg_spi(0xFFu);", "card startup must issue at least 80 clocks")
require(spi, "USER_SPI_SD_STAGE_CSD_VERIFY", "initialization must validate CMD9/CSD")
require(service, "sector_size != 512u", "service must reject non-512-byte sectors")
require(service, "sectors == 0u", "service must reject zero sector count")
require(ffconf, r"#define\s+_FS_EXFAT\s+0", "exFAT must remain disabled for the MVP", regex=True)
if errors:
    print("ERROR SD-card static contract gate")
    print("\n".join(f"- {e}" for e in errors))
    sys.exit(1)
print("PASS SD-card SPI, no-card hardening, FAT32, and geometry contracts")
