BASE_NAME = reader_calc
EXTRA_FILES = bitstream.c huffman.c dct.c
FILES_TO_COMP = $(BASE_NAME).c $(EXTRA_FILES)
FILE_NAME = bad.g1a
TOOLCHAIN = sh-elf
# -fsanitize=address
CFLAGSEX := -Ofast -Wall -Wextra -Wno-missing-field-initializers
CFLAGS := -mb -ffreestanding -nostdlib -fstrict-volatile-bitfields -D FX9860G -m3 $(CFLAGSEX)
LDFLAGS := -T fx9860g.ld -lgint-fx -lgcc

all: $(FILE_NAME)

switch_dct:
	rm -f reader.c reader_calc.c
	ln -s reader_dct.c reader.c
	ln -s reader_dct_calc.c reader_calc.c

switch_mono:
	rm -f reader.c reader_calc.c
	ln -s reader_mono.c reader.c
	ln -s reader_mono_calc.c reader_calc.c

reader: reader.c $(EXTRA_FILES)
	gcc $(CFLAGSEX) reader.c $(EXTRA_FILES) -o reader

reader_sdl: reader.c $(EXTRA_FILES)
	gcc $(CFLAGSEX) -DSDL $(shell sdl2-config --cflags) -g reader.c $(EXTRA_FILES) $(shell sdl2-config --libs) -o reader_sdl

bad.elf: $(FILES_TO_COMP)
	$(TOOLCHAIN)-gcc -g -I. $(CFLAGS) $(FILES_TO_COMP) $(LDFLAGS) -o bad.elf

bad.bin: bad.elf
	$(TOOLCHAIN)-objcopy -O binary -R .bss -R .comment -R .gint_bss bad.elf bad.bin

$(FILE_NAME): bad.bin
	fxg1a bad.bin -o $(FILE_NAME) -i "MainIcon.png" -n "fuck" --internal="@BAD"

dump: all
	sh3eb-elf-objdump -d -C -S bad.elf > dump
	subl dump

clean:
	rm bad.elf bad.bin $(FILE_NAME)

send: $(FILE_NAME)
	p7 send -f $(FILE_NAME)