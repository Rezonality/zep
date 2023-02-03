#pragma once
#include "base.h"

typedef struct Oosc_dev Oosc_dev;

typedef enum {
  Oosc_udp_create_error_ok = 0,
  Oosc_udp_create_error_getaddrinfo_failed = 1,
  Oosc_udp_create_error_couldnt_open_socket = 2,
} Oosc_udp_create_error;

Oosc_udp_create_error oosc_dev_create_udp(Oosc_dev **out_ptr,
                                          char const *dest_addr,
                                          char const *dest_port);
void oosc_dev_destroy(Oosc_dev *dev);

// Send a raw UDP datagram.
void oosc_send_datagram(Oosc_dev *dev, char const *data, Usz size);

// Send a list/array of 32-bit integers in OSC format to the specified "osc
// address" (a path like /foo) as a UDP datagram.
void oosc_send_int32s(Oosc_dev *dev, char const *osc_address, I32 const *vals,
                      Usz count);

// Susnote is for handling MIDI note sustains -- each MIDI on event should be
// matched with a MIDI note-off event. The duration/sustain length of a MIDI
// note is specified when it is first triggered, so the orca VM itself is not
// responsible for sending the note-off event. We keep a list of currently 'on'
// notes so that they can have a matching 'off' sent at the correct time.
typedef struct {
  float remaining;
  U16 chan_note;
} Susnote;

typedef struct {
  Susnote *buffer;
  Usz count, capacity;
} Susnote_list;

void susnote_list_init(Susnote_list *sl);
void susnote_list_deinit(Susnote_list *sl);
void susnote_list_clear(Susnote_list *sl);
void susnote_list_add_notes(Susnote_list *sl, Susnote const *restrict notes,
                            Usz count, Usz *restrict start_removed,
                            Usz *restrict end_removed);
void susnote_list_advance_time(
    Susnote_list *sl, double delta_time, Usz *restrict start_removed,
    Usz *restrict end_removed,
    // 1.0 if no notes remain or none are shorter than 1.0
    double *soonest_deadline);
void susnote_list_remove_by_chan_mask(Susnote_list *sl, Usz chan_mask,
                                      Usz *restrict start_removed,
                                      Usz *restrict end_removed);

// Returns 1.0 if no notes remain or none are shorter than 1.0
double susnote_list_soonest_deadline(Susnote_list const *sl);
