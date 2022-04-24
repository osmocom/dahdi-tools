
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>

#include "dahdi-sysfs.h"

/* read a sysfs attribute from a file, stripping any trailing newline */
char *sysfs_read_attr(const char *path)
{
	FILE *f;
	char *buf;

	f = fopen(path, "r");
	if (!f)
		return NULL;

	buf = malloc(256);
	if (!buf)
		goto out_close;

	if (!fgets(buf, 256, f))
		goto out_free;

	if (strlen(buf) && buf[strlen(buf)-1] == '\n')
		buf[strlen(buf)-1] = '\0';

	fclose(f);
	printf("%s: %s\n", path, buf);
	return buf;

out_free:
	free(buf);
out_close:
	fclose(f);
	return NULL;
}

static char *sysfs_read_span_attr(unsigned int spanno, const char *name)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "/sys/bus/dahdi_spans/devices/span-%u/%s", spanno, name);
	return sysfs_read_attr(path);
}

static int sysfs_read_span_attr_int(unsigned int spanno, const char *name)
{
	char *attr;
	int attr_int;

	attr = sysfs_read_span_attr(spanno, name);
	if (!attr)
		return -1;

	attr_int = atoi(attr);
	free(attr);

	return attr_int;
}

/* return number of channels in span */
int dahdi_span_get_basechan(unsigned int spanno)
{
	return sysfs_read_span_attr_int(spanno, "basechan");
}

/* return number of channels in span */
int dahdi_span_get_channels(unsigned int spanno)
{
	return sysfs_read_span_attr_int(spanno, "channels");
}

char *dahdi_span_get_name(unsigned int spanno)
{
	return sysfs_read_span_attr(spanno, "name");
}

char *dahdi_span_get_desc(unsigned int spanno)
{
	return sysfs_read_span_attr(spanno, "desc");
}

char *dahdi_span_get_spantype(unsigned int spanno)
{
	return sysfs_read_span_attr(spanno, "spantype");
}
