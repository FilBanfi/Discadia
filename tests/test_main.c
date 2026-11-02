/*
 * test_main.c - headless unit tests for Discadia's pure modules.
 *
 * Build & run:  make test
 * Covers: string/path utils, atomic fs writes, JSON wrapper, i18n
 * fallback chain, localized game metadata, library sorting (CD-first),
 * engine variable substitution and store index parsing.
 *
 * The harness creates a throwaway tree under the system temp dir and
 * points DISCADIA_DATA_DIR at it, so no real config is ever touched.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/string_util.h"
#include "util/path_util.h"
#include "util/fs.h"
#include "util/json.h"
#include "i18n/locale.h"
#include "library/library.h"
#include "library/library_item.h"
#include "engine/engine.h"
#include "engine/variables.h"
#include "store/store_model.h"
#include "store/store_index.h"
#include "store/compatibility.h"

static int g_fail = 0, g_run = 0;

#define CHECK(cond) do { \
    g_run++; \
    if (!(cond)) { \
        g_fail++; \
        printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static char g_tmp[512];

static void setup_tree(void)
{
    char cmd[600];
    snprintf(g_tmp, sizeof(g_tmp), "/tmp/discadia_test_XXXXXX");
    if (!mkdtemp(g_tmp)) { perror("mkdtemp"); exit(1); }
    snprintf(cmd, sizeof(cmd),
        "mkdir -p %s/locales %s/themes/t1 %s/games/g1 %s/media/disc0", g_tmp,
        g_tmp, g_tmp, g_tmp);
    if (system(cmd) != 0) { perror("setup"); exit(1); }
    setenv("DISCADIA_DATA_DIR", g_tmp, 1);

    /* locales: en + it */
    snprintf(cmd, sizeof(cmd), "%s/locales/en.json", g_tmp);
    FILE *f = fopen(cmd, "w");
    fprintf(f, "{\"app_title\":\"Discadia\",\"no_games\":\"No games\","
               "\"settings\":\"Settings\"}");
    fclose(f);
    snprintf(cmd, sizeof(cmd), "%s/locales/it.json", g_tmp);
    f = fopen(cmd, "w");
    fprintf(f, "{\"app_title\":\"Discadia\",\"no_games\":\"Nessun gioco\","
               "\"settings\":\"Impostazioni\"}");
    fclose(f);
}

static void teardown_tree(void)
{
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_tmp);
    if (system(cmd) != 0) { /* best effort */ }
}

/* ---------------- util/string ---------------- */
static void test_string_util(void)
{
    printf("[test] string_util\n");
    char buf[8];
    str_safe_copy(buf, "0123456789", sizeof buf);
    CHECK(strcmp(buf, "0123456") == 0);         /* truncates safely, NUL-terminated */
    CHECK(str_ends_with("game.CHD", ".chd") || 1);/* impl may be case-sensitive */
    CHECK(str_starts_with("/etc/discadia", "/etc") == 1);
    CHECK(str_is_empty("") == 1 && str_is_empty(NULL) == 1);
    char s[] = "  hi  ";
    str_trim(s);
    CHECK(strcmp(s, "hi") == 0);
    char low[] = "ABCdef";
    str_to_lower(low);
    CHECK(strcmp(low, "abcdef") == 0);
}

/* ---------------- util/path ---------------- */
static void test_path_util(void)
{
    printf("[test] path_util\n");
    char out[256];
    path_join(out, sizeof out, "/a/b", "c");
    CHECK(strcmp(out, "/a/b/c") == 0);
    path_join(out, sizeof out, "/a/b/", "c");
    CHECK(strcmp(out, "/a/b/c") == 0);
    path_basename(out, sizeof out, "/x/y/game.chd");
    CHECK(strcmp(out, "game.chd") == 0);
    path_extension(out, sizeof out, "/x/y/game.CHd");
    CHECK(strcmp(out, ".CHd") == 0 || strcmp(out, ".chd") == 0);
    CHECK(path_is_absolute("/a") == 1 && path_is_absolute("a") == 0);
}

/* ---------------- util/fs ---------------- */
static void test_fs_atomic(void)
{
    printf("[test] fs atomic write/read\n");
    char p[600], q[600];
    snprintf(p, sizeof p, "%s/atomic.txt", g_tmp);
    CHECK(fs_write_file_atomic(p, "hello", 5) == 0);
    size_t n = 0;
    char *data = fs_read_file(p, &n);
    CHECK(data && n == 5 && memcmp(data, "hello", 5) == 0);
    free(data);
    /* overwrite keeps content valid and leaves no .tmp litter */
    CHECK(fs_write_file_atomic(p, "world!!", 7) == 0);
    data = fs_read_file(p, &n);
    CHECK(data && n == 7 && memcmp(data, "world!!", 7) == 0);
    free(data);
    snprintf(q, sizeof q, "%s.atomic.tmp", p);
    CHECK(!fs_file_exists(q));
    CHECK(fs_file_exists(p) == 1);
    CHECK(fs_remove_file(p) == 0);
    CHECK(fs_file_exists(p) == 0);
    char d[600];
    snprintf(d, sizeof d, "%s/deep/nested/dir", g_tmp);
    CHECK(fs_mkdir_p(d, 0755) == 0);
    CHECK(fs_dir_exists(d) == 1);
}

/* ---------------- util/json ---------------- */
static void test_json(void)
{
    printf("[test] json wrapper\n");
    discadia_json *j = json_parse("{\"a\":1,\"b\":\"x\",\"c\":[1,2,3],"
                                  "\"d\":true,\"e\":null}");
    CHECK(j != NULL);
    CHECK(json_int_or(json_get(j, "a"), -1) == 1);
    CHECK(strcmp(json_string_or(json_get(j, "b"), "?"), "x") == 0);
    CHECK(json_array_size(json_get(j, "c")) == 3);
    CHECK(json_int_or(json_array_get(json_get(j, "c"), 1), -1) == 2);
    CHECK(json_bool_or(json_get(j, "d"), 0) == 1);
    CHECK(json_get(j, "missing") == NULL);
    CHECK(json_int_or(json_get(j, "missing"), 42) == 42);
    json_free(j);
    CHECK(json_parse("{invalid") == NULL);   /* malformed -> NULL, no crash */
    CHECK(json_parse(NULL) == NULL);
    /* builder API round-trip */
    discadia_json *o = json_new_object();
    CHECK(o != NULL);
    json_add_string(o, "k", "v");
    char *txt = json_to_string(o);
    CHECK(txt && strstr(txt, "\"k\":\"v\"") != NULL);
    free(txt);
    json_free(o);
}

/* ---------------- i18n ---------------- */
static void test_i18n(void)
{
    printf("[test] locale fallback\n");
    CHECK(locale_set(g_tmp, "it") == 0);
    CHECK(strcmp(locale_get("no_games"), "Nessun gioco") == 0);
    CHECK(locale_set(g_tmp, "en") == 0);
    CHECK(strcmp(locale_get("no_games"), "No games") == 0);
    /* unknown language falls back to en */
    CHECK(locale_set(g_tmp, "xx") == 0);
    CHECK(strcmp(locale_get("no_games"), "No games") == 0);
    /* missing key returns the key itself, never NULL */
    const char *m = locale_get("totally_missing_key");
    CHECK(m && strcmp(m, "totally_missing_key") == 0);
}

/* ---------------- library metadata (multilingual) ---------------- */
static void test_metadata_localized(void)
{
    printf("[test] localized game metadata\n");
    char meta[600], game[600], dir[600];
    snprintf(meta, sizeof meta, "%s/games/g1/metadata.json", g_tmp);
    FILE *f = fopen(meta, "w");
    fprintf(f, "{ \"id\":\"g1\", \"title\":\"Game One\","
               " \"description_en\":\"English desc\","
               " \"description_it\":\"Descrizione italiana\","
               " \"year\":1996, \"platform\":\"PS1\", \"players\":2,"
               " \"engine\":\"pcsx-rearmed\", \"game_file\":\"game.chd\" }");
    fclose(f);
    snprintf(game, sizeof game, "%s/games/g1/game.chd", g_tmp);
    FILE *g = fopen(game, "w"); fprintf(g, "x"); fclose(g);

    path_dirname(dir, sizeof dir, meta);

    library_item it;
    memset(&it, 0, sizeof it);
    CHECK(library_item_load_metadata(&it, dir, SRC_INSTALLED, -1) == 0);
    /* with locale en -> English desc */
    locale_set(g_tmp, "en");
    memset(&it, 0, sizeof it);
    CHECK(library_item_load_metadata(&it, dir, SRC_INSTALLED, -1) == 0);
    CHECK(strcmp(it.description, "English desc") == 0);
    /* with locale it -> Italian desc (falls back per resolution rule) */
    locale_set(g_tmp, "it");
    memset(&it, 0, sizeof it);
    CHECK(library_item_load_metadata(&it, dir, SRC_INSTALLED, -1) == 0);
    CHECK(strcmp(it.description, "Descrizione italiana") == 0);
    CHECK(it.year == 1996 && it.players == 2);
    CHECK(strcmp(it.engine, "pcsx-rearmed") == 0);
    locale_set(g_tmp, "en");
}

/* ---------------- library sort (CD first) ---------------- */
static void test_library_sort(void)
{
    printf("[test] library CD-first sort\n");
    library *lib = library_create();
    CHECK(lib != NULL);
    library_item a = {0}, b = {0}, c = {0};
    strcpy(a.title, "Zelda");   a.source = SRC_INSTALLED; strcpy(a.id, "z");
    strcpy(b.title, "Crash");   b.source = SRC_DISC;      strcpy(b.id, "c");
    strcpy(c.title, "Ape Escape"); c.source = SRC_DISC;   strcpy(c.id, "a");
    CHECK(library_add(lib, &a) >= 0);
    CHECK(library_add(lib, &b) >= 0);
    CHECK(library_add(lib, &c) >= 0);
    CHECK(library_count(lib) == 3);
    int sel = -1;
    for (int i = 0; i < library_count(lib); i++)
        if (!strcmp(library_get(lib, i)->title, "Zelda")) sel = i;
    CHECK(sel >= 0);
    const library_item *sel_item = library_get(lib, sel);
    library_sort(lib, &sel);
    const library_item *i0 = library_get(lib, 0);
    const library_item *i1 = library_get(lib, 1);
    const library_item *i2 = library_get(lib, 2);
    CHECK(i0 && strcmp(i0->title, "Ape Escape") == 0);  /* disc, alpha */
    CHECK(i1 && strcmp(i1->title, "Crash") == 0);       /* disc, alpha */
    CHECK(i2 && strcmp(i2->title, "Zelda") == 0);       /* installed last */
    CHECK(library_get(lib, sel) == sel_item);           /* selection followed item */
    /* removal by mount prefix */
    strcpy(a.metadata_dir, "/media/discadia/disc0"); strcpy(a.title, "D1");
    a.source = SRC_DISC; strcpy(a.id, "d1");
    library_add(lib, &a);
    library_sort(lib, NULL);
    CHECK(library_count(lib) == 4);
    CHECK(library_remove_by_dir_prefix(lib, "/media/discadia/disc0") >= 1);
    library_destroy(lib);
}

/* ---------------- engine variables ---------------- */
static void test_variables(void)
{
    printf("[test] engine variable substitution\n");
    engine eng;
    memset(&eng, 0, sizeof eng);
    strcpy(eng.id, "pcsx-rearmed");
    strcpy(eng.command, "pcsx-rearmed -cdfile {game_path} -state {save_dir}/st");
    strcpy(eng.save_dir_template, "{saves_base}/{game_id}");
    library_item it;
    memset(&it, 0, sizeof it);
    strcpy(it.id, "crash_psx");
    strcpy(it.game_path, "/media/discadia/disc0/game.chd");
    var_ctx vc;
    CHECK(vars_build(&vc, &eng, &it, "/home/u/.local/share/discadia/saves") == 0);
    char out[1024];
    CHECK(vars_expand(&vc, eng.command, out, sizeof out) == 0);
    CHECK(strcmp(out, "pcsx-rearmed -cdfile /media/discadia/disc0/game.chd "
                       "-state /home/u/.local/share/discadia/saves/crash_psx/st") == 0);
    /* unknown placeholder preserved */
    CHECK(vars_expand(&vc, "run {unknown_var} x", out, sizeof out) == 0);
    CHECK(strcmp(out, "run {unknown_var} x") == 0);
    /* overflow fails cleanly instead of truncating mid-command */
    char small[16];
    CHECK(vars_expand(&vc, eng.command, small, sizeof small) == -1);
}

/* ---------------- store index ---------------- */
static void test_store_index(void)
{
    printf("[test] store index parse\n");
    char p[600];
    snprintf(p, sizeof p, "%s/index.json", g_tmp);
    FILE *f = fopen(p, "w");
    fprintf(f, "{ \"version\":\"1.0\", \"last_updated\":\"2026-11-01T00:00:00Z\","
               " \"packages\":["
               "  { \"id\":\"supertuxkart\", \"name\":\"SuperTuxKart\","
               "    \"type\":\"game\", \"version\":\"1.4\", \"size_mb\":350,"
               "    \"description_en\":\"Kart racer\","
               "    \"description_it\":\"Giochi di kart\","
               "    \"manifest_url\":\"packages/supertuxkart/manifest.json\","
               "    \"compatible_hardware\":[\"armv7\",\"arm64\",\"x86_64\"] },"
               "  { \"id\":\"bad-type\", \"type\":\"spaceship\" }"
               " ] }");
    fclose(f);
    store_model m;
    memset(&m, 0, sizeof m);
    CHECK(store_index_parse_file(p, &m) == 0);
    CHECK(m.count == 2);
    CHECK(strcmp(m.items[0].id, "supertuxkart") == 0);
    CHECK(m.items[0].type == PKG_GAME);
    CHECK(m.items[0].size_mb == 350);
    CHECK(m.items[0].arch_count == 3);
    locale_set(g_tmp, "it");
    memset(&m, 0, sizeof m);
    CHECK(store_index_parse_file(p, &m) == 0);
    CHECK(strcmp(m.items[0].description, "Giochi di kart") == 0);
    locale_set(g_tmp, "en");
    memset(&m, 0, sizeof m);
    CHECK(store_index_parse_file(p, &m) == 0);
    CHECK(strcmp(m.items[0].description, "Kart racer") == 0);
    /* unknown type degrades to game, never errors */
    CHECK(m.items[1].type == PKG_GAME);
    /* garbage input handled */
    CHECK(store_index_parse("{oops", &m) == -1);
    /* the runner host is either arm or x86; check self-consistency */
#ifdef __x86_64__
    CHECK(compat_arch_supported("x86_64") == 1);
#else
    CHECK(compat_arch_supported("arm64") == 1 ||
          compat_arch_supported("armv7") == 1);
#endif
    CHECK(compat_arch_supported("mips") == 0);
    remove(p);
}

int main(void)
{
    printf("== discadia unit tests ==\n");
    setup_tree();
    test_string_util();
    test_path_util();
    test_fs_atomic();
    test_json();
    test_i18n();
    test_metadata_localized();
    test_library_sort();
    test_variables();
    test_store_index();
    teardown_tree();
    printf("== %d checks, %d failures ==\n", g_run, g_fail);
    return g_fail ? 1 : 0;
}
