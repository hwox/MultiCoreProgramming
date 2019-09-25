#include <chrono>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>
using namespace std;
using namespace std::chrono;


mutex mylock;
volatile int sum;
atomic <int> victim = 0;
atomic <bool> flag[2] = { false, false };


void plock(int myID) {
	int other = 1 - myID;
	flag[myID] = true;
	victim = myID;
	atomic_thread_fence(memory_order_seq_cst);
	while (flag[other] && victim == myID) {}
}
void punlock(int myID) {
	flag[myID] = false;
}


int x=0; // 0->Lock이 free다 (아무도 lock을 얻지 못함)
		// 1-> 누군가 lock을 얻어서 critical section을 실행중임

bool CAS(int *addr, int exp, int up)
{
	return atomic_compare_exchange_strong(reinterpret_cast<atomic_int *>(addr), &exp, up);
}

void CASLock()
{	
	while (!CAS(&x, 0, 1));
}
void CASUnlock()
{
	while (!CAS(&x, 1, 0));
}

void ThreadFunc2(int num_thread) {

	for (int i = 0; i < 50000000 / num_thread; ++i) {
		CASLock();
		sum += 2;
		CASUnlock();
	}

}



int main() {

	for (auto n = 1; n <= 16; n *= 2) {
		sum = 0;

		vector <thread> threads;
		auto start_time = high_resolution_clock::now();
		for (int i = 0; i < n; ++i) {
			threads.emplace_back(ThreadFunc2, n);
		}
		for (auto &th : threads) th.join();

		auto end_time = high_resolution_clock::now();
		auto exec_time = end_time - start_time;
		int exec_ms = duration_cast<milliseconds>(exec_time).count();
		cout << n << "Threads,  ";
		cout << "Sum = " << sum;
		cout << "  , Duration : " << exec_ms;
		cout << endl;
	}



	system("pause");
}