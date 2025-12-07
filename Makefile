EXTENSION = pg_usaddress
MODULE_big = pg_usaddress
DATA = sql/pg_usaddress--0.0.1.sql

CRFSUITE_SRCS = $(wildcard src/crfsuite/src/*.c)
# Exclude training files that require external dependencies (lbfgs)
# Keep train_l2sgd.c which is self-contained and likely used by the model
# Also exclude crfsuite_train.c which is the training interface (we only need tagging/inference)
CRFSUITE_EXCLUDE = %/train_arow.c %/train_averaged_perceptron.c %/train_lbfgs.c %/train_passive_aggressive.c %/stub_train.c %/crfsuite_train.c
CRFSUITE_OBJS = $(patsubst %.c,%.o,$(filter-out $(CRFSUITE_EXCLUDE), $(CRFSUITE_SRCS)))

OBJS = src/pg_usaddress.o src/crfsuite_wrapper.o src/feature_extractor.o src/crfsuite_stubs.o $(CRFSUITE_OBJS)

REGRESS = test_parsing
REGRESS_OPTS = --inputdir=tests

PG_CPPFLAGS += -Isrc/crfsuite/include -Isrc/crfsuite/src
# SHLIB_LINK = -lcrfsuite # Linked statically via source inclusion
PG_LDFLAGS += -L/usr/local/lib

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# Install model file to extension directory
install: install-model

install-model:
	$(INSTALL_DATA) include/usaddr.crfsuite '$(DESTDIR)$(datadir)/extension/'
