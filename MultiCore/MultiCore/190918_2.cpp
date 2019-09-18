#include <thread>
#include <atomic>
#include <iostream>

using namespace std;

const int MAX = 50000000;
volatile int *b;
volatile bool done = false;
int error; // volatile로 할 필요가 없음. 공유 변수가 아니라서

void thread_1() {
	for (int i = 0; i < MAX; i++)
	{
		int t =  -(1 + *b);
		atomic_thread_fence(memory_order_seq_cst);
		*b = t;

	}
	done = true;
}

void thread_2() {
	while (false == done) {
		int temp = *b;
		if ((temp != 0) && (temp != -1)) {
			
			cout << hex << temp << " ";
			error++;

		}

	}
}

int main() {

	int a[64];
	int temp = reinterpret_cast<int>(&a[31]);
	temp = (temp / 64) * 64; // temp를 64의 배수로.
	temp = temp - 1;
	b = reinterpret_cast<int *>(temp);
	*b = 0;

	thread t1{ thread_1 };
	thread t2{ thread_2 };

	t1.join();
	t2.join();

	cout << "Number of Error = " << error << endl;

	system("pause");
}