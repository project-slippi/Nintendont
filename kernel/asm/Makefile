ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif
include $(DEVKITPPC)/base_tools

ifeq ($(OS),Windows_NT)
	BIN2H =	../bin2h/bin2h.exe
else
	BIN2H =	../bin2h/bin2h
endif

SFILES		:=	$(wildcard *.S)
DFILES		:=	$(SFILES:.S=.d)
ELFFILES	:=	$(SFILES:.S=.elf)
BINFILES	:=	$(ELFFILES:.elf=.bin)
HFILES		:=	$(BINFILES:.bin=.h)

all: $(HFILES)

-include $(DFILES)
.PRECIOUS: %.elf %.bin

%.elf: %.S
	@echo " ASSEMBLE    $<"
	@$(AS) $< -mregnames -MD $(@:.elf=.d) -o $@

%.bin: %.elf
	@echo " STRIP       $<"
	@$(STRIP) $< -O binary -S -s -o $@

%.h: %.bin
	@echo " BIN2H       $<"
	@$(BIN2H) $<

clean:
	@echo clean ...
	@rm -fr *.d *.h *.bin *.elf
