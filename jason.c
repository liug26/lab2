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

  /* Additional fields here */
  u32 time_left;
  u32 time_at_finish;
  u32 first_exec_time;
  /* End of "Additional fields here" */
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

u32 min(u32 x, u32 y) {
    return (x < y) ? x : y;
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
  //while we still have active processes
  bool finished = false;
  u32 curr_time = 0;
  bool currently_running_proc = false;
  struct process *running_process = NULL;
  struct process *on_deck_proc = NULL;
  u32 running_procs_time_left = 0;
  //need to initialize the time_left to burst_time
  for (u32 i = 0; i < size; i++) {
    struct process *p = &data[i];
    p->time_left = p->burst_time;
    p->first_exec_time = -1;
  }

  while(!finished) {
    //check arrival time of each of the processes in data
    for (u32 i = 0; i < size; i++) {
      struct process *p = &data[i];
      if (p->arrival_time == curr_time){
        //add to queue
        TAILQ_INSERT_TAIL(&list, p, pointers);
      }
    }
    if (on_deck_proc != NULL) {
      TAILQ_INSERT_TAIL(&list, on_deck_proc, pointers);
      on_deck_proc = NULL;
    }
  //from the queue, put a process in cpu
    if (!currently_running_proc) { //if there is no process currently running,
      //pop the first process from the queue
      //TODO: do i need to check if the queue is empty here?
        //but i feel like if queue is empty and no currrunproc, we are done (?)
      running_process = TAILQ_FIRST(&list);
      if (running_process != NULL) {
        TAILQ_REMOVE(&list, running_process, pointers);
        //make it true that there is a currRunProc
        currently_running_proc = true;
        running_procs_time_left = min(quantum_length, running_process->time_left);
        if (running_process->first_exec_time == -1) {
          running_process->first_exec_time = curr_time;
        }
      }
    }
    if (running_process != NULL) {
      running_process->time_left -= 1;
      running_procs_time_left -= 1;
    }
    curr_time += 1;
    //TODO: if running_procs_time_left is 0, take care of everything like no currentlyrunningproc etc
    if (running_procs_time_left == 0) {
      //if the curr proc still has time left >0, send back to queue end (im unsure if this will make the order wrong according to spec)
      if (running_process->time_left > 0) {
        on_deck_proc = running_process;
      }
      else { //all done with this process!
        running_process->time_at_finish = curr_time;
      }
      running_process = NULL;
      currently_running_proc = false;
    }
    bool all_finished = true;
    for (u32 i = 0; i < size; i++) {
      struct process *p =  &data[i];
      if (p->time_left > 0) {
        all_finished = false;
        break;
      }
    }
    if (all_finished) {
      finished = true;
    }
  }
  
  //calculate times
  for (u32 i = 0; i < size; i++) {
    struct process *p = &data[i];
    total_waiting_time += p->time_at_finish - p->arrival_time - p->burst_time;
    total_response_time += p->first_exec_time - p->arrival_time;
  }
  /* End of "Your code here" */

  printf("Average waiting time: %.2f\n", (float)total_waiting_time / (float)size);
  printf("Average response time: %.2f\n", (float)total_response_time / (float)size);

  free(data);
  return 0;
}
