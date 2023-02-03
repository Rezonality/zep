#include "vmio.h"

void oevent_list_init(Oevent_list *olist) {
  olist->buffer = NULL;
  olist->count = 0;
  olist->capacity = 0;
}
void oevent_list_deinit(Oevent_list *olist) { free(olist->buffer); }
void oevent_list_clear(Oevent_list *olist) { olist->count = 0; }
void oevent_list_copy(Oevent_list const *src, Oevent_list *dest) {
  Usz src_count = src->count;
  if (dest->capacity < src_count) {
    Usz new_cap = orca_round_up_power2(src_count);
    dest->buffer = realloc(dest->buffer, new_cap * sizeof(Oevent));
    dest->capacity = new_cap;
  }
  memcpy(dest->buffer, src->buffer, src_count * sizeof(Oevent));
  dest->count = src_count;
}
Oevent *oevent_list_alloc_item(Oevent_list *olist) {
  Usz count = olist->count;
  if (olist->capacity == count) {
    // Note: no overflow check, but you're probably out of memory if this
    // happens anyway. Like other uses of realloc in orca, we also don't check
    // for a failed allocation.
    Usz capacity = count < 16 ? 16 : orca_round_up_power2(count + 1);
    olist->buffer = realloc(olist->buffer, capacity * sizeof(Oevent));
    olist->capacity = capacity;
  }
  Oevent *result = olist->buffer + count;
  olist->count = count + 1;
  return result;
}
