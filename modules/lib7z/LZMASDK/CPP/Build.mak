LIBS = $(LIBS) oleaut32.lib ole32.lib

!IFDEF CPU
!IFNDEF NO_BUFFEROVERFLOWU
LIBS = $(LIBS) bufferoverflowU.lib
!ENDIF
!ENDIF


!IFNDEF O
!IFDEF CPU
O=$(CPU)
!ELSE
O=O
!ENDIF
!ENDIF

!IF "$(CPU)" != "IA64"
!IF "$(CPU)" != "AMD64"
MY_ML = ml
!ELSE
MY_ML = ml64
!ENDIF
!ENDIF


!IFDEF UNDER_CE
RFLAGS = $(RFLAGS) -dUNDER_CE
!IFDEF MY_CONSOLE
LFLAGS = $(LFLAGS) /ENTRY:mainACRTStartup
!ENDIF
!ELSE
LFLAGS = $(LFLAGS) -OPT:NOWIN98
CFLAGS = $(CFLAGS) -Gr
LIBS = $(LIBS) user32.lib advapi32.lib shell32.lib
!ENDIF


COMPL_ASM = $(MY_ML) -c -Fo$O/ $**

CFLAGS = $(CFLAGS) -nologo -c -Fo$O/ -WX -EHsc -Gy -GR-

!IFDEF MY_STATIC_LINK
!IFNDEF MY_SINGLE_THREAD
CFLAGS = $(CFLAGS) -MT
!ENDIF
!ELSE
CFLAGS = $(CFLAGS) -MD
!ENDIF

!IFDEF NEW_COMPILER
CFLAGS = $(CFLAGS) -W4 -GS- -Zc:forScope
!ELSE
CFLAGS = $(CFLAGS) -W3
!ENDIF

CFLAGS_O1 = $(CFLAGS) -O1
CFLAGS_O2 = $(CFLAGS) -O2

LFLAGS = $(LFLAGS) -nologo -OPT:REF -OPT:ICF

!IFDEF DEF_FILE
LFLAGS = $(LFLAGS) -DLL -DEF:$(DEF_FILE)
!ENDIF

PROGPATH = $O\$(PROG)

COMPL_O1   = $(CC) $(CFLAGS_O1) $**
COMPL_O2   = $(CC) $(CFLAGS_O2) $**
COMPL_PCH  = $(CC) $(CFLAGS_O1) -Yc"StdAfx.h" -Fp$O/a.pch $**
COMPL      = $(CC) $(CFLAGS_O1) -Yu"StdAfx.h" -Fp$O/a.pch $**

all: $(PROGPATH)

clean:
	-del /Q $(PROGPATH) $O\*.exe $O\*.dll $O\*.obj $O\*.lib $O\*.exp $O\*.res $O\*.pch

$O:
	if not exist "$O" mkdir "$O"

$(PROGPATH): $O $(OBJS) $(DEF_FILE)
	link $(LFLAGS) -out:$(PROGPATH) $(OBJS) $(LIBS)

!IFNDEF NO_DEFAULT_RES
$O\resource.res: $(*B).rc
	rc $(RFLAGS) -fo$@ $**
!ENDIF
$O\StdAfx.obj: $(*B).cpp
	$(COMPL_PCH)
