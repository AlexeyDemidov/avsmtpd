VERSION = 0.0.1
PACKAGE = avsmtpd

AUTOMAKE_OPTIONS = foreign

EXTRA_DIST = TODO VERSION 

CFLAGS=-Wall -W -Wno-unused -D_ISOC99_SOURCE -D_GNU_SOURCE  -g

sbin_PROGRAMS = avsmtpd
avsmtpd_SOURCES = avsmtpd.c
DEPENDENCIES = libutils.a
INCLUDES = -I.
LDADD = libutils.a

noinst_LIBRARIES = libutils.a
libutils_a_SOURCES =  log.c daemon.c drweb.c readcfg.c sock.c smtp.c
noinst_HEADERS= daemon.h log.h drweb.h readcfg.h sock.h smtp.h
