#ifndef __ENCODER_H__
#define __ENCODER_H__

int encoder_gbk_to_utf8(const char *gbk, char *utf8, int max_len);
int encoder_utf8_to_gbk(const char *utf8, char *gbk, int max_len);

#endif