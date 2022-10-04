#include <array>
#include <vector>
#include <algorithm>

#include <atomic>
#include <mutex>
#include <thread>

#include <sstream>
#include <iostream>

#include <cstdint>

static constexpr const uint8_t BOARD_SIZE = 8;
static constexpr const uint8_t BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE;

using Move = uint8_t;
using Bitboard = uint64_t;

static constexpr const uint8_t hash_value_to_index[] = {
	 0,  1, 48,  2, 57, 49, 28,  3,
	61, 58, 50, 42, 38, 29, 17,  4,
	62, 55, 59, 36, 53, 51, 43, 22,
	45, 39, 33, 30, 24, 18, 12,  5,
	63, 47, 56, 27, 60, 41, 37, 16,
	54, 35, 52, 21, 44, 32, 23, 11,
	46, 26, 40, 15, 34, 20, 31, 10,
	25, 14, 19,  9, 13,  8,  7,  6
};

// Modified bit_scan_forward, originally by
// Charles E. Leiserson, Harald Prokop, Keith H. Randall
// http://supertech.csail.mit.edu/papers/debruijn.pdf
static inline uint8_t bit_scan_forward(const uint64_t x) {
	static constexpr const uint64_t debruijn_sequence = 0x03f79d71b4cb0a89;

	// assumption: x != 0
	return hash_value_to_index[((x & -x) * debruijn_sequence) >> 58];
}

// The value at knight_move_table[square] is a bitboard
// with every bit set that corresponds to a square
// that can be reached from the initial square.
// Generated with gen_knight_move_table.py.
static constexpr const Bitboard knight_move_table[] = {
	0x20400,                0x50800,                0xa1100,                0x142200,
	0x284400,               0x508800,               0xa01000,               0x402000,
	0x2040004,              0x5080008,              0xa110011,              0x14220022,
	0x28440044,             0x50880088,             0xa0100010,             0x40200020,
	0x204000402,            0x508000805,            0xa1100110a,            0x1422002214,
	0x2844004428,           0x5088008850,           0xa0100010a0,           0x4020002040,
	0x20400040200,          0x50800080500,          0xa1100110a00,          0x142200221400,
	0x284400442800,         0x508800885000,         0xa0100010a000,         0x402000204000,
	0x2040004020000,        0x5080008050000,        0xa1100110a0000,        0x14220022140000,
	0x28440044280000,       0x50880088500000,       0xa0100010a00000,       0x40200020400000,
	0x204000402000000,      0x508000805000000,      0xa1100110a000000,      0x1422002214000000,
	0x2844004428000000,     0x5088008850000000,     0xa0100010a0000000,     0x4020002040000000,
	0x400040200000000,      0x800080500000000,      0x1100110a00000000,     0x2200221400000000,
	0x4400442800000000,     0x8800885000000000,     0x100010a000000000,     0x2000204000000000,
	0x4020000000000,        0x8050000000000,        0x110a0000000000,       0x22140000000000,
	0x44280000000000,       0x88500000000000,       0x10a00000000000,       0x20400000000000
};

// The order of the squares results from the bitboard.
enum Square: uint8_t{
	H1, G1, F1, E1, D1, C1, B1, A1,
	H2, G2, F2, E2, D2, C2, B2, A2,
	H3, G3, F3, E3, D3, C3, B3, A3,
	H4, G4, F4, E4, D4, C4, B4, A4,
	H5, G5, F5, E5, D5, C5, B5, A5,
	H6, G6, F6, E6, D6, C6, B6, A6,
	H7, G7, F7, E7, D7, C7, B7, A7,
	H8, G8, F8, E8, D8, C8, B8, A8,

	SQUARES_END
};

static constexpr const char* const square_to_name[] = {
	"H1", "G1", "F1", "E1", "D1", "C1", "B1", "A1",
	"H2", "G2", "F2", "E2", "D2", "C2", "B2", "A2",
	"H3", "G3", "F3", "E3", "D3", "C3", "B3", "A3",
	"H4", "G4", "F4", "E4", "D4", "C4", "B4", "A4",
	"H5", "G5", "F5", "E5", "D5", "C5", "B5", "A5",
	"H6", "G6", "F6", "E6", "D6", "C6", "B6", "A6",
	"H7", "G7", "F7", "E7", "D7", "C7", "B7", "A7",
	"H8", "G8", "F8", "E8", "D8", "C8", "B8", "A8"
};

static inline bool query_bitboard(const Bitboard bitboard, const uint8_t pos)noexcept{
	return 0 != (bitboard & (static_cast<Bitboard>(1) << pos));
}

class Board{
	private:
		Bitboard m_bitboard;

	public:
		explicit Board()noexcept: m_bitboard{}{
		}

		inline Bitboard bitboard()const noexcept{
			return this->m_bitboard;
		}

		inline bool get(const uint8_t pos)noexcept{
			return query_bitboard(this->m_bitboard, pos);
		}

		inline void do_move(const Move move)noexcept{
			this->m_bitboard |= static_cast<Bitboard>(1) << move;
		}

		inline void undo_move(const Move move)noexcept{
			this->m_bitboard &= ~(static_cast<Bitboard>(1) << move);
		}
};

struct StackEntry{
	Move knight_pos;
	Bitboard move_field;

	StackEntry()noexcept: knight_pos{}, move_field{}{
	}
};

using Stack = std::array<StackEntry, BOARD_SQUARES + 1>;

static void output_tour(std::ostream& os, const Stack& stack)noexcept{
	static std::mutex output_mtx{};
	static std::atomic<uint64_t> tour_idx = 0;

	std::ostringstream oss{};
	oss << '[' << ++tour_idx << ", " << square_to_name[stack[0].knight_pos] << "]: ";
	for(uint8_t depth = 1; depth < BOARD_SQUARES; ++depth)
		oss << square_to_name[stack[depth].knight_pos] << ' ';

	std::lock_guard<std::mutex> lg{output_mtx};
	os << oss.str() << '\n';
}

static bool verify_tour(const Move init_move, const Stack& stack)noexcept{
	if(init_move != stack[0].knight_pos)
		return false;

	Board board{};
	board.do_move(init_move);

	uint8_t prev_move = init_move;
	for(uint8_t depth = 1; depth < BOARD_SQUARES; ++depth){
		const uint8_t pos_move = stack[depth].knight_pos;

		const bool new_square = !board.get(pos_move);
		if(!new_square){
			return false;
		}

		const bool valid_move = query_bitboard(
			knight_move_table[pos_move],
			prev_move
		);

		if(!valid_move){
			return false;
		}

		board.do_move(pos_move);
		prev_move = pos_move;
	}

	// Complete Tour?
	return init_move == prev_move;
}

static void gen_tour_iterative(const Move init_move)noexcept{
	Board board{};
	Stack stack{};

	board.do_move(init_move);
	stack[0].knight_pos = init_move;
	stack[0].move_field = knight_move_table[init_move];

	const uint64_t reachable_squares_from_init_move = stack[0].move_field;

	uint8_t depth = 0;	// depth \in [0; BOARD_SQUARES[
	static constexpr const uint8_t max_depth = BOARD_SQUARES - 1;

	while(true){
		// Open moves left?
		if(stack[depth].move_field){
			const Move next_move = bit_scan_forward(stack[depth].move_field);
			// Set the last set bit to zero.
			stack[depth].move_field &= stack[depth].move_field - 1;

			board.do_move(next_move);

			// Prepare next iteration.
			++depth;
			stack[depth].knight_pos = next_move;
			// Consider only valid moves to unvisited squares.
			stack[depth].move_field = knight_move_table[stack[depth].knight_pos] & ~board.bitboard();

			// Complete tour found?
			if(depth == max_depth){
				// The initial square is reachable from the last Square of the Tour?
				if(knight_move_table[next_move] & reachable_squares_from_init_move){
					output_tour(std::cout, stack);

					const bool valid_tour = verify_tour(init_move, stack);
					if(!valid_tour){
						std::cerr << "invalid_tour\n";
						return;
					}
				}
			}
		// Potentially unexamined moves above the current node?
		}else if(depth){
			board.undo_move(stack[depth].knight_pos);
			--depth;
		// Search completed.
		}else
			return;
	}
}

static void thread_loop(const Move init_move, const uint8_t every_n_th_move)noexcept{
	const Move end_move = BOARD_SQUARES;
	for(Move current_move = init_move; current_move < end_move; current_move += every_n_th_move){
		std::clog << "thread " << static_cast<uint32_t>(init_move)
				  << ": " << square_to_name[current_move] << '\n';

		gen_tour_iterative(current_move);
	}
}

static void start_multithreaded_search(const uint8_t num_threads)noexcept{
	std::vector<std::thread> thread_pool{};

	for(uint8_t thread_id = 0; thread_id < num_threads; ++thread_id){
		// Use the thread_id and num_threads variables
		// to distribute the squares among the threads.
		const Move init_move = thread_id;
		const uint8_t search_n_th_move = num_threads;

		thread_pool.emplace_back(&thread_loop, init_move, search_n_th_move);
	}

	for(auto& thread : thread_pool)
		thread.join();
}

int main(){
	// At least two threads.
	const uint8_t num_threads = std::max<uint8_t>(2, std::thread::hardware_concurrency());

	std::clog << "starting search with " << static_cast<uint32_t>(num_threads) << " threads...\n";
	start_multithreaded_search(num_threads);

	std::cout << "done.\n";
	return 0;
}
