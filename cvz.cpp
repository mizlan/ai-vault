#undef _GLIBCXX_DEBUG
#pragma GCC optimize("Ofast", "unroll-loops", "omit-frame-pointer", "inline")
#pragma GCC option("arch=native,tune=native")
#pragma GCC target("movbe,pclmul,aes,rdrnd,bmi,bmi2,lzcnt,popcnt,avx,avx2,f16c,fma,sse3,ssse3,sse4.1,sse4.2")
#include <chrono>
#include <iostream>
#include <fstream>
#include <random>
#include <algorithm>
#include <string>
#include <x86intrin.h>
#include <array>

#ifdef chungus
std::ofstream BLOG("loggy");
#endif

namespace Color {
    enum Code {
        FG_RED      = 31,
        FG_GREEN    = 32,
        FG_BLUE     = 34,
        FG_DEFAULT  = 39,
        BG_RED      = 41,
        BG_GREEN    = 42,
        BG_BLUE     = 44,
        BG_DEFAULT  = 49
    };
    class Modifier {
        Code code;
        public:
        Modifier(Code pCode) : code(pCode) {}
        friend std::ostream&
            operator<<(std::ostream& os, const Modifier& mod) {
                return os << "\033[" << mod.code << "m";
            }
    };
}

Color::Modifier red(Color::FG_RED);
Color::Modifier green(Color::FG_GREEN);
Color::Modifier blue(Color::FG_BLUE);
Color::Modifier def(Color::FG_DEFAULT);

const int WIDTH = 16000;
const int HEIGHT = 9000;

using std::cerr;
using std::cin;
using std::cout;
using std::endl;

inline float rsqrt_fast(float x) { return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x))); }

std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

int randval(int n)
{
    return std::uniform_int_distribution<>(0, n - 1)(rng);
}

struct Unit {
    int id;
    int x;
    int y;
    bool active = 1; /* all units start off active, then are killed */
    int on_target = -1; /* -1 = not on target, otherwise index of human, only for zombies */
};

struct Strategy {
    int initial_moves = 0; /* 0-3 */
    int x[3], y[3];
    int result = 0;
};

struct GameState {
    Unit ash;
    int num_zombies;
    int cur_zombie = 0;
    std::array<Unit, 100> zombies;
    int num_humans;
    std::array<Unit, 100> humans;
};

inline int sq(int n)
{
    return n * n;
}

inline float sq(float n)
{
    return n * n;
}

inline int dist_sq(const Unit& a, const Unit& b)
{
    return sq(a.x - b.x) + sq(a.y - b.y);
}

/* returns if the move landed exactly on */
inline bool dmove(Unit& u, int x, int y, float dist)
{
    float dx = x - u.x, dy = y - u.y;
    float s = sq(dx) + sq(dy);
    if (sq(dist) >= s) {
        u.x = x;
        u.y = y;
        return true;
    }
    float inv_sq_dist = rsqrt_fast(s);
    u.x += (int)(dist * inv_sq_dist * dx);
    u.y += (int)(dist * inv_sq_dist * dy);
    return false;
};

inline bool dmove(Unit& u, Unit& target, float dist)
{
    return dmove(u, target.x, target.y, dist);
}

/* returns -1 if all humans die, otherwise the # of points */
/* mutates s */
int turn(int x, int y, GameState& s)
{
    /*
     * preliminary: check for alive humans (and track # of alive humans)
     */
    int num_humans_alive = 0;
    for (int i = 0; i < s.num_humans; i++) {
        if (s.humans[i].active)
            num_humans_alive++;
    }
    if (num_humans_alive == 0) {
        /* cerr << "dead bruh" << endl; */
        return -1;
    }

    /*
     * step 1. zombies move
     */
    for (int i = 0; i < s.num_zombies; i++) {
        if (!s.zombies[i].active)
            continue;

        /* -1 == ash */
        int target_idx = -1;
        int target_dist_sq = dist_sq(s.zombies[i], s.ash);
        for (int j = 0; j < s.num_humans; j++) {
            if (!s.humans[j].active)
                continue;
            int cand_dist_sq = dist_sq(s.zombies[i], s.humans[j]);
            if (cand_dist_sq < target_dist_sq) {
                target_idx = j;
                target_dist_sq = cand_dist_sq;
            }
        }

        Unit& target = target_idx == -1 ? s.ash : s.humans[target_idx];
        bool is_on = dmove(s.zombies[i], target, 400);

        /* if the zombie successfully latched on to a human, keep track of it */
        if (is_on && target_idx != -1) {
            s.zombies[i].on_target = target_idx;
        }
    }


    /*
     * step 2. ash moves
     */
    dmove(s.ash, x, y, 1000);


    /*
     * step 3. ash kills zombies
     */
    int points = 0;
    int fib_multiplier = 1;
    int sub_fib_multiplier = 2;
    for (int i = 0; i < s.num_zombies; i++) {
        if (s.zombies[i].active && dist_sq(s.zombies[i], s.ash) <= 4000000) {
            points += fib_multiplier * sq(num_humans_alive) * 10;

            /* perform fib update */
            int tmp = fib_multiplier;
            fib_multiplier = sub_fib_multiplier;
            sub_fib_multiplier += tmp;

            s.zombies[i].active = false;
        }
    }

    /*
     * step 4. zombies kill humans
     */
    for (int i = 0; i < s.num_zombies; i++) {
        if (s.zombies[i].active && s.zombies[i].on_target != -1 && s.humans[s.zombies[i].on_target].active) {
            s.humans[s.zombies[i].on_target].active = false;
            s.zombies[i].on_target = -1;
            num_humans_alive--;
        }
    }

#ifdef chungus
    BLOG << "newframe" << endl;
    BLOG << "circle " << s.ash.x << "," << s.ash.y << " " << 2000 << " #0000ff" << endl;
    BLOG << "circle " << s.ash.x << "," << s.ash.y << " " << 200 << " #0000AA" << endl;
    for (int i = 0; i < s.num_zombies; i++) if (s.zombies[i].active) BLOG << "circle " << s.zombies[i].x << "," << s.zombies[i].y << " " << 400 << " #ff0000" << endl;
    for (int i = 0; i < s.num_humans; i++) if (s.humans[i].active) BLOG << "circle " << s.humans[i].x << "," << s.humans[i].y << " " << 400 << " #00ff00" << endl;
#endif



    if (num_humans_alive > 0)
        return points;
    else {
        /* cerr << "dead #2" << endl; */
        return -1;
    }
}

/*
 * TODO
 * define an order in which to kill the zombies
 * - thoughts
 *   - shuffle s.zombies
 *   - have a int-ptr to the current zombie
 *   - once it's dead, advance to the next alive zombie
 */
Strategy playout(GameState s)
{
    shuffle(s.zombies.begin(), s.zombies.begin() + s.num_zombies, rng);
    int attempted_initial_moves = randval(4); /* 0-3 */
    /* int attempted_initial_moves = 1; */
    Strategy ret;
    for (int mv = 0; mv < attempted_initial_moves; mv++) {
        int x = randval(WIDTH);
        int y = randval(HEIGHT);
        int idx = ret.initial_moves++;
        ret.x[idx] = x;
        ret.y[idx] = y;

        /* while (s.ash.x != x && s.ash.y != y) { */
        /*     int pts = turn(x, y, s); */
        /*     if (pts == -1) { */
        /*         ret.result = 0; */
        /*         return ret; */
        /*     } else { */
        /*         ret.result += pts; */
        /*     } */
        /* } */

        int pts = turn(x, y, s);
        if (pts == -1) {
            ret.result = 0;
            return ret;
        } else {
            ret.result += pts;
        }
    }

    while (true) {
        while (s.cur_zombie < s.num_zombies && !s.zombies[s.cur_zombie].active) {
            s.cur_zombie++;
        }
        if (s.cur_zombie == s.num_zombies) {
            return ret;
        }

        Unit& target = s.zombies[s.cur_zombie];

        int pts = turn(target.x, target.y, s);

        if (pts == -1) {
            ret.result = 0;
            return ret;
        } else {
            ret.result += pts;
        }
    }
}

/* Unit a{-1, 36000, 8000, 1, 1}; */
/* dmove(a, 1000, 2000, 500); */
/* cout << a.x << " " << a.y << endl; */

int main()
{
#ifdef chungus
    BLOG << "dimensions 16000 9000" << endl;
    BLOG << "scale 0.1" << endl;
#endif
    int dump;
    while (1) {
        GameState g;
        cin >> g.ash.x >> g.ash.y;
        cin.ignore();
        cin >> g.num_humans;
        cin.ignore();
        /* g.humans = new Unit[g.num_humans]; */
        for (int i = 0; i < g.num_humans; i++) {
            cin >> g.humans[i].id >> g.humans[i].x >> g.humans[i].y;
            cin.ignore();
        }
        cin >> g.num_zombies;
        cin.ignore();
        /* g.zombies = new Unit[g.num_zombies]; */
        for (int i = 0; i < g.num_zombies; i++) {
            cin >> g.zombies[i].id >> g.zombies[i].x >> g.zombies[i].y >> dump >> dump;
            cin.ignore();
        }

        Strategy best;
        int c = 0;
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(90)) {
            c++;
            Strategy s = playout(g);
            if (s.result > best.result) {
                best = s;
            }
            /* break; */
        }
        cerr << "attempted " << c << " rollouts, best score = " << best.result << endl;
        if (best.initial_moves == 0) {
            /* cout << "CRASH" << endl; */
            cout << g.ash.x << " " << g.ash.y << endl;
        } else {
            cout << best.x[0] << " " << best.y[0] << endl;
        }
        /* break; */
    }
}

// vim: ts=4 sts=4 sw=4 et
