VERSION = 0.0.1
PACKAGE = avsmtpd

AUTOMAKE_OPTIONS = foreign

EXTRA_DIST = TODO VERSION 

CFLAGS=-Wall -g

sbin_PROGRAMS = avsmtpd
avsmtpd_SOURCES = avsmtpd.c
DEPENDENCIES = libutils.a
INCLUDES = -I.
LDADD = libutils.a

noinst_LIBRARIES = libutils.a
libutils_a_SOURCES =  log.c daemon.c drweb.c
noinst_HEADERS= daemon.h log.h drweb.h
