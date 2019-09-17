#include <chrono>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>

//volatile int sum;

using namespace std;
using namespace chrono;

mutex mylock;
volatile int sum;

void do_work(int num_thread) {
	volatile int local_sum = 0;
	for (int i = 0; i < 50000000 / num_thread ; ++i) {
		//mylock.lock();
		local_sum += 2;
		//mylock.unlock();

		//_asm lock add sum, 2;
	}
	mylock.lock();
	sum += local_sum;
		
	mylock.unlock();
}

int main() {

	for (int num_thread = 1; num_thread <= 16; num_thread *= 2)
	{
		sum = 0;

		// �̰� ���ϰ� �׳� ������? ���������� �ż� �׳� ��� ������
		vector <thread> threads;
		auto start_time = high_resolution_clock::now();

		for (int i = 0; i < num_thread; ++i) {
			// �����忡 ���ϰ��̶�� �� ����
			threads.emplace_back(do_work, num_thread);
		}

		for (auto &th : threads) th.join();

		auto end_time = high_resolution_clock::now();
		threads.clear();
		auto exec_time = end_time - start_time;

		int exec_ms = duration_cast<milliseconds>(exec_time).count();
		cout << "Threads ["<< num_thread<< "] , sum= " << sum << endl;
		cout << ", Exec_time =" << exec_ms << " msecs\n";
	}
	//system("pause");
}