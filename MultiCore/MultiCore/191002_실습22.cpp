
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

volatile int sum;
class NODE {
public:
	int key;
	NODE *next;
	bool marked;
	mutex nlock;
	bool removed;
	NODE() { next = NULL; removed = false; }

	NODE(int key_value) {
		next = NULL;
		key = key_value;
		removed = false;
		marked = false;
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


class ZLIST {
	NODE head, tail;
	NODE* freelist;
	NODE freetail;
	mutex fl;
public:
	ZLIST()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
		freetail.key = 0x7FFFFFFF;
		freelist = &freetail;
	}
	~ZLIST() {}

	void Init()
	{
		NODE *ptr;
		while (head.next != &tail) {
			ptr = head.next;
			head.next = head.next->next;
			delete ptr;
		}
	}

	bool validate(NODE* pred, NODE* curr) {


		return !pred->marked && !curr->marked  &&pred->next == curr;
	}

	void recycle_freelist()
	{
		NODE* p = freelist;
		while (p != &freetail)
		{
			NODE* n = p->next;
			delete p;
			p = n;
		}
		freelist = &freetail;
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
			if (key == curr->key) {

				pred->next = curr->next;
				fl.lock();
				curr->next = freelist;
				freelist = curr;
				fl.unlock();
				pred->unlock();//
				curr->unlock();//
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
		curr = &head;
		while (curr->key < key)
		{
			curr = curr->next;
		}
		return curr->key == key && !curr->marked;

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

	for (auto n = 1; n <= 8; n *= 2) {
		sum = 0;
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
		clist.display20();
		clist.recycle_freelist();
		cout << "Threads[ " << n << " ] , sum= " << sum;
		cout << ", Exec_time =" << exec_ms << " msecs\n";
		cout << endl;
	}

	system("pause");
}