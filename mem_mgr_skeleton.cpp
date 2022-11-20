//  mem_mgr.cpp
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <cassert>
//#include <iostream>

#pragma warning(disable : 4996)

#define ARGC_ERROR 1
#define FILE_ERROR 2

#define FRAME_SIZE  256
#define FIFO 0
#define LRU 1
#define REPLACE_POLICY FIFO

// SET TO 128 to use replacement policy: FIFO or LRU,
#define NFRAMES 128
#define PTABLE_SIZE 256
#define TLB_SIZE 16

struct page_node {    
    size_t npage;
    size_t frame_num;
    bool is_present;
    bool is_used;
};

char* ram = (char*)malloc(NFRAMES * FRAME_SIZE);
page_node pg_table[PTABLE_SIZE];  // page table and (single) TLB
page_node tlb[TLB_SIZE];

const char* passed_or_failed(bool condition) { return condition ? " + " : "fail"; }
size_t failed_asserts = 0;

size_t get_page(size_t x)   { return 0xff & (x >> 8); }
size_t get_offset(size_t x) { return 0xff & x; }

void get_page_offset(size_t x, size_t& page, size_t& offset) {
    page = get_page(x);
    offset = get_offset(x);
    // printf("x is: %zu, page: %zu, offset: %zu, address: %zu, paddress: %zu\n", 
    //        x, page, offset, (page << 8) | get_offset(x), page * 256 + offset);
}

void update_frame_ptable(size_t npage, size_t frame_num) {
    pg_table[npage].frame_num = frame_num;
    pg_table[npage].is_present = true;
    pg_table[npage].is_used = true;
}

int find_frame_ptable(size_t frame) {  // FIFO
    for (int i = 0; i < PTABLE_SIZE; i++) {
        if (pg_table[i].frame_num == frame && 
            pg_table[i].is_present == true) { return i; }
    }
    return -1;
}

size_t get_used_ptable() {  // LRU
    size_t unused = -1;
    for (size_t i = 0; i < PTABLE_SIZE; i++) {
        if (pg_table[i].is_used == false && 
            pg_table[i].is_present == true) { return (size_t)i; }
    }
    // All present pages have been used recently, set all page entry used flags to false
    for (size_t i = 0; i < PTABLE_SIZE; i++) { pg_table[i].is_used = false; }
    for (size_t i = 0; i < PTABLE_SIZE; i++) {
        page_node& r = pg_table[i];
        if (!r.is_used && r.is_present) { return i; }
    }
    return (size_t)-1;
}

int check_tlb(size_t page) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].npage == page) { return i; }
    }
    return -1;
}

void open_files(FILE*& fadd, FILE*& fcorr, FILE*& fback) { 
    fadd = fopen("addresses.txt", "r");
    if (fadd == NULL) { fprintf(stderr, "Could not open file: 'addresses.txt'\n");  exit(FILE_ERROR); }

    fcorr = fopen("correct.txt", "r");
    if (fcorr == NULL) { fprintf(stderr, "Could not open file: 'correct.txt'\n");  exit(FILE_ERROR); }

    fback = fopen("BACKING_STORE.bin", "rb");
    if (fback == NULL) { fprintf(stderr, "Could not open file: 'BACKING_STORE.bin'\n");  exit(FILE_ERROR); }
}
void close_files(FILE* fadd, FILE* fcorr, FILE* fback) { 
    fclose(fadd);
    fclose(fcorr);
    fclose(fback);
}

void initialize_pg_table_tlb() { 
    for (int i = 0; i < PTABLE_SIZE; ++i) {
        pg_table[i].npage = (size_t)i;
        pg_table[i].is_present = false;
        pg_table[i].is_used = false;
    }
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].npage = (size_t)-1;
        tlb[i].is_present = false;
        tlb[i].is_used = false; //was pg_table
    }
}

void summarize(size_t pg_faults, size_t tlb_hits) { 
    printf("\nPage Fault Percentage: %1.3f%%", (double)pg_faults / 1000);
    printf("\nTLB Hit Percentage: %1.3f%%\n\n", (double)tlb_hits / 1000);
    printf("ALL logical ---> physical assertions PASSED!\n");
    printf("\n\t\t...done.\n");
}

void tlb_add(int index, page_node entry) { 
    tlb[index] = entry;
    tlb[index].is_used = true;
    tlb[index].is_present = true;
    //std::cout << "=======in tlb_add tlb_track is " << index << "=======\n";
    //std::cout << "=======in tlb_add entry.frame_num is " << entry.frame_num << "=======\n";
}  // TODO

void tlb_remove(int index) { 
    tlb[index].npage = (size_t)-1;
    tlb[index].is_present = false;
    tlb[index].is_used = false;
}  // TODO

void tlb_hit(size_t& frame, size_t& page, size_t& tlb_hits, int result) {  
    //in this case that means that the page number was found in the tlb and we should set the frame equal to the corresponding frame value
    frame = tlb[result].frame_num;
    ++tlb_hits; //for the summary at the end
}  // TODO

void tlb_miss(size_t& frame, size_t& page, size_t& tlb_track) {
    //In this case the page number was not found in the tlb but it was found to exist in the page table
    //So we should set frame to be the frame found in page_table. We should also update the tlb
    //for future use. *this is not where fifo or lru is used. That is in the physical memory*
    // frame = pg_table[page].frame_num;
    // tlb_remove(tlb_track);
    // tlb_add(tlb_track, pg_table[page]);
    //printf("=========%lu========", pg_table[page].frame_num);
    frame = pg_table[page].frame_num;
    tlb_remove(tlb_track);
    tlb_add(tlb_track, pg_table[page]);
    //tlb[tlb_track].npage = frame;

    //tlb[tlb_track] = pg_table[page];
 } // TODO

void fifo_replace_page(size_t& frame ) {
    //int cur_index = find_frame_ptable(frame);
    //printf("cur_index: %d\n", cur_index);
    // tlb_remove(cur_index);
    // printf("fifo_replace was called\n");
    // tlb_add(cur_index, pg_table[frame]);

    int index_rn = find_frame_ptable(frame);
    tlb_remove(index_rn);
    tlb_add(index_rn, pg_table[index_rn]);
    //based on comparing the correct output and the programs output it can be determinded that ram is the
    //values that is wrong and causing an incorrect val result.

    
}   // TODO

void lru_replace_page(size_t& frame) { 
    int index_spot = get_used_ptable();
    tlb_remove(frame);
    tlb_add(index_spot, pg_table[index_spot]);
} // TODO

void page_fault(size_t& frame, size_t& page, size_t& frames_used, size_t& pg_faults, 
              size_t& tlb_track, FILE* fbacking) {  
    unsigned char buf[BUFSIZ];
    memset(buf, 0, sizeof(buf));
    bool is_memfull = false; //we are assuming mem is not full

    ++pg_faults; //add one to the number of pg_faults for summary because another has occured
    if (frames_used >= NFRAMES) { is_memfull = true; } //if all of the frames of physical memory are full is_memfull is now true
    frame = frames_used % NFRAMES;    // FIFO only     //used to wrap frame back for fifo

    if (is_memfull) {                                   //This case is when physical memory is full and it is now nessacary to chose how to handle that
        if (REPLACE_POLICY == FIFO) {
            //printf("calling fifo_replace\n");
            fifo_replace_page(frame);
            //frame = page;
            //(int)*(ram + (frame * FRAME_SIZE) + offset);
        } else {
            lru_replace_page(frame);
        }
        //TODO
    }
         // load page into RAM, update pg_table, TLB
    fseek(fbacking, page * FRAME_SIZE, SEEK_SET);
    fread(buf, FRAME_SIZE, 1, fbacking);
    printf("%s\n\n\n",buf);
    //printf("%lu\n", frame);
    //frame += 1;
    //printf("%lu\n", page);

    for (int i = 0; i < FRAME_SIZE; i++) {
        *(ram + (frame * FRAME_SIZE) + i) = buf[i]; //loading into ram
        //printf("===============Right now ram is %c===============\n", *ram);
        //printf("+-+-+-+-+-+-+-+%d+-+-+-+-+-+-+-+\n", buf[i]);
    }
    update_frame_ptable(page, frame); //update page table
    tlb_add(tlb_track++, pg_table[page]); //update tlb
    if (tlb_track > 15) { tlb_track = 0; } //if the tlb is full start back at 0
    //std::cout << "=======tlb_track is " << tlb_track << "=======\n";
    
    ++frames_used;                          //add to frames_used now that one has been added
} 

void check_address_value(size_t logic_add, size_t page, size_t offset, size_t physical_add,
                         size_t& prev_frame, size_t frame, int val, int value, size_t o) { 
    printf("log: %5lu 0x%04lu (pg:%3lu, off:%3lu)-->phy: %5lu (frm: %3lu) (prv: %3lu)--> val: %4d == value: %4d -- %s", 
          logic_add, logic_add, page, offset, physical_add, frame, prev_frame, 
          val, value, passed_or_failed(val == value));

    if (frame < prev_frame) {  printf("   HIT!\n");
    } else {
        prev_frame = frame;
        printf("----> pg_fault\n");
    }
    if (o % 5 == 4) { printf("\n"); }
// if (o > 20) { exit(-1); }             // to check out first 20 elements

    if (val != value) { ++failed_asserts; }
    if (failed_asserts > 5) { exit(-1); }
//     assert(val == value);
}

void run_simulation() { 
        // addresses, pages, frames, values, hits and faults
    size_t logic_add, virt_add, phys_add, physical_add;
    size_t page, frame, offset, value, prev_frame = 0, tlb_track = 0;
    size_t frames_used = 0, pg_faults = 0, tlb_hits = 0;
    int val = 0;
    char buf[BUFSIZ];

    bool is_memfull = false;     // physical memory to store the frames

    initialize_pg_table_tlb();

        // addresses to test, correct values, and pages to load
    FILE *faddress, *fcorrect, *fbacking;
    open_files(faddress, fcorrect, fbacking);

    for (int o = 0; o < 1000; o++) {     // read from file correct.txt
        fscanf(fcorrect, "%s %s %lu %s %s %lu %s %ld", buf, buf, &virt_add, buf, buf, &phys_add, buf, &value);  

        fscanf(faddress, "%ld", &logic_add);  
        get_page_offset(logic_add, page, offset);

        int result = check_tlb(page);
        if (result >= 0) {  
            tlb_hit(frame, page, tlb_hits, result);
            //printf("\n====We've got a hit====\n\n"); 
        } else if (pg_table[page].is_present) {
            tlb_miss(frame, page, tlb_track);
            //printf("\n====We've got a miss====\n\n");
        } else {         // page fault
            page_fault(frame, page, frames_used, pg_faults, tlb_track, fbacking);
           // printf("\n====We've got a fault====\n\n");
        }

        physical_add = (frame * FRAME_SIZE) + offset;
        val = (int)*(ram + physical_add);
        // printf("===============Right now ram is %hhd===============\n", *ram);
        // printf("=================Physical_add is %lu===============\n", physical_add);
        // printf("===================Frame size is %d================\n", FRAME_SIZE);
        // printf("===================Offset is %lu===================\n", offset);
        // printf("====================Frame is %lu===================\n", frame);
        // printf("+-+-+-+-+-+-+-+-+-+-+-+-+In that physical mem is %d+-+-+-+-+-+-+-+-+-+-+-+-+\n", *(ram + physical_add));

        check_address_value(logic_add, page, offset, physical_add, prev_frame, frame, val, value, o);
    }
    close_files(faddress, fcorrect, fbacking);  // and time to wrap things up
    free(ram);
    summarize(pg_faults, tlb_hits);
}


int main(int argc, const char * argv[]) {
    run_simulation();
// printf("\nFailed asserts: %lu\n\n", failed_asserts);   // allows asserts to fail silently and be counted
    return 0;
}
