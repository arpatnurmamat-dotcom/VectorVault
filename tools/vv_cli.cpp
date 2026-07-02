#include <vectorvault/vectorvault.h>
#include <vectorvault/vectorvault.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void PrintUsage() {
    std::printf("VectorVault CLI v0.1.0\n");
    std::printf("Usage: vv <command> [args]\n");
    std::printf("Commands:\n");
    std::printf("  info <file>              Show database info\n");
    std::printf("  version                  Print version\n");
}

int main(int argc, char** argv) {
    if (argc < 2) { PrintUsage(); return 1; }

    if (std::strcmp(argv[1], "version") == 0) {
        std::printf("%s\n", vv_version_string());
        return 0;
    }

    if (std::strcmp(argv[1], "info") == 0 && argc >= 3) {
        vv_t* db = nullptr;
        vv_config_t cfg{};
        cfg.dimension = 768;
        int rc = vv_open(&db, argv[2], VV_OPEN_READONLY, &cfg);
        if (rc != VV_OK) {
            std::fprintf(stderr, "Error opening %s: %s\n", argv[2], vv_error_string(rc));
            return 1;
        }
        uint32_t maj = 0, min = 0, dim = 0;
        vv_distance_t dist{};
        vv_index_t idx{};
        uint64_t count = 0, fsize = 0;
        vv_info(db, &maj, &min, &dim, &dist, &idx, &count, &fsize);
        std::printf("VectorVault %u.%u | dim=%u | vectors=%llu | size=%llu bytes\n",
                    maj, min, dim,
                    static_cast<unsigned long long>(count),
                    static_cast<unsigned long long>(fsize));
        vv_close(db);
        return 0;
    }

    PrintUsage();
    return 1;
}
