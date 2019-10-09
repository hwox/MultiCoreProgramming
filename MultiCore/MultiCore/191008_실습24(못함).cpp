#include <chrono>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>

using namespace std;
using namespace std::chrono;


const auto NUM_TEST = 40000;
const auto KEY_RANGE = 1000;

class SPNODE {
public:
	int key;
	shared_ptr<SPNODE> next;
	mutex nlock;
	bool marked;

	SPNODE() { next = nullptr; marked = false; }

	SPNODE(int key_value) {
		next = nullptr;
		key = key_value;
		marked = false;
	}
	void lock() {
		nlock.lock();

	}
	void unlock()
	{
		nlock.unlock();
	}

	~SPNODE() {}
};

class nullmutex {

	// mutex를빼려면?

	// 싱글스레드는 잘 돌아가는데 멀티스레드는 돌다가 죽는다. 

public:
	void lock() {}
	void unlock() {

	}

};

class ZLIST {
	shared_ptr<SPNODE> head, tail;
public:
	ZLIST()
	{
		head = make_shared<SPNODE>();
		tail = make_shared<SPNODE>();
		head->key = 0x80000000;
		tail->key = 0x7FFFFFFF;
		head->next = tail;
	}
	~ZLIST() {}

	void Init()
	{
		head->next = tail;
	}

	bool validate(shared_ptr<SPNODE> pred, shared_ptr<SPNODE> curr)
	{
		return !pred->marked && !curr->marked && pred->next == curr;
	}

	void recycle_freelist()
	{
		return;
	}
	bool Add(int key)
	{
		shared_ptr<SPNODE> pred, curr;
		//	pred = head;
		//	curr = pred->next;
		while (true) {
			while (curr->key < key) {
				pred = curr;
				curr = curr->next;

			}
			pred->lock(); curr->lock();
			if (validate(pred, curr) == false) {
				curr->unlock();
				pred->unlock();
				continue;
			}
			else {
				shared_ptr<SPNODE> node;
				node->next = curr;
				pred->next = node;
				curr->unlock();
				pred->unlock();
				return true;
			}

		}


	}
	bool Remove(int key)
	{
		shared_ptr<SPNODE> pred, curr;
		pred = make_shared<SPNODE>();
		curr = make_shared<SPNODE>();
		pred = head;
		curr = pred->next;
		while (curr->key < key) {
			pred = curr;
			curr = curr->next;
		}

		if (key == curr->key)
		{
			pred->next = curr->next;
			//delete curr;
			return true;
		}
		else {
			return false;
		}

	}
	bool Contains(int key)
	{
		shared_ptr<SPNODE> pred, curr;
		pred = make_shared<SPNODE>();
		curr = make_shared<SPNODE>();
		curr = head;
		while (curr->key < key) {
			curr = curr->next;
		}
		return curr->key == key && !curr->marked;
	}

	void display20() {

		int c = 20;
		shared_ptr<SPNODE> p;
		p = make_shared<SPNODE>();
		p = head->next;

		while (p != tail) {
			cout << p->key << ", ";
			p = p->next;
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}
};



ZLIST clist;
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

	for (auto n = 1; n <= 16; n *= 2) {
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
		clist.recycle_freelist();
		cout << " Duration : " << exec_ms;
		cout << endl;
	}

	system("pause");
}