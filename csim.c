/* Name: Manuel Takeshi Gomez
 * CS login: gomez
 * Section(s): CS 354 Spring 2018 
 *
 * csim.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions.  The replacement policy is LRU.
 *
 * Implementation and assumptions:
 *  1. Each load/store can cause at most one cache miss plus a possible eviction.
 *  2. Instruction loads (I) are ignored.
 *  3. Data modify (M) is treated as a load followed by a store to the same
 *  address. Hence, an M operation can result in two cache hits, or a miss and a
 *  hit plus a possible eviction.
 *
 * The function printSummary() is given to print output.
 * Please use this function to print the number of hits, misses and evictions.
 * This is crucial for the driver to evaluate your work. 
 */

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

/****************************************************************************/
/***** DO NOT MODIFY THESE VARIABLE NAMES ***********************************/

/* Globals set by command line args */
int s = 0; /* set index bits */
int E = 0; /* associativity */
int b = 0; /* block offset bits */
int verbosity = 0; /* print trace if set */
char* trace_file = NULL;

/* Derived from command line args */
int B; /* block size (bytes) B = 2^b */
int S; /* number of sets S = 2^s In C, you can use the left shift operator */

/* Counters used to record cache statistics */
int hit_cnt = 0;
int miss_cnt = 0;
int evict_cnt = 0;
/*****************************************************************************/


/* Type: Memory address 
 * Use this type whenever dealing with addresses or address masks
 */
typedef unsigned long long int mem_addr_t;

/* Type: Cache line
 * 
 * NOTE: 
 * You might (not necessarily though) want to add an extra field to this struct
 * depending on your implementation
 * 
 * For example, to use a linked list based LRU,
 * you might want to have a field "struct cache_line * next" in the struct 
 */
typedef struct cache_line {                     
    int valid;  
    mem_addr_t tag;
    struct cache_line * next;
    struct cache_line * prev;
} cache_line_t;

//Pointer to the set, then the double pointer to the line within the set.
typedef cache_line_t* cache_set_t;
typedef cache_set_t* cache_t;//TODO idk if the above commt is valid, but ill findout later.

//The cache we are simulating (a double pointer):
cache_t cache;  

/* 
 * initCache - TODO fixx all of this shitty broken code please. 
 * Allocate data structures to hold info regrading the sets and cache lines
 * use struct "cache_line_t" here
 * Initialize valid and tag field with 0s.
 * use S (= 2^s) and E while allocating the data structures here
 */
void initCache() {
  //cache represents a cache of S sets, E lines (2D array).                         
  S = pow(s, 2);
  //Now we have S set pointers:
  cache = malloc(sizeof(cache_set_t) * S);
    
  int i, j;  
  for (i = 0; i < S; i++) { 
    *(cache + i) = malloc(sizeof(cache_line_t) * E);
    for (j = 0; j < E; j++) {
      (*(*(cache + i) + j)).tag = 0;
      (*(*(cache + i) + j)).valid = 0;
      (*(*(cache + i) + j)).next = NULL; 
      (*(*(cache + i) + j)).prev = NULL; 
    } 
  }
}

/*  
 * freeCache - free each piece of memory you allocated using malloc 
 * inside initCache() function
 */
void freeCache() {                      
  //Free columns (each set holding columns), then free row (ptr to sets).
  int i; 
  for (i = 0; i < S; i++)
    free(*(cache + i));
  
  free(cache);
}

/* 
 * accessData - Access data at memory address addr.
 *   If it is already in cache, increase hit_cnt
 *   If it is not in cache, bring it in cache, increase miss count (cold miss).
 *   Also increase evict_cnt if a line is evicted (capacity or conflict miss).
 */
void accessData(mem_addr_t addr) {                      
  int linesTaken = 0;  
  mem_addr_t tbits = addr >> (s + b); 
  
  //Extracting the set index bits:
  int t = 64 - (b + s);
  mem_addr_t setIdx_2 = addr << t;
  mem_addr_t setIdx = setIdx_2 >> (t + b);
  
  //Various pointer to our linkedlist:
  cache_line_t *currLine; 
  cache_line_t *head;   
  cache_line_t *tail;  
  cache_line_t *newHead;
  cache_line_t *newTail;  
  
  //Initializing the head, tail, and curr pointers: 
  head = &cache[setIdx][0]; 
  tail = &cache[setIdx][E-1];
  currLine = &cache[setIdx][0];  

  //TODO 3: Test, use gdb, cry, rinse and repeat.
  //Iterating through the lines of respective set:
  while (currLine != NULL) {    
    //Cache hit:
    if (tbits == currLine->tag) { 
      if (E > 2) { 
 	if (currLine == tail) {
	  tail->next = head;
	  head->prev = tail;
	  newTail = tail->prev;
	  newTail->next = NULL;
	  tail->prev = NULL;
	  tail = newTail;
	  head = head->prev;
	}	
	else if (currLine != head && currLine != tail) {
	  (currLine->prev)->next = currLine->next;
	  (currLine->next)->prev = currLine->prev; 
	  currLine->next = head;
	  currLine->prev = NULL;
	  head->prev = currLine;
	  head = currLine;  
	}
      }
      else if (E == 2) {
	if (currLine == tail) {
	  tail->next = head;
	  head = head->next;
	  tail = tail->next;
	  tail->next = NULL;
	}
      }
      //Updating the cache:
      *(cache + setIdx) = head; //TODO learn if this works. 
      hit_cnt++;
    }
    
    //Cold miss:
    else if (currLine->valid == 0) {
      //Create the line with its data:
      currLine->tag = tbits;	
      currLine->valid = 1;
      
      if (E > 2) {
        if (currLine == tail) {
	  tail->next = head;
	  head->prev = tail;
	  newTail = tail->prev;
	  newTail->next = NULL;
	  tail->prev = NULL;
	  tail = newTail;
	  head = head->prev;
        }
	else if (currLine == head) {
	  head->next = (cache_line_t *)currLine+1; //TODO learn if this works.
          (head->next)->prev = head;
	}
	else {
	  prevNode = currLine->prev;
	  prevNode->next = (cache_line_t *)currLine+1;
	  (prevNode->next)->prev = prevNode;
	  currLine->next = head;
	  currLine->prev = NULL;
	  head = currLine;
	}
      }
      if (E == 2) {
	if (currLine == tail) {
          tail->next = head;
	  head->prev = tail;
	  newTail = tail->prev;
	  newTail->next = NULL;
	  tail->prev = NULL;
	  tail = newTail;
	  head = head->prev;
	}
	else {
          head->next = (cache_line_t *)currLine+1;
          (head->next)->prev = head;
	}
      }
      *(cache + setIdx) = head;  
      miss_cnt++;
      break;  
    }
    
    //Increase the chance for an eviction:
    else {
      takenLines++;
    }
    currLine = currLine->next;  
  }
  
  //TODO 4: Fix and optimize the cache eviction case
  //TODO 5: Test eviction case, cry, gdb, etc...
  //
  //Cache eviction:
  if (linesTaken == E) {
    if (E > 1) {
      //Replace old data with new data:
      tail->tag = tbits;  
      
      tail->next = head;
      head->prev = tail;
      newTail = tail->prev;
      newTail->next = NULL;
      tail->prev = NULL;
      tail = newTail;
      head = head->prev;
    }
    else { 
      head->tag = tbits;
    }
    *(cache + setIdx) = head;
    //Evictions also infer cache misses: 
    evict_cnt++;
    miss_cnt++;
  }
}


/* 
 * replayTrace - replays the given trace file against the cache 

 * reads the input trace file line by line
 * extracts the type of each memory access : L/S/M
 * YOU MUST TRANSLATE one "L" as a load i.e. 1 memory access
 * YOU MUST TRANSLATE one "S" as a store i.e. 1 memory access
 * YOU MUST TRANSLATE one "M" as a load followed by a store i.e. 2 memory accesses 
 */
void replayTrace(char* trace_fn) {                      
    char buf[1000];
    mem_addr_t addr = 0;  
    unsigned int len = 0;
    FILE* trace_fp = fopen(trace_fn, "r");

    if (!trace_fp) {
        fprintf(stderr, "%s: %s\n", trace_fn, strerror(errno));
        exit(1);
    }

    while (fgets(buf, 1000, trace_fp) != NULL) {
        if (buf[1] == 'S' || buf[1] == 'L' || buf[1] == 'M') {
            sscanf(buf+3, "%llx,%u", &addr, &len);  //Initializing the addr
      	
           if (verbosity)
                printf("%c %llx,%u ", buf[1], addr, len);
	    
	    //Access memory once for either a load or a store to memory.
	    //Data storing writes data to mem (eviction++, miss++, or hit++).
	    //Data loading reads data from mem (miss++ or hit++).   
	    if (buf[1] == 'S' || buf[1] == 'L')
	      accessData(addr); 
	    if (buf[1] == 'M') {
	      accessData(addr); //load 	      
	      accessData(addr); //then store.
	    }
            if (verbosity)
              printf("\n");
        }
    }

    fclose(trace_fp);
}

/*
 * printUsage - Print usage info
 */
void printUsage(char* argv[]) {                 
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
    exit(0);
}

/*
 * printSummary - Summarize the cache simulation statistics. Student cache simulators
 *                must call this function in order to be properly autograded.
 */
void printSummary(int hits, int misses, int evictions) {                        
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
    FILE* output_fp = fopen(".csim_results", "w");
    assert(output_fp);
    fprintf(output_fp, "%d %d %d\n", hits, misses, evictions);
    fclose(output_fp);
}

/*
 * main - Main routine 
 */
int main(int argc, char* argv[]) {                      
    char c;
    
    // Parse the command line arguments: -h, -v, -s, -E, -b, -t 
    while ((c = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch (c) {
            case 'b':
                b = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'h':
                printUsage(argv);
                exit(0);
            case 's':
                s = atoi(optarg);
                break;
            case 't':
                trace_file = optarg;
                break;
            case 'v':
                verbosity = 1;
                break;
            default:
                printUsage(argv);
                exit(1);
        }
    }

    /* Make sure that all required command line args were specified */
    if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        printUsage(argv);
        exit(1);
    }

    /* Initialize cache */
    initCache();

    replayTrace(trace_file);

    /* Free allocated memory */
    freeCache();

    /* Output the hit and miss statistics for the autograder */
    printSummary(hit_cnt, miss_cnt, evict_cnt);
    return 0;
}
