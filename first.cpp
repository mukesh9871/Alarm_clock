#include<iostream>
#include<ctime>
//For sleep() function
#include <windows.h>
#include<conio.h>
//multi-threading liabrary 
#include<pthread.h>

using namespace std;
static int alarm_index =0;
static int alarm_hour,alarm_min;
pthread_cond_t cond;

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
	if(alarm_index==0){
		cout<<"Do you wany to set alarm?"<<endl;
		pthread_cond_wait(&cond,0);
		if(getchar()=='y'||getchar()=='Y'){
			cout<<"Enter alarm time in Hour:";
			cin>>alarm_hour;
			cin.ignore();
			cout<<"Enter alarm time in Minute:";
			cin>>alarm_min;
			alarm_index=1;
		}
		pthread_cond_signal(&cond);
	}		
	tm *ltm;
	do{
		time_t tm =time(0);
		ltm = localtime(&tm);
		clear_line(25);
		if(alarm_index!=0){
			if(alarm_hour == ltm->tm_hour && alarm_min == ltm->tm_min){
				cout<<"BOOOOOOOOOOOOOM Wake up"<<endl;
				cout<<"						"<<ltm->tm_hour<<":"<<ltm->tm_min<<":"<<ltm->tm_sec<<endl;
			}
		}
		else
			cout<<"						"<<ltm->tm_hour<<":"<<ltm->tm_min<<":"<<ltm->tm_sec<<endl;
		clear_line(25);
		sleep(50);
	}while(1);
}
void *cargRet(void *arg)
{
	if(cin.get())
		exit(0);
}
int main()
{
	pthread_t t1;
	pthread_t t2;
	pthread_cond_init(&cond,0);
	pthread_create(&t1,0,&printTime,NULL);
	pthread_create(&t2,0,&cargRet,NULL);

	pthread_join(t1,0);
	pthread_join(t2,0);
	
	pthread_cond_destroy(&cond);
	
	return 0;
}