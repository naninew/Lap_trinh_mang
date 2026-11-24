#include "account.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_ACCOUNTS 256
static Account g_accounts[MAX_ACCOUNTS];
static int g_account_count = 0;
static char g_account_filename[MAX_FILENAME_LEN] = "accounts.txt";

static void trim_str(char *s) {
    if (!s) return;
    int len = (int)strlen(s);
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n' || isspace((unsigned char)s[len - 1]))) {
        s[len - 1] = '\0';
        len--;
    }
}

static int compare_accounts(const void *a, const void *b) {
    const Account *accA = (const Account *)a;
    const Account *accB = (const Account *)b;
    if (accB->score != accA->score) return accB->score - accA->score;
    if (accB->wins != accA->wins) return accB->wins - accA->wins;
    return strcmp(accA->username, accB->username);
}

void account_recalculate_ranks(void) {
    if (g_account_count <= 0) return;
    qsort(g_accounts, g_account_count, sizeof(Account), compare_accounts);
    for (int i = 0; i < g_account_count; i++) {
        g_accounts[i].rank = i + 1;
    }
}

static bool save_accounts_to_file(void) {
    FILE *fp = fopen(g_account_filename, "w");
    if (!fp) return false;
    fprintf(fp, "# Username Password Score Wins Draws Losses\n");
    for (int i = 0; i < g_account_count; i++) {
        fprintf(fp, "%s %s %d %d %d %d\n",
                g_accounts[i].username,
                g_accounts[i].password,
                g_accounts[i].score,
                g_accounts[i].wins,
                g_accounts[i].draws,
                g_accounts[i].losses);
    }
    fclose(fp);
    return true;
}

bool account_init(const char *filename) {
    if (filename && strlen(filename) > 0) {
        strncpy(g_account_filename, filename, sizeof(g_account_filename) - 1);
    }
    g_account_count = 0;
    FILE *fp = fopen(g_account_filename, "r");
    if (!fp) {
        /* Create initial accounts file if not exists */
        account_register("grandmaster", "pass123", NULL, 0);
        account_register("alice", "pass123", NULL, 0);
        account_register("bob", "pass123", NULL, 0);
        account_register("charlie", "pass123", NULL, 0);
        account_register("david", "pass123", NULL, 0);
        account_register("eva", "pass123", NULL, 0);
        account_register("frank", "pass123", NULL, 0);
        account_register("grace", "pass123", NULL, 0);
        account_register("helen", "pass123", NULL, 0);
        account_register("ian", "pass123", NULL, 0);
        account_register("jack", "pass123", NULL, 0);
        account_register("novice_user", "pass123", NULL, 0);
        return true;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        trim_str(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        if (g_account_count >= MAX_ACCOUNTS) break;

        Account acc;
        memset(&acc, 0, sizeof(Account));
        if (sscanf(line, "%31s %63s %d %d %d %d",
                   acc.username, acc.password,
                   &acc.score, &acc.wins, &acc.draws, &acc.losses) >= 2) {
            g_accounts[g_account_count++] = acc;
        }
    }
    fclose(fp);
    account_recalculate_ranks();
    return true;
}

Account* account_find(const char *username) {
    if (!username) return NULL;
    for (int i = 0; i < g_account_count; i++) {
        if (strcmp(g_accounts[i].username, username) == 0) {
            return &g_accounts[i];
        }
    }
    return NULL;
}

bool account_register(const char *username, const char *password, char *err_msg, size_t err_size) {
    if (!username || strlen(username) == 0 || !password || strlen(password) == 0) {
        if (err_msg) snprintf(err_msg, err_size, "Username and password cannot be empty");
        return false;
    }
    if (account_find(username) != NULL) {
        if (err_msg) snprintf(err_msg, err_size, "Username already exists");
        return false;
    }
    if (g_account_count >= MAX_ACCOUNTS) {
        if (err_msg) snprintf(err_msg, err_size, "Account database full");
        return false;
    }

    Account acc;
    memset(&acc, 0, sizeof(Account));
    strncpy(acc.username, username, sizeof(acc.username) - 1);
    strncpy(acc.password, password, sizeof(acc.password) - 1);
    acc.score = 100;
    acc.wins = 0;
    acc.draws = 0;
    acc.losses = 0;
    g_accounts[g_account_count++] = acc;

    account_recalculate_ranks();
    save_accounts_to_file();
    return true;
}

bool account_login(const char *username, const char *password, Account **out_account, char *err_msg, size_t err_size) {
    Account *acc = account_find(username);
    if (!acc) {
        if (err_msg) snprintf(err_msg, err_size, "Account does not exist");
        return false;
    }
    if (strcmp(acc->password, password) != 0) {
        if (err_msg) snprintf(err_msg, err_size, "Incorrect password");
        return false;
    }
    if (out_account) *out_account = acc;
    return true;
}

void account_update_score(const char *username, int score_delta, int result_type) {
    Account *acc = account_find(username);
    if (!acc) return;
    acc->score += score_delta;
    if (result_type == 1) acc->wins++;
    else if (result_type == 2) acc->losses++;
    else if (result_type == 0) acc->draws++;

    account_recalculate_ranks();
    save_accounts_to_file();
}

int account_get_rank(const char *username) {
    Account *acc = account_find(username);
    return acc ? acc->rank : -1;
}

bool account_can_challenge(const char *challenger, const char *target, int *diff_out) {
    Account *a = account_find(challenger);
    Account *b = account_find(target);
    if (!a || !b) return false;
    int diff = abs(a->rank - b->rank);
    if (diff_out) *diff_out = diff;
    return diff <= RANK_DIFF_LIMIT;
}

int account_get_all_count(void) {
    return g_account_count;
}

Account* account_get_by_index(int idx) {
    if (idx >= 0 && idx < g_account_count) return &g_accounts[idx];
    return NULL;
}
