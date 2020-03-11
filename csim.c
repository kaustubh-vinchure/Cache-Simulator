// Kaustubh Vinchure
// vinchure

#define __STDC_FORMAT_MACROS
#define INT_MAX

#include <inttypes.h>
#include "cachelab.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>






// Cache lines
typedef struct 
{
	int s; // set bits
	int S; // num sets

	int E; // associativity

	int b; // block bits
	int B; // block size

	char * trace_file;
} cache_params;


typedef struct {
	int time;
	uint64_t address;
	char valid; 
} cache_line; 

// Cache Sets
typedef struct {
	cache_line ** lines;
	int num_lines;
} cache_set;

typedef struct {
	cache_set ** sets;
	int num_sets;
} cache;

typedef struct {
	uint64_t address;
	int size;
	char operation;
} event;

typedef struct 
{
	int hits;
	int misses;
	int evictions;
} stats;

// Functions
void build_params(int argc, char * argv[], cache_params * params);
cache * build_cache(cache_params * params);
void free_cache(cache * end_this);
void handle_cache(cache * ch, event * operation, stats * stat, cache_params * params, int time);

int main(int argc, char * argv[])
{

	cache_params * params = (cache_params *) malloc(sizeof(cache_params));
	build_params(argc, argv, params);
    cache * cache = build_cache(params);

    // Parse input file
    FILE * pFile = fopen(params->trace_file, "r");
   	char operation;
   	unsigned int address;
   	int size;

   	stats * st = malloc(sizeof(stats));
   	st->hits = 0;
   	st->misses = 0;
   	st->evictions = 0;

   	int time = 0;
   	while(fscanf(pFile, " %c %x,%d", &operation, &address, &size) > 0)
   	{
   		uint64_t addr = (uint64_t) address;
   		event * op = (event *) malloc(sizeof(event));
   		//printf("Address: %d \n", (int) addr);
   		op->address = addr;
   		op->size = size;
   		op->operation = operation;
   		if (operation == 'M' || operation == 'L' || operation == 'S')
   		{
   			//printf("operation: %c \n", operation);
   			handle_cache(cache, op, st, params, time);
   			// if (operation == 'M')
   			// {
	   		// 	handle_cache(cache, op, st, params, time);
   			// }

   		}
   		free(op);
   		time++;
   	}

   	//printf("time: %d \n", time);

   	fclose(pFile);
    free_cache(cache);
    free(params);
    printSummary(st->hits, st->misses, st->evictions);
   	free(st);

    return 0;
}




void handle_cache(cache * ch, event * ev, stats * st, cache_params * params, int time)
{
	uint64_t tag = (ev->address >> params->s) >> params->b;

	//printf("tag: %lld \n", (long long int) tag);
	int set_id = ((ev->address) << (64 - params->s - params->b)) >> (64 - params->s);
	//printf("set id: %d \n", set_id);

	cache_set * set = ch->sets[set_id];
	//printf("Location: %d, Set ID: %d", (int) ev->address, set_id);

	int num_lines = set->num_lines;

	int had_hit = 0;
	int had_evict = 1;
	int lru_time = time;
	int lru_idx = 0;

	// Check for hit
	for (int i = 0; i < num_lines; i++)
	{
		cache_line * line = set->lines[i];

		if (line->valid == 1 && line->address == tag)
		{
			// We have a hit
			line->time = time;
			had_hit = 1;
			had_evict = 0;
			break;
		}
		// we have a miss
		else if (line->valid == 0)
		{
			line->address = tag;
			line->time = time;
			line->valid = 1;
			had_evict = 0;
			break;
		}

		// find lru
		if (line->time < lru_time)
		{
			lru_time = line->time;
			lru_idx = i;
		}

		if (had_evict)
		{
			//printf("i: %d \n", i);
		}


	}
	if (had_evict == 1)
	{
		set->lines[lru_idx]->time = time;
		set->lines[lru_idx]->address = tag;
		set->lines[lru_idx]->valid = 1;
	}

	if (ev->operation == 'M')
	{
		if (had_hit)
		{
			st->hits += 2;
			//printf("hit hit \n");
		}
		else if (had_evict)
		{
			st->misses++;
			st->evictions++;
			st->hits++;
			//printf("miss eviction hit\n");
		}
		else
		{
			st->misses++;
			st->hits++;
			//printf("miss hit \n");
		}
	}
	else
	{
		if (had_hit)
		{
			st->hits++;
			//printf("hit\n");
		}
		else if(had_evict)
		{
			st->misses++;
			st->evictions++;

			//printf("evict \n");
		}
		else
		{
			st->misses++;
			//printf("miss \n");
		}
	}
	return;

}


void build_params(int argc, char * argv[], cache_params * params)
{
	int c;
	// Build from command line arguments
	while ((c = getopt(argc, argv, "s:E:b:t:")) != -1)
	{
		switch (c)
		{
			case 's':
				params->s = atoi(optarg);
				//printf("s: %d \n", params->s);
				break;

			case 'E':
				params->E = atoi(optarg);
				//printf("E: %d \n", params->E);
				break;

			case 'b':
				params->b = atoi(optarg);
				//printf("b: %d \n", params->b);
				break;
			case 't':
				params->trace_file = optarg;
				//printf("trace file: %s \n", params->trace_file);
				break;

			default:
				abort();
		}
	}
	params->S = 1 << params->s;
	params->B = 1 << params->b;


	return;
}

cache * build_cache(cache_params * params)
{
	// Allocate for each line first
	int num_sets = params->S;
	int num_lines = params->E;

	// Allocate cache
	cache * new_cache = (cache *) malloc(sizeof(cache));
	new_cache->num_sets = num_sets;

	// Array of cache_set *
	cache_set ** new_sets = (cache_set **) malloc(sizeof(cache_set *) * num_sets);

	// Build each set
	for (int i = 0; i < num_sets; i++)
	{
		cache_set * new_set = (cache_set *) malloc(sizeof(cache_set));
		new_set->num_lines = num_lines;
		
		// Array of cache_line *
		cache_line ** lines = (cache_line **) malloc(sizeof(cache_line *) * num_lines);

		// Build each line
		for (int j = 0; j < num_lines; j++)
		{
			cache_line * new_line = (cache_line *) malloc(sizeof(cache_line));
			new_line->time = 0;
			new_line->valid = 0;
			new_line->address = INT_MAX;

			lines[j] = new_line; 
		}

		new_set->lines = lines;
		new_sets[i] = new_set;

	}

	new_cache->sets = new_sets;
	return new_cache;

}


void free_cache(cache * end_this)
{
	int num_sets = end_this->num_sets;

	for (int i = 0; i < num_sets; i++)
	{
		int num_lines = end_this->sets[i]->num_lines;

		for (int j = 0; j < num_lines; j++)
		{
			free(end_this->sets[i]->lines[j]);
		}

		free(end_this->sets[i]->lines);
		free(end_this->sets[i]);
	}

	free(end_this->sets);
	free(end_this);

	return;
}

