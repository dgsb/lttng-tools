AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src -I$(srcdir) -O2 -g
AM_LDFLAGS =

if LTTNG_TOOLS_BUILD_WITH_LIBDL
AM_LDFLAGS += -ldl
endif
if LTTNG_TOOLS_BUILD_WITH_LIBC_DL
AM_LDFLAGS += -lc
endif

if HAVE_LIBLTTNG_UST_CTL
noinst_PROGRAMS = gen-ust-nevents
gen_ust_nevents_SOURCES = gen-ust-nevents.c tp.c tp.h
gen_ust_nevents_LDADD = -llttng-ust -lurcu-bp
endif
