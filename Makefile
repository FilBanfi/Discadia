# Discadia - retro launcher (C11 + SDL2)
#
# Useful targets:
#   make            build the binary (./discadia)
#   make DEBUG=1    build with -g -O0 and no NDEBUG
#   make run        build and run (dev data dir ./data is used automatically)
#   make test       build and run the unit-test binary
#   make clean      remove objects and binaries
#   make install    install binary to /usr/local/bin and data to /etc/discadia

CC ?= gcc
BASE_CFLAGS = -std=c11 -Wall -Wextra -I./src -I./third_party/cJSON -D_POSIX_C_SOURCE=200809L
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf SDL2_mixer 2>/dev/null || echo -I/usr/include/SDL2)
SDL_LIBS   := $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf SDL2_mixer 2>/dev/null || echo -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer)

ifeq ($(DEBUG),1)
BASE_CFLAGS += -g -O0 -DDISCADIA_DEBUG_BUILD
else
BASE_CFLAGS += -O2
endif

CFLAGS = $(BASE_CFLAGS) $(SDL_CFLAGS)
LDLIBS = $(SDL_LIBS) -lm

TARGET = discadia
TEST_TARGET = discadia_tests

CORE_SRCS = \
       src/log.c \
       src/ui/renderer.c src/ui/layout.c src/ui/home_screen.c \
       src/ui/top_bar.c src/ui/carousel.c src/ui/metadata_panel.c \
       src/ui/empty_screen.c src/ui/launch_screen.c src/ui/widgets.c \
       src/ui/texture_cache.c src/ui/font_cache.c src/ui/animation.c \
       src/ui/toast.c src/ui/boot_screen.c src/ui/menu_screen.c \
       src/theme/theme.c src/theme/theme_parser.c src/theme/theme_assets.c \
       src/input/input.c src/input/controller.c \
       src/input/input_mapping.c src/input/input_hotplug.c \
       src/library/library.c src/library/library_item.c src/library/metadata.c \
       src/library/scanner.c src/library/sorter.c src/library/desktop_entries.c \
       src/media/disc_manager.c src/media/disc_scanner.c \
       src/engine/engine.c src/engine/engine_registry.c src/engine/launcher.c \
       src/engine/variables.c src/engine/process.c \
       src/settings/settings_screen.c src/settings/settings_model.c \
       src/settings/settings_store.c src/settings/input_remap_screen.c \
       src/settings/theme_selector.c src/settings/language_selector.c \
       src/store/store_screen.c src/store/store_model.c src/store/store_index.c \
       src/store/store_client.c src/store/package_manifest.c \
       src/store/package_installer.c src/store/compatibility.c \
       src/audio/audio.c src/audio/sound_cache.c \
       src/i18n/locale.c \
       src/system/system_info.c src/system/power.c src/system/time_service.c \
       src/system/network_status.c \
       src/app/app.c src/app/app_state.c src/app/navigation.c \
       src/util/fs.c src/util/json.c src/util/string_util.c src/util/path_util.c \
       third_party/cJSON/cJSON.c

APP_SRCS = src/main.c $(CORE_SRCS)
TEST_SRCS = tests/test_main.c $(CORE_SRCS)

APP_OBJS = $(APP_SRCS:.c=.o)
TEST_OBJS = $(TEST_SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(APP_OBJS)
	$(CC) $(APP_OBJS) -o $@ $(LDLIBS)

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(TEST_OBJS) -o $@ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	DISCADIA_DATA_DIR=$(CURDIR)/data ./$(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(APP_OBJS) $(TEST_OBJS) $(TARGET) $(TEST_TARGET)

install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/discadia
	install -d $(DESTDIR)/etc/discadia
	cp -r data/. $(DESTDIR)/etc/discadia/

.PHONY: all run test clean install
