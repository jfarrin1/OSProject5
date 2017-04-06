#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int pagefault=0;
int diskread=0;
int diskwrite=0;
int counter=0;
int *arr;
struct disk *disk;
int check;

int LinearSearch(int l, int r, int key)
{
    int m;
    for(m=l;m<=r;m++){
    if(arr[m]==key)
        return m;
    }
    return -1;
}



void page_fault_handler( struct page_table *pt, int page )
{
    int no_pages=page_table_get_npages(pt);
    int no_frames=page_table_get_nframes(pt);
    char *physmem = page_table_get_physmem(pt);

    if(no_frames >= no_pages){
    printf("page fault on page #%d\n",page);
    page_table_set_entry(pt,page,page,PROT_READ|PROT_WRITE);
    pagefault++;
    diskread=0;
    diskwrite=0;
    }

    else{
        //Implementation of Custom Code.
        /*This custom code is a hash function implementation of my own.*/
        if(check==3){
            pagefault++;
            int temp=page%no_frames;
            if(arr[temp]==page){
            page_table_set_entry(pt,page,temp,PROT_READ|PROT_WRITE);
            pagefault--;
            }
            else if(arr[temp]==-1){
            page_table_set_entry(pt,page,temp,PROT_READ);
            disk_read(disk,page,&physmem[temp*PAGE_SIZE]);
            diskread++;
            }
            else{
            disk_write(disk,arr[temp],&physmem[temp*PAGE_SIZE]);
            disk_read(disk,page,&physmem[temp*PAGE_SIZE]);
            diskread++;
            diskwrite++;
            page_table_set_entry(pt,page,temp,PROT_READ);
            }
            arr[temp]=page;
            page_table_print(pt);
        }

        //Implementation of FIFO.
        else if(check==2){
            pagefault++;
            int k=LinearSearch(0,no_frames-1,page);
            if(k > -1){
            page_table_set_entry(pt,page,k,PROT_READ|PROT_WRITE);
            counter--;
            pagefault--;
            }
            else if(arr[counter]==-1){
            page_table_set_entry(pt,page,counter,PROT_READ);
            disk_read(disk,page,&physmem[counter*PAGE_SIZE]);
            diskread++;
            }
            else{
            disk_write(disk,arr[counter],&physmem[counter*PAGE_SIZE]);
            disk_read(disk,page,&physmem[counter*PAGE_SIZE]);
            diskread++;
            diskwrite++;
            page_table_set_entry(pt,page,counter,PROT_READ);
            }
            arr[counter]=page;
            counter=(counter+1)%no_frames;
            page_table_print(pt);
        }

        //Implementation of RAND.
        else{
            pagefault++;
            int k=LinearSearch(0,no_frames-1,page);
            int temp=lrand48()%no_frames;
            if(k > -1){
            page_table_set_entry(pt,page,k,PROT_READ|PROT_WRITE);
            pagefault--;
            }
            else if(counter < no_frames){
            while(arr[temp]!=-1){
            temp=lrand48()%no_frames;
            pagefault++;
            }
            page_table_set_entry(pt,page,temp,PROT_READ);
            disk_read(disk,page,&physmem[temp*PAGE_SIZE]);
            diskread++;
            arr[temp]=page;
            counter++;
            }
            else{
            disk_write(disk,arr[temp],&physmem[temp*PAGE_SIZE]);
            disk_read(disk,page,&physmem[temp*PAGE_SIZE]);
            diskread++;
            diskwrite++;
            page_table_set_entry(pt,page,temp,PROT_READ);
            arr[temp]=page;
            }
            page_table_print(pt);

        }
    }
}

int main( int argc, char *argv[] )
{
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}

	int npages = atoi(argv[1]);
	int nframes = atoi(argv[2]);

	if(!strcmp(argv[3],"rand"))
        check=1;
    else if(!strcmp(argv[3],"fifo"))
        check=2;
    else
        check=3;
	const char *program = argv[4];

	arr=(int *)malloc(nframes*sizeof(int));
	int i;
	for(i=0;i<nframes;i++)
        arr[i]=-1;

	disk = disk_open("myvirtualdisk",npages);


	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}

	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );

	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	char *virtmem = page_table_get_virtmem(pt);

	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);
	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);
	}

	page_table_delete(pt);
	disk_close(disk);

	printf("\nSUMMARY:\n");
	printf("No. of page faults:%d\n",pagefault);
	printf("No. of disk reads:%d\n",diskread);
	printf("No. of disk writes:%d\n",diskwrite);

	return 0;
}
