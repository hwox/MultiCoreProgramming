#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace std;
using namespace chrono;

const auto NUM_TEST = 4000000;
const auto KEY_RANGE = 1000;
volatile int sum;


class NODE {
public:
	int key;
	NODE *next;
	mutex n_lock;

	NODE() { next = NULL; }

	NODE(int key_value) {
		next = NULL;
		key = key_value;
	}

	~NODE() {}

	void f_lock() {
		n_lock.lock();
	};

	void f_unlock() {
		n_lock.unlock();
	};
};

class FLIST {

	NODE head, tail;

public:
	FLIST()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}
	~FLIST() {}

	void Init()
	{
		NODE *ptr;
		while (head.next != &tail) {
			ptr = head.next;
			head.next = head.next->next;
			delete ptr;
		}
	}
	bool Add(int key)
	{
		NODE *pred = NULL, *curr = NULL;

		head.f_lock();
		pred = &head;
		curr = pred->next;
		curr->f_lock();
		while (curr->key < key) {
			pred->f_unlock();
			pred = curr;
			curr = curr->next;
			curr->f_lock();
		}

		if (key == curr->key) {
			curr->f_unlock();
			pred->f_unlock();
			return false;
		}
		else {
			NODE *node = new NODE(key);
			node->next = curr;
			pred->next = node;
			curr->f_unlock();
			pred->f_unlock();
			return true;
		}
	}
	bool Remove(int key)
	{
		NODE *pred = NULL, *curr = NULL;
		head.f_lock();
		pred = &head;
		curr = pred->next;
		curr->f_lock();
		while (curr->key < key) {
			pred->f_unlock();
			pred = curr;
			curr = curr->next;
			curr->f_lock();
		}

		if (key == curr->key) {
			pred->next = curr->next;
			curr->f_unlock();
			delete curr;
			pred->f_unlock();
			return true;
		}
		else {
			curr->f_unlock();
			pred->f_unlock();
			return false;
		}

	}
	bool Contains(int key)
	{
		NODE *pred = NULL, *curr = NULL;
		head.f_lock();
		pred = &head;
		curr = pred->next;
		curr->f_lock();
		while (curr->key < key) {
			pred->f_unlock();
			pred = curr;
			curr = curr->next;
			curr->f_lock();
		}
		if (key == curr->key) {
			curr->f_unlock();
			pred->f_unlock();
			return true;
		}
		else {
			curr->f_unlock();
			pred->f_unlock();
			return false;
		}
	}

	void display20()
	{
		int c = 20;
		NODE* p = head.next;
		while (p != &tail)
		{
			cout << p->key << ", ";
			p = p->next;
			c--;
			if (c == 0)
				break;
		}
		cout << endl;
	}
};

FLIST flist;

void ThreadFunc(int num_thread)
{
	int key;

	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		switch (rand() % 3) {
		case 0: key = rand() % KEY_RANGE;
			flist.Add(key); break;
		case 1: key = rand() % KEY_RANGE;
			flist.Remove(key);
			break;
		case 2: key = rand() % KEY_RANGE;
			flist.Contains(key); break;
		default: cout << "Error\n";
			exit(-1);
		}
	}
}


int main() {

	for (auto n = 1; n <= 16; n *= 2)
	{
		flist.Init();
		sum = 0;
		vector<thread> threads;

		auto start_time = high_resolution_clock::now();

		for (int i = 0; i < n; ++i)
			threads.emplace_back(ThreadFunc, n);

		for (auto &th : threads)
			th.join();

		auto end_time = high_resolution_clock::now();
		threads.clear();
		auto exec_time = end_time - start_time;

		int exec_ms = duration_cast<milliseconds>(exec_time).count();

		flist.display20();
		cout << "Threads[ " << n << " ] , sum= " << sum;
		cout << ", Exec_time =" << exec_ms << " msecs\n";
	}

	system("pause");

}