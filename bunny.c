/*
 * Copyright (c) 2021 Will Eccles
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <linux/reboot.h>

#define DEFAULT_HOSTNAME "linux"

static void mount_filesystems();
static void init_hostname();
static void seed_urandom();
static void shutdown(bool should_reboot);
static void run(const char* path);

static void signal_pid1(int sig);

static void handle_sig(int sig);

int main(int arc, char** argv) {
  if (getpid() != 1) {
    char* base = basename(argv[0]);

    if (strncmp(base, "shutdown", 8) == 0) {
      signal_pid1(SIGTERM);
    } else if (strncmp(base, "restart", 7) == 0) {
      signal_pid1(SIGINT);
    } else {
      fputs("Must be run as PID 1", stderr);
      exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
  }

  signal(SIGINT, handle_sig);
  signal(SIGTERM, handle_sig);
  signal(SIGCHLD, handle_sig);

  mount_filesystems();

  // do filesystem things
  run("/etc/bunny/1");

  init_hostname();

  seed_urandom();

  // do device/boot things
  run("/etc/bunny/2");

  // cause control-alt-delete to send SIGINT to PID 1
  reboot(LINUX_REBOOT_CMD_CAD_OFF);

  // start ttys
  run("/etc/bunny/3");

  // if we have done a tty or something to block above, we should not be able to
  // get here, and if we do we should try to execute a shell
  run("/bin/sh");

  // if that doesn't work, or the user exits the shell, or whatever, just reboot
  shutdown(true);

  exit(EXIT_FAILURE);
}

static void mount_filesystems() {
  mount("proc", "/proc", "proc", MS_NOEXEC | MS_NOSUID | MS_NODEV, NULL);
  mount("sys", "/sys", "sysfs", MS_NOEXEC | MS_NOSUID | MS_NODEV, NULL);
  mount("run", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755");
  mount("dev", "/dev", "devtmpfs", MS_NOSUID, "mode=0755");
}

static void init_hostname() {
  char* hostname = NULL;

  struct stat st = {0};
  int fd = open("/etc/hostname", O_RDONLY);
  if (fd < 0 || fstat(fd, &st) < 0) {
    close(fd);
    hostname = DEFAULT_HOSTNAME;
  }

  if (hostname == NULL) {
    hostname = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (hostname == MAP_FAILED) {
      hostname = DEFAULT_HOSTNAME;
    }
  }

  fd = open("/proc/sys/kernel/hostname", O_WRONLY);
  if (fd != -1) {
    write(fd, hostname, strlen(hostname));
    close(fd);
  }

  munmap(hostname, st.st_size);
}

static void seed_urandom() {
  struct stat st = {0};
  int fd = open("/var/bunny/random-seed", O_RDONLY);
  if (fd < 0) {
    return;
  }

  if (fstat(fd, &st) < 0) {
    close(fd);
    return;
  }

  char* data = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);

  if (data != MAP_FAILED) {
    fd = open("/dev/urandom", O_WRONLY);
    if (fd != 0) {
      write(fd, data, st.st_size);
      close(fd);
    }

    munmap(data, st.st_size);
  }
}

static void shutdown(bool should_reboot) {
  run("/etc/bunny/zzz");
  sync();
  reboot(should_reboot ? LINUX_REBOOT_CMD_RESTART : LINUX_REBOOT_CMD_POWER_OFF);
}

static void signal_pid1(int sig) {
  if (kill(1, sig) < 0) {
    perror("failed to send signal to init");
    exit(EXIT_FAILURE);
  }
}

static void run(const char* path) {
  pid_t pid = fork();
  if (pid == 0) {
    execl(path, path, NULL);
  } else if (pid) {
    waitpid(pid, NULL, 0);
  }
}

static void handle_sig(int sig) {
  switch (sig) {
    case SIGINT:  // reboot/control-alt-delete
      shutdown(true);
      break;
    case SIGTERM: // shutdown
      shutdown(false);
      break;
    case SIGCHLD: // reap zombies
      waitpid(-1, NULL, WNOHANG);
      return;
  }
}
