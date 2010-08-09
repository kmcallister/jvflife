CC     = wcc
CFLAGS = -zq -2
LINKER = wlink
LFLAGS = option quiet

OBJS = jvflife.obj

.c.obj : .autodepend
    $(CC) $(CFLAGS) $<

jvflife.exe : $(OBJS)
    $(LINKER) $(LFLAGS) name $@ file { $< }
