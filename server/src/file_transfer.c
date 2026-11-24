#include "file_transfer.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(d) _mkdir(d)
#else
    #define MKDIR(d) mkdir(d, 0755)
#endif

bool log_save_to_disk(const char *filename, const char *content) {
    MKDIR("logs");
    MKDIR("server/logs");

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        char fallback[256];
        snprintf(fallback, sizeof(fallback), "logs/%s", filename);
        fp = fopen(fallback, "w");
    }
    if (!fp) return false;

    fputs(content, fp);
    fclose(fp);
    return true;
}

bool log_generate_match_report(const GameRoom *room, char *log_content, size_t max_size) {
    if (!room || !log_content || max_size == 0) return false;

    char start_str[32] = "N/A";
    char end_str[32] = "N/A";
    struct tm *tm_info;

    if (room->start_time > 0) {
        tm_info = localtime(&room->start_time);
        strftime(start_str, sizeof(start_str), "%Y-%m-%d %H:%M:%S", tm_info);
    }
    time_t end_t = (room->end_time > 0) ? room->end_time : time(NULL);
    tm_info = localtime(&end_t);
    strftime(end_str, sizeof(end_str), "%Y-%m-%d %H:%M:%S", tm_info);

    int duration_sec = (int)difftime(end_t, room->start_time);
    if (duration_sec < 0) duration_sec = 0;

    const char *res_str = "ONGOING";
    if (room->result == GAME_RESULT_WHITE_WIN) res_str = "WHITE_WIN";
    else if (room->result == GAME_RESULT_BLACK_WIN) res_str = "BLACK_WIN";
    else if (room->result == GAME_RESULT_DRAW) res_str = "DRAW";

    int offset = snprintf(log_content, max_size,
        "============================================================\n"
        "                 NETCHESS MATCH REPORT LOG                  \n"
        "============================================================\n"
        "Match ID:        MATCH_%04d\n"
        "Start Time:      %s\n"
        "End Time:        %s\n"
        "Duration:        %d min %d sec\n"
        "White Player:    %s (IP: %s)\n"
        "Black Player:    %s (IP: %s)\n"
        "Match Result:    %s\n"
        "Reason:          %s\n"
        "Total Moves:     %d\n"
        "------------------------------------------------------------\n"
        "MOVE HISTORY:\n",
        room->room_id, start_str, end_str,
        duration_sec / 60, duration_sec % 60,
        room->white_user, room->white_ip,
        room->black_user, room->black_ip,
        res_str, room->result_reason,
        room->move_count
    );

    for (int i = 0; i < room->move_count; i++) {
        if (offset >= (int)max_size - 64) break;
        if (i % 2 == 0) {
            offset += snprintf(log_content + offset, max_size - offset,
                               "%3d. %-8s", (i / 2) + 1, room->moves_san[i]);
        } else {
            offset += snprintf(log_content + offset, max_size - offset,
                               " %-8s\n", room->moves_san[i]);
        }
    }
    if (room->move_count % 2 != 0 && offset < (int)max_size - 2) {
        offset += snprintf(log_content + offset, max_size - offset, "\n");
    }

    if (offset < (int)max_size - 64) {
        snprintf(log_content + offset, max_size - offset,
            "============================================================\n"
            "End of report.\n");
    }

    return true;
}
