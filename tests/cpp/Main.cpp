// Test runner: aggregates the M1 core suites. Returns non-zero on failure.
#include <cstdio>

int testBits();
int testPlacement();
int testVerifier();
int testNbt();
int testReader();
int testMapping();
int testGzip();

int main() {
    int total = 0;
    std::printf("== Bits ==\n");
    total += testBits();
    std::printf("== Placement ==\n");
    total += testPlacement();
    std::printf("== Verifier ==\n");
    total += testVerifier();
    std::printf("== NBT ==\n");
    total += testNbt();
    std::printf("== Reader ==\n");
    total += testReader();
    std::printf("== Mapping ==\n");
    total += testMapping();
    std::printf("== Gzip+E2E ==\n");
    total += testGzip();
    std::printf("%d failure(s)\n", total);
    return total == 0 ? 0 : 1;
}
