#include <stdio.h>
#include <stdlib.h>
#include "util/string_util.h"
#include "util/path_util.h"
#include "util/fs.h"
#include "util/json.h"

#define DISCADIA_VERSION "0.1.0"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("Discadia %s\n", DISCADIA_VERSION);
    printf("Initializing...\n");
    
    // Quick self-test of utilities
    char buf[256];
    path_join(buf, sizeof(buf), "/etc/discadia", "themes");
    printf("Config path test: %s\n", buf);
    
    char cfg[512];
    fs_get_config_dir(cfg, sizeof(cfg));
    printf("User config dir: %s\n", cfg);
    
    printf("Ready.\n");
    return 0;
}
