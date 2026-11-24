#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <stdbool.h>
#include "protocol.h"

typedef struct {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    int score;
    int wins;
    int draws;
    int losses;
    int rank;
} Account;

bool account_init(const char *filename);
bool account_register(const char *username, const char *password, char *err_msg, size_t err_size);
bool account_login(const char *username, const char *password, Account **out_account, char *err_msg, size_t err_size);
void account_update_score(const char *username, int score_delta, int result_type);
int account_get_rank(const char *username);
bool account_can_challenge(const char *challenger, const char *target, int *diff_out);
int account_get_all_count(void);
Account* account_get_by_index(int idx);
Account* account_find(const char *username);
void account_recalculate_ranks(void);

#endif
