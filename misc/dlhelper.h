#ifndef DLHELPER_H
#define DLHELPER_H
extern const char *xfercommand;

int download_with_xfercommand(const char *url, const char *localpath,
		int force);

#endif
