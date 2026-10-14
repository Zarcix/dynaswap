#ifndef DYNASWAP_COMMON
#define DYNASWAP_COMMON

/*
 * This should only contain stuff that is used LITERALLY everywhere
 * No actual code will be defined here (hopefully)
 */

#define log_err(fmt, ...) pr_err("[dynaswap::%s] " fmt "\n", __FILE__, ##__VA_ARGS__)
#define log_debug(fmt, ...) pr_debug("[dynaswap::%s] " fmt "\n", __FILE__, ##__VA_ARGS__)
#define log_info(fmt, ...) pr_info("[dynaswap::%s] " fmt "\n", __FILE__, ##__VA_ARGS__)
#define log_alert(fmt, ...) pr_alert("[dynaswap::%s] " fmt "\n", __FILE__, ##__VA_ARGS__)
#define log_emerg(fmt, ...) pr_emerg("[dynaswap::%s] " fmt "\n", __FILE__, ##__VA_ARGS__)

#endif