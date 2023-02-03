#include "base.h"
#include "field.h"
#include "gbuffer.h"
#include "sim.h"
#include "vmio.h"
#include <getopt.h>

static ORCA_NOINLINE void usage(void) { // clang-format off
fprintf(stderr,
"Usage: cli [options] infile\n\n"
"Options:\n"
"    -t <number>   Number of timesteps to simulate.\n"
"                  Must be 0 or a positive integer.\n"
"                  Default: 1\n"
"    -q or --quiet Don't print the result to stdout.\n"
"    -h or --help  Print this message and exit.\n"
);} // clang-format on

int main(int argc, char **argv) {
  static struct option cli_options[] = {{"help", no_argument, 0, 'h'},
                                        {"quiet", no_argument, 0, 'q'},
                                        {NULL, 0, NULL, 0}};

  char *input_file = NULL;
  int ticks = 1;
  bool print_output = true;

  for (;;) {
    int c = getopt_long(argc, argv, "t:qh", cli_options, NULL);
    if (c == -1)
      break;
    switch (c) {
    case 't':
      ticks = atoi(optarg);
      if (ticks == 0 && strcmp(optarg, "0")) {
        fprintf(stderr,
                "Bad timestep argument %s.\n"
                "Must be 0 or a positive integer.\n",
                optarg);
        return 1;
      }
      break;
    case 'q':
      print_output = false;
      break;
    case 'h':
      usage();
      return 0;
    case '?':
      usage();
      return 1;
    }
  }

  if (optind == argc - 1) {
    input_file = argv[optind];
  } else if (optind < argc - 1) {
    fprintf(stderr, "Expected only 1 file argument.\n");
    usage();
    return 1;
  }

  if (input_file == NULL) {
    fprintf(stderr, "No input file.\n");
    usage();
    return 1;
  }
  if (ticks < 0) {
    fprintf(stderr, "Time must be >= 0.\n");
    usage();
    return 1;
  }

  Field field;
  field_init(&field);
  Field_load_error fle = field_load_file(input_file, &field);
  if (fle != Field_load_error_ok) {
    field_deinit(&field);
    fprintf(stderr, "File load error: %s.\n", field_load_error_string(fle));
    return 1;
  }
  Mbuf_reusable mbuf_r;
  mbuf_reusable_init(&mbuf_r);
  mbuf_reusable_ensure_size(&mbuf_r, field.height, field.width);
  Oevent_list oevent_list;
  oevent_list_init(&oevent_list);
  Usz max_ticks = (Usz)ticks;
  for (Usz i = 0; i < max_ticks; ++i) {
    mbuffer_clear(mbuf_r.buffer, field.height, field.width);
    oevent_list_clear(&oevent_list);
    orca_run(field.buffer, mbuf_r.buffer, field.height, field.width, i,
             &oevent_list, 0);
  }
  mbuf_reusable_deinit(&mbuf_r);
  oevent_list_deinit(&oevent_list);
  if (print_output)
    field_fput(&field, stdout);
  field_deinit(&field);
  return 0;
}
