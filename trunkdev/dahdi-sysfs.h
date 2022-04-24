#pragma once

char *sysfs_read_attr(const char *path);

int dahdi_span_get_basechan(unsigned int spanno);
int dahdi_span_get_channels(unsigned int spanno);
char *dahdi_span_get_name(unsigned int spanno);
char *dahdi_span_get_desc(unsigned int spanno);
char *dahdi_span_get_spantype(unsigned int spanno);
