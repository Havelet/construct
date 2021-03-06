prefix = @prefix@

AM_CXXFLAGS = \
	-ftls-model=local-exec \
	@EXTRA_CXXFLAGS@ \
	###

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@ROCKSDB_CPPFLAGS@ \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@ \
	@SSL_CPPFLAGS@ \
	@CRYPTO_CPPFLAGS@ \
	@SODIUM_CPPFLAGS@ \
	@MAGIC_CPPFLAGS@ \
	@SNAPPY_CPPFLAGS@ \
	@LZ4_CPPFLAGS@ \
	@Z_CPPFLAGS@ \
	@MALLOC_CPPFLAGS@ \
	@EXTRA_CPPFLAGS@ \
	###

AM_LDFLAGS = \
	-dlopen self \
	-Wl,--warn-execstack \
	-Wl,--warn-common \
	-Wl,--detect-odr-violations \
	-Wl,--unresolved-symbols=report-all \
	-Wl,--allow-shlib-undefined \
	-Wl,--dynamic-list-data \
	-Wl,--dynamic-list-cpp-new\
	-Wl,--dynamic-list-cpp-typeinfo \
	-Wl,--icf=all \
	-Wl,--rosegment \
	-Wl,--gdb-index \
	-Wl,-z,noexecstack \
	-L$(top_srcdir)/ircd \
	-L$(top_srcdir)/modules \
	$(PLATFORM_LDFLAGS) \
	@EXTRA_LDFLAGS@ \
	###

if GCC
AM_LDFLAGS += \
	-Wl,-fuse-ld=gold \
	###
endif

if CLANG
AM_LDFLAGS += \
	-fuse-ld=gold \
	###
endif

bin_PROGRAMS = construct

construct_LDFLAGS = \
	$(AM_LDFLAGS) \
	@ROCKSDB_LDFLAGS@ \
	@JS_LDFLAGS@ \
	@BOOST_LDFLAGS@ \
	@SSL_LDFLAGS@ \
	@CRYPTO_LDFLAGS@ \
	@SODIUM_LDFLAGS@ \
	@MAGIC_LDFLAGS@ \
	@SNAPPY_LDFLAGS@ \
	@LZ4_LDFLAGS@ \
	@Z_LDFLAGS@ \
	@MALLOC_LDFLAGS@ \
	###

construct_LDADD = \
	-lircd \
	@ROCKSDB_LIBS@ \
	@JS_LIBS@ \
	@BOOST_LIBS@ \
	@SSL_LIBS@ \
	@CRYPTO_LIBS@ \
	@SODIUM_LIBS@ \
	@MAGIC_LIBS@ \
	@SNAPPY_LIBS@ \
	@LZ4_LIBS@ \
	@Z_LIBS@ \
	@MALLOC_LIBS@ \
	@EXTRA_LIBS@ \
	###

construct_SOURCES = \
	construct.cc    \
	signals.cc      \
	console.cc      \
	lgetopt.cc      \
	###
