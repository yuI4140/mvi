#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <threads.h>
#include <unistd.h>

#define GLOB_IMPLEMENTATION
#include "./glob.h/glob.h"
#define NOB_IMPLEMENTATION
#include "./nob.h/nob.h"
#define YUIS_UTILS
#include "./yuis-headers/yuis_utils.h"
#include <pthread.h>

bool is_force = false;
bool is_pedantic = false;
bool is_backup = true;
#define VERSION "0.0.4"
#define NOB_FOREACH(array) for (size_t i = 0; i < array.count; ++i)
#define N_THREAD 1
typedef void*(*vFunc)(void*);

int nob_file_exists_in_dir(const char *filepath, const char *dir) {
  Nob_File_Paths ddir = {0};
  if (!nob_read_entire_dir(dir, &ddir)) {
    nob_log(NOB_ERROR, "Cannot check if the destination dir");
    return -1;
  }
  NOB_FOREACH(ddir) {
    if (!is_force && (strcmp(ddir.items[i], filepath)) == 0) {
      nob_log(NOB_WARNING,
              "File '%s' already exists in directory '%s'. Use --force (-f) to "
              "overwrite.",
              filepath, dir);
      return -1;
    }
  }
  return 0;
}
int move_file(const char *file, const char *dest_dir) {
  struct stat sbuf;

  if (stat(file, &sbuf) != 0) {
    nob_log(NOB_ERROR,
            "Failed to scan file '%s'. Ensure the file exists and you have "
            "read permissions.",
            file);
    return -1;
  } else {
    if (is_pedantic) {
      nob_log(NOB_INFO, "Successfully scanned file '%s'.", file);
    }
  }
  if (nob_file_exists_in_dir(file, dest_dir) < 0) {
    return -1;
  } else {
    if (is_pedantic) {
      nob_log(NOB_INFO, "file doesn't exists at the destination directory");
    }
  }
  char *new_file = nob_temp_alloc(1024);
  if ((strcmp(dest_dir, ".")) == 0) {
    sprintf(new_file, "%s/%s", dest_dir, basename((char *)file));
  } else if ((strcmp(dest_dir, "./")) == 0) {
    sprintf(new_file, "%s%s", dest_dir, basename((char *)file));
  } else if (GLOB_UNMATCHED == glob_utf8("/", dest_dir) &&
             GLOB_UNMATCHED == glob_utf8(".", dest_dir)) {
    sprintf(new_file, "%s/%s", dest_dir, basename((char *)file));
  } else if (GLOB_MATCHED == glob_utf8("/", dest_dir) &&
             GLOB_UNMATCHED == glob_utf8(".", dest_dir)) {
    sprintf(new_file, "%s%s", dest_dir, basename((char *)file));
  } else {
    new_file = (char *)file;
  }
  char *backup_file = nob_temp_sprintf("./buckup_%s", basename((char *)file));
  if (is_backup) {
    char *content = read_file(file);
    Nob_String_Builder writer = {0};
    if (!nob_write_entire_file(backup_file, content, sbuf.st_size)) {
      nob_log(NOB_ERROR, "Unable to make a backup");
      return -1;
    } else {
      if (is_pedantic) {
        nob_log(NOB_INFO, "written backup -> Successfully");
      }
    }
  }
  if (rename(file, new_file) != 0) {
    nob_log(
        NOB_ERROR,
        "Failed to move file '%s' to '%s'.\n Ensure you have write permissions.",
        file, new_file);
    perror("SubError");
    return -1;
  }
  if (is_backup) {
    if (nob_file_exists(new_file)) {
      if (unlink(backup_file) != 0) {
        nob_log(NOB_ERROR, "Unable to remove a backup");
        return -1;
      } else {
        if (is_pedantic) {
          nob_log(NOB_INFO, "Remove backup -> Successfully");
        }
      }
    } else {
      nob_log(NOB_WARNING, "The file was lost during the moving process,\n but "
                           "a backup was saved in the current directory.");
      return 0;
    }
  }
  if (is_pedantic) {
    nob_log(NOB_INFO, "move '%s' -> '%s'", file, new_file);
  }
  return 0;
}
void remove_leading_dashes(char *str) {
    if (str == NULL || strlen(str) == 0) {
        return;
    }
    if (str[0] == '-') {
        if (str[1] == '-') {
            memmove(str, str + 2, strlen(str) - 1); 
        } else {
            memmove(str, str + 1, strlen(str));
        }
    }
}
typedef struct Thread {
    pthread_t th;
} Thread;

typedef struct ThreadPool {
    Thread *pool;
    size_t in_used;
    size_t n_threads;
} ThreadPool;

ThreadPool thread_pool_init(size_t n_threads) {
    ThreadPool thread_pool = {0};
    thread_pool.n_threads=n_threads;
    thread_pool.pool = nob_temp_alloc(sizeof(Thread) * n_threads);
    memset(thread_pool.pool, 0, sizeof(Thread) * n_threads);
    return thread_pool;
}

int thread_pool_run(ThreadPool *pool, vFunc fn, void *args) {
    if (pthread_create(&pool->pool[pool->in_used].th, NULL, fn, args)) {
        nob_log(NOB_ERROR, "Error creating thread %zu", pool->in_used);
        return 1;
    }
    ++pool->in_used;
    return 0;
}

void thread_pool_join(ThreadPool *pool) {
    for (size_t i = 0; i < pool->in_used; ++i) {
        pthread_join(pool->pool[i].th, NULL);
    }
}

typedef struct MoveArgs {
    const char *file;
    const char *dest_dir;
} MoveArgs;

void *move_file_thread(void *arg) {
    MoveArgs *args = (MoveArgs *)arg;
    move_file(args->file, args->dest_dir);
    return NULL;
}
int main(int argc, char *argv[]) {
  if (argc < 2) {
    nob_log(NOB_ERROR, "no arguments given or missing destination-dir!");
    nob_log(
        NOB_INFO,
        "%s: [-p | -h | -v | -f] <file-path or expression> <destination-dir>",
        argv[0]);
    return -1;
  }
  if (argc == 2) {
    char *opt = strdup(argv[1]);
    remove_leading_dashes(opt);
    if ((strcmp(opt, "version")) == 0 || (strcmp(opt, "v")) == 0) {
      nob_log(NOB_INFO, "Version: %s", VERSION);
      return 0;
    }
    if ((strcmp(opt, "help")) == 0 || (strcmp(opt, "h")) == 0) {
      nob_log(NOB_INFO, "Usage: %s <file-path> <destination>", argv[0]);
      nob_log(NOB_INFO, "Options:");
      nob_log(NOB_INFO, "  -h, --help: Show this help message");
      nob_log(NOB_INFO, "  -v, --version: Display the current version");
      nob_log(NOB_INFO,
              "  -f, --force: Force move, overwriting files if they exist");
      nob_log(NOB_INFO, "  -p, --pedantic: Verbose mode");
      nob_log(NOB_INFO, "  -j<n_threads>, --jobs <n_threads>: multithreading mode");
      return 0;
    }
  }
  size_t n_threads=1;
  if (argc > 2) {
    for (size_t i = 1; i < argc - 2; ++i) {
      char *opt = strdup(argv[i]);
      remove_leading_dashes(opt);
      if ((strcmp(opt, "version")) == 0 || (strcmp(opt, "v")) == 0) {
        nob_log(NOB_INFO, "Version: %s\n", VERSION);
        return 0;
      } else if ((strcmp(opt, "force")) == 0 || (strcmp(opt, "f")) == 0) {
        is_force = true;
      } else if ((strcmp(opt, "pedantic")) == 0 || (strcmp(opt, "p")) == 0) {
        is_pedantic = true;
      }else if ((strcmp(opt, "no-backup")) == 0) {
        is_backup = false;
      } else if ((strcmp(opt, "jobs")) == 0 || (strcmp(opt, "j")) == 0) {
         n_threads=strtol(argv[i+1],NULL,10); 
      } else if (GLOB_MATCHED==glob_utf8("j*",opt)) {
          char ch[2]={0}; ch[0]=opt[1]; ch[1]='\0';
         n_threads=strtol(ch,NULL,10); 
      }
    }
  }
  ThreadPool thread_pool=thread_pool_init(n_threads);
  const char *param1 = argv[argc - 2];
  const char *dest_dir = argv[argc - 1];
  struct stat dest_stat;
  if (stat(dest_dir, &dest_stat)) {
    nob_log(NOB_ERROR, "Destination '%s' is not a valid directory.", dest_dir);
    return -1;
  }

  Nob_File_Paths verified_files = {0};

  if (nob_file_exists(param1)) {
    MoveArgs *args = nob_temp_alloc(sizeof(MoveArgs));
    args->file = param1;
    args->dest_dir = dest_dir;
    thread_pool_run(&thread_pool, move_file_thread, args);
  } else {
    Nob_File_Paths current_dir = {0};
    if (!nob_read_entire_dir(".", &current_dir)) {
      nob_log(NOB_ERROR, "Unable to read current directory. Ensure you have "
                         "the necessary permissions.");
      return -1;
    }

    NOB_FOREACH(current_dir) {
      const char *current_file = current_dir.items[i];
      if (GLOB_MATCHED == glob_utf8(param1, current_file)) {
        nob_da_append(&verified_files, current_file);
      }
    }
    if (is_pedantic && verified_files.count == 0) {
      nob_log(NOB_WARNING,
              "No files matched the expression '%s' in the current directory.",
              param1);
      return -1;
    }
    NOB_FOREACH(verified_files) {
     const char *file = verified_files.items[i];
     MoveArgs *args = nob_temp_alloc(sizeof(MoveArgs));
     args->file = file;
     args->dest_dir = dest_dir;
     thread_pool_run(&thread_pool, move_file_thread, args); 
    }
  }
  return EXIT_SUCCESS;
}
