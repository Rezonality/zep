#pragma once
#include "base.h"
#include "vmio.h"

void orca_run(Glyph* gbuffer, Mark* mbuffer, Usz height,
              Usz width, Usz tick_number, Oevent_list *oevent_list,
              Usz random_seed);
