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

constexpr unsigned int ST_EMPTY = 0;
constexpr unsigned int ST_WAIT = 0x40000000;
constexpr unsigned int ST_BUSY = 0x80000000;

class EXCHANGER {

	volatile int slot;

public:
	EXCHANGER() { slot = 0; }
	bool CAS(unsigned int old_status, int value, unsigned int new_status)
	{
		// CAS�� �� �׳� �ϸ� �ȵǰ� �ٸ� �� �ٲ����� �ȹٲ������ Ȯ���ϸ鼭 �ؾ� ��.
		int old_v = (slot & 0x3FFFFFFF) | old_status; // ������ �� ��Ʈ�� ����� 
		int new_v = (value & 0x3FFFFFFF) | new_status;

		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_int*>(&slot), &old_v, new_v);
	}
	int exchange(int value, bool *time_out)
	{
		*time_out = false;
		for (int i = 0; i < 100; i++) {
			while (true) {
				switch (slot && 0xC0000000) // slot�� �տ� �� ��Ʈ�� ���´ϱ� ���� 2��Ʈ�� ��󳻱�
				{
				case ST_EMPTY:
				{
					int cur_value = slot;
					if (true == CAS(ST_EMPTY, value, ST_WAIT)) {
						// �����ϸ� ��ٸ���
						//������ Ƚ����ŭ ���ƾ� �ϴϱ� for��
						for (int i = 0; i < 100; i++) {
							if ((slot && 0xC0000000) != ST_WAIT) {
								// �ٸ� �� ������ �ٲ����� ���������� ��ȯ�� �Ͼ ��
								int ret = slot & 0x3FFFFFFF;// �� ���� �� ��Ʈ�� �󤼤��ϱ� �װ� �����ϸ� �ȵ��ݾ�. 
								slot = 0;
								return ret;
							}
						}

						//	slot = 0; // �ƹ��� �ȿԴٰ� slot = 0�ϴ� �� ������ �ٸ� �� �� ���� �����ϱ� CAS����.
						if (true == CAS(ST_WAIT, 0, ST_EMPTY)) {
							//�����ϸ� time_out
							*time_out = true;
							return 0;
						}
						else {
							int ret = slot & 0x3FFFFFFF;
							slot = ST_EMPTY;
							return slot & 0x3FFFFFFF;
						}
					}
				}
				break;
				case ST_WAIT:
					for (int i = 0; i < 100; i++) {
						if (true == CAS(ST_WAIT, value, ST_BUSY)) {
							int ret = slot & 0x3FFFFFFF;// �� ���� �� ��Ʈ�� �󤼤��ϱ� �װ� �����ϸ� �ȵ��ݾ�. 
							slot = 0;
							return ret;
						}
					}

					break;
				case ST_BUSY:
					break;
				default:
					cout << "Invalid State! \n";
					while (true);
				}
			}

			*time_out = true;
			return 0;
		}
	}


};

constexpr int CAPACITY = 8;

class EL_ARRAY {

	int range;
	EXCHANGER exchanger[CAPACITY];

public:
	EL_ARRAY() {
		range = 0;
	}
	int visit(int value, bool *time_out)
	{
		int index = rand() % range;
		range += 1;
		return exchanger[index].exchange(value, time_out);
	}

	int range_change() {
		range -= 1;
	}

};

class LFELSTACK {
	NODE *volatile top;
	EL_ARRAY el_array;
public:
	LFELSTACK()
	{
		top = nullptr;
	}
	~LFELSTACK() {}

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
			bool time_out;
			int ret = el_array.visit(key, &time_out);
			if (false == time_out) {
				// true�̸� �ٽ� �õ������ ��
				// false�϶��� �����߳� ���� �Ѵ�.
				if (0 == ret) return; // ���������� ��ȯ�� ���̴ϱ� ��ȯ
			}
			// �ƴϸ� �ٽ� ����������

		}
	}
	int Pop() {

		while (true) {
			NODE* p = top;
			if (nullptr == p) { return 0; }
			NODE* next = p->next;
			int value = p->key; //cas�ϰ� �д°� �ǹ̾���. �̸� �о�־��Ѵ�.
			if (p != top) continue;
			if (true == CAS(&top, p, next)) {
				//delete first;
				return value;
			}
			bool time_out;
			int ret = el_array.visit(0, &time_out);
			if (false == time_out) {
				if (0 != ret) return ret; //�ƴϸ� ó������ �ٽ� 
			}
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


LFELSTACK my_stack;

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
