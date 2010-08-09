CC     = wcc
CFLAGS = -zq -2 -oxh
LINKER = wlink
LFLAGS = option quiet

OBJS = jvflife.obj

.c.obj : .autodepend
    $(CC) $(CFLAGS) $<

jvflife.exe : $(OBJS)
    $(LINKER) $(LFLAGS) name $@ file { $< }
