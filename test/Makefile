# Tests
DIR_TEST_ITEMS := nofonts:nofonts.c;harness.c;mocks.c \
	ucsinit:ucsinit.c;harness.c;mocks.c

ifeq ($(HOST),arm-unknown-riscos)
  DIR_TEST_ITEMS := $(DIR_TEST_ITEMS) \
	  rufl_test:rufl_test.c \
	  rufl_chars:rufl_chars.c
endif

include $(NSBUILD)/Makefile.subdir
