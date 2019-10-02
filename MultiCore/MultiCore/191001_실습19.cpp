//#include <chrono>
//#include <stdio.h>
//#include <iostream>
//#include <thread>
//#include <mutex>
//#include <vector>
//#include <atomic>
//
//using namespace std;
//using namespace std::chrono;
//
//
//const auto NUM_TEST = 4000000;
//const auto KEY_RANGE = 1000;
//
//class NODE {
//public:
//	int key; NODE *next;
//
//	NODE() { next = NULL; }
//
//	NODE(int key_value) {
//		next = NULL;
//		key = key_value;
//	}
//
//	~NODE() {}
//};
//
//class nullmutex {
//
//	// mutex를빼려면?
//
//	// 싱글스레드는 잘 돌아가는데 멀티스레드는 돌다가 죽는다. 
//	
//public:
//	void lock(){}
//	void unlock() {
//
//	}
//
//};
//
//class CLIST {
//	NODE head, tail;
//	mutex glock;
//public:
//	CLIST()
//	{
//		head.key = 0x80000000; tail.key = 0x7FFFFFFF; head.next = &tail;
//	}
//	~CLIST() {}
//
//	void Init()
//	{
//		NODE *ptr;
//		while (head.next != &tail) {
//			ptr = head.next;
//			head.next = head.next->next;
//			delete ptr;
//		}
//	}
//	bool Add(int key)
//	{
//		NODE *pred, *curr;
//
//		pred = &head;
//		glock.lock();
//		curr = pred->next;
//		while (curr->key < key) {
//			pred = curr;
//			curr = curr->next;
//		}
//
//		if (key == curr->key) {
//			glock.unlock(); return false;
//		}
//		else {
//			NODE *node = new NODE(key); 
//			node->next = curr;
//			pred->next = node;
//			glock.unlock();
//			return true;
//		}
//
//	}
//	bool Remove(int key)
//	{
//		NODE *pred, *curr;
//
//		pred = &head;
//		glock.lock();
//		curr = pred->next;
//		while (curr->key < key) {
//			pred = curr;
//			curr = curr->next;
//		}
//
//		if (key == curr->key)
//		{
//			pred->next =
//				curr->next;
//			delete curr; 
//			glock.unlock();
//			return true;
//		}
//		else {
//			glock.unlock(); return false;
//		}
//
//	}
//	bool Contains(int key)
//	{
//		NODE *pred, *curr;
//
//		pred = &head;
//		glock.lock();
//		curr = pred->next;
//		while (curr->key < key) {
//			pred = curr;
//			curr = curr->next;
//		}
//		if (key == curr->key) {
//			glock.unlock(); return true;
//		}
//		else {
//			glock.unlock(); return false;
//		}
//
//	}
//
//	void display20() {
//	
//		int c = 20;
//		NODE *p = head.next;
//	
//		while(p != &tail) {
//			cout << p->key << ", ";
//			p = p->next;
//			c--;
//			if (c == 0) break;
//		}
//		cout << endl;
//	}
//};
//
//
//
//CLIST clist;
//void ThreadFunc(/*void *lpVoid,*/ int num_thread)
//{
//	int key;
//
//	for (int i = 0; i < NUM_TEST / num_thread; i++) {
//		switch (rand() % 3) {
//		case 0: key = rand() % KEY_RANGE;
//			clist.Add(key); 
//			break;
//		case 1: key = rand() % KEY_RANGE;
//			clist.Remove(key);
//			break;
//		case 2: key = rand() % KEY_RANGE;
//			clist.Contains(key); 
//			break;
//		default: cout << "Error\n";
//			exit(-1);
//		}
//	}
//}
//
//int main() {
//
//	for (auto n = 1; n <= 16; n *= 2) {
//		clist.Init();
//		vector <thread> threads;
//		auto start_time = high_resolution_clock::now();
//		for (int i = 0; i < n; ++i) {
//			threads.emplace_back(ThreadFunc, n);
//		}
//		for (auto &th : threads) th.join();
//
//		auto end_time = high_resolution_clock::now();
//		auto exec_time = end_time - start_time;
//		int exec_ms = duration_cast<milliseconds>(exec_time).count();
//		cout << n << " Threads,  ";
//		clist.display20();
//		//cout << "Sum = " << sum;
//		cout << " Duration : " << exec_ms;
//		cout << endl;
//	}
//
//	system("pause");
//}