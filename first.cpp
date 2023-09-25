#include<iostream>
#include<ctime>
#include<thread>
//For sleep() function
#include <windows.h>
#include<conio.h>
#include<mmsystem.h>
//multi-threading liabrary 
#include<pthread.h>

using namespace std;
static int alarm_index =0;
static int alarm_hour,alarm_min;
pthread_cond_t cond;
pthread_mutex_t mutex;
void sleep(int x)
{
	Sleep(x);
}

void clear_line(int n)
{
	for(int i=0;i<n;i++)
		cout<<endl;
}
void *printTime(void *arg)
{
	tm *ltm;
	do{
		time_t tm =time(0);
		ltm = localtime(&tm);
		//clear_line(25);
		if(alarm_index!=0){
			if(alarm_hour == ltm->tm_hour && alarm_min == ltm->tm_min){
				PlaySound("music1.wav",NULL,SND_FILENAME);
				cout<<"						"<<ltm->tm_hour<<":"<<ltm->tm_min<<":"<<ltm->tm_sec<<endl;
				alarm_index=0;
				//exit(0);
			}
			else
				cout<<"						"<<ltm->tm_hour<<":"<<ltm->tm_min<<":"<<ltm->tm_sec<<endl;
		}
		else
			cout<<"						"<<ltm->tm_hour<<":"<<ltm->tm_min<<":"<<ltm->tm_sec<<endl;
		//clear_line(25);
		sleep(1000);
	}while(1);
}
void *cargRet(void *arg)
{
	if(cin.get())
		exit(0);
	return NULL;
}

int main()
{
	pthread_t t1;
	pthread_t t2;

	if(alarm_index==0){
		cout<<"Do you wany to set alarm?"<<endl;
		char c = getchar();
		cin.ignore();
		if(c=='y'||c=='Y'){
			cout<<"Enter alarm time in Hour:";
			cin>>alarm_hour;
			cin.ignore();
			cout<<"Enter alarm time in Minute:";
			cin>>alarm_min;
			cin.ignore();
			alarm_index=1;
		}
	}
	pthread_create(&t1, 0, &printTime, NULL);
	pthread_create(&t2, 0, &cargRet, NULL);

	pthread_join(t1, 0);
	pthread_join(t2, 0);
	
	return 0;
}