CC = gcc
CFLAGS = -Wall -Wextra -O2 -I/usr/include/SDL2 -I./src -I./third_party
LDFLAGS = -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -ludev -lm

TARGET = discadia
SRCS = src/main.c \
       src/app/app.c src/app/app_state.c src/app/navigation.c \
       src/ui/renderer.c src/ui/layout.c src/ui/home_screen.c \
       src/ui/top_bar.c src/ui/carousel.c src/ui/metadata_panel.c \
       src/ui/empty_screen.c src/ui/launch_screen.c src/ui/widgets.c \
       src/ui/texture_cache.c src/ui/font_cache.c src/ui/animation.c \
       src/theme/theme.c src/theme/theme_parser.c src/theme/theme_assets.c \
       src/input/input.c src/input/controller.c src/input/keyboard.c \
       src/input/input_mapping.c src/input/input_hotplug.c \
       src/library/library.c src/library/library_item.c src/library/metadata.c \
       src/library/scanner.c src/library/sorter.c src/library/desktop_entries.c \
       src/media/disc_manager.c src/media/disc_drive.c src/media/disc_scanner.c \
       src/engine/engine.c src/engine/engine_registry.c src/engine/launcher.c \
       src/engine/variables.c src/engine/process.c \
       src/settings/settings_screen.c src/settings/settings_model.c \
       src/settings/settings_store.c src/settings/input_remap_screen.c \
       src/settings/theme_selector.c src/settings/language_selector.c \
       src/store/store_screen.c src/store/store_model.c src/store/store_index.c \
       src/store/store_client.c src/store/package_manifest.c \
       src/store/package_installer.c src/store/compatibility.c \
       src/audio/audio.c src/audio/sound_cache.c \
       src/i18n/locale.c src/i18n/locale_store.c \
       src/system/system_info.c src/system/power.c src/system/time_service.c \
       src/util/fs.c src/util/json.c src/util/string_util.c src/util/path_util.c \
       third_party/cJSON/cJSON.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -d /usr/local/bin
	install -m 755 $(TARGET) /usr/local/bin/discadia
	install -d /etc/discadia
	cp -r data/* /etc/discadia/

.PHONY: all clean install
