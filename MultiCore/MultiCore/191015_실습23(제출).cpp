
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

class LFNODE;

class MPTR {
	int value;

public:
	void set(LFNODE* node, bool removed) {
		value = reinterpret_cast<int>(node);
		if (true == value)
			value = value | 0x01;
		else
			value = value & 0xFFFFFFFE;
	}

	LFNODE* getptr() {
		return reinterpret_cast<LFNODE*>(value & 0xFFFFFFFE);
	}

	LFNODE* getptr(bool* removed) {
		int temp = value;
		if (0 == (temp & 0x1)) *removed = false;
		else *removed = true;
		return reinterpret_cast<LFNODE*>(temp & 0xFFFFFFFE);
	}

	bool CAS(LFNODE* old_node, LFNODE* new_node, bool old_removed, bool new_removed)
	{
		int old_value, new_value;

		old_value = reinterpret_cast<int>(old_node);
		if (true == old_removed) old_value = old_value | 0x01;
		else old_value = old_value & 0xFFFFFFFFE;

		new_value = reinterpret_cast<int>(new_node);
		if (true == old_removed) new_value = new_value | 0x01;
		else new_value = new_value & 0xFFFFFFFFE;

		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(&value), &old_value, new_value);
	}

	bool TryMarking(LFNODE* old_node, bool new_removed)
	{
		int old_value, new_value;
		old_value = reinterpret_cast<int>(old_node);
		old_value = old_value & 0xFFFFFFFFE;

		new_value = old_value;
		if (true == new_removed) new_value = new_value | 0x01;

		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(&value), &old_value, new_value);
	}
};

class LFNODE {
public:
	int key;
	MPTR next;
	//LFNODE* next;
	//LFNODE *next; 합쳐야함
	//bool marked;
	//mutex nlock;

	LFNODE() { next.set(nullptr, false); }

	LFNODE(int key_value) {
		next.set(nullptr, false);
		key = key_value;
	}

	~LFNODE() {}
};

//마킹 포인터 자료구조



class LFLIST {
	LFNODE head, tail;
	LFNODE* freelist;
	LFNODE freetail;
	mutex fl;
public:
	LFLIST()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next.set(&tail, false);
		freetail.key = 0x7FFFFFFF;
		freelist = &freetail;
	}
	~LFLIST() {}

	void Init()
	{
		LFNODE *ptr;
		while (head.next.getptr() != &tail) {
			ptr = head.next.getptr();
			head.next = head.next.getptr()->next;
			delete ptr;
		}
	}

	//따로 하지 않는다. 락 걸지 않으니까 
	//bool validate(NODE* pred, NODE* curr) {
	//   return !pred->marked && !curr->marked  &&pred->next == curr;
	//}

	void recycle_freelist()
	{
		LFNODE* p = freelist;
		while (p != &freetail)
		{
			LFNODE* n = p->next.getptr();
			delete p;
			p = n;
		}
		freelist = &freetail;
	}

	//이래야 변경했을때 값이 바뀐다. 포인터를 레퍼런스로 캐스팅할떄 이렇게 해야한다. 
	void find(int key, LFNODE* (&pred), LFNODE* (&curr))
	{
	retry:
		pred = &head;
		curr = pred->next.getptr();
		while (true)
		{
			bool removed;
			LFNODE* succ = curr->next.getptr(&removed);
			while (true == removed) {
				if (false == pred->next.CAS(curr, succ, false, false))
					goto retry; //두단계 빠져나가게 하는 명령어가 c++ 에 없으니까. 다르게 할수도 있지만 복잡해진다. 
				curr = succ;
				succ = curr->next.getptr(&removed);
			}

			if (curr->key >= key) return;

			pred = curr;
			curr = curr->next.getptr();
		}
	}

	bool Add(int key)
	{
		LFNODE *pred = NULL, *curr = NULL;

		while (true) {

			find(key, pred, curr);

			if (key == curr->key) {
				return false;
			}
			else {
				LFNODE *node = new LFNODE(key);
				node->next.set(curr, false);

				if (pred->next.CAS(curr, node, false, false))
					return true;
			}
		}
		// 끝나기 전에 unlock
	}
	bool Remove(int key)
	{
		LFNODE *pred, *curr;
		//pred = &head;
		//curr = pred->next.getptr();
		bool snip;
		while (true) {
			find(key, pred, curr);

			if (key != curr->key) {
				return false;
			}
			else {
				LFNODE* succ = curr->next.getptr();
				snip = curr->next.TryMarking(succ, true);
				if (!snip)
					continue;
				pred->next.CAS(curr, succ, false, false);
				return true;
			}
		}

	}
	bool Contains(int key)
	{
		LFNODE *pred, *curr;
		bool marked = false;
		curr = &head;
		while (curr->key < key)
		{
			curr = curr->next.getptr();
			LFNODE* succ = curr->next.getptr(&marked);
		}
		return curr->key == key && !marked;

	}

	void display20() {

		int c = 20;
		LFNODE *p = head.next.getptr();

		while (p != &tail) {
			cout << p->key << ", ";
			p = p->next.getptr();
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}


};



LFLIST LFlist;
void ThreadFunc(/*void *lpVoid,*/ int num_thread)
{
	int key;

	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		switch (rand() % 3) {
		case 0: key = rand() % KEY_RANGE;
			LFlist.Add(key);
			break;
		case 1: key = rand() % KEY_RANGE;
			LFlist.Remove(key);
			break;
		case 2: key = rand() % KEY_RANGE;
			LFlist.Contains(key);
			break;
		default: cout << "Error\n";
			exit(-1);
		}
	}
}

int main() {

	for (auto n = 1; n <= 8; n *= 2) {
		sum = 0;
		LFlist.Init();
		vector <thread> threads;
		auto start_time = high_resolution_clock::now();
		for (int i = 0; i < n; ++i) {
			threads.emplace_back(ThreadFunc, n);
		}
		for (auto &th : threads) th.join();

		auto end_time = high_resolution_clock::now();
		auto exec_time = end_time - start_time;
		int exec_ms = duration_cast<milliseconds>(exec_time).count();
		LFlist.display20();
		LFlist.recycle_freelist();
		cout << "Threads[ " << n << " ] , sum= " << sum;
		cout << ", Exec_time =" << exec_ms << " msecs\n";
		cout << endl;
	}

	system("pause");
}