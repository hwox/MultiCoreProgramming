
#include <chrono>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>

using namespace std;
using namespace std::chrono;


const auto NUM_TEST = 4000000;
const auto KEY_RANGE = 1000;

class NODE {
public:
	int key;
	NODE *next;

	mutex nlock;
	NODE() { next = NULL; }

	NODE(int key_value) {
		next = NULL;
		key = key_value;
	}
	void lock() {
		nlock.lock();

	}
	void unlock()
	{
		nlock.unlock();
	}

	~NODE() {}
};


class CLIST {
	NODE head, tail;
public:
	CLIST()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}
	~CLIST() {}

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
		NODE *pred, *curr;
		pred = &head;
		curr = pred->next;
		while (curr->key < key) {
			pred = curr;
			curr = curr->next;
		}
		pred->lock();
		curr->lock();
		if (validate(pred, curr)) {
			if (key == curr->key) {
				curr->unlock();
				pred->unlock();
				return false;
			}
			else {
				NODE *node = new NODE(key);
				node->next = curr;
				pred->next = node;
				curr->unlock();
				pred->unlock();
				return true;
			}
		}
		curr->unlock();
		pred->unlock();
		// 끝나기 전에 unlock
	}
	bool Remove(int key)
	{
		NODE *pred, *curr;
		pred = &head;
		curr = pred->next;
		while (curr->key < key) {
			pred = curr;
			curr = curr->next;
		}

		pred->lock();
		curr->lock();
		if (validate(pred, curr)) {
			if (key == curr->key)
			{
				pred->next = curr->next;
				pred->unlock();
				curr->unlock();
				return true;
			}
			else {
				pred->unlock();
				curr->unlock();
				return false;
			}
		}
		curr->unlock();
		pred->unlock();
	}
	bool Contains(int key)
	{
		NODE *pred, *curr;
		pred = &head;
		curr = pred->next;
		while (curr->key < key) {
			pred = curr;
			curr = curr->next;
		}
		pred->lock();
		curr->lock();
		if (validate(pred, curr)) {
			pred->unlock();
			curr->unlock();
			return key == curr->key;
		}
		curr->unlock();
		pred->unlock();
	}

	void display20() {

		int c = 20;
		NODE *p = head.next;

		while (p != &tail) {
			cout << p->key << ", ";
			p = p->next;
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}

	bool validate(NODE* pred, NODE* curr) {
		NODE *m_node = &head;
		while (m_node->key <= pred->key) {
			if (m_node == pred) {
				return pred->next == curr;
			}
			m_node = m_node->next;
		}
		return false;
	}
};



CLIST clist;
void ThreadFunc(/*void *lpVoid,*/ int num_thread)
{
	int key;

	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		switch (rand() % 3) {
		case 0: key = rand() % KEY_RANGE;
			clist.Add(key);
			break;
		case 1: key = rand() % KEY_RANGE;
			clist.Remove(key);
			break;
		case 2: key = rand() % KEY_RANGE;
			clist.Contains(key);
			break;
		default: cout << "Error\n";
			exit(-1);
		}
	}
}

int main() {

	for (auto n = 1; n <= 8; n *= 2) {
		clist.Init();
		vector <thread> threads;
		auto start_time = high_resolution_clock::now();
		for (int i = 0; i < n; ++i) {
			threads.emplace_back(ThreadFunc, n);
		}
		for (auto &th : threads) th.join();

		auto end_time = high_resolution_clock::now();
		auto exec_time = end_time - start_time;
		int exec_ms = duration_cast<milliseconds>(exec_time).count();
		cout << n << " Threads,  ";
		clist.display20();
		//cout << "Sum = " << sum;
		cout << " Duration : " << exec_ms;
		cout << endl;
	}

	system("pause");
}