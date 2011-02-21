#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <sys/time.h>

int BLOCKSIZE;
#define MAXBLOCKSIZE 8 * 1024 * 1024
long FILESIZE = 512 * 1024 * 1024 ;
#define FILEBLOCKS ( FILESIZE / BLOCKSIZE )
int TIMEOUT = 5;

int count;
struct timeval start;
int end_writing = 0;
int end_reading = 0;

int from_block_size = 512;
int to_block_size = MAXBLOCKSIZE;

int do_read_test = 1;
int do_write_test = 1;

void done()
{
	struct timeval end;

	gettimeofday(&end,NULL);

	if (count < FILEBLOCKS && start.tv_sec + TIMEOUT > end.tv_sec) {
		alarm(1);
		return;
	}

	long seconds, useconds;    
	float expired;

	seconds  = end.tv_sec  - start.tv_sec;
	useconds = end.tv_usec - start.tv_usec;

	expired = ((seconds) + useconds/1000000.0);

	printf("%5.1f\t", (float) BLOCKSIZE * count / 1024 /1024 / expired);

	if (!end_writing) 
		end_writing = 1;
	else
		end_reading = 1;
}

void handle(const char *string, int error)
{
	if (error) {
		perror(string);
		exit(EXIT_FAILURE);
	}
}

void print_usage()
{
	printf("Usage: mammothbench [-rw] [-f blocksize] [-t blocksize] [-s maxsize] [-d maxduraton] <filename>\n\n");
	printf("Test sequential I/O performance by reading and writing to <filename>.\n");
	printf("The contents of <filename> will be destroyed.\n\n");
	printf("\t-r\t\tPerform read tests only (default: read and write)\n");
	printf("\t-w\t\tPerform write tests only (default: read and write)\n");
	printf("\t-f blocksize\tPerform tests starting at blocksize kilobyte (default 0.5)\n");
	printf("\t-t blocksize\tPerform tests ending at blocksize kilobyte (default 8192)\n");
	printf("\t-s maxsize\tTarget length of data to read/write during each test in megabytes (default 512MB)\n");
	printf("\t-d maxduration\tMaximum time to wait for each test to reach the specified file length (default 5 seconds)\n");
	printf("\n");

}

char* parse_opts(int argc, char** argv)
{
	int opt;
	while ((opt = getopt(argc,argv,"f:t:rws:d:")) != -1) {
		switch (opt) {
			case 'f':
				from_block_size = atoi(optarg)*1024;
				break;
			case 't':
				to_block_size = atoi(optarg)*1024;
				break;
			case 'r':
				do_write_test = 0;
				break;
			case 'w':
				do_read_test = 0;
				break;
			case 's':
				FILESIZE = atol(optarg) * 1024*1024;
				break;
			case 'd':
				TIMEOUT = atoi(optarg);
				break;
			default:
				print_usage();
				exit(EXIT_FAILURE);
		}
	}
	if (optind >= argc) {
		print_usage();
		exit(EXIT_FAILURE);
	}

	return argv[optind];
}

int main(int argc, char **argv)
{
	void* buffer;
	int fd, retval;
	unsigned long numblocks;
	off64_t offset;

	retval = posix_memalign(&buffer, 512, MAXBLOCKSIZE);
	handle("posix_memalign", retval < 0);

	setvbuf(stdout, NULL, _IONBF, 0);

	printf("MammothBench v0.2, 2011-02-22, "
	       "http://www.mammothvps.com.au\n\n");

	char* filename = parse_opts(argc,argv);

	fd = open(filename, O_RDWR | O_EXCL | O_DIRECT | O_NOATIME, S_IRUSR | S_IWUSR);
	handle("open", fd < 0);

	printf("Benchmarking %s: max %ldMB or %dsec timeout\n",
	       filename, (long)(FILESIZE/1024/1024), TIMEOUT);

	printf("\nBLKSIZE\twMB/s\trMB/s\n-------\t-----\t-----");

	BLOCKSIZE = from_block_size;
	while (BLOCKSIZE <= to_block_size) 
	{
		if (BLOCKSIZE < 1024)
			printf("\n%4.1f\t", (float)BLOCKSIZE/1024);
		else
			printf("\n%4d\t", BLOCKSIZE/1024);
	
		end_writing = end_reading = 0;

		// start writing test
		if (do_write_test) {
			gettimeofday(&start,NULL);
			srand(start.tv_sec);
			signal(SIGALRM, &done);
			alarm(1);
			count = 0;

			while (!end_writing) {
				retval = write(fd, buffer, BLOCKSIZE);
				handle("write", retval != BLOCKSIZE);
				count++;
			}
			retval = lseek(fd, 0, SEEK_SET);
			handle("lseek",retval<0);

			retval = fsync(fd);
			handle("fsync",retval<0);
		} else {
			end_writing = 1;
			printf("skip\t");
		}

		// Start reading test
		if (do_read_test) {
			gettimeofday(&start,NULL);
			srand(start.tv_sec);
			signal(SIGALRM, &done);
			alarm(1);
			count = 0;

			while (!end_reading) {
				retval = read(fd, buffer, BLOCKSIZE);
				handle("read", retval < 0);
				if (retval == 0) {
					retval = lseek(fd,0,SEEK_SET);
					handle("lseek",retval<0);
				}
				count++;
			}
		} else {
			end_reading = 1;
			printf("skip\t");
		}

		BLOCKSIZE *= 2;
	}

	printf("\n");
}
