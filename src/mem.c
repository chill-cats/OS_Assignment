
#include "mem.h"
#include "common.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static BYTE _ram[RAM_SIZE];

static struct
{
    uint32_t proc; // ID of process currently uses this page
    int index;     // Index of the page in the list of pages allocated
                   // to the process.
    int next;      // The next page in the list. -1 if it is the last
                   // page.
} _mem_stat[NUM_PAGES];

static pthread_mutex_t mem_lock;

void init_mem(void) {
    memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
    memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
    pthread_mutex_init(&mem_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) { return addr & ~((~0U) << OFFSET_LEN); }

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
    return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
    return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
static struct page_table_t *
get_page_table(addr_t index,                    // Segment level index
               struct seg_table_t *seg_table) { // first level table

    if (seg_table == NULL) {
        return NULL;
    }

    for (int index = 0; index < seg_table->segment_count; index++) {
        if (seg_table->segments[index].v_index == index) {
            return seg_table->segments[index].pages;
        }
    }

    return NULL;
}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(addr_t virtual_addr,   // Given virtual address
                     addr_t *physical_addr, // Physical address to be returned
                     struct pcb_t *proc) {  // Process uses given virtual address

    /* Offset of the virtual address */
    addr_t offset = get_offset(virtual_addr);
    /* get the segment index */
    addr_t first_lv = get_first_lv(virtual_addr);
    /* get the page index */
    addr_t second_lv = get_second_lv(virtual_addr);

    /* Search in the first level */
    struct page_table_t *page_table = NULL;
    page_table = get_page_table(first_lv, proc->seg_table); // get the segment according to the index
    if (page_table == NULL) {
        return 0;
    }

    for (int i = 0; i < page_table->page_count; i++) {
        if (page_table->pages[i].v_index == second_lv) { // found the page with the correct virtual page index
            /* DONE: Concatenate the offset of the virtual addess
             * to [p_index] field of page_table->table[i] to
             * produce the correct physical address and save it to
             * [*physical_addr]  */
            uint32_t page_index = page_table->pages[i].p_index;
            uint32_t physical_address = (page_index << OFFSET_LEN) | offset;
            *physical_addr = physical_address;
            return 1;
        }
    }
    return 0;
}

addr_t alloc_mem(uint32_t size, struct pcb_t *proc) {
    pthread_mutex_lock(&mem_lock);
    addr_t ret_addr = 0;
    /* TODO: Allocate [size] byte in the memory for the
     * process [proc] and save the address of the first
     * byte in the allocated memory region to [ret_mem].
     * */

    uint32_t num_pages = (size % PAGE_SIZE) ? size / PAGE_SIZE : size / PAGE_SIZE + 1; // Number of pages we will use

    /* First we must check if the amount of free memory in
     * virtual address space and physical address space is
     * large enough to represent the amount of required
     * memory. If so, set 1 to [mem_avail].
     * Hint: check [proc] bit in each page of _mem_stat
     * to know whether this page has been used by a process.
     * For virtual memory space, check bp (break pointer).
     * */

    int *free_page_index = (int *)malloc(sizeof(int) * num_pages);
    int free_page_count = 0;
    for (int index = 0; index < NUM_PAGES && free_page_count < num_pages; index++) {
        if (_mem_stat[index].proc == 0) { // free page
            free_page_index[free_page_count++] = index;
        }
    }

    uint32_t end_of_allocated_mem = proc->bp + PAGE_SIZE * num_pages;
    if (free_page_count != num_pages || end_of_allocated_mem > RAM_SIZE) {
        pthread_mutex_unlock(&mem_lock);
        return ret_addr; // bail out early
    }

    /* We could allocate new memory region to the process */
    ret_addr = proc->bp;
    proc->bp += num_pages * PAGE_SIZE;
    /* Update status of physical pages which will be allocated
     * to [proc] in _mem_stat. Tasks to do:
     * 	- Update [proc], [index], and [next] field
     * 	- Add entries to segment table page tables of [proc]
     * 	  to ensure accesses to allocated memory slot is
     * 	  valid. */
    if (proc->seg_table == NULL) { // allocated new memory segment table if the process doesn't have one already (shouldn't occur at all)
        proc->seg_table = malloc(sizeof(struct seg_table_t));
        memset(proc->seg_table, 0, sizeof(struct seg_table_t));
    }

    struct page_table_t *new_segment = (struct page_table_t *)malloc(sizeof(struct page_table_t));
    memset(new_segment, 0, sizeof(struct page_table_t));
    new_segment->page_count = num_pages;

    uint32_t segment_address = get_first_lv(ret_addr);

    proc->seg_table->segments[proc->seg_table->segment_count].pages = new_segment;
    proc->seg_table->segments[proc->seg_table->segment_count].v_index = segment_address;

    for (int index = 0; index < num_pages; index++) {
        uint32_t page_virtual_address = get_second_lv(ret_addr + index * PAGE_SIZE);
        new_segment->pages[index].v_index = page_virtual_address;
        new_segment->pages[index].p_index = free_page_index[index];

        _mem_stat[free_page_index[index]].index = index;
        _mem_stat[free_page_index[index]].proc = proc->pid;

        if (index == num_pages - 1) {
            _mem_stat[free_page_index[index]].next = -1;
        } else {
            _mem_stat[free_page_index[index]].next = free_page_index[index + 1];
        }
    }
    pthread_mutex_unlock(&mem_lock);
    return ret_addr;
}

int free_mem(addr_t address, struct pcb_t *proc) {
    /*TODO: Release memory region allocated by [proc]. The first byte of
     * this region is indicated by [address]. Task to do:
     * 	- Set flag [proc] of physical page use by the memory block
     * 	  back to zero to indicate that it is free.
     * 	- Remove unused entries in segment table and page tables of
     * 	  the process [proc].
     * 	- Remember to use lock to protect the memory from other
     * 	  processes.  */
    pthread_mutex_lock(&mem_lock);

    addr_t physical_address = 0;
    if (!translate(address, &physical_address, proc)) {
        return 1;
    }

    struct page_table_t *page_table = get_page_table(address, proc->seg_table);

    uint32_t segment_size = PAGE_SIZE * page_table->page_count;
    uint32_t end_of_segment_address = address + segment_size;
    bool freeing_block_at_the_end_of_proc_mem = end_of_segment_address == proc->bp;

    if (freeing_block_at_the_end_of_proc_mem) {
        proc->bp -= segment_size;
    }

    for (int index = 0; index < page_table->page_count; index++) {
        // reset physical pages
        uint32_t physical_index = page_table->pages[index].p_index;
        _mem_stat[physical_index].index = 0;
        _mem_stat[physical_index].next = -1;
        _mem_stat[physical_index].proc = 0;
    }
    pthread_mutex_unlock(&mem_lock);
    return 0;
}

int read_mem(addr_t address, struct pcb_t *proc, BYTE *data) {
    addr_t physical_addr;
    if (translate(address, &physical_addr, proc)) {
        *data = _ram[physical_addr];
        return 0;
    } else {
        return 1;
    }
}

int write_mem(addr_t address, struct pcb_t *proc, BYTE data) {
    addr_t physical_addr;
    if (translate(address, &physical_addr, proc)) {
        _ram[physical_addr] = data;
        return 0;
    } else {
        return 1;
    }
}

void dump(void) {
    int i;
    for (i = 0; i < NUM_PAGES; i++) {
        if (_mem_stat[i].proc != 0) {
            printf("%03d: ", i);
            printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n", i << OFFSET_LEN,
                   ((i + 1) << OFFSET_LEN) - 1, _mem_stat[i].proc, _mem_stat[i].index,
                   _mem_stat[i].next);
            int j;
            for (j = i << OFFSET_LEN; j < ((i + 1) << OFFSET_LEN) - 1; j++) {
                if (_ram[j] != 0) {
                    printf("\t%05x: %02x\n", j, _ram[j]);
                }
            }
        }
    }
}
