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
  u32 start_execute_time;
  u32 end_time;
  bool init_start_execute_time;
  bool added_to_list;
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
    if (quantum_length <= 0)
    {
        fprintf(stderr, "Quantum length should > 0\n");
        exit(EINVAL);
    }

    for(u32 i = 0; i < size; i++)
    {
        data[i].remaining_time = data[i].burst_time;
        data[i].start_execute_time = 0;
        data[i].end_time = 0;
        data[i].init_start_execute_time = false;
        data[i].added_to_list = false;
    }

    u32 time_now = 0;
    u32 num_unfinished_processes = size;

    while(num_unfinished_processes > 0)
    {
        for(u32 i = 0; i < size; i++)
        {
            if(data[i].arrival_time == time_now && !data[i].added_to_list)
            {
                TAILQ_INSERT_TAIL(&list, &data[i], pointers);
                data[i].added_to_list = true;
            }
        }

        struct process *current_process = TAILQ_FIRST(&list);
        if(current_process == NULL)
        {
            time_now++;
            continue;
        }

        if(!current_process->init_start_execute_time)
        {
            current_process->start_execute_time = time_now;
            current_process->init_start_execute_time = true;
        }

        u32 process_runtime;
        if(current_process->remaining_time > quantum_length)
            process_runtime = quantum_length;
        else
            process_runtime = current_process->remaining_time;

        // run current process
        for (; process_runtime > 0; process_runtime--)
        {
            current_process->remaining_time--;
            time_now++;

            for(u32 i = 0; i < size; i++)
            {
                if(data[i].arrival_time == time_now && !data[i].added_to_list)
                {
                    TAILQ_INSERT_TAIL(&list, &data[i], pointers);
                    data[i].added_to_list = true;
                }
            }
        }

        TAILQ_REMOVE(&list, current_process, pointers);

        if(current_process->remaining_time > 0)
            TAILQ_INSERT_TAIL(&list, current_process, pointers);
        else
        {
            num_unfinished_processes--;
            current_process->end_time = time_now;
        }   
    }

    for (u32 i = 0; i < size; ++i)
    {
        total_waiting_time += data[i].end_time - data[i].arrival_time - data[i].burst_time;
        total_response_time += data[i].start_execute_time - data[i].arrival_time;
    }
    /* End of "Your code here" */

    printf("Average waiting time: %.2f\n", (float)total_waiting_time / (float)size);
    printf("Average response time: %.2f\n", (float)total_response_time / (float)size);

    free(data);
    return 0;
}
