
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
	NODE *freelist;  // 추가 
	NODE freetail;
	mutex f_mutex;
public:
	CLIST()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
		freetail.key = 0x7FFFFFF;
		freelist = &freetail;
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

		////////////////////////////////////
		// 여기를 위해 새로 판 cpp파일.
		// continue를 사용?
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
		/*while (true) {
			pred = curr;
			curr = curr->next;
			while (curr->key < key) {
				pred->lock();
				curr->lock();
				if (validate(pred, curr)) {
					if (key == curr->key) {
						curr->unlock();
						pred->unlock();
						continue;
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
			}
		}*/

		//curr->unlock();
		//pred->unlock();
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
				//  // 추가 
				// 그냥 remove하면서 나몰라라 하지 말고
				pred->next = curr->next;
				f_mutex.lock();
				curr->next = freelist;
				freelist = curr;
				f_mutex.unlock();
				pred->unlock();//
				curr->unlock();//
				//delete curr;
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

	// 추가 
	void recycle_freelist() {
		NODE *p = freelist;
		// 멀티스레드에서 freelist를 하면 서로 데이터를 덮어쓰면서 데이터 레이스가 존재할 수 있다.
		// 그래서 함수를 만들어서 따로 넣어야 하는데 그냥 freelist mutex를 넣는다
		while (p != &freetail) {
			// 맨 마지막에 들어간 노드가 null을 ㅏㄱ리켜야 되는데 아무거나 가지고 왔으니까
			// linked list의 노드를 가리킨다. 그래서 freelist도 보초노드가 하나 있어야 한다.
			NODE *n = p->next;
			delete p;
			p = n;
		}
		freelist = &freetail;
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

		// 얘는 멀티스레드가 아닌 싱글스레드에서 돌아갈 때 호출을 해줘야 하므로
		// main에서 호출ㄴ
		clist.recycle_freelist();
		cout << " Duration : " << exec_ms;
		cout << endl;
	}

	system("pause");
}