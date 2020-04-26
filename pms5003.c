#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <byteswap.h>
#include <signal.h>

void set_interface_attribs(int fd, int speed)
{
  struct termios tty;

  if (tcgetattr(fd, &tty) < 0) {
    fprintf(stderr, "fatal: tcgetattr(): %s\n", strerror(errno));
    exit(-1);
  }

  cfsetospeed(&tty, (speed_t) speed);
  cfsetispeed(&tty, (speed_t) speed);

  tty.c_cflag |= (CLOCAL | CREAD);      /* ignore modem controls */
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;           /* 8-bit characters */
  tty.c_cflag &= ~PARENB;       /* no parity bit */
  tty.c_cflag &= ~CSTOPB;       /* only need 1 stop bit */
  tty.c_cflag &= ~CRTSCTS;      /* no hardware flowcontrol */

  /* setup for non-canonical mode */
  tty.c_iflag &=
      ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  tty.c_oflag &= ~OPOST;

  /* fetch bytes as they become available */
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 1;

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    fprintf(stderr, "fatal: tcsetattr(): %s\n", strerror(errno));
    exit(-1);
  }
}

void forced_read(int fd, void *buf, size_t count)
{
  size_t num_read = 0;

  do {
    ssize_t rdlen = read(fd, (char *) buf + num_read, count - num_read);
    if (rdlen < 1) {
      fprintf(stderr,
              "fatal: read() == %zd (got %zu of %zu requested): %s\n",
              rdlen, num_read, count, strerror(errno));
      exit(-1);
    }
    num_read += rdlen;
  } while (num_read < count);
}

void print_data(size_t num_measurements, double *data)
{
  static const char *labels[] = { "std_pm1", "std_pm2_5", "std_pm10",
    "atm_pm1", "atm_pm2_5", "atm_pm10",
    "count_0_3um", "count_0_5um", "count_1um",
    "count_2_5um", "count_5um", "count_10um"
  };

  printf("{\"num_measurements\":%zu", num_measurements);

  for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++)
    printf(",\"%s\":%.02f", labels[i], data[i]);

  printf("}\n");
}

double avg_sum[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

size_t avg_num = 0;

void sigalrm_handler()
{
  if (avg_num > 0) {
    for (size_t i = 0; i < sizeof(avg_sum) / sizeof(avg_sum[0]); i++)
      avg_sum[i] /= avg_num;
    print_data(avg_num, avg_sum);
    exit(0);
  } else {
    fprintf(stderr,
            "warning: no data frames collected in the given time span.\n");
    exit(-1);
  }
}

int main(int argc, char **argv)
{
  if (argc != 2 && argc != 3) {
    fprintf(stderr,
            "usage: %s <port> [ <average-over-this-many-seconds>]\n",
            argv[0]);
    exit(-1);
  }

  size_t average_over = 0;
  if (argc == 3) {
    average_over = atoi(argv[2]);
    signal(SIGALRM, sigalrm_handler);
    alarm(average_over);
  }

  const char *portname = argv[1];
  int fd;

  fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd < 0) {
    fprintf(stderr, "fatal: open(): %s: %s\n", portname, strerror(errno));
    exit(-1);
  }

  set_interface_attribs(fd, B9600);

  for (;;) {
    char ch;
    forced_read(fd, &ch, 1);
    if (ch != 0x42)
      continue;
    forced_read(fd, &ch, 1);
    if (ch != 0x4d)
      continue;

    unsigned short data[15];
    forced_read(fd, &data, sizeof(data));
    for (size_t i = 0; i < sizeof(data) / sizeof(short); i++)
      data[i] = __bswap_16(data[i]);

    // Check frame length.
    if (data[0] != 2 * 13 + 2)
      continue;

    // Check sum.
    size_t check_sum = 0x42 + 0x4d;
    for (size_t i = 0; i < sizeof(data) - sizeof(data[0]); i++)
      check_sum += ((unsigned char *) data)[i];
    if (check_sum != data[14])
      continue;

    double data_fp[12];
    for (size_t i = 0; i < sizeof(avg_sum) / sizeof(avg_sum[0]); i++) {
      data_fp[i] = data[i + 1];
      avg_sum[i] += data_fp[i];
    }
    avg_num++;

    // Print each frame if not averaging over time.
    if (!average_over)
      print_data(1, data_fp);
  }

  close(fd);                    // Won't happen...
}
