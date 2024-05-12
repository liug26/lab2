#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint32_t u32;
typedef int32_t i32;

struct process
{
  u32 pid;
  u32 arrival_time;
  u32 burst_time;

  TAILQ_ENTRY(process) pointers;

  u32 remaining_time;
  u32 start_exec_time;
  u32 end_time;

  bool isNew;
  bool added;
};

TAILQ_HEAD(process_list, process);

u32 next_int(const char **data, const char *data_end)
{
  u32 current = 0;
  bool started = false;
  while (*data != data_end)
  {
    char c = **data;

    if (c < 0x30 || c > 0x39)
    {
      if (started)
      {
        return current;
      }
    }
    else
    {
      if (!started)
      {
        current = (c - 0x30);
        started = true;
      }
      else
      {
        current *= 10;
        current += (c - 0x30);
      }
    }

    ++(*data);
  }

  printf("Reached end of file while looking for another integer\n");
  exit(EINVAL);
}

u32 next_int_from_c_str(const char *data)
{
  char c;
  u32 i = 0;
  u32 current = 0;
  bool started = false;
  while ((c = data[i++]))
  {
    if (c < 0x30 || c > 0x39)
    {
      exit(EINVAL);
    }
    if (!started)
    {
      current = (c - 0x30);
      started = true;
    }
    else
    {
      current *= 10;
      current += (c - 0x30);
    }
  }
  return current;
}

void init_processes(const char *path,
                    struct process **process_data,
                    u32 *process_size)
{
  int fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    int err = errno;
    perror("open");
    exit(err);
  }

  struct stat st;
  if (fstat(fd, &st) == -1)
  {
    int err = errno;
    perror("stat");
    exit(err);
  }

  u32 size = st.st_size;
  const char *data_start = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_start == MAP_FAILED)
  {
    int err = errno;
    perror("mmap");
    exit(err);
  }

  const char *data_end = data_start + size;
  const char *data = data_start;

  *process_size = next_int(&data, data_end);

  *process_data = calloc(sizeof(struct process), *process_size);
  if (*process_data == NULL)
  {
    int err = errno;
    perror("calloc");
    exit(err);
  }

  for (u32 i = 0; i < *process_size; ++i)
  {
    (*process_data)[i].pid = next_int(&data, data_end);
    (*process_data)[i].arrival_time = next_int(&data, data_end);
    (*process_data)[i].burst_time = next_int(&data, data_end);
  }

  munmap((void *)data, size);
  close(fd);
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    return EINVAL;
  }
  struct process *data;
  u32 size;
  init_processes(argv[1], &data, &size);

  u32 quantum_length = next_int_from_c_str(argv[2]);

  struct process_list list;
  TAILQ_INIT(&list);

  u32 total_waiting_time = 0;
  u32 total_response_time = 0;

  /* Your code here */
  if (quantum_length <= 0 ){
		fprintf(stderr, "Error: Invalid Quantum Time\n");
    exit(EINVAL);
	}

  for(u32 i = 0; i < size; i++){
    data[i].remaining_time = data[i].burst_time;
    data[i].start_exec_time = 0;
    data[i].end_time = 0;
    data[i].isNew = true;
    data[i].added = false;
  }

  u32 simulation_time = 0;
  u32 remaining_process = size;

  while(remaining_process)
  {
    //checks incoming new process
    for( u32 i = 0; i < size; i++)
    {
      if(data[i].arrival_time == simulation_time && !data[i].added)
      {
        // fprintf(stderr,"added pid1: %d\n", data[i].pid);
        TAILQ_INSERT_TAIL(&list, &data[i], pointers);
        data[i].added = true;
      }
    }

    //get first element from queue
    struct process *current = TAILQ_FIRST(&list);

    if(current == NULL)
    {
      simulation_time++;
      continue;
    }


    //when did it first start
    if(current->isNew)
    {
      current->start_exec_time = simulation_time;
      // fprintf(stderr,"pid: %d, exectime: %d\n", current->pid, current->start_exec_time);
      current->isNew = false;
    }

    //get the right amount of run
    u32 time_block;
    if(current->remaining_time <= quantum_length)
    {
      time_block = current->remaining_time;
    } else {
      time_block = quantum_length;
    }

    //the current process turn
    while(time_block)
    {
      current->remaining_time--;
      time_block--;
      simulation_time++;

      //check for incoming again
      for( u32 i = 0; i < size; i++)
      {
        if(data[i].arrival_time == simulation_time && !data[i].added)
        {
          // fprintf(stderr,"added pid2: %d\n", data[i].pid);
          TAILQ_INSERT_TAIL(&list, &data[i], pointers);
          data[i].added = true;
        }
      }
    }

    //reinsert or done?
    if(current->remaining_time == 0){
      current->end_time = simulation_time;
      // fprintf(stderr,"pid: %d, endtime: %d\n", current->pid, current->end_time);
      remaining_process--;
      TAILQ_REMOVE(&list, current, pointers);
    }else{
      TAILQ_REMOVE(&list, current, pointers);
      TAILQ_INSERT_TAIL(&list, current, pointers);
    }
  }

  //calculate response and waiting
  for (u32 i = 0; i < size; ++i)
  {
    total_response_time += data[i].start_exec_time - data[i].arrival_time;
    total_waiting_time += (data[i].end_time - data[i].arrival_time - data[i].burst_time);
  }

  /* End of "Your code here" */

  printf("Average waiting time: %.2f\n", (float)total_waiting_time / (float)size);
  printf("Average response time: %.2f\n", (float)total_response_time / (float)size);

  free(data);
  return 0;
}
