# MULoader
The Most Useless Bootloader for ATmega328p.  
More diplomatic name is Many Users Bootloader, because every user can upload their own code easily.

### What it does
It utilizes RC522 RFID reader to load code from programmable MIFARE tags.  
At the moment only 1K Classic tags are working because:
1. It's in early development phase
2. Code itself is close to 4kbytes, which is the biggest size of bootloader partition for this uC
3. I have bought plenty of them from China
4. They're **awfully** cheap

### Obstacles
Only because MIFARE tags say they're 1k, doesn't mean you can use that space.  
These tags have special memory blocks, that define access conditions to whole sector 
(block = 16 bytes, sector = 4 blocks).  
It's not end yet, block #0 is *NOT* meant for data storage, so all in all, instead of 1k (64 blocks * 16 bytes) we get 47 blocks (752 bytes).  
During development I decided to take away one more block for sketch metadata (number of parts, code size etc) so we end up with 736 bytes of storage available per 1k tag (also it makes code a bit smaller, yay!).  

Full 28672 byte code (in the worst case) will take up to 40 tags. Don't forget, that they will have to be scanned in specific order or your uC will crash at some point.  
I don't know why, but millis() is not working, but I cannot override them, because they're already defined ¯\_(ツ)_/¯

Also, I didn't have any experience in working with AVR bootloaders nor RFID tags.

### Causalities
* 2 MIFARE Classic tags bricked by overwriting trailer blocks with random data  
* MFRC522 library by @miguelbalboa

### Motivation
N/A

### Hardware
* ATmega328P based board
* MFRC522 reader from China (the working one)
* Breadboard and some jumper wires
* MIFARE Classic 1k tags (plenty of them)
* External programmer (USBasp or AVRispmkII)

Fuse bits:
* Low: 0xFF
* High: 0xD8
* Extended: 0xFD
* Lock: 0x3F

|Arduino|MFRC522|
|-------|-------|
|  D13  |  SCK  |
|  D12  | MISO  |
|  D11  | MOSI  |
| RESET | RESET |
|  D10  |CS(SDA)|
|  GND  |  GND  |
|  3V3  |  3V3  |

Reset circuit like this allowed to slim code a bit and as far as I see causes no problems so... It's working?  
I added an LED on A1 to watch if flash is being updated.  
Test code lights up LED on A0.  

### Software
1. Compile whatever you want for Arduino
2. Convert compiled code to C array
3. Add this array to Writer source and upload to Arduino
4. Scan tag and pray it transferred correctly (because there is no verification)
5. Copy platform.local.txt to arduino/hardware/avr/[version] (remember to use it only with Reader)
6. Upload MULoaderReader to target
7. Scan tag and watch stuff appearing in Serial Monitor

Remember to uncomment define at the beginning of Reader code, otherwise you won't see anything :)

### Tag memory layout (MIFARE 1K)
Every tag will have 46 blocks available. It makes programming easier (fixed amount of blocks) and leaves 1 block for metadata.  
First tag: part number, code size, amount of blocks and pages and parts.  
Every next tag: part number only.  

In this way I can implement verification, if correct tag is being read or programmed.

### What works
* Uploading code manually to tags
* Reading from tag
* Updating flash

### What doesn't work
* Programming tags in parts of 736 bytes
* Bootloader timeout
* Tags other than Mifare Classic 1K
* PC client software
