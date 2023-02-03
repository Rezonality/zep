#include "sim.h"
#include "gbuffer.h"

//////// Utilities

#define restrict

static Glyph const glyph_table[36] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', //  0-11
    'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', // 12-23
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', // 24-35
};
enum { Glyphs_index_count = sizeof glyph_table };
static inline Glyph glyph_of(Usz index) {
  assert(index < Glyphs_index_count);
  return glyph_table[index];
}

static U8 const index_table[128] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  //   0-15
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  //  16-31
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  //  32-47
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,  //  48-63
    0,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, //  64-79
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 0,  0,  0,  0,  0,  //  80-95
    0,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, //  96-111
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 0,  0,  0,  0,  0}; // 112-127
static ORCA_FORCEINLINE Usz index_of(Glyph c) { return index_table[c & 0x7f]; }

// Reference implementation:
// static Usz index_of(Glyph c) {
//   if (c >= '0' && c <= '9') return (Usz)(c - '0');
//   if (c >= 'A' && c <= 'Z') return (Usz)(c - 'A' + 10);
//   if (c >= 'a' && c <= 'z') return (Usz)(c - 'a' + 10);
//   return 0;
// }

static ORCA_FORCEINLINE bool glyph_is_lowercase(Glyph g) { return g & 1 << 5; }
static ORCA_FORCEINLINE Glyph glyph_lowered_unsafe(Glyph g) {
  return (Glyph)(g | 1 << 5);
}
static inline Glyph glyph_with_case(Glyph g, Glyph caser) {
  enum { Case_bit = 1 << 5, Alpha_bit = 1 << 6 };
  return (Glyph)((g & ~Case_bit) | ((~g & Alpha_bit) >> 1) |
                 (caser & Case_bit));
}

static ORCA_PURE bool oper_has_neighboring_bang(Glyph const *gbuf, Usz h, Usz w,
                                                Usz y, Usz x) {
  Glyph const *gp = gbuf + w * y + x;
  if (x < w - 1 && gp[1] == '*')
    return true;
  if (x > 0 && *(gp - 1) == '*')
    return true;
  if (y < h - 1 && gp[w] == '*')
    return true;
  // note: negative array subscript on rhs of short-circuit, may cause ub if
  // the arithmetic under/overflows, even if guarded the guard on lhs is false
  if (y > 0 && *(gp - w) == '*')
    return true;
  return false;
}

// Returns UINT8_MAX if not a valid note.
static U8 midi_note_number_of(Glyph g) {
  int sharp = (g & 1 << 5) >> 5; // sharp=1 if lowercase
  g &= (Glyph) ~(1 << 5);        // make uppercase
  if (g < 'A' || g > 'Z')        // A through Z only
    return UINT8_MAX;
  // We want C=0, D=1, E=2, etc. A and B are equivalent to H and I.
  int deg = g <= 'B' ? 'G' - 'B' + g - 'A' : g - 'C';
  return (U8)(deg / 7 * 12 + (I8[]){0, 2, 4, 5, 7, 9, 11}[deg % 7] + sharp);
}

typedef struct {
  Glyph *vars_slots;
  Oevent_list *oevent_list;
  Usz random_seed;
} Oper_extra_params;

static void oper_poke_and_stun(Glyph *restrict gbuffer, Mark *restrict mbuffer,
                               Usz height, Usz width, Usz y, Usz x, Isz delta_y,
                               Isz delta_x, Glyph g) {
  Isz y0 = (Isz)y + delta_y;
  Isz x0 = (Isz)x + delta_x;
  if (y0 < 0 || x0 < 0 || (Usz)y0 >= height || (Usz)x0 >= width)
    return;
  Usz offs = (Usz)y0 * width + (Usz)x0;
  gbuffer[offs] = g;
  mbuffer[offs] |= Mark_flag_sleep;
}

// For anyone editing this in the future: the "no inline" here is deliberate.
// You may think that inlining is always faster. Or even just letting the
// compiler decide. You would be wrong. Try it. If you really want this VM to
// run faster, you will need to use computed goto or assembly.
#define OPER_FUNCTION_ATTRIBS ORCA_NOINLINE static void

#define BEGIN_OPERATOR(_oper_name)                                             \
  OPER_FUNCTION_ATTRIBS oper_behavior_##_oper_name(                            \
      Glyph *const restrict gbuffer, Mark *const restrict mbuffer,             \
      Usz const height, Usz const width, Usz const y, Usz const x,             \
      Usz Tick_number, Oper_extra_params *const extra_params,                  \
      Mark const cell_flags, Glyph const This_oper_char) {                     \
    (void)gbuffer;                                                             \
    (void)mbuffer;                                                             \
    (void)height;                                                              \
    (void)width;                                                               \
    (void)y;                                                                   \
    (void)x;                                                                   \
    (void)Tick_number;                                                         \
    (void)extra_params;                                                        \
    (void)cell_flags;                                                          \
    (void)This_oper_char;

#define END_OPERATOR }

#define PEEK(_delta_y, _delta_x)                                               \
  gbuffer_peek_relative(gbuffer, height, width, y, x, _delta_y, _delta_x)
#define POKE(_delta_y, _delta_x, _glyph)                                       \
  gbuffer_poke_relative(gbuffer, height, width, y, x, _delta_y, _delta_x,      \
                        _glyph)
#define STUN(_delta_y, _delta_x)                                               \
  mbuffer_poke_relative_flags_or(mbuffer, height, width, y, x, _delta_y,       \
                                 _delta_x, Mark_flag_sleep)
#define POKE_STUNNED(_delta_y, _delta_x, _glyph)                               \
  oper_poke_and_stun(gbuffer, mbuffer, height, width, y, x, _delta_y,          \
                     _delta_x, _glyph)
#define LOCK(_delta_y, _delta_x)                                               \
  mbuffer_poke_relative_flags_or(mbuffer, height, width, y, x, _delta_y,       \
                                 _delta_x, Mark_flag_lock)

#define IN Mark_flag_input
#define OUT Mark_flag_output
#define NONLOCKING Mark_flag_lock
#define PARAM Mark_flag_haste_input

#define LOWERCASE_REQUIRES_BANG                                                \
  if (glyph_is_lowercase(This_oper_char) &&                                    \
      !oper_has_neighboring_bang(gbuffer, height, width, y, x))                \
  return

#define STOP_IF_NOT_BANGED                                                     \
  if (!oper_has_neighboring_bang(gbuffer, height, width, y, x))                \
  return

#define PORT(_delta_y, _delta_x, _flags)                                       \
  mbuffer_poke_relative_flags_or(mbuffer, height, width, y, x, _delta_y,       \
                                 _delta_x, (_flags) ^ Mark_flag_lock)
//////// Operators

#define UNIQUE_OPERATORS(_)                                                    \
  _('!', midicc)                                                               \
  _('#', comment)                                                              \
  _('%', midi)                                                                 \
  _('*', bang)                                                                 \
  _(':', midi)                                                                 \
  _(';', udp)                                                                  \
  _('=', osc)                                                                  \
  _('?', midipb)

#define ALPHA_OPERATORS(_)                                                     \
  _('A', add)                                                                  \
  _('B', subtract)                                                               \
  _('C', clock)                                                                \
  _('D', delay)                                                                \
  _('E', movement)                                                             \
  _('F', if)                                                                   \
  _('G', generator)                                                            \
  _('H', halt)                                                                 \
  _('I', increment)                                                            \
  _('J', jump)                                                                 \
  _('K', konkat)                                                               \
  _('L', lesser)                                                               \
  _('M', multiply)                                                             \
  _('N', movement)                                                             \
  _('O', offset)                                                               \
  _('P', push)                                                                 \
  _('Q', query)                                                                \
  _('R', random)                                                               \
  _('S', movement)                                                             \
  _('T', track)                                                                \
  _('U', uclid)                                                                \
  _('V', variable)                                                             \
  _('W', movement)                                                             \
  _('X', teleport)                                                             \
  _('Y', yump)                                                                 \
  _('Z', lerp)

BEGIN_OPERATOR(movement)
  if (glyph_is_lowercase(This_oper_char) &&
      !oper_has_neighboring_bang(gbuffer, height, width, y, x))
    return;
  Isz delta_y, delta_x;
  switch (glyph_lowered_unsafe(This_oper_char)) {
  case 'n':
    delta_y = -1;
    delta_x = 0;
    break;
  case 'e':
    delta_y = 0;
    delta_x = 1;
    break;
  case 's':
    delta_y = 1;
    delta_x = 0;
    break;
  case 'w':
    delta_y = 0;
    delta_x = -1;
    break;
  default:
    // could cause strict aliasing problem, maybe
    delta_y = 0;
    delta_x = 0;
    break;
  }
  Isz y0 = (Isz)y + delta_y;
  Isz x0 = (Isz)x + delta_x;
  if (y0 >= (Isz)height || x0 >= (Isz)width || y0 < 0 || x0 < 0) {
    gbuffer[y * width + x] = '*';
    return;
  }
  Glyph *restrict g_at_dest = gbuffer + (Usz)y0 * width + (Usz)x0;
  if (*g_at_dest == '.') {
    *g_at_dest = This_oper_char;
    gbuffer[y * width + x] = '.';
    mbuffer[(Usz)y0 * width + (Usz)x0] |= Mark_flag_sleep;
  } else {
    gbuffer[y * width + x] = '*';
  }
END_OPERATOR

BEGIN_OPERATOR(midicc)
  for (Usz i = 1; i < 4; ++i) {
    PORT(0, (Isz)i, IN);
  }
  STOP_IF_NOT_BANGED;
  Glyph channel_g = PEEK(0, 1);
  Glyph control_g = PEEK(0, 2);
  Glyph value_g = PEEK(0, 3);
  if (channel_g == '.' || control_g == '.')
    return;
  Usz channel = index_of(channel_g);
  if (channel > 15)
    return;
  Oevent_midi_cc *oe =
      (Oevent_midi_cc *)oevent_list_alloc_item(extra_params->oevent_list);
  oe->oevent_type = Oevent_type_midi_cc;
  oe->channel = (U8)channel;
  oe->control = (U8)index_of(control_g);
  oe->value = (U8)(index_of(value_g) * 127 / 35); // 0~35 -> 0~127
END_OPERATOR

BEGIN_OPERATOR(comment)
  // restrict probably ok here...
  Glyph const *restrict gline = gbuffer + y * width;
  Mark *restrict mline = mbuffer + y * width;
  Usz max_x = x + 255;
  if (width < max_x)
    max_x = width;
  for (Usz x0 = x + 1; x0 < max_x; ++x0) {
    Glyph g = gline[x0];
    mline[x0] |= (Mark)Mark_flag_lock;
    if (g == '#')
      break;
  }
END_OPERATOR

BEGIN_OPERATOR(bang)
  gbuffer_poke(gbuffer, height, width, y, x, '.');
END_OPERATOR

BEGIN_OPERATOR(midi)
  for (Usz i = 1; i < 6; ++i) {
    PORT(0, (Isz)i, IN);
  }
  STOP_IF_NOT_BANGED;
  Glyph channel_g = PEEK(0, 1);
  Glyph octave_g = PEEK(0, 2);
  Glyph note_g = PEEK(0, 3);
  Glyph velocity_g = PEEK(0, 4);
  Glyph length_g = PEEK(0, 5);
  U8 octave_num = (U8)index_of(octave_g);
  if (octave_num == 0)
    return;
  if (octave_num > 9)
    octave_num = 9;
  U8 note_num = midi_note_number_of(note_g);
  if (note_num == UINT8_MAX)
    return;
  Usz channel_num = index_of(channel_g);
  if (channel_num > 15)
    channel_num = 15;
  Usz vel_num;
  if (velocity_g == '.') {
    // If no velocity is specified, set it to full.
    vel_num = 127;
  } else {
    vel_num = index_of(velocity_g);
    // MIDI notes with velocity zero are actually note-offs. (MIDI has two ways
    // to send note offs. Zero-velocity is the alternate way.) If there is a zero
    // velocity, we'll just not do anything.
    if (vel_num == 0)
      return;
    vel_num = vel_num * 8 - 1; // 1~16 -> 7~127
    if (vel_num > 127)
      vel_num = 127;
  }
  Oevent_midi_note *oe =
      (Oevent_midi_note *)oevent_list_alloc_item(extra_params->oevent_list);
  oe->oevent_type = (U8)Oevent_type_midi_note;
  oe->channel = (U8)channel_num;
  oe->octave = octave_num;
  oe->note = note_num;
  oe->velocity = (U8)vel_num;
  // Mask used here to suppress bad GCC Wconversion for bitfield. This is bad
  // -- we should do something smarter than this.
  oe->duration = (U8)(index_of(length_g) & 0x7Fu);
  oe->mono = This_oper_char == '%' ? 1 : 0;
END_OPERATOR

BEGIN_OPERATOR(udp)
  Usz n = width - x - 1;
  if (n > 16)
    n = 16;
  Glyph const *restrict gline = gbuffer + y * width + x + 1;
  Mark *restrict mline = mbuffer + y * width + x + 1;
  Glyph cpy[Oevent_udp_string_count];
  Usz i;
  for (i = 0; i < n; ++i) {
    Glyph g = gline[i];
    if (g == '.')
      break;
    cpy[i] = g;
    mline[i] |= Mark_flag_lock;
  }
  n = i;
  STOP_IF_NOT_BANGED;
  Oevent_udp_string *oe =
      (Oevent_udp_string *)oevent_list_alloc_item(extra_params->oevent_list);
  oe->oevent_type = (U8)Oevent_type_udp_string;
  oe->count = (U8)n;
  for (i = 0; i < n; ++i) {
    oe->chars[i] = cpy[i];
  }
END_OPERATOR

BEGIN_OPERATOR(osc)
  PORT(0, 1, IN | PARAM);
  PORT(0, 2, IN | PARAM);
  Usz len = index_of(PEEK(0, 2));
  if (len > Oevent_osc_int_count)
    len = Oevent_osc_int_count;
  for (Usz i = 0; i < len; ++i) {
    PORT(0, (Isz)i + 3, IN);
  }
  STOP_IF_NOT_BANGED;
  Glyph g = PEEK(0, 1);
  if (g != '.') {
    U8 buff[Oevent_osc_int_count];
    for (Usz i = 0; i < len; ++i) {
      buff[i] = (U8)index_of(PEEK(0, (Isz)i + 3));
    }
    Oevent_osc_ints *oe =
        &oevent_list_alloc_item(extra_params->oevent_list)->osc_ints;
    oe->oevent_type = (U8)Oevent_type_osc_ints;
    oe->glyph = g;
    oe->count = (U8)len;
    for (Usz i = 0; i < len; ++i) {
      oe->numbers[i] = buff[i];
    }
  }
END_OPERATOR

BEGIN_OPERATOR(midipb)
  for (Usz i = 1; i < 4; ++i) {
    PORT(0, (Isz)i, IN);
  }
  STOP_IF_NOT_BANGED;
  Glyph channel_g = PEEK(0, 1);
  Glyph msb_g = PEEK(0, 2);
  Glyph lsb_g = PEEK(0, 3);
  if (channel_g == '.')
    return;
  Usz channel = index_of(channel_g);
  if (channel > 15)
    return;
  Oevent_midi_pb *oe =
      (Oevent_midi_pb *)oevent_list_alloc_item(extra_params->oevent_list);
  oe->oevent_type = Oevent_type_midi_pb;
  oe->channel = (U8)channel;
  oe->msb = (U8)(index_of(msb_g) * 127 / 35); // 0~35 -> 0~127
  oe->lsb = (U8)(index_of(lsb_g) * 127 / 35);
END_OPERATOR

BEGIN_OPERATOR(add)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Glyph a = PEEK(0, -1);
  Glyph b = PEEK(0, 1);
  Glyph g = glyph_table[(index_of(a) + index_of(b)) % Glyphs_index_count];
  POKE(1, 0, glyph_with_case(g, b));
END_OPERATOR

BEGIN_OPERATOR(subtract)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Glyph a = PEEK(0, -1);
  Glyph b = PEEK(0, 1);
  Isz val = (Isz)index_of(b) - (Isz)index_of(a);
  if (val < 0)
    val = -val;
  POKE(1, 0, glyph_with_case(glyph_of((Usz)val), b));
END_OPERATOR

BEGIN_OPERATOR(clock)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Usz rate = index_of(PEEK(0, -1));
  Usz mod_num = index_of(PEEK(0, 1));
  if (rate == 0)
    rate = 1;
  if (mod_num == 0)
    mod_num = 8;
  Glyph g = glyph_of(Tick_number / rate % mod_num);
  POKE(1, 0, g);
END_OPERATOR

BEGIN_OPERATOR(delay)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Usz rate = index_of(PEEK(0, -1));
  Usz mod_num = index_of(PEEK(0, 1));
  if (rate == 0)
    rate = 1;
  if (mod_num == 0)
    mod_num = 8;
  Glyph g = Tick_number % (rate * mod_num) == 0 ? '*' : '.';
  POKE(1, 0, g);
END_OPERATOR

BEGIN_OPERATOR(if)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Glyph g0 = PEEK(0, -1);
  Glyph g1 = PEEK(0, 1);
  POKE(1, 0, g0 == g1 ? '*' : '.');
END_OPERATOR

BEGIN_OPERATOR(generator)
  LOWERCASE_REQUIRES_BANG;
  Isz out_x = (Isz)index_of(PEEK(0, -3));
  Isz out_y = (Isz)index_of(PEEK(0, -2)) + 1;
  Isz len = (Isz)index_of(PEEK(0, -1));
  PORT(0, -3, IN | PARAM); // x
  PORT(0, -2, IN | PARAM); // y
  PORT(0, -1, IN | PARAM); // len
  for (Isz i = 0; i < len; ++i) {
    PORT(0, i + 1, IN);
    PORT(out_y, out_x + i, OUT | NONLOCKING);
    Glyph g = PEEK(0, i + 1);
    POKE_STUNNED(out_y, out_x + i, g);
  }
END_OPERATOR

BEGIN_OPERATOR(halt)
  LOWERCASE_REQUIRES_BANG;
  PORT(1, 0, OUT);
END_OPERATOR

BEGIN_OPERATOR(increment)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, IN | OUT);
  Glyph ga = PEEK(0, -1);
  Glyph gb = PEEK(0, 1);
  Usz rate = 1;
  if (ga != '.' && ga != '*')
    rate = index_of(ga);
  Usz max = index_of(gb);
  Usz val = index_of(PEEK(1, 0));
  if (max == 0)
    max = 36;
  val = val + rate;
  val = val % max;
  POKE(1, 0, glyph_with_case(glyph_of(val), gb));
END_OPERATOR

BEGIN_OPERATOR(jump)
  LOWERCASE_REQUIRES_BANG;
  PORT(-1, 0, IN);
  PORT(1, 0, OUT);
  POKE(1, 0, PEEK(-1, 0));
END_OPERATOR

// Note: this is merged from a pull request without being fully tested or
// optimized
BEGIN_OPERATOR(konkat)
  LOWERCASE_REQUIRES_BANG;
  Isz len = (Isz)index_of(PEEK(0, -1));
  if (len == 0)
    len = 1;
  PORT(0, -1, IN | PARAM);
  for (Isz i = 0; i < len; ++i) {
    PORT(0, i + 1, IN);
    Glyph var = PEEK(0, i + 1);
    if (var != '.') {
      Usz var_idx = index_of(var);
      if (var_idx != 0) {
        Glyph result = extra_params->vars_slots[var_idx];
        PORT(1, i + 1, OUT);
        POKE(1, i + 1, result);
      }
    }
  }
END_OPERATOR

BEGIN_OPERATOR(lesser)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Glyph ga = PEEK(0, -1);
  Glyph gb = PEEK(0, 1);
  if (ga == '.' || gb == '.') {
    POKE(1, 0, '.');
  } else {
    Usz ia = index_of(ga);
    Usz ib = index_of(gb);
    Usz out = ia < ib ? ia : ib;
    POKE(1, 0, glyph_with_case(glyph_of(out), gb));
  }
END_OPERATOR

BEGIN_OPERATOR(multiply)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Glyph a = PEEK(0, -1);
  Glyph b = PEEK(0, 1);
  Glyph g = glyph_table[(index_of(a) * index_of(b)) % Glyphs_index_count];
  POKE(1, 0, glyph_with_case(g, b));
END_OPERATOR

BEGIN_OPERATOR(offset)
  LOWERCASE_REQUIRES_BANG;
  Isz in_x = (Isz)index_of(PEEK(0, -2)) + 1;
  Isz in_y = (Isz)index_of(PEEK(0, -1));
  PORT(0, -1, IN | PARAM);
  PORT(0, -2, IN | PARAM);
  PORT(in_y, in_x, IN);
  PORT(1, 0, OUT);
  POKE(1, 0, PEEK(in_y, in_x));
END_OPERATOR

BEGIN_OPERATOR(push)
  LOWERCASE_REQUIRES_BANG;
  Usz key = index_of(PEEK(0, -2));
  Usz len = index_of(PEEK(0, -1));
  PORT(0, -1, IN | PARAM);
  PORT(0, -2, IN | PARAM);
  PORT(0, 1, IN);
  if (len == 0)
    return;
  Isz out_x = (Isz)(key % len);
  for (Usz i = 0; i < len; ++i) {
    LOCK(1, (Isz)i);
  }
  PORT(1, out_x, OUT);
  POKE(1, out_x, PEEK(0, 1));
END_OPERATOR

BEGIN_OPERATOR(query)
  LOWERCASE_REQUIRES_BANG;
  Isz in_x = (Isz)index_of(PEEK(0, -3)) + 1;
  Isz in_y = (Isz)index_of(PEEK(0, -2));
  Isz len = (Isz)index_of(PEEK(0, -1));
  Isz out_x = 1 - len;
  PORT(0, -3, IN | PARAM); // x
  PORT(0, -2, IN | PARAM); // y
  PORT(0, -1, IN | PARAM); // len
  // todo direct buffer manip
  for (Isz i = 0; i < len; ++i) {
    PORT(in_y, in_x + i, IN);
    PORT(1, out_x + i, OUT);
    Glyph g = PEEK(in_y, in_x + i);
    POKE(1, out_x + i, g);
  }
END_OPERATOR

BEGIN_OPERATOR(random)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Usz a = index_of(PEEK(0, -1));
  Usz b = index_of(PEEK(0, 1));
  if (b == 0)
    b = 36;
  Usz min, max;
  if (a == b) {
    POKE(1, 0, glyph_of(a));
    return;
  } else if (a < b) {
    min = a;
    max = b;
  } else {
    min = b;
    max = a;
  }
  // Initial input params for the hash
  Usz key = (extra_params->random_seed + y * width + x) ^
            (Tick_number << UINT32_C(16));
  // 32-bit shift_mult hash to evenly distribute bits
  key = (key ^ UINT32_C(61)) ^ (key >> UINT32_C(16));
  key = key + (key << UINT32_C(3));
  key = key ^ (key >> UINT32_C(4));
  key = key * UINT32_C(0x27d4eb2d);
  key = key ^ (key >> UINT32_C(15));
  // Hash finished. Restrict to desired range of numbers.
  Usz val = key % (max - min) + min;
  POKE(1, 0, glyph_of(val));
END_OPERATOR

BEGIN_OPERATOR(track)
  LOWERCASE_REQUIRES_BANG;
  Usz key = index_of(PEEK(0, -2));
  Usz len = index_of(PEEK(0, -1));
  PORT(0, -2, IN | PARAM);
  PORT(0, -1, IN | PARAM);
  if (len == 0)
    return;
  Isz read_val_x = (Isz)(key % len) + 1;
  for (Usz i = 0; i < len; ++i) {
    LOCK(0, (Isz)(i + 1));
  }
  PORT(0, (Isz)read_val_x, IN);
  PORT(1, 0, OUT);
  POKE(1, 0, PEEK(0, read_val_x));
END_OPERATOR

// https://www.computermusicdesign.com/
// simplest-euclidean-rhythm-algorithm-explained/
BEGIN_OPERATOR(uclid)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Glyph left = PEEK(0, -1);
  Usz steps = 1;
  if (left != '.' && left != '*')
    steps = index_of(left);
  Usz max = index_of(PEEK(0, 1));
  if (max == 0)
    max = 8;
  Usz bucket = (steps * (Tick_number + max - 1)) % max + steps;
  Glyph g = (bucket >= max) ? '*' : '.';
  POKE(1, 0, g);
END_OPERATOR

BEGIN_OPERATOR(variable)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  Glyph left = PEEK(0, -1);
  Glyph right = PEEK(0, 1);
  if (left != '.') {
    // Write
    Usz var_idx = index_of(left);
    extra_params->vars_slots[var_idx] = right;
  } else if (right != '.') {
    // Read
    PORT(1, 0, OUT);
    Usz var_idx = index_of(right);
    Glyph result = extra_params->vars_slots[var_idx];
    POKE(1, 0, result);
  }
END_OPERATOR

BEGIN_OPERATOR(teleport)
  LOWERCASE_REQUIRES_BANG;
  Isz out_x = (Isz)index_of(PEEK(0, -2));
  Isz out_y = (Isz)index_of(PEEK(0, -1)) + 1;
  PORT(0, -2, IN | PARAM); // x
  PORT(0, -1, IN | PARAM); // y
  PORT(0, 1, IN);
  PORT(out_y, out_x, OUT | NONLOCKING);
  POKE_STUNNED(out_y, out_x, PEEK(0, 1));
END_OPERATOR

BEGIN_OPERATOR(yump)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN);
  PORT(0, 1, OUT);
  POKE(0, 1, PEEK(0, -1));
END_OPERATOR

BEGIN_OPERATOR(lerp)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, IN | OUT);
  Glyph g = PEEK(0, -1);
  Isz rate = g == '.' || g == '*' ? 1 : (Isz)index_of(g);
  Isz goal = (Isz)index_of(PEEK(0, 1));
  Isz val = (Isz)index_of(PEEK(1, 0));
  Isz mod = val <= goal - rate ? rate : val >= goal + rate ? -rate : goal - val;
  POKE(1, 0, glyph_of((Usz)(val + mod)));
END_OPERATOR

//////// Run simulation

void orca_run(Glyph *restrict gbuf, Mark *restrict mbuf, Usz height, Usz width,
              Usz tick_number, Oevent_list *oevent_list, Usz random_seed) {
  Glyph vars_slots[Glyphs_index_count];
  memset(vars_slots, '.', sizeof(vars_slots));
  Oper_extra_params extras;
  extras.vars_slots = &vars_slots[0];
  extras.oevent_list = oevent_list;
  extras.random_seed = random_seed;

  for (Usz iy = 0; iy < height; ++iy) {
    Glyph const *glyph_row = gbuf + iy * width;
    Mark const *mark_row = mbuf + iy * width;
    for (Usz ix = 0; ix < width; ++ix) {
      Glyph glyph_char = glyph_row[ix];
      if (ORCA_LIKELY(glyph_char == '.'))
        continue;
      Mark cell_flags = mark_row[ix] & (Mark_flag_lock | Mark_flag_sleep);
      if (cell_flags & (Mark_flag_lock | Mark_flag_sleep))
        continue;
      switch (glyph_char) {
#define UNIQUE_CASE(_oper_char, _oper_name)                                    \
  case _oper_char:                                                             \
    oper_behavior_##_oper_name(gbuf, mbuf, height, width, iy, ix, tick_number, \
                               &extras, cell_flags, glyph_char);               \
    break;

#define ALPHA_CASE(_upper_oper_char, _oper_name)                               \
  case _upper_oper_char:                                                       \
  case (char)(_upper_oper_char | 1 << 5):                                      \
    oper_behavior_##_oper_name(gbuf, mbuf, height, width, iy, ix, tick_number, \
                               &extras, cell_flags, glyph_char);               \
    break;
        UNIQUE_OPERATORS(UNIQUE_CASE)
        ALPHA_OPERATORS(ALPHA_CASE)
#undef UNIQUE_CASE
#undef ALPHA_CASE
      }
    }
  }
}
