#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include "game_room.h"

bool log_generate_match_report(const GameRoom *room, char *log_content, size_t max_size);
bool log_save_to_disk(const char *filename, const char *content);

#endif
