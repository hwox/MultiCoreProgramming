#include <thread>
#include <iostream>

using namespace std;

// ppt 2  번째의 3번

volatile int message;
volatile bool ready = false;


void recv() 
{
	while (false == ready);
	cout << " I got " << message << endl;
}

void send() {
	message = 999;
	ready = true;
}
int main() {
	
	thread reciever{ recv };
	thread sender{ send };

	reciever.join();
	sender.join();

	system("pause");

}