/* gotta go fast */
#undef _GLIBCXX_DEBUG
#pragma GCC optimize("Ofast", "unroll-loops", "omit-frame-pointer", "inline")
#pragma GCC option("arch=native,tune=native")
#pragma GCC target("movbe,pclmul,aes,rdrnd,bmi,bmi2,lzcnt,popcnt,avx,avx2,f16c,fma,sse3,ssse3,sse4.1,sse4.2")
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <x86intrin.h>

/* AstroBytes gave me the following three functions from MSmits */
inline float fastlogf(const float& x)
{
    union {
        float f;
        uint32_t i;
    } vx = { x };
    float y = vx.i;
    y *= 8.2629582881927490e-8f;
    return (y - 87.989971088f);
}

inline float fastsqrtf(const float& x)
{
    union {
        int i;
        float x;
    } u;
    u.x = x;
    u.i = (1 << 29) + (u.i >> 1) - (1 << 22);
    return (u.x);
}

inline float rsqrt_fast(float x) { return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x))); }

std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

int randval(int n)
{
    return std::uniform_int_distribution<>(0, n - 1)(rng);
}

uint16_t random_bit(uint16_t available_moves)
{
    return __builtin_ctzl(__builtin_ia32_pdep_si(1UL << randval(__builtin_popcountl(available_moves)), available_moves));
}

const float EXPLORATION_PARAMETER = sqrt(2);

enum Condition {
    /* loss is only used in rollout result. win is more general and refers to when there is a winner */
    win = 0,
    loss = 1,
    draw = 2,
    unfinished = 3
};

/* TODO change this for ultimate */
struct State {
    uint16_t boards[2] = { 0 };
    bool player = 0;
};

/* TODO change this for ultimate */
void show_board(uint16_t a, uint16_t b)
{
    auto transform = [&a, &b](const int8_t sq) {
        int a_val = a & (1 << sq);
        int b_val = b & (1 << sq);
        return a_val ? 'x' : b_val ? 'o'
                                   : ' ';
    };
    std::printf("===========\n"
                " %c ║ %c ║ %c \n"
                "═══╬═══╬═══\n"
                " %c ║ %c ║ %c \n"
                "═══╬═══╬═══\n"
                " %c ║ %c ║ %c \n"
                "===========\n",
        transform(0),
        transform(1),
        transform(2),
        transform(3),
        transform(4),
        transform(5),
        transform(6),
        transform(7),
        transform(8));
}

/* adapted from Wontonimo's code snippet */
static constexpr std::array<bool, (1 << 9)> is_win = [] {
    const int16_t configs[] = {
        0b000000111,
        0b000111000,
        0b111000000,

        0b001001001,
        0b010010010,
        0b100100100,

        0b100010001,
        0b001010100
    };
    auto a = decltype(is_win) {};
    for (int i = 0; i < (1 << 9); i++)
        for (int w = 0; w < 8; w++)
            a[i] |= (configs[w] & i) == configs[w];
    return a;
}();

inline uint16_t available_moves(uint16_t a, uint16_t b)
{
    return 0b111111111 ^ (a | b);
}

inline Condition board_state(const State& s)
{
    return is_win[s.boards[s.player]] ? win : available_moves(s.boards[0], s.boards[1]) ? unfinished
                                                                                        : draw;
}

/* TODO change this for ultimate */
Condition rollout(State s)
{
    bool og_player = s.player;
    while (board_state(s) == unfinished) {
        s.player ^= 1;
        uint16_t move = random_bit(available_moves(s.boards[0], s.boards[1]));
        s.boards[s.player] |= 1 << move;
    }
    Condition end = board_state(s);
    return end == win ? (s.player == og_player ? win : loss) : draw;
}

/* TODO change this for ultimate */
struct Node {
    Node(State s, std::shared_ptr<Node> p)
        : state { s }
        , parent { p }
    {
        children.reserve(9);
    }
    const State state;
    std::shared_ptr<Node> parent;
    int64_t visits = 0;
    int64_t score = 0;
    std::vector<std::shared_ptr<Node>> children;
};

/* TODO change this for ultimate */
void expand(std::shared_ptr<Node> n)
{
    if (board_state(n->state) != unfinished)
        return;
    bool next_player = n->state.player ^ 1;
    uint16_t moves = available_moves(n->state.boards[0], n->state.boards[1]);
    for (int i = 0; i < 9; i++) {
        if ((moves & (1 << i)) != 0) {
            State new_state = n->state;
            new_state.player = next_player;
            new_state.boards[next_player] |= 1 << i;
            n->children.push_back(std::make_shared<Node>(new_state, n));
        }
    }
}

void backpropagate(std::shared_ptr<Node> n, Condition endstate)
{
    std::shared_ptr<Node> nc = n;
    if (endstate == draw) {
        while (nc != nullptr) {
            nc->visits++;
            nc = nc->parent;
        }
    } else {
        int perspective_delta = endstate == win ? +1 : -1;
        while (nc != nullptr) {
            nc->visits++;
            nc->score += perspective_delta;
            perspective_delta = -perspective_delta;
            nc = nc->parent;
        }
    }
}

/*
 * constant is c * sqrt(ln(N)).
 * This is just used to speed up calculations.
 */
float UCT(std::shared_ptr<Node> n, float constant)
{
    if (n->visits == 0) {
        return std::numeric_limits<float>::infinity();
    }
    float inv_sqrt_visits = rsqrt_fast(n->visits);
    return ((n->score * inv_sqrt_visits) + constant) * inv_sqrt_visits;
}

/*
 * SHEEEEEEEEEEESH
 * the speed
 */
std::shared_ptr<Node> select(std::shared_ptr<Node> n)
{
    while (!n->children.empty()) {
        float constant = EXPLORATION_PARAMETER * fastsqrtf(fastlogf(n->visits));
        std::shared_ptr<Node> p;

        /* set to lowest.
         * Not using std::max_element since that's slower
         */
        float highest_UCT = -std::numeric_limits<float>::infinity();
        for (auto c : n->children) {
            float UCT_underdog = UCT(c, constant);
            if (UCT_underdog > highest_UCT) {
                p = c;
                highest_UCT = UCT_underdog;
            }
        }
        n = p;
    }
    return n;
}

std::shared_ptr<Node> mcts_next_move(std::shared_ptr<Node> root, int iterations, int time_limit_ms)
{
    if (root->children.empty())
        expand(root);
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations && (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(time_limit_ms)); i++) {
        std::shared_ptr<Node> leaf = select(root);
        if (leaf->visits != 0) {
            expand(leaf);
            if (!(leaf->children.empty())) {
                leaf = leaf->children[randval(leaf->children.size())];
            }
        }
        Condition res = rollout(leaf->state);
        backpropagate(leaf, res);
    }
    /* for (auto c: root->children) { */
    /* 	show_board(c->state.boards[0], c->state.boards[1]); */
    /* 	std::printf("child with score[%lld]/visits[%lld]\n\n", c->score, c->visits); */
    /* } */
    return *std::max_element(root->children.begin(), root->children.end(), [](const auto a, const auto b) { return a->visits < b->visits; });
}

int main(int argc, char* argv[])
{
    int iterations = argc >= 2 ? std::stoi(argv[1]) : 200;
    int time_limit_ms = argc >= 3 ? std::stoi(argv[2]) : 1000;
    std::printf("iterations=%d time_limit_ms=%d\n", iterations, time_limit_ms);
    uint16_t initial_board = 0b000'000'000;
    /* uint16_t initial_board1 = 0b100'010'000; */
    /* uint16_t initial_board2 = 0b000'000'011; */
    State s { { initial_board, initial_board }, 0 };
    /* State s{{initial_board1, initial_board2}, 1}; */
    std::shared_ptr<Node> root = std::make_shared<Node>(s, nullptr);
    while (board_state(root->state) == unfinished) {
        std::shared_ptr<Node> next = mcts_next_move(root, iterations, time_limit_ms);
        State new_state = next->state;
        show_board(new_state.boards[0], new_state.boards[1]);
        /* std::printf("this had score[%lld]/visits[%lld]\n", next->score, next->visits); */
        /* re-root tree at the current move */
        root = next;
    }
    std::printf("%d\n", board_state(root->state));
}

// vim: ts=4 sts=4 sw=4
