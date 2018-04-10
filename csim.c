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
    char valid;
    mem_addr_t tag;
    struct cache_line * next; //cache_line pointer to next cache_line in linked-list
} cache_line_t;

//Other representations of our structure above.
typedef cache_line_t* cache_set_t;
typedef cache_set_t* cache_t;

/* The cache we are simulating (a double pointer)*/
cache_t cache;  

/* 
 * initCache - 
 * Allocate data structures to hold info regrading the sets and cache lines
 * use struct "cache_line_t" here
 * Initialize valid and tag field with 0s.
 * use S (= 2^s) and E while allocating the data structures here
 */
void initCache() {
  /*cache represents a cache of S sets, E lines (2D array).*/                         
  S = pow(s, 2);
  cache = malloc(sizeof(cache_set_t) * S);
  
  int i, j;  
  for (i = 0; i < S; i++) { 
    *(cache + i) = malloc(sizeof(cache_line_t) * E);
    for (j = 0; j < E; j++) {
      (*(*(cache + i) + j)).tag = 0;
      (*(*(cache + i) + j)).valid = 0;
      (*(*(cache + i) + j)).next = NULL; //No lines are set, next is nulled.
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
 *   you will manipulate data structures allocated in initCache() here
 */
void accessData(mem_addr_t addr) {                      
  int j;          //Loop counter.
  int takenline;  //If all lines are taken, then do an eviction.
  mem_addr_t tbits = addr >> (s + b); //Get last t bits of addr.
  mem_addr_t setIdx = addr << tbits;  //Get the set to index into of cache.
  setIdx = setIdx >> (tbits + b); 
  
  cache_line_t *lineptr;  //A pointer to access the lines of sets.
  cache_line_t *head;     //A pointer to head of linked list.
  cache_line_t *tail;     //A pointer to tail of linked list.
  cache_line_t *newHead;  //Backup pointer to head of list.
  cache_line_t *newTail;  //Backup pointer to tail of list.
 
  lineptr = *(cache + setIdx);  //Points to head node of E lines in set setIdx.
  while (lineptr != NULL) {     //Break loop when lineptr goes off list.
    //Increment the number of busy lines to do a possible LRU evict.
    if (lineptr == NULL) 
      printf("OUR LINEPTR IS NULL. ERROR.\n");
    else 
	printf("Ok, the lineptr isnt null\n");
    if ((lineptr->valid) == 1) //FIXME GETTING A SFAULT ON THIS LINE. 
      takenline++;
    //We have a hit:
    if (tbits == lineptr->tag && lineptr->valid == 1) { 
      if (E > 1) { 
        //Hit on current line, current line becomes tail.
        newTail = lineptr;
        tail->next = newTail;
        head->next = newTail->next;
        newTail->next = NULL;
  
        tail = newTail; //Update tail after updating linked list.
        *(cache + setIdx) = head; //Updating cache. 
      }
      else //No head and tail needed when E = 1.
        *(cache + setIdx) = lineptr;
      hit_cnt++;
      break; 
    }
    //An addr to data outside of cache is requested on a full set, evict:
    if (takenline == E) {
      //Replace old data at head w/new data (becomes newest).
      //Unlink head from list to update the tail (newest node at tail).
      //Link the prior tail to recently updated node/line.
      if (E > 1) {
        head->tag = tbits;
        newHead = head->next;
        newTail = head;
        tail->next = newTail;
        newTail->next = NULL;
      
        //Updating head and tail pointers after rearranging list:
        head = newHead;
        tail = newTail;
        *(cache + setIdx) = head; //Updating cache. 
      }
      else {  //No head and tail needed when E = 1.
        lineptr->tag = tbits;
        *(cache + setIdx) = lineptr;
      }
      //Increment  both evicts and misses because of  conflict/capacity miss.
      evict_cnt++;
      miss_cnt++; 
    }
    //Cold miss:
    if (lineptr->valid == 0) {
      //Create the line with its data:
      lineptr->tag = tbits;
      lineptr->valid = 1;
      //Update the next pointer of curr line. 
      if ((j+1) != E) 
        lineptr->next = (cache_line_t *)lineptr + 1; //FIXME could our segault stem from this syntax???
      miss_cnt++;
    }
    j++;
    lineptr = lineptr->next; //Move ptr to next node in linked list. 
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
