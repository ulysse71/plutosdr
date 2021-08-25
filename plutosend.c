#include <iio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <math.h>
#include <stdio.h>
#include <unistd.h>

/* build and use example
gcc -Wall -g plutosend.c -liio -lm -o plutosend && ./plutosend 10000000 # exectest
 */

#define IIO_ENSURE(expr) { \
    if (!(expr)) { \
            (void) fprintf(stderr, "assertion failed (%s:%d)\n", __FILE__, __LINE__); \
            (void) abort(); \
    } \
  }

double pow2db(double x) { return 10. * log(x) / log(10.); }

void send(struct iio_context *ctx, long ns, int fdi)
{
  struct iio_device *dev;
  // struct iio_device *phy;
  struct iio_channel *tx0_i, *tx0_q;
  struct iio_buffer *txbuf;
  int bsize = 4096;
  long nb = 0;

  dev = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");
  // phy = iio_context_find_device(ctx, "ad9361-phy");

  tx0_i = iio_device_find_channel(dev, "voltage0", 1);
  tx0_q = iio_device_find_channel(dev, "voltage1", 1);

  iio_channel_enable(tx0_i);
  iio_channel_enable(tx0_q);

  txbuf = iio_device_create_buffer(dev, bsize, 0);
  if (!txbuf) {
    perror("Could not create RX buffer");
    exit(255);
  }

  int i=0, nr=0;
  int16_t *buf = (int16_t*)malloc(2*bsize*sizeof(int16_t));
  ns = ns / bsize;
  for (int c=0; c<ns; c++) {
    void *p_dat, *p_end, *t_dat;
    ptrdiff_t p_inc;

    nr = read(fdi, buf, 2*bsize*sizeof(int16_t));
    if (nr<4*bsize) break;  // end of file

    i=0;
    p_inc = iio_buffer_step(txbuf);
    p_end = iio_buffer_end(txbuf);
    for (p_dat = iio_buffer_first(txbuf, tx0_i); p_dat < p_end; p_dat += p_inc, t_dat += p_inc, i++) {
      ((int16_t*)p_dat)[0] = buf[2*i]; // Real (I)
      ((int16_t*)p_dat)[1] = buf[2*i+1]; // Imag (Q)
    }
    if (i!=bsize)  printf(" && i %d\n", i);

    nb = iio_buffer_push(txbuf);
    if (nb<0) printf("nb %ld\n", nb);
    printf(".");
  }
  printf("\n");

  iio_buffer_destroy(txbuf);
}


int main (int argc, char **argv)
{
  struct iio_context *ctx;
  struct iio_device *dds;
  struct iio_device *phy;

  long frequency = 10000000,
       sampling = 3000000,
       atten = 10,
       ms = 1000;

  long samples = 0;

  if (argc>1) frequency = atoi(argv[1]);
  if (argc>2) sampling = atoi(argv[2]);
  if (argc>3) atten = atoi(argv[3]);
  if (argc>4) ms = atoi(argv[4]);

  printf("# frequency %ld\n", frequency);
  printf("# sampling %ld\n", sampling);
  printf("# atten %ld\n", atten);
  printf("# ms %ld\n", ms);

  samples = (sampling * ms) / 1000;

  int ret = 0;

  if (argc>5) {
    ctx = iio_create_context_from_uri(argv[5]);
  }
  else {
    //ctx = iio_create_context_from_uri("local:");
    ctx = iio_create_context_from_uri("ip:192.168.2.1");
    //ctx = iio_create_context_from_uri("usb:1.4.5");
  }

  if (ctx!=NULL) printf("# ctx created 0x%lx\n", (unsigned long)ctx);
  else { fprintf(stderr, "failed to create context\n"); fflush(stderr); exit(255); }

  phy = iio_context_find_device(ctx, "ad9361-phy");
  if (phy == NULL) { fprintf(stderr, "failed to find device\n"); fflush(stderr); exit(254); }

  dds = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");
  if (dds == NULL) { fprintf(stderr, "failed to find device\n"); fflush(stderr); exit(255); }

  ret = iio_channel_attr_write_longlong(
      iio_device_find_channel(phy, "voltage0", true),
      "sampling_frequency", sampling); /* TX baseband rate */
  printf("# ret samp %d\n", ret);

  ret = iio_channel_attr_write_longlong(
      iio_device_find_channel(phy, "voltage0", true),
      "hardwaregain", atten); /* TX atten */
  printf("# ret atten %d\n", ret);

  ret = iio_channel_attr_write_longlong(
      iio_device_find_channel(phy, "altvoltage1", true),
      "frequency", frequency); /* TX LO frequency */
  printf("# ret freq %d\n", ret);

  ret = iio_channel_attr_write_double(
      iio_device_find_channel(dds, "altvoltage1", true),
      "scale", 1.);
  printf("# ret scale %d\n", ret);

  ret = iio_channel_attr_write_bool(
      iio_device_find_channel(dds, "altvoltage1", true),
      "raw", 1);
  printf("# ret raw %d\n", ret);

  int fdi = open("input.iq",  O_RDONLY);

  send(ctx, samples, fdi);

  close(fdi);

  iio_context_destroy(ctx);

  return 0;
}
