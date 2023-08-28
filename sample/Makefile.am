# sample/Makefile.am for libevent
# Copyright 2000-2007 Niels Provos
# Copyright 2007-2012 Niels Provos and Nick Mathewson
#
# See LICENSE for copying information.

# AUTOMAKE_OPTIONS用于给automake传递自定义选项，可以影响构建过程、编译器选项、文件分发等
# foreign 不遵循GNU规则
# no-dependencies 不要生成依赖关系
AUTOMAKE_OPTIONS = foreign no-dependencies

# 在链接时，去除不被使用的代码段
LDADD = $(LIBEVENT_GC_SECTIONS) ../libevent.la

# 设置预处理器头文件路径
AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/compat -I$(top_srcdir)/include -I../include

noinst_PROGRAMS = event-test time-test signal-test dns-example hello-world http-server

event_test_SOURCES = event-test.c
time_test_SOURCES = time-test.c
signal_test_SOURCES = signal-test.c
dns_example_SOURCES = dns-example.c
hello_world_SOURCES = hello-world.c
http_server_SOURCES = http-server.c

if OPENSSL
noinst_PROGRAMS += le-proxy
le_proxy_SOURCES = le-proxy.c
le_proxy_LDADD = $(LDADD) ../libevent_openssl.la -lssl -lcrypto ${OPENSSL_LIBADD}
endif

verify:

DISTCLEANFILES = *~
