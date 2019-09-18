#include <chrono>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>


using namespace std;
using namespace chrono;

mutex mylock;
volatile int sum;


volatile int victim = 0;
volatile bool flag[2] = { false, false };

void plock(int myID) {
	int other = 1 - myID;
	flag[myID] = true;
	atomic_thread_fence(memory_order_seq_cst);
	victim = myID;
	while (flag[other] && victim == myID) {}
}
void punlock(int myID) {
	flag[myID] = false;
}

void do_work2(int num_thread, int myID) {

	for (int i = 0; i < 50000000 / num_thread; ++i) {
		plock(myID);
		sum += 2;
		punlock(myID);
	}

}

int main() {

	for (int num_thread = 1; num_thread <= 2; num_thread *= 2)
	{
		sum = 0;
		vector <thread> threads;
		auto start_time = high_resolution_clock::now();

		for (int i = 0; i < num_thread; ++i) {
			threads.emplace_back(do_work2, num_thread, i);
		}

		for (auto &th : threads) th.join();

		auto end_time = high_resolution_clock::now();
		threads.clear();
		auto exec_time = end_time - start_time;

		int exec_ms = duration_cast<milliseconds>(exec_time).count();
		cout << "Threads [" << num_thread << "] , sum= " << sum << endl;
		cout << ", Exec_time =" << exec_ms << " msecs\n";
	}
	system("pause");
}