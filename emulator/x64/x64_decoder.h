#pragma once

#include "x64_instruction.h"

class x64_execution_context;

bool decode_x64_instruction(x64_execution_context& context, x64_decoded_instruction* out_instruction);
