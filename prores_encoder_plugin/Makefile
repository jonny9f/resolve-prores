include .mk.defs

BASEDIR = ./

BUILD_DIR = .
SUBDIRS = wrapper
X264_DIR = /home/jon/dev/resolve/build/

CFLAGS +=  -g -fPIC -Wextra 

ifeq ($(OS_TYPE), Linux)
LDFLAGS = -shared '-Wl,-rpath,$$ORIGIN' -Wl,-z,origin -lpthread  
else
LDFLAGS = -dynamiclib
endif

TARGET = $(BINDIR)/prores_encoder_plugin.dvcp

LDFLAGS += -lz -lavformat -lavcodec -lavutil

OBJDIR = $(BUILD_DIR)/build
BINDIR = $(BUILD_DIR)/bin

.PHONY: all

HEADERS = plugin.h prores_encoder.h audio_encoder.h mov_container.h prores_props.h
SRCS = plugin.cpp prores_encoder.cpp mov_container.cpp audio_encoder.cpp
OBJS = $(SRCS:%.cpp=$(OBJDIR)/%.o)

all: prereq make-subdirs $(HEADERS) $(SRCS) $(OBJS) $(TARGET)

prereq:
	mkdir -p $(OBJDIR)
	mkdir -p $(BINDIR)

$(OBJDIR)/%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET):
	$(CC) $(OBJDIR)/*.o $(LDFLAGS) -o $(TARGET)

clean: clean-subdirs
	rm -rf $(OBJDIR)
	rm -rf $(BINDIR)

make-subdirs:
	@for subdir in $(SUBDIRS); do \
	echo "Making $$subdir"; \
	(cd $$subdir; make; cd ..) \
	done

clean-subdirs:
	@for subdir in $(SUBDIRS); do \
	echo "Making clean in $$subdir"; \
	(cd $$subdir; make clean; cd ..) \
	done


# install: $(TARGET)
install: all
	mkdir -p $(BINDIR)/prores_encoder_plugin.dvcp.bundle/Contents/Linux-x86-64/
	cp $(TARGET) $(BINDIR)/prores_encoder_plugin.dvcp.bundle/Contents/Linux-x86-64/

