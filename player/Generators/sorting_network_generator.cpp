#include <cstdio>
#include <algorithm>
using namespace std;
int A[1000];
int tot;
void Swap(int a, int b, int x, int y) {
	if (b >= (x + (1 << (y - 1))))
		b = (x + (1 << (y - 1))) + (1 << (y - 1)) - (b - (x + (1 << (y - 1)))) - 1;
	if (a >= (x + (1 << (y - 1))))
		a = (x + (1 << (y - 1))) + (1 << (y - 1)) - (a - (x + (1 << (y - 1)))) - 1;
	if (a > b)
		swap(a, b);
	// printf("%d %d\n", a, b);
	
	if (a < 89 && b < 89) {
		tot += 1;
		// printf("swap(%d,%d);\n", a, b);
		printf("if (move_list[(%d)]<move_list[(%d)]) {sortable_move_t tmp = move_list[(%d)]; move_list[(%d)] = move_list[(%d)]; move_list[(%d)] = tmp;}\n", a, b, a, a, b, b);
	}
	if (A[a] > A[b])
		swap(A[a], A[b]);
}

void construct(int x, int y) {
	for (int i = y - 1; i >= 0; i--)
		for (int j = x; j < x + (1 << y); j += (1 << (i + 1)))
			for (int p = j; p < j + (1 << i); p++)
				Swap(p, p + (1 << i), x, y);
}

int main() {
	for (int i = 0; i < 128; i++) {
		A[i] = rand() % 10;
	}
	for (int i = 1; i <= 7; i++) {
		for (int j = 0; j < 128; j += (1 << i))
			construct(j, i);
	}
	// printf("%d\n", tot);
	// for (int i = 0; i < 128; i++)
	// 	printf("%d ", A[i]);
	// printf("\n");
}