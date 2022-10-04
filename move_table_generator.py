def main():
	table = [[] for _ in range(64)]

	corner_moves = [0, 7, 63, 56]
	options = [(-2, -1), (-2, 1), (-1, -2), (-1, 2), (1, -2), (1, 2), (2, -1), (2, 1)]
	for x in range(8):
		for y in range(8):
			for option in options:
				move_x = x + option[0]
				move_y = y + option[1]
				if move_x >= 0 and move_x <= 7 and move_y >= 0 and move_y <= 7:
					start_square = x + y * 8
					move = move_x + move_y * 8
					table[start_square] += [move]

	print('static constexpr const Bitboard knight_attack_table[64] = {')
	for square_entry in table:
		val = 0
		for move in square_entry:
			val = val | (1 << move)

		print('\t0x{:x},'.format(val))

	print('};')

if __name__ == '__main__':
	main()