#include <cstdio>
#include <array>
#include <limits>
#include <cmath>
#include <vector>
#include <random>
#include <memory>
#include <chrono>
#include <algorithm>

std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

int randval(int n) {
    return std::uniform_int_distribution<>(0, n - 1)(rng);
}

uint16_t random_bit(uint16_t available_moves) {
    return __builtin_ctzl(__builtin_ia32_pdep_si(1UL << (randval( __builtin_popcountl(available_moves))), available_moves));
}

int main() {}
