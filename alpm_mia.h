#ifndef _ALPM_MIA_H
#define _ALPM_MIA_H

/* These are missing in alpm.h */

/* from deps.h */
struct __pmdepend_t {
	pmdepmod_t mod;
	char *name;
	char *version;
};

struct __pmdepmissing_t {
	char *target;
	pmdepend_t *depend;
	char *causingpkg; /* this is used in case of remove dependency error only */
};

/* from group.h */
struct __pmgrp_t {
	/*group name*/
	char *name;
	/*list of pmpkg_t packages*/
	alpm_list_t *packages;
};

/* from sync.h */
struct __pmsyncpkg_t {
	pmpkgreason_t newreason;
	pmpkg_t *pkg;
	alpm_list_t *removes;
};

/* from conflicts.h */

struct __pmconflict_t {
    char *package1;
    char *package2;
    char *reason;
};

struct __pmfileconflict_t {
    char *target;
    pmfileconflicttype_t type;
    char *file;
    char *ctarget;
};

#endif /* _ALPM_MIA_H */
