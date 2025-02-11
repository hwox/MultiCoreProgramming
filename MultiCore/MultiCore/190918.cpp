#include <thread>
#include <atomic>
#include <iostream>

using namespace std;

const int MAX = 50000000;
volatile int x, y;
volatile int trace_x[MAX];
volatile int trace_y[MAX];


void thread_x() {
	for (int i = 0; i < MAX; i++)
	{
		x = i;
		atomic_thread_fence(memory_order_seq_cst);
		trace_y[x] = y;
	}
}

void thread_y() {
	for (int i = 0; i < MAX; i++)
	{
		y = i;
		atomic_thread_fence(memory_order_seq_cst);
		trace_x[y] = x;
	}
}

int main() {
	int count = 0;

	thread t1{ thread_x };
	thread t2{ thread_y };

	t1.join();
	t2.join();

	for (int i = 0; i < (MAX-1); ++i)
	{
		if (trace_x[i] != trace_x[i + 1]) continue;
		int x = trace_x[i];
		if (trace_y[x] != trace_y[x + 1]) continue;
		if (trace_y[x] == i) //이러면 두개의 스레드의 메모리 읽고쓰는 순서가 다른것.
			count++;

	}
	cout << "Number of Error = " << count << endl; // 메모리 읽고 쓰기가 몇 번이나 오류가 났는지
	system("pause");
}