#include <iio.h>

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <math.h>
#include <stdio.h>
#include <unistd.h>

/* build and use example
gcc -Wall -g plutorec.c -liio -lm -o plutorec && ./plutorec 446093750 3000000 69 100 # && mks.opt output.iq -type bin 1 s16le  # exectest
 */

#define IIO_ENSURE(expr) { \
    if (!(expr)) { \
            (void) fprintf(stderr, "assertion failed (%s:%d)\n", __FILE__, __LINE__); \
            (void) abort(); \
    } \
  }

double pow2db(double x) { return 10. * log(x) / log(10.); }

double sqr(double x) { return x*x; }

enum dmode {
  DUMP_FINAL,
  DUMP_CONTINUOUS,
};

double receive(struct iio_context *ctx, long ns, enum dmode dm, int fdo)
{
  struct iio_device *dev;
  struct iio_channel *rx0_i, *rx0_q;
  struct iio_buffer *rxbuf;
  const int bsize = 4096;
  int16_t *buf=malloc(2*2*bsize);

  dev = iio_context_find_device(ctx, "cf-ad9361-lpc");

  rx0_i = iio_device_find_channel(dev, "voltage0", 0);
  rx0_q = iio_device_find_channel(dev, "voltage1", 0);

  iio_channel_enable(rx0_i);
  iio_channel_enable(rx0_q);

  rxbuf = iio_device_create_buffer(dev, bsize, false);
  if (!rxbuf) {
    perror("Could not create RX buffer");
    exit(255);
  }

  int ic=0;
  long maxi=0, maxq=0;
  long mini=0, minq=0;
  double sum2=0.;
  ns = ns / bsize;
  for (int c=0; c<ns; c++) {
    void *p_dat, *p_end, *t_dat;
    ptrdiff_t p_inc;

    sum2 = 0.;
    iio_buffer_refill(rxbuf);

    ic=0;
    p_inc = iio_buffer_step(rxbuf);
    p_end = iio_buffer_end(rxbuf);
    for (p_dat = iio_buffer_first(rxbuf, rx0_i); p_dat < p_end; p_dat += p_inc, t_dat += p_inc, ic++) {
      int16_t i = ((int16_t*)p_dat)[0]; // Real (I)
      int16_t q = ((int16_t*)p_dat)[1]; // Imag (Q)
      /* Process here */
      mini = i<mini?i:mini; minq = q<minq?q:minq;
      maxi = i>maxi?i:maxi; maxq = q>maxq?q:maxq;
      //i = ((i&0xff)<<8) | ((i&0xff00)>>8);
      //q = ((q&0xff)<<8) | ((q&0xff00)>>8);
      sum2 += sqr(i) + sqr(q);
      buf[2*ic] = i;
      buf[2*ic+1] = q;
    }
    write(fdo, buf, 2*2*bsize);
    if (dm == DUMP_CONTINUOUS) printf("%d %.10lg %.10lg\n", 2*(c+1)*bsize, sum2, pow2db(sum2));
  }
  printf("# mini %ld\n# minq %ld\n", mini, minq);
  printf("# maxi %ld\n# maxq %ld\n", maxi, maxq);

  iio_buffer_destroy(rxbuf);
  free(buf);
  return sum2;
}


int main (int argc, char **argv)
{
  struct iio_context *ctx;
  struct iio_device *phy;

  long frequency = 10000000,
       sampling = 240000,
       gain = 69,
       ms = 1000;

  long samples = 0;

  // enum dmode dm = DUMP_CONTINUOUS;
  enum dmode dm;
  if (strstr(argv[0], "plutorecg")) dm = DUMP_FINAL;
  else dm = DUMP_CONTINUOUS;

  if (argc>1) frequency = atol(argv[1]);
  if (argc>2) sampling = atol(argv[2]);
  if (argc>3) gain = atol(argv[3]);
  if (argc>4) ms = atol(argv[4]);

  printf("# frequency %ld\n", frequency);
  printf("# sampling %ld\n", sampling);
  printf("# gain %ld\n", gain);
  printf("# ms %ld\n", ms);

  samples = (sampling * ms) / 1000;

  int ret = 0;

  if (argc>5) {
    ctx = iio_create_context_from_uri(argv[5]);
  }
  else {
    //ctx = iio_create_context_from_uri("local:");
    //ctx = iio_create_context_from_uri("usb:1.4.5");
    //ctx = iio_create_context_from_uri("ip:pluto.local");
    ctx = iio_create_context_from_uri("ip:192.168.2.1");
  }

  if (ctx!=NULL) printf("# ctx created 0x%lx\n", (unsigned long)ctx);
  else { fprintf(stderr, "failed to create context\n"); fflush(stderr); exit(255); }

  phy = iio_context_find_device(ctx, "ad9361-phy");
  if (phy == NULL) { fprintf(stderr, "failed to find device\n"); fflush(stderr); exit(255); }

  ret = iio_channel_attr_write_longlong(
      iio_device_find_channel(phy, "altvoltage0", true),
      "frequency", frequency); /* RX LO frequency */
  printf("# ret freq %d\n", ret);

  ret = iio_channel_attr_write_longlong(
      iio_device_find_channel(phy, "voltage0", false),
      "sampling_frequency", sampling); /* RX baseband rate */
  printf("# ret samp %d\n", ret);

  ret = iio_channel_attr_write(
      iio_device_find_channel(phy, "voltage0", false),
      "gain_control_mode", "manual");
  printf("# ret agc %d\n", ret);

  ret = iio_channel_attr_write_longlong(
      iio_device_find_channel(phy, "voltage0", false),
      "hardwaregain", gain);
  printf("# ret gain %d\n", ret);

  int fdo = open("output.iq",  O_WRONLY|O_CREAT|O_TRUNC, 0644);

  double sum2 = receive(ctx, samples, dm, fdo);

  if (dm == DUMP_FINAL) printf("%lu %lg\n", frequency, pow2db(sum2));

  close(fdo);

  iio_context_destroy(ctx);

  return 0;
}
