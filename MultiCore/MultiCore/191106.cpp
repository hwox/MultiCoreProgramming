#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

using namespace std;
using namespace chrono;

class NODE {
public:
	int key;
	NODE* next;

	NODE() : key(0) { next = NULL; }

	NODE(int key_value) {
		next = NULL;
		key = key_value;
	}

	~NODE() {}
};

bool CAS(NODE* volatile* next, NODE* old_v, NODE* new_v) {
	return atomic_compare_exchange_strong(
		reinterpret_cast<atomic_int volatile*>(next),
		reinterpret_cast<int*>(&old_v),
		reinterpret_cast<int>(new_v)
	);
}
//bool STAMP_CAS(NODE *addr, NODE old_v, NODE new_v)
//{
//	new_v.stamp = old_v.stamp + 1;
//	return atomic_compare_exchange_strong(reinterpret_cast<atomic_llong *>(addr), reinterpret_cast<long long *>(&old_v),
//		*reinterpret_cast<long long *>(&new_v));
//}

class NQUEUE {
	NODE* volatile head;
	NODE* volatile tail;

public:
	NQUEUE() {
		head = tail = new NODE(0);
	}

	~NQUEUE() {}

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
		NODE* e = new NODE(key);
		while (true) {
			NODE* last = tail;
			NODE* next = last->next;

			if (last != tail) continue;
			if (nullptr == next) {
				if (CAS(&(last->next), nullptr, e)) {
					CAS(&tail, last, e);
					return;
				}
			}
			else CAS(&tail, last, next);
		}
	}

	int Deq() {
		int result{ 0 };

		while (true) {
			NODE* first = head;
			NODE* last = tail;
			NODE* next = first->next;
			NODE* lastnext = last->next;

			if (first != head) continue;

			if (first == last) {
				if (lastnext == nullptr) {
					cout << "EMPTY!!";

					this_thread::sleep_for(1ms);
					return -1;
				}
				if (nullptr == next) EMPTY_ERROR();
				CAS(&tail, last, next);
				continue;
			}

			int value = next->key;
			if (false == CAS(&head, first, next)) continue;
			delete first;
			return value;
		}
	}

	int EMPTY_ERROR() {
		cout << "EMPTY_QUEUE \n";
		return 0;
	}

	void display20() {
		int c = 20;
		NODE* p = head->next;

		while (p != nullptr) {
			cout << p->key;
			p = p->next;
			c--;
			if (c == 0) break;
			cout << ", ";
		}
		cout << endl;
	}
};

const auto NUM_TEST = 10000000;
const auto KEY_RANGE = 100;

NQUEUE nqueue;

void Exec25(int num_thread) {
	for (int i = 0; i < NUM_TEST / num_thread; ++i)
	{
		if (rand() % 2 == 0 || i < (100 / num_thread))
		{
			nqueue.Enq(i);
		}
		else {
			nqueue.Deq();
		}
	}
}

int main()
{
	for (int num_threads = 1; num_threads <= 16; num_threads *= 2)
	{
		nqueue.Init();
		vector<thread> threads;

		auto start_time = high_resolution_clock::now();

		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(Exec25, num_threads);

		for (auto& thread : threads)
			thread.join();

		threads.clear();

		auto end_time = high_resolution_clock::now();
		auto exec_time = end_time - start_time;
		int exec_ms = duration_cast<milliseconds>(exec_time).count();

		nqueue.display20();
		//nlist.recycle_freeList();
		cout << "Threads [" << num_threads << "] "
			<< " Exec_time  = " << exec_ms << "ms\n\n";
	}

	system("pause");
}