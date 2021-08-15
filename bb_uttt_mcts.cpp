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
#include <utility>
#include <cassert>
#include <x86intrin.h>

/* gotta go fast */

/* only used for the 3rd board in big_board */
#define TIE 2

/* one larger for tied squares */
using big_board_t = std::array<uint32_t, 3>;
using small_board_t = std::array<uint32_t, 2>;

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

inline uint16_t random_bit(uint16_t available_moves)
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

struct State {
    big_board_t big_board;
    std::array<small_board_t, 9> small_board;

    /*
     * 9 = play anywhere
     *  0-8 = that part of the big_board
     */
    uint16_t valid_square = 9;
    bool player = 0;
};

/* TODO change this for ultimate */
void show_board(const State& s)
{
    auto big_transform = [&s](const int8_t sq) {
        int a_val = s.big_board[0] & (1 << sq);
        int b_val = s.big_board[1] & (1 << sq);
        int t_val = s.big_board[TIE] & (1 << sq);
        return a_val ? 'x' : b_val ? 'o' : t_val ? '-' : ' ';
    };
    auto transform = [&s](const int8_t v) {
        int r = v / 9;
        int c = v % 9;

        int row = r / 3;
        int col = c / 3;
        int sq = row * 3 + col;

        int srow = r % 3;
        int scol = c % 3;
        int mv = srow * 3 + scol;

        uint16_t a_val = s.small_board[sq][0] & (1 << mv);
        uint16_t b_val = s.small_board[sq][1] & (1 << mv);
        return a_val ? 'x' : b_val ? 'o'
                                   : '.';
    };
    std::fprintf(stderr,
                "=======================\n"
                "[%c] %c%c%c║[%c] %c%c%c║[%c] %c%c%c\n"
                "    %c%c%c║    %c%c%c║    %c%c%c\n"
                "    %c%c%c║    %c%c%c║    %c%c%c\n"
                "═══════╬═══════╬═══════\n"
                "[%c] %c%c%c║[%c] %c%c%c║[%c] %c%c%c\n"
                "    %c%c%c║    %c%c%c║    %c%c%c\n"
                "    %c%c%c║    %c%c%c║    %c%c%c\n"
                "═══════╬═══════╬═══════\n"
                "[%c] %c%c%c║[%c] %c%c%c║[%c] %c%c%c\n"
                "    %c%c%c║    %c%c%c║    %c%c%c\n"
                "    %c%c%c║    %c%c%c║    %c%c%c\n"
                "=======================\n",
        big_transform(0),
        transform(0),
        transform(1),
        transform(2),
        big_transform(1),
        transform(3),
        transform(4),
        transform(5),
        big_transform(2),
        transform(6),
        transform(7),
        transform(8),
        transform(9),
        transform(10),
        transform(11),
        transform(12),
        transform(13),
        transform(14),
        transform(15),
        transform(16),
        transform(17),
        transform(18),
        transform(19),
        transform(20),
        transform(21),
        transform(22),
        transform(23),
        transform(24),
        transform(25),
        transform(26),
        big_transform(3),
        transform(27),
        transform(28),
        transform(29),
        big_transform(4),
        transform(30),
        transform(31),
        transform(32),
        big_transform(5),
        transform(33),
        transform(34),
        transform(35),
        transform(36),
        transform(37),
        transform(38),
        transform(39),
        transform(40),
        transform(41),
        transform(42),
        transform(43),
        transform(44),
        transform(45),
        transform(46),
        transform(47),
        transform(48),
        transform(49),
        transform(50),
        transform(51),
        transform(52),
        transform(53),
        big_transform(6),
        transform(54),
        transform(55),
        transform(56),
        big_transform(7),
        transform(57),
        transform(58),
        transform(59),
        big_transform(8),
        transform(60),
        transform(61),
        transform(62),
        transform(63),
        transform(64),
        transform(65),
        transform(66),
        transform(67),
        transform(68),
        transform(69),
        transform(70),
        transform(71),
        transform(72),
        transform(73),
        transform(74),
        transform(75),
        transform(76),
        transform(77),
        transform(78),
        transform(79),
        transform(80)
            );
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

inline uint16_t available_moves(big_board_t b)
{
    return 0b111111111 ^ (b[0] | b[1] | b[TIE]);
}

inline uint16_t available_moves(small_board_t b)
{
    return 0b111111111 ^ (b[0] | b[1]);
}

/* player is necessary because we must differentiate between a loss and a win, since a player can lose after his move */
inline Condition board_state(const big_board_t& b, bool player)
{
    if (is_win[b[0]] || is_win[b[1]]) return win;
    else if (available_moves(b)) return unfinished;
    const int p_w = __builtin_popcountl(b[player]);
    const int o_w = __builtin_popcountl(b[player ^ 1]);
    if (p_w > o_w) return win;
    else if (p_w < o_w) return loss;
    else return draw;
}

inline Condition board_state(const small_board_t& b)
{
    return is_win[b[0]] || is_win[b[1]] ? win : available_moves(b) ? unfinished
        : draw;
}

/* TODO change this for ultimate */
Condition rollout(State s)
{
    /* std::cout << "starting rollout()" << std::endl; */
    bool og_player = s.player;
    uint16_t move, sq;
    while (board_state(s.big_board, s.player) == unfinished) {
        s.player ^= 1;
        if (s.valid_square == 9 || board_state(s.small_board[s.valid_square]) != unfinished) {
            /* this isn't optimal but the alternative is supposedly slower */
            sq = random_bit(available_moves(s.big_board));
        } else {
            sq = s.valid_square;
        }
        move = random_bit(available_moves(s.small_board[sq]));
        s.small_board[sq][s.player] |= 1 << move;
        Condition post = board_state(s.small_board[sq]);
        if (post == win) { s.big_board[s.player] |= 1 << sq; }
        else if (post == draw) { s.big_board[TIE] |= 1 << sq; }
        s.valid_square = move;
    }
    Condition end = board_state(s.big_board, s.player);
    /* std::cout << "|-ending rollout()" << std::endl; */
    if (end == win) return s.player == og_player ? win : loss;
    else if (end == loss) return s.player != og_player ? win : loss;
    else return draw;
}

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

std::pair<int, int> get_move(std::shared_ptr<Node> n) {
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            if ((n->state.small_board[i][n->state.player] & (1 << j)) != (n->parent->state.small_board[i][n->state.player] & (1 << j))) {
                int R = i / 3;
                int C = i % 3;
                int r = j / 3;
                int c = j % 3;
                return std::make_pair(R * 3 + r, C * 3 + c);
            }
        }
    }
    return std::make_pair(-1, -1);
}

std::pair<uint16_t, uint16_t> transform_coords(int i, int j) {
    uint16_t r = i / 3;
    uint16_t c = j / 3;
    uint16_t sq = r * 3 + c;

    uint16_t sr = i % 3;
    uint16_t sc = j % 3;
    uint16_t mv = sr * 3 + sc;

    return std::make_pair(sq, mv);
}

std::shared_ptr<Node> get_child(std::shared_ptr<Node> n, int i, int j) {
    uint16_t r = i / 3;
    uint16_t c = j / 3;
    uint16_t sr = i % 3;
    uint16_t sc = j % 3;
    uint16_t sq = r * 3 + c;

    uint16_t mv = sr * 3 + sc;
    bool next_player = n->state.player ^ 1;
    for (auto c: n->children) {
        if ((c->state.small_board[sq][next_player] & (1 << mv)) != 0) {
            return c;
        }
    }
    std::printf("You dumbo\n");
    return nullptr;
}

void expand(std::shared_ptr<Node> n)
{
    /* std::cout << "starting expand()" << std::endl; */
    const State& s = n->state;
    if (board_state(s.big_board, s.player) != unfinished)
        return;
    bool next_player = s.player ^ 1;
    std::vector<uint16_t> valid_squares;
    if (s.valid_square == 9 || board_state(s.small_board[s.valid_square]) != unfinished) {
        for (int sq = 0; sq < 9; sq++) {
            if (board_state(s.small_board[sq]) == unfinished) {
                uint16_t moves = available_moves(s.small_board[sq]);
                for (int i = 0; i < 9; i++) {
                    if ((moves & (1 << i)) != 0) {
                        State new_state = s;
                        new_state.player = next_player;
                        new_state.small_board[sq][next_player] |= 1 << i;
                        Condition post = board_state(new_state.small_board[sq]);
                        if (post == win) { new_state.big_board[next_player] |= 1 << sq; }
                        else if (post == draw) { new_state.big_board[TIE] |= 1 << sq; }
                        new_state.valid_square = i;
                        n->children.push_back(std::make_shared<Node>(new_state, n));
                    }
                }
            }
        }
    } else {
        uint16_t sq = s.valid_square;
        uint16_t moves = available_moves(s.small_board[sq]);
        for (int i = 0; i < 9; i++) {
            if ((moves & (1 << i)) != 0) {
                State new_state = s;
                new_state.player = next_player;
                new_state.small_board[s.valid_square][next_player] |= 1 << i;
                Condition post = board_state(new_state.small_board[sq]);
                if (post == win) { new_state.big_board[next_player] |= 1 << sq; }
                else if (post == draw) { new_state.big_board[TIE] |= 1 << sq; }
                new_state.valid_square = i;
                n->children.push_back(std::make_shared<Node>(new_state, n));
            }
        }
    }
    /* std::cout << "|-ending expand()" << std::endl; */
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

std::shared_ptr<Node> mcts_next_move(std::shared_ptr<Node> root, int time_limit_ms)
{
    if (root->children.empty())
        expand(root);
    time_limit_ms -= 10;
    int c = 0;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    while (c % 16 != 0 || std::chrono::steady_clock::now() - start < std::chrono::milliseconds(time_limit_ms)) {
        c++;
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
    std::fprintf(stderr, "performed %d rollouts\n", c);
    /* for (auto c: root->children) { */
    /* 	show_board(c->state.boards[0], c->state.boards[1]); */
    /* 	std::printf("child with score[%lld]/visits[%lld]\n\n", c->score, c->visits); */
    /* } */
    std::shared_ptr<Node> b = root->children[0];
    for (int i = 1; i < root->children.size(); i++) {
        if (root->children[i]->visits > b->visits) {
            b = root->children[i];
        }
    }
    return b;
    /* return *std::max_element(root->children.begin(), root->children.end(), [](const auto a, const auto b) { return a->visits < b->visits; }); */
}

int main(int argc, char* argv[])
{
    /* int iterations = argc >= 2 ? std::stoi(argv[1]) : 200; */
    /* int time_limit_ms = argc >= 3 ? std::stoi(argv[2]) : 1000; */
    /* std::printf("iterations=%d time_limit_ms=%d\n", iterations, time_limit_ms); */

    std::printf("My node size is %d\n",(int)sizeof(Node));
    State s{};
    std::shared_ptr<Node> root = std::make_shared<Node>(s, nullptr);
    bool first_move = true;
    while (1) {
        int opp_row;
        int opp_col;
        std::cin >> opp_row >> opp_col; std::cin.ignore();

        if (opp_row != -1) {
            /* auto pos = transform_coords(opp_row, opp_col); */
            /* root->state.small_board[pos.first] |= 1 << pos.second; */
            /* root->state.valid_square = pos.second; */
            expand(root);
            root = get_child(root, opp_row, opp_col);
        }

        int validActionCount;
        std::cin >> validActionCount; std::cin.ignore();
        for (int i = 0; i < validActionCount; i++) {
            int row;
            int col;
            std::cin >> row >> col; std::cin.ignore();
        }

        std::shared_ptr<Node> next = mcts_next_move(root, first_move ? 1000 : 100);
        /* show_board(next->state); */
        auto mv = get_move(next);
        std::printf("%d %d\n", mv.first, mv.second);
        root = next;

        first_move = false;
    }


    /* while (board_state(root->state.big_board, root->state.player) == unfinished) { */
    /*     std::shared_ptr<Node> next = mcts_next_move(root, 100000, 2000); */
    /*     State new_state = next->state; */
    /*     show_board(new_state); */
    /*     std::printf("Chance of winning: %lld%% (score[%lld]/visits[%lld])\n", 100*next->score/next->visits, next->score, next->visits); */
    /*     /1* re-root tree at the current move *1/ */
    /*     root = next; */
    /* } */
}

// vim: ts=4 sts=4 sw=4
