/*
 * Discadia unit tests (headless).
 *
 * No SDL video/audio init here: we only exercise pure logic modules
 * (store parsing, compatibility, settings model, string/path utils,
 * animation math). Built with `make test`, linked against CORE_SRCS.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "store/store_model.h"
#include "store/store_index.h"
#include "store/compatibility.h"
#include "store/package_manifest.h"
#include "settings/settings_model.h"
#include "util/string_util.h"
#include "util/path_util.h"
#include "ui/animation.h"

/* ------------------------------------------------------------------ */
/* Tiny test harness                                                    */
/* ------------------------------------------------------------------ */
static int g_tests_run   = 0;
static int g_tests_pass  = 0;
static int g_tests_fail  = 0;

#define CHECK(cond) do {                                                  \
    g_tests_run++;                                                        \
    if (cond) { g_tests_pass++; }                                         \
    else {                                                                \
        g_tests_fail++;                                                   \
        printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
    }                                                                     \
} while (0)

#define RUN(suite) do {                                                   \
    printf("[suite] %s\n", #suite);                                       \
    suite();                                                              \
} while (0)

/* ------------------------------------------------------------------ */
/* store_index: parse a representative index.json                       */
/* ------------------------------------------------------------------ */
static const char *SAMPLE_INDEX = "{\n"
"  \"version\": \"2026.11\",\n"
"  \"last_updated\": \"2026-11-25T10:00:00Z\",\n"
"  \"packages\": [\n"
"    {\n"
"      \"id\": \"cd-player\",\n"
"      \"name\": \"CD Player\",\n"
"      \"type\": \"app\",\n"
"      \"version\": \"1.2.0\",\n"
"      \"size_mb\": 4,\n"
"      \"description_en\": \"Audio CD player\",\n"
"      \"compatible_hardware\": [\"x86_64\", \"arm64\"],\n"
"      \"manifest\": \"packages/cd-player/manifest.json\"\n"
"    },\n"
"    {\n"
"      \"id\": \"neon-theme\",\n"
"      \"name\": \"Neon Dusk\",\n"
"      \"type\": \"theme\",\n"
"      \"version\": \"0.9\",\n"
"      \"description_it\": \"Tema scuro con accenti neon\",\n"
"      \"description_en\": \"Dark theme with neon accents\"\n"
"    },\n"
"    {\n"
"      \"id\": \"\", \"name\": \"Ghost entry - must be skipped\"\n"
"    }\n"
"  ]\n"
"}";

static void test_store_index(void)
{
    store_model m;
    memset(&m, 0, sizeof(m));

    CHECK(store_index_parse(SAMPLE_INDEX, &m) == 0);
    /* empty-id entry is skipped defensively -> 2 valid packages */
    CHECK(m.count == 2);
    CHECK(strcmp(m.index_version, "2026.11") == 0);

    const store_pkg *p0 = &m.items[0];
    CHECK(strcmp(p0->id, "cd-player") == 0);
    CHECK(p0->type == PKG_APP);
    CHECK(p0->size_mb == 4);
    CHECK(p0->arch_count == 2);
    CHECK(strcmp(p0->archs[0], "x86_64") == 0);
    CHECK(strlen(p0->description) > 0);

    const store_pkg *p1 = &m.items[1];
    CHECK(p1->type == PKG_THEME);
    /* description fallback chain: _<lang> then _en then bare key */
    CHECK(strstr(p1->description, "neon") != NULL);

    /* malformed JSON must not crash and must return nonzero */
    store_model bad;
    memset(&bad, 0, sizeof(bad));
    CHECK(store_index_parse("{ this is not json", &bad) != 0);
    CHECK(bad.count == 0);
}

/* ------------------------------------------------------------------ */
/* store_model helpers                                                  */
/* ------------------------------------------------------------------ */
static void test_pkg_type(void)
{
    CHECK(pkg_type_from_string("game")   == PKG_GAME);
    CHECK(pkg_type_from_string("theme")  == PKG_THEME);
    CHECK(pkg_type_from_string("engine") == PKG_ENGINE);
    CHECK(pkg_type_from_string("app")    == PKG_APP);
    CHECK(pkg_type_from_string("bogus")  == PKG_GAME);   /* safe default */
    CHECK(pkg_type_from_string(NULL)     == PKG_GAME);

    /* roundtrip for every known type */
    for (int t = 0; t < PKG_TYPE_COUNT; t++)
        CHECK(pkg_type_from_string(pkg_type_to_string((pkg_type_t)t)) == t);

    CHECK(pkg_type_locale_key(PKG_THEME) != NULL);
    CHECK(strlen(pkg_type_locale_key(PKG_GAME)) > 0);
}

/* ------------------------------------------------------------------ */
/* compatibility: arch aliasing                                         */
/* ------------------------------------------------------------------ */
static void test_compatibility(void)
{
    /* empty token acts as wildcard */
    CHECK(compat_arch_supported("") == 1);
    CHECK(compat_arch_supported(NULL) == 1);

    /* aliases map onto the same canonical token */
    CHECK(compat_arch_supported("amd64") == compat_arch_supported("x86_64"));
    CHECK(compat_arch_supported("x64")   == compat_arch_supported("x86_64"));
    CHECK(compat_arch_supported("aarch64") == compat_arch_supported("arm64"));

    /* a package with no arch constraint is always compatible */
    store_pkg p;
    memset(&p, 0, sizeof(p));
    str_safe_copy(p.id, "any", ST_ID_LEN);
    CHECK(compat_check_pkg(&p) == 1);

    /* a package restricted to an impossible arch is never compatible */
    str_safe_copy(p.archs[0], "msp430-impossible", 16);
    p.arch_count = 1;
    CHECK(compat_check_pkg(&p) == 0);
}

/* ------------------------------------------------------------------ */
/* package manifest                                                     */
/* ------------------------------------------------------------------ */
static void test_manifest(void)
{
    pkg_manifest mf;
    memset(&mf, 0, sizeof(mf));

    const char *json = "{\n"
        "  \"id\": \"cd-player\",\n"
        "  \"name\": \"CD Player\",\n"
        "  \"version\": \"1.2.0\",\n"
        "  \"type\": \"app\",\n"
        "  \"archive\": \"files/cd-player-1.2.0.tar.gz\",\n"
        "  \"sha256\": \"deadbeef\",\n"
        "  \"unpacked_size_mb\": 9\n"
        "}";

    CHECK(pkg_manifest_parse(json, "https://example.com/store/", &mf) == 0);
    CHECK(strcmp(mf.id, "cd-player") == 0);
    CHECK(mf.type == PKG_APP);
    /* relative archive resolved against base URL */
    CHECK(strncmp(mf.archive_url, "https://example.com/store/files/", 34) == 0);
    CHECK(strcmp(mf.sha256, "deadbeef") == 0);
    CHECK(mf.unpacked_size_mb == 9);

    /* missing required fields -> error, no crash */
    pkg_manifest bad;
    memset(&bad, 0, sizeof(bad));
    CHECK(pkg_manifest_parse("{\"name\":\"no id\"}", "http://x/", &bad) != 0);
    CHECK(pkg_manifest_parse("garbage", "http://x/", &bad) != 0);
}

/* ------------------------------------------------------------------ */
/* settings model                                                       */
/* ------------------------------------------------------------------ */
static void test_settings_defaults(void)
{
    settings_model m;
    settings_set_defaults(&m);

    CHECK(strcmp(m.theme, "arcade-classic") == 0);
    CHECK(strcmp(m.language, "en") == 0);
    CHECK(m.audio_enabled == 1);
    CHECK(fabsf(m.audio_volume - 0.7f) < 0.001f);
    CHECK(m.animations_enabled == 1);
    CHECK(m.clock_format == CLOCK_FORMAT_24H);
    CHECK(m.confirm_shutdown == 1);
    CHECK(m.store_cache_days == 7);
}

/* ------------------------------------------------------------------ */
/* string + path utilities                                              */
/* ------------------------------------------------------------------ */
static void test_string_util(void)
{
    char buf[8];
    str_safe_copy(buf, "0123456789", sizeof(buf));
    CHECK(strcmp(buf, "012345") == 0);            /* truncation, NUL-safe */

    char trim[] = "  hi there \t";
    str_trim(trim);
    CHECK(strcmp(trim, "hi there") == 0);

    char lower[] = "MiXeD CaSe";
    str_to_lower(lower);
    CHECK(strcmp(lower, "mixed case") == 0);

    CHECK(str_starts_with("discadia", "disc") == 1);
    CHECK(str_starts_with("discadia", "meta") == 0);
    CHECK(str_ends_with("file.tar.gz", ".gz") == 1);
    CHECK(str_ends_with("file.tar.gz", ".zip") == 0);
    CHECK(str_is_empty("") == 1);
    CHECK(str_is_empty(NULL) == 1);
    CHECK(str_is_empty("x") == 0);

    char dup[] = "abc";
    char *d = str_duplicate(dup);
    CHECK(d && strcmp(d, "abc") == 0);
    free(d);
}

static void test_path_util(void)
{
    char out[256];

    path_join(out, sizeof(out), "/a/b", "c.json");
    CHECK(strcmp(out, "/a/b/c.json") == 0);

    path_join(out, sizeof(out), "/a/b/", "c.json");
    CHECK(strcmp(out, "/a/b/c.json") == 0);       /* no double slash */

    path_basename(out, sizeof(out), "/etc/discadia/config.json");
    CHECK(strcmp(out, "config.json") == 0);

    path_dirname(out, sizeof(out), "/etc/discadia/config.json");
    CHECK(strcmp(out, "/etc/discadia") == 0);

    path_extension(out, sizeof(out), "game.tar.gz");
    CHECK(strcmp(out, "gz") == 0);

    CHECK(path_is_absolute("/x/y") == 1);
    CHECK(path_is_absolute("x/y")  == 0);
}

/* ------------------------------------------------------------------ */
/* animation math                                                       */
/* ------------------------------------------------------------------ */
static void test_animation(void)
{
    CHECK(anim_clamp(5.0f, 0.0f, 1.0f) == 1.0f);
    CHECK(anim_clamp(-5.0f, 0.0f, 1.0f) == 0.0f);
    CHECK(anim_clamp(0.5f, 0.0f, 1.0f) == 0.5f);

    CHECK(fabsf(anim_ease_smooth(0.0f)) < 1e-6f);
    CHECK(fabsf(anim_ease_smooth(1.0f) - 1.0f) < 1e-6f);
    CHECK(fabsf(anim_ease_out_cubic(0.0f)) < 1e-6f);
    CHECK(fabsf(anim_ease_out_cubic(1.0f) - 1.0f) < 1e-6f);

    /* approach: after enough time it converges on target */
    float v = 0.0f;
    for (int i = 0; i < 1000; i++) v = anim_approach(v, 100.0f, 16.0f, 50.0f);
    CHECK(fabsf(v - 100.0f) < 0.5f);

    /* pingpong triangle wave stays in [0,1] and hits both extremes */
    CHECK(anim_pingpong_ms(0.0f, 1000.0f) < 0.01f);
    CHECK(fabsf(anim_pingpong_ms(250.0f, 1000.0f) - 0.5f) < 0.01f);
    CHECK(fabsf(anim_pingpong_ms(500.0f, 1000.0f) - 1.0f) < 0.01f);
    CHECK(fabsf(anim_pingpong_ms(1500.0f, 1000.0f) - 1.0f) < 0.01f); /* wraps */
    for (float age = 0; age < 10000.0f; age += 37.0f) {
        float p = anim_pingpong_ms(age, 1000.0f);
        CHECK(p >= -0.001f && p <= 1.001f);
    }
}

/* ------------------------------------------------------------------ */
int main(void)
{
    printf("=== Discadia unit tests ===\n");

    RUN(test_store_index);
    RUN(test_pkg_type);
    RUN(test_compatibility);
    RUN(test_manifest);
    RUN(test_settings_defaults);
    RUN(test_string_util);
    RUN(test_path_util);
    RUN(test_animation);

    printf("\n%d checks: %d passed, %d failed\n",
           g_tests_run, g_tests_pass, g_tests_fail);
    return g_tests_fail ? 1 : 0;
}
