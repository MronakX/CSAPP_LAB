#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

typedef struct
{
	int valid;
	int tag;
	int LRU_Counter;
}Cache_Line;

int hit_count = 0, miss_count = 0, evict_count = 0;
int verbose_flag = 0;
Cache_Line** cache_lines = NULL;
int s=0, E=0, b=0, t=0;
int S=0;
char* tracefile = NULL;

void myGetopt(int argc, char *argv[]);
void create_cache();
void get_input();
void read_bit(unsigned long addr, int size);
void delete_cache();

void myGetopt(int argc, char *argv[])
{
	int opt;
	while((opt = getopt(argc, argv, "-hvs:E:b:t:")) != -1)
	{
		switch(opt)
		{
			case 'h':
				//FOR HELP
				break;
			case 'v':
				verbose_flag = 1;
				break;
			case 's':
				s = atoi(optarg);
				S = 1<<s;	
				break;
			case 'E':
				E = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
				break;
			case 't':
				tracefile = optarg;
				break;
			default:
				printf("Wrong Args\n");	
				exit(0);
				break;
		}
	}
}

void create_cache()
{
	cache_lines = (Cache_Line**)malloc(sizeof(Cache_Line*) * S);
	for(int i=0; i<S; i++)
	{
		cache_lines[i] = (Cache_Line*)malloc(sizeof(Cache_Line) * E);
	}
	for(int i=0; i<S; i++)
	{
		for(int j=0; j<E; j++)
		{
			cache_lines[i][j].valid = 0;
			cache_lines[i][j].tag = -1;
			cache_lines[i][j].LRU_Counter = 0;
		}
	}
}

void get_input()
{
	char identifier;
	unsigned long addr;
	int size;
	FILE* pfile = fopen(tracefile, "r");
	if(!pfile)
	{
		fclose(pfile);
		printf("No Such File!\n");
		exit(0);
	}
	while(fscanf(pfile, " %c %lx,%d", &identifier, &addr, &size) > 0)
	{
		//printf("%c, %x, %d\n", identifier, addr, size);
		switch(identifier)
		{
			case 'I':
				break;
			case 'L':
				read_bit(addr, size);
				break;
			case 'S':
				read_bit(addr, size);
				//printf("update");
				break;
			case 'M':
				read_bit(addr, size);
				read_bit(addr, size);
				break;
			default:
				printf("Wrong Input!");
				exit(0);
		}
	}
	fclose(pfile);
}

void read_bit(unsigned long addr, int size)
{
	//S, E, b, T
	int S_num = (addr>>b) & (S-1);//set num
	int mask = 0;//0x00..ff
	for(int i=0;i<32-b-s;i++)
	{
		mask += (1<<i);
	}
	int tag = (addr >> b >> s);
	int tmp = -1;
	int E_num = -1;

	//printf("set = %x\n", S_num);
	//printf("tag = %x\n", tag);

	for(int i=0; i<E; i++)
	{
		if(cache_lines[S_num][i].tag == tag)//hit
		{
			hit_count++;
			E_num = i;
			//printf("hit\n");
			break;
		}
	}

	if(E_num == -1)
		for(int i=0; i<E; i++)
		{
			if(cache_lines[S_num][i].valid == 0)//miss && !evict
			{
				miss_count++;
				E_num = i;
				//printf("miss\n");
				break;
			}
		}

	if(E_num == -1)
	{
		for(int i=0; i<E; i++)
		{
			if((cache_lines[S_num][i].LRU_Counter > tmp))//evict
			{
				tmp = cache_lines[S_num][i].LRU_Counter;
				E_num = i;
			}
		}
		miss_count++;
		evict_count++;
		//printf("miss&&evict\n");
	}
	
	for(int i=0; i<S; i++)//update LRU
	{
		for(int j=0; j<E;j++)
		{
			cache_lines[i][j].LRU_Counter++;
		}
	}
	cache_lines[S_num][E_num].valid = 1;
	cache_lines[S_num][E_num].tag = tag;
	cache_lines[S_num][E_num].LRU_Counter = 0;
}

void delete_cache()
{
	for(int i=0; i<S; i++)
		free(cache_lines[i]);
	free(cache_lines);
}


int main(int argc, char *argv[])
{
	myGetopt(argc, argv);
	//printf("%d", S);
	create_cache();
	get_input();
	delete_cache();
	printSummary(hit_count, miss_count, evict_count);
	return 0;
}
