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

const double EXPLORATION_PARAMETER = 2;

using board_t = std::array<int8_t, 9>;

void raw_board(const board_t b) {
	for (int i: b) {
		std::printf("%d|", i);
	}
	std::printf("\n");
}

void show_board(const board_t b) {
	auto transform = [](const int8_t sq) { return sq == 0 ? '.' : sq == 1 ? 'o' : 'x'; };
	std::printf("===========\n"
			" %c ║ %c ║ %c \n"
			"═══╬═══╬═══\n"
			" %c ║ %c ║ %c \n"
			"═══╬═══╬═══\n"
			" %c ║ %c ║ %c \n"
			"===========\n",
			transform(b[0]),
			transform(b[1]),
			transform(b[2]),
			transform(b[3]),
			transform(b[4]),
			transform(b[5]),
			transform(b[6]),
			transform(b[7]),
			transform(b[8]));
}

enum Condition {
	player1_win, player2_win, draw, unfinished
};

struct State {
	/*
	 * 0 -> empty
	 * 1 -> player 1
	 * 2 -> player 2
	 */
	State(board_t b, int8_t p) : board(b), player(p) {}
	board_t board;
	/* the player who just went */
	int8_t player;
	std::vector<State> next_states() const {
		std::vector<State> ret;
		int8_t next_player = 3 - player;
		for (int i = 0; i < 9; i++) {
			if (board[i] == 0) {
				board_t copied_board = board;
				copied_board[i] = next_player;
				ret.emplace_back(copied_board, next_player);
			}
		}
		return ret;
	}
};

Condition board_state(const board_t& board) {
	constexpr std::array<std::array<int, 3>, 8> winning_configurations = {{{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}}};
	
	for (auto [a, b, c]: winning_configurations) {
		if (board[a] != 0 && (board[a] == board[b]) && (board[b] == board[c])) {
			return board[a] == 1 ? player1_win : player2_win;
		}
	}
	for (int sq: board) {
		if (sq == 0) {
			return unfinished;
		}
	}
	return draw;
}

bool terminal(const board_t& b) {
	Condition c = board_state(b);
	return c != unfinished;
}

Condition rollout(const State& s) {
	board_t b = s.board;
	uint8_t p = s.player;
	while (!terminal(b)) {
		p = 3 - p;
		std::vector<int> candidates;
		for (int i = 0; i < 9; i++) {
			if (b[i] == 0) {
				candidates.push_back(i);
			}
		}
		int idx = randval(candidates.size());
		b[candidates[idx]] = p;
	}
	/* raw_board(b); */
	return board_state(b);
}

struct Node {
	Node(State s, std::shared_ptr<Node> p): state(s), parent(p) { visits = 0; score = 0; }
	const State state;
	std::shared_ptr<Node> parent;
	int64_t visits;
	int64_t score;
	std::vector<std::shared_ptr<Node>> children;
};

void expand(std::shared_ptr<Node> n) {
	for (State s: n->state.next_states()) {
		n->children.push_back(std::make_shared<Node>(s, n));
	}
}

void backpropagate(std::shared_ptr<Node> n, Condition endstate) {
	std::shared_ptr<Node> nc = n;
	while (nc != nullptr) {
		nc->visits++;
		if (nc->state.player == 1 && endstate == player1_win) {
			nc->score++;
		} else if (nc->state.player == 2 && endstate == player2_win) {
			nc->score++;
		}
		else if (nc->state.player == 2 && endstate == player1_win) {
			nc->score--;
		} else if (nc->state.player == 1 && endstate == player2_win) {
			nc->score--;
		}
		nc = nc->parent;
	}
}

double UCB1(std::shared_ptr<Node> n) {
	double c = EXPLORATION_PARAMETER;
	if (n->visits == 0) {
		return std::numeric_limits<double>::infinity();
	}
	return ((double)(n->score) / n->visits) + c * sqrt(log(n->parent->visits) / n->visits);
}

std::shared_ptr<Node> select(std::shared_ptr<Node> n) {
	std::shared_ptr<Node> nc = n;
	while (!nc->children.empty()) {
		std::shared_ptr<Node> p = *std::max_element(nc->children.begin(), nc->children.end(),
				[](const auto a, const auto b) { return UCB1(a) < UCB1(b); });
		nc = p;
	}
	return nc;
}

std::shared_ptr<Node> mcts_next_move(State s) {
	std::shared_ptr<Node> root = std::make_shared<Node>(s, nullptr);
	expand(root);
	for (int i = 0; i < 80000; i++) {
		std::shared_ptr<Node> leaf = select(root);
		if (leaf->visits > 0) {
			expand(leaf);
			if (leaf->children.size() > 0) {
				int idx = randval(leaf->children.size());
				leaf = leaf->children[idx];
			}
		}
		Condition res = rollout(leaf->state);
		backpropagate(leaf, res);
	}
	/* for (auto c: root->children) { */
	/* 	std::printf("child with score[%lld]/visits[%lld]\n", c->score, c->visits); */
	/* 	show_board(c->state.board); */
	/* } */
	return *std::max_element(root->children.begin(), root->children.end(), [](const auto a, const auto b) { return a->visits < b->visits; });
}

int main() {
	board_t initial_board { {0, 0, 0, 0, 0, 0, 0, 0, 0} };
	State s(initial_board, 1);
	printf("%d\n", s.player);
	while (!terminal(s.board)) {
		State new_state = mcts_next_move(s)->state;
		show_board(new_state.board);
		s = new_state;
		int a;
	}
}
