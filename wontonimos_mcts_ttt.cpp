#pragma GCC optimize("Ofast", "inline", "omit-frame-pointer", "unroll-loops")
#pragma GCC target("avx","popcnt","bmi2")

#include <random>
#include <iostream>

#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>
#include <chrono>

using namespace std;
using namespace chrono;

#define PLAYER_X 0
#define PLAYER_O 1

#define PlayerType uint8_t

#define undetermined 2
#define tie 3

#define outcome_t uint8_t

uint16_t randomBit(uint16_t availableMoves) { // to get a random move
    return __builtin_ctzl(__builtin_ia32_pdep_si(1UL << (rand() % __builtin_popcountl(availableMoves)), availableMoves));
}

class MiniBoard {
public:
    uint16_t player[2] = {0};

    bool is_win(PlayerType playerId) {
        static const __m128i v1 = _mm_set1_epi16(1);

        // pack all the winning board configs into 128 bits
        static const __m128i masks = _mm_setr_epi16(
            // minimal winning board configurations
            0b000000111,
            0b000111000,
            0b111000000,

            0b001001001,
            0b010010010,
            0b100100100,

            0b100010001,
            0b001010100);

        __m128i boards128 = _mm_set1_epi16(this->player[playerId]); // fill each 16 bits with board
        __m128i andResult = _mm_and_si128(masks, boards128);        // and board and mask
        __m128i result = _mm_cmpeq_epi16(andResult, masks);         // check if result equals mask
        return !_mm_test_all_zeros(result, v1);                     // not( test all results are zero )
    }

    uint16_t getAvailableMoves() {
        uint16_t taken = player[0] | player[1];
        return 0b111111111 ^ taken;
    }

    outcome_t calcWinState(PlayerType playerId) {
        if (is_win(playerId)) {
            return playerId;
        } else if (getAvailableMoves() == 0) {
            return tie;
        } else {
            return undetermined;
        }
    }

    outcome_t move(PlayerType playerId, uint16_t idx) {
        player[playerId] = player[playerId] | (0b1 << idx);
        return calcWinState(playerId);
    }

    outcome_t randomPlayout(PlayerType currentPlayer, int firstMove) {
        outcome_t status = move(currentPlayer, firstMove);
        while( status==undetermined ) {
            currentPlayer = 1 - currentPlayer;
            status = move(currentPlayer, randomBit(getAvailableMoves()));
        }
        return status;
    }

    uint16_t getBestMove(PlayerType playerId) {
        long moveScores[9] = { 0 };

        time_point<steady_clock> start_time;
        start_time = steady_clock::now();

        int num_of_moves = 0;
        int moves[9] = {0};
        uint16_t available = getAvailableMoves();
        for( int i = 0 ; i < 9 ; i++ ) {
            if ( (available & ( 0b1 << i )) != 0 ) {
                moves[num_of_moves] = i;
                num_of_moves++;
            }
        }

        long loop = 0;
        while(((steady_clock::now()-start_time).count() < 99000000 ))
        {
            for (int i = 0; i<num_of_moves; i++) {
                MiniBoard simulateBoard;
                this->clone(&simulateBoard);

                outcome_t status = simulateBoard.randomPlayout(playerId, moves[i]);
                if (status == playerId) {
                    moveScores[i] += 2;
                } else if (status == tie) {
                    moveScores[i] += 1;
                }
            }
            loop+=2;
        }
        cout << "Total moves: " << loop/2 <<endl;

        int bestIdx = moves[0];
        long bestScore = moveScores[0];
        cout << "move: " << moves[0] << " = " << 100*moveScores[0]/loop << " % win" << endl;
        for (int i = 1; i < num_of_moves; i++) {
            cout << "move: " << moves[i] << " = " << 100*moveScores[i]/loop << " % win" << endl;
            if (bestScore < moveScores[i]) {
                bestIdx = moves[i];
                bestScore = moveScores[i];
            }
        }
        return bestIdx;

    }

    void clone(MiniBoard *other) // to make a copy of this
    {
        other->player[0] = this->player[0];
        other->player[1] = this->player[1];
    }

};



int main()
{
    cout << "For a board set up like so" << endl;
    cout << "   X . .      0 1 2" << endl;
    cout << "   O . O      3 4 5" << endl;
    cout << "   X . .      6 7 8" << endl;

    MiniBoard board = MiniBoard();
    board.player[0] = 0b100000100;
    board.player[1] = 0b000101000;

    cout << endl;
    cout << "If X goes first" << endl;
 
    board.getBestMove(PLAYER_X);
 
    cout << endl;
    cout << "If O goes first" << endl;
    board.getBestMove(PLAYER_O);

    cout << endl;
    cout << "For an empty board" << endl;
    cout << "   . . .      0 1 2" << endl;
    cout << "   . . .      3 4 5" << endl;
    cout << "   . . .      6 7 8" << endl;
    cout << "We should see that the center (4) is the preferred move" << endl;
    cout << "And the corners (0,2,6,8) are second best" << endl;

    board.player[0] = 0b000000000;
    board.player[1] = 0b000000000;

    cout << endl;
    cout << "If X goes first" << endl;
 
    board.getBestMove(PLAYER_X);
 
    cout << endl;
    cout << "If O goes first" << endl;
    board.getBestMove(PLAYER_O);

}
