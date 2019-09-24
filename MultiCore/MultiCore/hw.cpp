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

const int n_size = 5;
volatile bool * volatile flag = new bool[5];
volatile int *volatile label = new int[5]; // 번호표 값 



void mlock(int m_ID) {

	int id = m_ID;
	flag[id] = true;

	for (int i = 0; i < n_size - 1; i++) {
		for (int j = i; j < n_size; j++) {
			if (label[i] <= label[j]) label[id] = label[j] + 1;
		}
	}
	for (int j = 0; j < m_ID; j++) {

		while (flag[j] && ((label[j] != 0 && (label[j], j)) < (label[id], id))) {};
	}
}

void munlock(int m_ID) {
	flag[m_ID] = false;
}
void do_work2(int num_thread, int myID) {

	for (int i = 0; i < 50000000 / num_thread; ++i) {
		mlock(myID);
		sum += 2;
		munlock(myID);
	}

}

int main() {

	for (int i = 0; i < 5; i++) {
		flag[i] = false;
		label[i] = 0;
	}

	for (int num_thread = 1; num_thread <= 16; num_thread *= 2)
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