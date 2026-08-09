#ifndef LICENSE_MANAGER_H
#define LICENSE_MANAGER_H
#include <stdint.h>
void lm_init(void);
int  lm_issue_license(uint64_t node_id);
int  lm_renew_license(uint64_t node_id);
void lm_update_reputation(uint64_t node_id, double delta);
void lm_decay_all(void);
void lm_print_status(void);
#endif
