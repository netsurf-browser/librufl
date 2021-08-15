# Tests
DIR_TEST_ITEMS := simple:simple.c;mocks.c

ifeq ($(HOST),arm-unknown-riscos)
  DIR_TEST_ITEMS := $(DIR_TEST_ITEMS) \
	  rufl_test:rufl_test.c \
	  rufl_chars:rufl_chars.c
endif

include $(NSBUILD)/Makefile.subdir
