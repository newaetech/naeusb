NULL :=
TAB := $(NULL)    $(NULL)
define NEWLINE


endef

EXTRAINCDIRS += naeusb
EXTRAINCDIRS += config

######## HAL OPTIONS ###########
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE = arm-none-eabi-size
AR = arm-none-eabi-ar rcs
NM = arm-none-eabi-nm
CFLAGS += -fdata-sections -ffunction-sections -mlong-calls -g3 -Wall -pipe 
CFLAGS += -fno-strict-aliasing -Wall -Wstrict-prototypes -Wmissing-prototypes -Wchar-subscripts 
CFLAGS += -Wcomment -Wformat=2 --param max-inline-insns-single=500
CFLAGS += -DDEBUG -DARM_MATH_CM3=true -Dprintf=iprintf -DUDD_ENABLE -Dscanf=iscanf -DPLATFORMCW1190=1
CFLAGS += -Wno-discarded-qualifiers -Wno-unused-function -Wno-unused-variable -Wno-strict-prototypes -Wno-missing-prototypes
CFLAGS += -Wno-pointer-sign -Wno-unused-value -Wno-unused-but-set-variable
PROGCMD="import sys, time; assert len(sys.argv) > 1; import chipwhisperer as cw; exec('try: scope = cw.scope(); p = cw.SAMFWLoader(scope); p.enter_bootloader(True); time.sleep(2);\nexcept: pass'); cw.program_sam_firmware(fw_path=sys.argv[1])" $(TARGET).bin

ifeq ($(TARGET),ChipWhisperer-Lite)
	CFLAGS += -D__SAM3U2C__
	CFLAGS += -D__PLAT_CWLITE__
	CDC=YES
	HAL = SAM3U
else ifeq ($(TARGET),ChipWhisperer-Husky)
	CFLAGS += -D__SAM3U2C__
	CFLAGS += -D__PLAT_HUSKY__
	CDC=YES
	HAL = SAM3U
else ifeq ($(TARGET),ChipWhisperer-CW305)
	CFLAGS += -D__SAM3U2E__
	CFLAGS += -D__PLAT_CW305__
	CDC=NO
	HAL = SAM3U
	PROGCMD="import sys, time; assert len(sys.argv) > 1; import chipwhisperer as cw; exec('try: scope = cw.target(None, cw.targets.CW305); p = cw.SAMFWLoader(scope); p.enter_bootloader(True); time.sleep(2);\nexcept: pass'); cw.program_sam_firmware(fw_path=sys.argv[1])" $(TARGET).bin
else ifeq ($(TARGET),ChipWhisperer-Pro)
	CFLAGS += -D__SAM3U4E__
	CFLAGS += -D__PLAT_PRO__
	CDC=YES
	HAL = SAM3U
else ifeq ($(TARGET),ChipWhisperer-Nano)
	CFLAGS += -D__SAM4SD16B__ -DUDD_NO_SLEEP_MGR
	CFLAGS += -D__PLAT_NANO__
	HAL = SAM4S
	CDC=YES
	LDFLAGS += --specs=nano.specs --specs=nosys.specs
else ifeq ($(TARGET),cw521)
	CFLAGS += -D__SAM3U4E__
	CFLAGS += -D__PLAT_CW521__
	SRC += naeusb/sam3u_hal/chipid.c naeusb/sam3u_hal/cycle_counter.c naeusb/sam3u_hal/efc.c naeusb/sam3u_hal/exceptions.c
	SRC += naeusb/sam3u_hal/flash_efc.c naeusb/sam3u_hal/interrupt_sam_nvic.c naeusb/sam3u_hal/led.c
	SRC += naeusb/sam3u_hal/pio_handler.c naeusb/sam3u_hal/pio.c naeusb/sam3u_hal/pmc.c
	SRC += naeusb/sam3u_hal/read.c naeusb/sam3u_hal/sleep.c naeusb/sam3u_hal/sleepmgr.c
	SRC += naeusb/sam3u_hal/smc.c naeusb/sam3u_hal/spi.c naeusb/sam3u_hal/startup_sam3u.c
	SRC += naeusb/sam3u_hal/syscalls.c naeusb/sam3u_hal/sysclk.c naeusb/sam3u_hal/system_sam3u.c
	SRC += naeusb/sam3u_hal/tc.c naeusb/sam3u_hal/twi.c naeusb/sam3u_hal/uart.c
	SRC += naeusb/sam3u_hal/udi_vendor.c naeusb/sam3u_hal/udphs_device.c naeusb/sam3u_hal/usart_serial.c
	SRC += naeusb/sam3u_hal/usart.c naeusb/sam3u_hal/write.c naeusb/sam3u_hal/usb_no_cdc/udi_vendor_desc.c

	EXTRAINCDIRS += naeusb/sam3u_hal/inc
else ifeq ($(TARGET),CW310)
	HAL = SAM3X
	CFLAGS += -D__SAM3X8E__
	CFLAGS += -D__PLAT_CW310__
	CDC=YES
else ifeq ($(TARGET),CW340)
# same for now, might be different at some point?
	HAL = SAM3X
	CFLAGS += -D__PLAT_CW340__
	CFLAGS += -D__SAM3X8E__
	CDC=YES
else ifeq ($(TARGET),phywhisperer)
	CFLAGS += -D__SAM3U2E__
	CFLAGS += -D__PLAT_PHY__
	CDC=NO
	HAL = SAM3U
	PROGCMD="import sys, time; assert len(sys.argv) > 1; \
	import phywhisperer.usb as pw; \
	exec('try: phy = pw.Usb(); phy.con(); phy.usb.enterBootloader(True); time.sleep(2);\
	\nexcept Exception as e: print(e)'); pw.program_sam_firmware(fw_path=sys.argv[1])" $(TARGET).bin
endif

ifeq ($(HAL),SAM3U)
	SRC += $(wildcard naeusb/sam3u_hal/*.c)
	ifeq ($(CDC),YES)
		SRC += naeusb/sam3u_hal/usb_cdc/udi_cdc.c
		SRC += naeusb/sam3u_hal/usb_cdc/udi_composite_desc.c
	else
		SRC += naeusb/sam3u_hal/usb_no_cdc/udi_vendor_desc.c
	endif
	EXTRAINCDIRS += naeusb/sam3u_hal/inc
else ifeq ($(HAL),SAM4S)
	SRC += $(wildcard naeusb/sam4s_hal/*.c)
	EXTRAINCDIRS += naeusb/sam4s_hal/inc
else ifeq ($(HAL),SAM3X)
	SRC += $(wildcard naeusb/sam3x_hal/*.c)
	EXTRAINCDIRS += naeusb/sam3x_hal/inc
endif
##### 


MSG_ERRORS_NONE = Errors: none
MSG_SIZE_BEFORE = Size before:
MSG_SIZE_AFTER = Size after:
MSG_FLASH = Creating load file for Flash:
MSG_EEPROM = Creating load file for EEPROM:
MSG_EXTENDED_LISTING = Creating Extended Listing:
MSG_SYMBOL_TABLE = Creating Symbol Table:
MSG_LINKING = Linking:
MSG_COMPILING = Compiling C:
MSG_COMPILING_CPP = Compiling C++:
MSG_ASSEMBLING = Assembling:
MSG_CLEANING = Cleaning project:
MSG_CREATING_LIBRARY = Creating library:

OBJDIR=./obj

OPT=2

SHELL = sh

MCU_FLAGS = -mcpu=cortex-m3

FORMAT = binary

CSTANDARD = -std=gnu99

CFLAGS += $(CDEFS)
CFLAGS += -O$(OPT)
CFLAGS += -funsigned-char -funsigned-bitfields -fshort-enums
CFLAGS += $(patsubst %,-I%,$(EXTRAINCDIRS))
CFLAGS += $(CSTANDARD)

GCCCOLOURS?=YES
ifeq ($(GCCCOLOURS),YES)
	CFLAGS += -fdiagnostics-color=always
else ifeq ($(GCCCOLOURS),NO)
	CFLAGS += -fdiagnostics-color=never
else
	CFLAGS += -fdiagnostics-color=auto
endif

LDFLAGS += -Wl,-Map=$(TARGET).map,--cref
LDFLAGS += $(EXTMEMOPTS)
LDFLAGS += $(patsubst %,-L%,$(EXTRALIBDIRS))
LDFLAGS += $(MATH_LIB)
LDFLAGS += $(PRINTF_LIB) $(SCANF_LIB)
LDFLAGS += -mthumb -Wl,--start-group -L naeusb -lm -Wl,--end-group   
LDFLAGS += -T $(LINKERFILE) -Wl,--gc-sections 
LDFLAGS += -Wl,--entry=Reset_Handler -Wl,--cref -mthumb

PRINTF_LIB_MIN = -Wl,-u,vfprintf -lprintf_min

# Floating point printf version (requires MATH_LIB = -lm below)
PRINTF_LIB_FLOAT = -Wl,-u,vfprintf -lprintf_flt

# If this is left blank, then it will use the Standard printf version.
PRINTF_LIB =

# Minimalistic scanf version
SCANF_LIB_MIN = -Wl,-u,vfscanf -lscanf_min

# Floating point + %[ scanf version (requires MATH_LIB = -lm below)
SCANF_LIB_FLOAT = -Wl,-u,vfscanf -lscanf_flt

VERBOSE ?= false

ifeq ($(VERBOSE), false)
	COMPMSG = "    $< ..."
	LINKMSG = "    $@ ..."
	DONEMSG = "Done!"
else
	COMPMSG = "    $(CC) -c $(ALL_CFLAGS) $< -o $@ ..."
	LINKMSG = "    $@ w/ opts $(ALL_CFLAGS) $(LDFLAGS) ..."
	DONEMSG = "Done!\n"
endif

########### OS OPTIONS #############################
REMOVE = rm -f --
REMOVEDIR = rm -rf
COPY = cp
WINSHELL = cmd
#Depending on if echo is unix or windows, they respond differently to no arguments. Windows will annoyingly
#print "echo OFF", so instead we're forced to give it something to echo. The windows one will also print
#passed ' or " symbols, so we use a . as it's pretty small...
ifeq ($(OS),Windows_NT)
	AdjustPath = $(addprefix $1\, $(subst /,\,$2 ) )
	ECHO_BLANK = echo
	MAKEDIR = mkdir
else
	AdjustPath = $(addprefix $1/, $2)
	MAKEDIR = mkdir -p
	ECHO_BLANK = echo
endif

# Define all object files.
OBJ = $(SRC:%.c=$(OBJDIR)/%.o) $(CPPSRC:%.cpp=$(OBJDIR)/%.o) $(ASRC:%.S=$(OBJDIR)/%.o)
#CR_OBJ_DIRS = $(patsubst %.o,,$(OBJ))

# Define all listing files.
LST = $(SRC:%.c=$(OBJDIR)/%.lst) $(CPPSRC:%.cpp=$(OBJDIR)/%.lst) $(ASRC:%.S=$(OBJDIR)/%.lst)


# Compiler flags to generate dependency files.
GENDEPFLAGS = -MMD -MP -MF .dep/$(@F).d

# Combine all necessary flags and optional flags.
# Add target processor to flags.
ALL_CFLAGS = $(MCU_FLAGS) -I. $(CFLAGS) $(GENDEPFLAGS)
ALL_CPPFLAGS = $(MCU_FLAGS) -I. -x c++ $(CPPFLAGS) $(GENDEPFLAGS)
ALL_ASFLAGS = $(MCU_FLAGS) -I. -x assembler-with-cpp $(ASFLAGS)

all: 
	@$(MAKE) --no-print-directory -O clean_objs .dep 
	@$(MAKE) --no-print-directory -O begin build
	@$(MAKE) --no-print-directory -O version end sizeafter

# Change the build target to build a HEX file or a library.
build: elf hex bin eep lss sym

beginmsg: begin gccversion
#build: lib


elf: $(TARGET).elf
hex: $(TARGET).hex
bin: $(TARGET).bin
eep: $(TARGET).eep
lss: $(TARGET).lss
sym: $(TARGET).sym
LIBNAME=lib$(TARGET).a
lib: $(LIBNAME)


begin:
	@echo +--------------------------------------------------------
	@echo Building for board $(TARGET):
	@echo +--------------------------------------------------------

end:
	@echo   +--------------------------------------------------------
	@echo   + Built for board "$(TARGET)"
	@echo   +--------------------------------------------------------

fastnote:
	@echo   +--------------------------------------------------------
	@echo   + Default target does full rebuild each time.
	@echo   + Specify buildtarget == allquick == to avoid full rebuild
	@echo   +--------------------------------------------------------

# Display size of file.
HEXSIZE = $(SIZE) --target=ihex $(TARGET).hex

# Note: custom ELFSIZE command can be specified in Makefile.platform
# See avr/Makefile.avr for example
ifeq ($(ELFSIZE),)
  ELFSIZE = $(SIZE) $(TARGET).elf -xA | grep .text
endif

#@$(SIZE) $(TARGET).elf -xA > /tmp/$(TARGET)-buildsize.txt
sizeafter: build
	@$(ECHO_BLANK)
	@head -n 30 $(TARGET).lss > /tmp/$(TARGET)-buildsize.txt
	@echo "Size of $(TARGET) binary:"

	@echo "  FLASH:"
	@echo -n "    "
	@egrep "(Idx)" /tmp/$(TARGET)-buildsize.txt && echo -n "    " || true
	@egrep "(.text)" /tmp/$(TARGET)-buildsize.txt && echo -n "    " || true
	@egrep "(.relocate)" /tmp/$(TARGET)-buildsize.txt || true

	@echo "  SRAM:"
	@echo -n "    "
	@egrep "(Idx)" /tmp/$(TARGET)-buildsize.txt && echo -n "    " || true
	@egrep "(.relocate)" /tmp/$(TARGET)-buildsize.txt && echo -n "    " || true
	@egrep "(.bss)" /tmp/$(TARGET)-buildsize.txt && echo -n "    " || true
	@egrep "(.mpsse)" /tmp/$(TARGET)-buildsize.txt && echo -n "    " || true
	@egrep "(.stack)" /tmp/$(TARGET)-buildsize.txt || true

	@$(ECHO_BLANK)

$(OBJ): | $(OBJDIR)

$(OBJDIR):
	$(MAKEDIR) $(OBJDIR) $(call AdjustPath,$(OBJDIR),$(MKDIR_LIST) )

.dep:
	@$(MAKEDIR) .dep

# Display compiler version information.
gccversion :
	@$(CC) --version


# Create final output files (.hex, .eep) from ELF output file.
%.hex: %.elf
	@$(OBJCOPY) -O ihex -R .eeprom -R .fuse -R .lock -R .signature $< $@

%.bin: %.elf
	@$(OBJCOPY) -O binary -R .eeprom -R .fuse -R .lock -R .signature $< $@


%.eep: %.elf
	@-$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom="alloc,load" \
	--change-section-lma .eeprom=0 --no-change-warnings -O ihex $< $@ || exit 0

# Create extended listing file from ELF output file.
%.lss: %.elf
	@$(OBJDUMP) -h -S -z $< > $@

# Create a symbol table from ELF output file.
%.sym: %.elf
	@$(NM) -n $< > $@



# Create library from object files.
.SECONDARY : $(TARGET).a
.PRECIOUS : $(OBJ)
%.a: $(OBJ)
	@echo $(MSG_CREATING_LIBRARY) $@
	$(AR) $@ $(OBJ)


# Link: create ELF output file from object files.
.SECONDARY : $(TARGET).elf
.PRECIOUS : $(OBJ)
%.elf: $(OBJ)
	@$(ECHO_BLANK)
	@echo LINKING:
	@echo -en $(LINKMSG)
	@$(CC) $(ALL_CFLAGS) $^ --output $@ $(LDFLAGS)
	@echo -e $(DONEMSG)

# Compile: create object files from C source files.
#@echo $(MSG_COMPILING) $<
$(OBJDIR)/%.o : %.c
	@$(MAKEDIR) -p ./$(dir $@)
	@echo Compiling: 
	@echo -en $(COMPMSG)
	@$(CC) -c $(ALL_CFLAGS) $< -o $@
	@echo -e $(DONEMSG)


# Compile: create object files from C++ source files.
$(OBJDIR)/%.o : %.cpp
	@echo $(MSG_COMPILING_CPP) $<
	$(CC) -c $(ALL_CPPFLAGS) $< -o $@


# Compile: create assembler files from C source files.
%.s : %.c
	$(CC) -S $(ALL_CFLAGS) $< -o $@


# Compile: create assembler files from C++ source files.
%.s : %.cpp
	$(CC) -S $(ALL_CPPFLAGS) $< -o $@


# Assemble: create object files from assembler source files.
$(OBJDIR)/%.o : %.S
	@echo $(MSG_ASSEMBLING) $<
	$(CC) -c $(ALL_ASFLAGS) $< -o $@


# Create preprocessed source for use in sending a bug report.
%.i : %.c
	$(CC) -E $(MCU_FLAGS) -I. $(CFLAGS) $< -o $@

version :
	@$(CC) -dD -E $(ALL_CFLAGS) ./config/conf_usb.h -o symbols.txt
	@grep -o -m 1 "FW_VER_MAJOR.*" ./symbols.txt > version.txt
	@grep -o -m 1 "FW_VER_MINOR.*" ./symbols.txt >> version.txt
	@grep -o -m 1 "FW_VER_DEBUG.*" ./symbols.txt >> version.txt

# Clean all object files specific to this platform
clean_objs :
	@echo cleaning objects
	@$(REMOVE) $(TARGET).hex
	@$(REMOVE) $(TARGET).eep
	@$(REMOVE) $(TARGET).cof
	@$(REMOVE) $(TARGET).elf
	@$(REMOVE) $(TARGET).map
	@$(REMOVE) $(TARGET).sym
	@$(REMOVE) $(TARGET).lss
	@$(REMOVEDIR) $(OBJDIR)/*
	@$(REMOVE) $(OBJDIR)/*.lst
	@$(REMOVE) $(SRC:.c=.s)
	@$(REMOVE) $(SRC:.c=.d)
	@$(REMOVE) $(SRC:.c=.i)

# Target: clean project.
clean: begin clean_print clean_all_objs clean_list end

clean_print :
	@$(ECHO_BLANK)
	@echo $(MSG_CLEANING)

# Clean all object files related to any of the platforms
clean_all_objs :
	@echo cleaning all objects
	@$(REMOVE) $(addsuffix .hex,$(TARGET-ALL))
	@$(REMOVE) $(addsuffix .eep,$(TARGET-ALL))
	@$(REMOVE) $(addsuffix .bin,$(TARGET-ALL))
	@$(REMOVE) $(addsuffix .cof,$(TARGET-ALL))
	@$(REMOVE) $(addsuffix .elf,$(TARGET-ALL))
	@$(REMOVE) $(addsuffix .map,$(TARGET-ALL))
	@$(REMOVE) $(addsuffix .sym,$(TARGET-ALL))
	@$(REMOVE) $(addsuffix .lss,$(TARGET-ALL))
	@$(REMOVE) $(OBJDIR)/*.o
	@$(REMOVE) $(OBJDIR)/*.lst
	@$(REMOVEDIR) $(OBJDIR)
	@$(REMOVE) $(SRC:.c=.s)
	@$(REMOVE) $(SRC:.c=.d)
	@$(REMOVE) $(SRC:.c=.i)

clean_list :
	$(REMOVEDIR) .dep

# this makes a file called 1 because of the assert ... > 1
# idk how to fix it so we just remove the file
program : 
	python -c $(PROGCMD)
	@rm 1


# Create object files directory
#$(shell mkdir $(OBJDIR) 2>/dev/null)

# Include the dependency files.
#-include $(shell mkdir .dep 2>/dev/null) $(wildcard .dep/*)
-include $(wildcard .dep/*)


# Listing of phony targets.
.PHONY : all allquick begin finish end sizeafter gccversion \
build elf hex bin eep lss sym coff extcoff \
clean clean_list clean_print clean_objs program debug gdb-config \
fastnote version