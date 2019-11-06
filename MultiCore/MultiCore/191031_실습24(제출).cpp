#include <thread>
#include <iostream>
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>
#include <Windows.h>

using namespace std;
using namespace chrono;

const auto NUM_TEST = 10000000;
const auto KEY_RANGE = 1000000;

class NODE {
public:
	int key;
	NODE* next;

	NODE() { next = nullptr; }

	NODE(int key_value) {
		next = nullptr;
		key = key_value;
	}

	~NODE() {}

};
class CQUEUE {
	NODE *head, *tail;
	mutex glock;
public:
	CQUEUE() {
		head = tail = new NODE(0);
	}
	~CQUEUE() {}

	void Init() {
		NODE* ptr;
		while (head->next != nullptr) {
			ptr = head->next;
			head->next = head->next->next;
			delete ptr;
		}
		tail = head;
	}

	void Enq(int key) {
		glock.lock();
		NODE * e = new NODE(key);
		tail->next = e;
		tail = e;
		glock.unlock();
	}
	int Dnq() {
		int result{ 0 };

		glock.lock();

		if (head->next != nullptr)
		{
			result = head->next->key;
			head = head->next;
		}

		glock.unlock();

		return result;
	}

	void display20()
	{
		int c = 20;
		NODE* p = head->next;
		while (p != nullptr)
		{
			cout << p->key << ", ";
			p = p->next;
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}
};

CQUEUE myqueue;

void ThreadFunc(int num_thread)
{
	int key;
	for (int i = 0; i < 1000 / num_thread; i++) {
		key = rand() % KEY_RANGE;
		myqueue.Enq(i);
	}

	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		switch (rand() % 2) {
		case 0:
			myqueue.Enq(i);
			break;
		case 1:
			//key = rand() % KEY_RANGE;
			key = myqueue.Dnq();
			break;
		default:
			cout << "Error\n";
			exit(-1);
		}
	}
}



int main()
{
	for (int num_thread = 1; num_thread <= 16; num_thread *= 2)
	{
		vector<thread> threads;
		auto start_time = high_resolution_clock::now();
		for (int i = 0; i < num_thread; ++i) {
			threads.emplace_back(ThreadFunc, num_thread);
		}
		for (auto &th : threads)
			th.join();
		auto end_time = high_resolution_clock::now();
		threads.clear();
		auto exec_time = end_time - start_time;
		int exec_ms = duration_cast<milliseconds>(exec_time).count();
		myqueue.display20();
		cout << "Threads[" << num_thread << "] ";
		cout << ", Exec_time = " << exec_ms << "ms\n";
		cout << endl;
		myqueue.Init();
	}

	system("pause");
}