
ifeq "$(filter x64 x86,$(Platform))" ""
  $(error Need VS Environment)
endif

.PHONY : all
all : exe dll
	@echo make done.

DESTPATH	:= $(Platform)

CC 			:= cl.exe
LINK		:= link.exe

######## CFLAGS
CFLAGS		= /c /MP /GS- /Qpar /GL /analyze- /W4 /Gy /Zc:wchar_t /Zi /Gm- /Ox /Zc:inline /fp:precise /DWIN32 /DNDEBUG /D_UNICODE /DUNICODE /fp:except- /errorReport:none /GF /WX /Zc:forScope /GR- /Gd /Oy /Oi /MT /EHa /nologo
CFLAGS		+= /Fd"$(DESTPATH)/"

ifeq "$(Platform)" "x86"
CFLAGS		+= /D_USING_V110_SDK71_
endif

CFLAGS		+= /I"E:/work/xlib"
CFLAGS		+= /D_USRDLL /D_WINDLL

######## LDFLAGS
LDFLAGS		= /MANIFEST:NO /LTCG /NXCOMPAT /DYNAMICBASE "kernel32.lib" "user32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib" "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib" "odbc32.lib" "odbccp32.lib" /OPT:REF /INCREMENTAL:NO /OPT:ICF /ERRORREPORT:NONE /NOLOGO /MACHINE:$(Platform)
LDFLAGS		+= /LIBPATH:"$(DESTPATH)"

LDFLAGS		+= /LIBPATH:"E:/work/xlib/$(Platform)"

ifeq "$(Platform)" "x86"
LDFLAGS_CONSOLE	:= /SAFESEH /SUBSYSTEM:CONSOLE",5.01"
LDFLAGS_WINDOWS	:= /SAFESEH /SUBSYSTEM:WINDOWS",5.01"
else
LDFLAGS_CONSOLE	:= /SUBSYSTEM:CONSOLE
LDFLAGS_WINDOWS	:= /SUBSYSTEM:WINDOWS
endif

vpath %.exe $(DESTPATH)
vpath %.dll $(DESTPATH)
vpath %.o 	$(DESTPATH)

%.o : %.cc syringes.h | $(DESTPATH)
	$(CC) $(CFLAGS) /Fo"$(DESTPATH)/$(@F)" "$<"

$(DESTPATH) :
	@mkdir "$@"


.PHONY : exe
exe : syringes.exe

.PHONY : dll
dll : syringes.dll

syringes.dll : slog.o UpperToken.o LoadDll.o UnloadDll.o dll.o
	$(LINK) $(LDFLAGS) /DLL $(LDFLAGS_WINDOWS) /OUT:"$(DESTPATH)/$(@F)" $^

syringes.exe : slog.o UpperToken.o LoadDll.o UnloadDll.o exe.o
	$(LINK) $(LDFLAGS) $(LDFLAGS_CONSOLE) /OUT:"$(DESTPATH)/$(@F)" $^
