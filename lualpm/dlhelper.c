/*
 *  dlhelper.c
 *
 *  This file contains code from pacman (pacman.c and util.c). 
 *  Only minor changes were done: e.g. removed pacman specific pm_printf
 *  Praise the Pacman dev team for their great code.
 *
 *  Copyright (c) 2010 rck <dev.rck@gmail.com>
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <alpm_list.h>

#include "dlhelper.h"

static char *get_filename(const char *url) {
	char *filename = strrchr(url, '/');
	if(filename != NULL) {
		filename++;
	}
	return(filename);
}

static char *get_destfile(const char *path, const char *filename) {
	char *destfile;
	/* len = localpath len + filename len + null */
	size_t len = strlen(path) + strlen(filename) + 1;
	destfile = calloc(len, sizeof(char));
	snprintf(destfile, len, "%s%s", path, filename);

	return(destfile);
}

static char *get_tempfile(const char *path, const char *filename) {
	char *tempfile;
	/* len = localpath len + filename len + '.part' len + null */
	size_t len = strlen(path) + strlen(filename) + 6;
	tempfile = calloc(len, sizeof(char));
	snprintf(tempfile, len, "%s%s.part", path, filename);

	return(tempfile);
}

static char *strreplace(const char *str, const char *needle, const char *replace)
{
	const char *p = NULL, *q = NULL;
	char *newstr = NULL, *newp = NULL;
	alpm_list_t *i = NULL, *list = NULL;
	size_t needlesz = strlen(needle), replacesz = strlen(replace);
	size_t newsz;

	if(!str) {
		return(NULL);
	}

	p = str;
	q = strstr(p, needle);
	while(q) {
		list = alpm_list_add(list, (char *)q);
		p = q + needlesz;
		q = strstr(p, needle);
	}

	/* no occurences of needle found */
	if(!list) {
		return(strdup(str));
	}
	/* size of new string = size of old string + "number of occurences of needle"
	 * x "size difference between replace and needle" */
	newsz = strlen(str) + 1 +
		alpm_list_count(list) * (replacesz - needlesz);
	newstr = malloc(newsz);
	if(!newstr) {
		return(NULL);
	}
	*newstr = '\0';

	p = str;
	newp = newstr;
	for(i = list; i; i = alpm_list_next(i)) {
		q = alpm_list_getdata(i);
		if(q > p){
			/* add chars between this occurence and last occurence, if any */
			strncpy(newp, p, q - p);
			newp += q - p;
		}
		strncpy(newp, replace, replacesz);
		newp += replacesz;
		p = q + needlesz;
	}
	alpm_list_free(list);

	if(*p) {
		/* add the rest of 'p' */
		strcpy(newp, p);
		newp += strlen(p);
	}
	*newp = '\0';

	return(newstr);
}

/** External fetch callback */
int download_with_xfercommand(const char *url, const char *localpath,
		int force) {
	int ret = 0;
	int retval;
	int usepart = 0;
	struct stat st;
	char *parsedcmd,*tempcmd;
	char cwd[PATH_MAX];
	char *destfile, *tempfile, *filename;

	if(!xfercommand) {
		return -1;
	}

	filename = get_filename(url);
	if(!filename) {
		return -1;
	}
	destfile = get_destfile(localpath, filename);
	tempfile = get_tempfile(localpath, filename);

	if(force && stat(tempfile, &st) == 0) {
		unlink(tempfile);
	}
	if(force && stat(destfile, &st) == 0) {
		unlink(destfile);
	}

	tempcmd = strdup(xfercommand);
	/* replace all occurrences of %o with fn.part */
	if(strstr(tempcmd, "%o")) {
		usepart = 1;
		parsedcmd = strreplace(tempcmd, "%o", tempfile);
		free(tempcmd);
		tempcmd = parsedcmd;
	}
	/* replace all occurrences of %u with the download URL */
	parsedcmd = strreplace(tempcmd, "%u", url);
	free(tempcmd);

	/* cwd to the download directory */
	getcwd(cwd, PATH_MAX);
	if(chdir(localpath)) {
		ret = -1;
		goto cleanup;
	}
	/* execute the parsed command via /bin/sh -c */
	retval = system(parsedcmd);

	if(retval == -1) {
		ret = -1;
	} else if(retval != 0) {
		/* download failed */
		ret = -1;
	} else {
		/* download was successful */
		if(usepart) {
			rename(tempfile, destfile);
		}
		ret = 0;
	}

cleanup:
	chdir(cwd);
	if(ret == -1) {
		/* hack to let an user the time to cancel a download */
		sleep(2);
	}
	free(destfile);
	free(tempfile);
	free(parsedcmd);

	return(ret);
}
