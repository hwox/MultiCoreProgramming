
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
	void set(LFNODE *node, bool removed) {
		value = reinterpret_cast<int>(node);
		if (true == removed) {
			value = value | 0x01;
		}
		else {
			value = value & 0xFFFFFFFE; // 다 1로 하고 마지막만 0으로
		}
	}

	LFNODE *getptr() {
		// 마킹을 빼고 주소만 리턴해주기
		return reinterpret_cast<LFNODE *>(value & 0xFFFFFFFE);
	}

	LFNODE *getptr(bool *removed) {
		int temp = value;
		if (0 == (temp & 0x1)) *removed = false;
		else *removed = true;

		// value를 atomic하게 얻었기 때문에 여기서는 value를 하면 안되고
		// temp로 검사해야 한대ㅐ
		return reinterpret_cast<LFNODE *>(temp & 0xFFFFFFFE);
	}
};

class LFNODE {
public:
	int key;
	MPTR next;


	LFNODE() { next.set(nullptr, false); }

	LFNODE(int key_value) {
		next.set(nullptr, false);
		key = key_value;
	}
	~LFNODE() {}

	// lock, unlock은 필요가없음
	//void lock() {
	//	nlock.lock();
	//}
	//void unlock()
	//{
	//	nlock.unlock();
	//}
};


class LFIST {
	LFNODE head, tail;
	LFNODE* freelist;
	LFNODE freetail;
	mutex fl;
public:
	LFIST()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		//head.next = &tail; 이렇게 하면 안된다.
		head.next.set(&tail, false); // 이렇게 해야 돼

		freetail.key = 0x7FFFFFFF;
		freelist = &freetail;
	}
	~LFIST() {}

	void Init()
	{
		LFNODE *ptr;
		while (head.next.getptr() != &tail) {
			ptr = head.next.getptr();
			head.next = head.next.getptr()->next;
			delete ptr;
		}
	}

	// 따로 하지 않는다. 의미가 없음. 삭제
	//bool validate(LFNODE* pred, LFNODE* curr) {
	//	return !pred->marked && !curr->marked  &&pred->next == curr;
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

	void find(int key, LFNODE *(&pred), LFNODE *(&curr)) {
		retry:
		pred = &head;
		curr = pred->next.getptr();
		while (true)
		{
			bool removed;
			LFNODE *succ = curr->next.getptr(&removed);
			while (true == removed)
			{
				//	pred->next.set(succ); 이렇게 그냥 바꾸면 다른 쓰레드가 접근할 수 있으니까 CAS사용ㅎ나다.
				if (false == pred->next.CAS(curr, succ, false, false)) {
					goto retry; // 왜 break하지 않을까? break하면 얘만 빠져 나가니까 (지금 while (true == removed)) 
					// goto 안쓰고 할 수도 있는데 그러면 너무 복잡해진다. 
				}
				curr = succ;

			}
			if (curr->key >= key) return;
			//curr이 마킹이 돼있는지 안돼있는지 검사를 해야 한다.
			pred = curr;
			curr = curr->next.getptr();
		}
	}

	bool Add(int key)
	{
		LFNODE *pred, *curr;

		while (true) {
			find(key, pred, curr);

			if (key == curr->key) {
				curr->unlock();
				pred->unlock();
			}
			else {
				LFNODE *node = new LFNODE(key);
				node->next = curr;
				pred->next = node;
				curr->unlock();
				pred->unlock();
				return true;
			}
		}

	}
	// 끝나기 전에 unlock

	bool Remove(int key)
	{
		LFNODE *pred, *curr;
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
		LFNODE *pred, *curr;

		curr = &head;
		while (curr->key < key)
		{
			curr = curr->next;
		}
		return curr->key == key && !curr->marked;

	}

	void display20() {

		int c = 20;
		LFNODE *p = head.next;

		while (p != &tail) {
			cout << p->key << ", ";
			p = p->next;
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}


};



LFIST clist;
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