#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>

void ThreadFunc(int threadNum);
using namespace std;
using namespace std::chrono;
const auto NUM_TEST = 10000000;
const auto KEY_RANGE = 1000;

class NODE {
public:
	int key;
	NODE *next;

	NODE() { next = nullptr; }
	NODE(int key_value) {
		next = nullptr;
		key = key_value;
	}
	~NODE() {}
};


class BACKOFF {
public:
	int limit;
	const int minDelay = 10;
	const int maxDelay = 100000;
	BACKOFF() {
		limit = minDelay;
	}
	void wait()
	{
		int delay = 1 + (rand() % limit);
		if (delay == 0) return;
		limit = limit + limit;
		if (limit > maxDelay) limit = maxDelay;

		_asm mov eax, delay;
	myloop:
		_asm dec eax
		_asm jnz myloop;
	}
};

class CSTACK {
	NODE *top;
	mutex glock;
public:
	CSTACK()
	{
		top = nullptr;
	}
	~CSTACK() {
		Init();
	}

	void Init()
	{
		NODE *ptr;
		while (top != nullptr)
		{
			ptr = top;
			top = top->next;
			delete ptr;
		}
	}
	void Push(int key)
	{
		NODE *e = new NODE(key);
		glock.lock();
		e->next = top;
		top = e;
		glock.unlock();
	}
	int Pop()
	{
		glock.lock();

		if (nullptr == top)
		{

			glock.unlock();
			return 0;
		}
		int result = top->key;
		NODE *temp = top;
		top = top->next;
		glock.unlock();
		delete temp;
		return result;
	}

	void display20()
	{
		int c = 20;
		NODE *p = top->next;
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

class LFSTACK {
	NODE* top;
	mutex glock;
public:
	LFSTACK()
	{
		top = nullptr;
	}
	~LFSTACK() {}

	void Init()
	{
		NODE *ptr;
		while (top != nullptr)
		{
			ptr = top;
			top = top->next;
			delete ptr;
		}
	}
	bool CAS(NODE * volatile * addr, NODE *old_node, NODE *new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int *>(addr),
			reinterpret_cast<int *>(&old_node),
			reinterpret_cast<int>(new_node));
	}

	void Push(int key)
	{
		NODE* e = new NODE(key);
		while (true) {
			NODE* last = top;
			if (last != top) continue;
			e->next = last;
			// top�� top�̸� e�� �ٲٱ�? �ٸ� �����尡 �ͼ� 
			// top�� �� ������ top�� �� ������ ��������? 
			// �׷��� top�� ���� ������ ��带 ����Ű�� ��. ���ϸ� ������ �߸��Ǹ� ���
			// CAS�� �� �׷��� last�� ������ ���� ������ CAS�� �ϴ� ��
			// �����ϸ� return �����ϸ� ó������ �ٽ� �д°�
			if (CAS(&top, last, e)) {
				return;
			}
		}
	}
	int Pop()
	{
		while (true) {
			//if (top == nullptr) return 0;
			//top�� null���� ���� �ȵǰ� top�� �����س��� �°� null���� ���� �Ѵ�.
			// ��? �� ���߿� ������ �������鼭 �ٲ� ���� �����ϱ�
			NODE* first = top;
			if (first == nullptr) return 0;
			NODE* last = first->next;
			int value = first->key;
			if (first != top)
				continue;

			if (false == CAS(&top, first, last))
				continue;
			return value;
		}
		return 0;
	}

	void display20()
	{
		int c = 20;
		NODE *p = top->next;
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

class LFBOSTACK {
	NODE* top;
	mutex glock;
public:
	LFBOSTACK()
	{
		top = nullptr;
	}
	~LFBOSTACK() {}

	void Init()
	{
		NODE *ptr;
		while (top != nullptr)
		{
			ptr = top;
			top = top->next;
			delete ptr;
		}
	}
	bool CAS(NODE * volatile * addr, NODE *old_node, NODE *new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int *>(addr),
			reinterpret_cast<int *>(&old_node),
			reinterpret_cast<int>(new_node));
	}

	void Push(int key)
	{
		// �������� �� �׳� �����µ� �����ϸ�?
		BACKOFF bo;
		NODE* e = new NODE(key);
		while (true) {
			NODE* last = top;
			if (last != top) continue;
			e->next = last;

			if (CAS(&top, last, e)) {
				return;
			}
			bo.wait(); // ȣ������� ��. �����ϸ�
		}
	}
	int Pop()
	{
		BACKOFF bo;
		while (true) {

			NODE* first = top;
			if (first == nullptr) return 0;
			NODE* last = first->next;
			int value = first->key;
			if (first != top)
				continue;

			if (false == CAS(&top, first, last))
				continue;
			bo.wait();
			return value;

		}
	}

	void display20()
	{
		int c = 20;
		NODE *p = top->next;
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
LFBOSTACK my_stack;

int main()
{
	for (auto n = 1; n <= 16; n *= 2) {
		my_stack.Init();
		vector <thread> threads;
		auto s = high_resolution_clock::now();
		for (int i = 0; i < n; ++i)
			threads.emplace_back(ThreadFunc, n);
		for (auto &th : threads) th.join();
		//my_stack.recycle_freelist();
		auto d = high_resolution_clock::now() - s;
		my_stack.display20();
		cout << n << "Threads,  ";
		cout << ",  Duration : " << duration_cast<milliseconds>(d).count() << " msecs.\n";
	}
}

void ThreadFunc(int num_thread)
{
	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		if ((rand() % 2 == 0) || 10000 / num_thread) {
			my_stack.Push(i);
		}
		else {
			int key = my_stack.Pop();
		}
	}
}
