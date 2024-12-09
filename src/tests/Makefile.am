
check_PROGRAMS = \
  test_dada_nexus\
  $(TESTS)

TESTS = \
  test_ascii_header \
  test_command_parser \
  test_disk_array \
  test_ipcbuf \
  test_median_smooth_zap \
  test_multilog \
  test_uint64_t \
  test_ipcio \
  test_ipcio_lifecycle

bin_PROGRAMS = \
  test_command_parse_server \
  test_multilog_server

AM_CFLAGS = -I$(top_srcdir)/src
LDADD = $(top_builddir)/src/libpsrdada.la
